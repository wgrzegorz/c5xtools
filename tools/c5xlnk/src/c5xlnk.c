/*
 *         ___|       |               |
 *     __| __ \ \  / __|  _ \   _ \  |  __|
 *    (      ) |`  <  |   (   | (   | |\__ \ 
 *   \___|____/ _/\_\\__|\___/ \___/ _|____/
 * ======================================dSc==
 *   Brought to you by + Cult of the Modem +
 *
 * TMS320C5x assembler/linker/disassembler/hex
 *
 * SPDX-License-Identifier: MIT
 * Author:  Grzegorz Worona <grzegorz@worona.pl>
 * Version: 1.0.1
 *
 * c5xlnk - TMS320C5x COFF linker (native, POSIX C99).
 *
 * Reads TI COFF v2 relocatable objects (as produced by c5xasm), places
 * their .text/.data/.bss sections, resolves symbols, applies relocations, and
 * writes a linked image plus a map. The default memory map is:
 *   PAGE 0 PROG  origin 0x0800   (.text then .data, contiguous)
 *   PAGE 1 DATA  origin 0x0800   (.bss)
 *
 * Relocation model (TI COFF .out layout): each entry is
 *   { vaddr(4, section-relative), symndx(4), type(2) }.
 * symndx == 0xFFFFFFFF relocates by the containing section's final base; a real
 * symbol index relocates by that symbol's final address. The stored word is the
 * section-relative value; the linker adds the relocation base.
 *
 * v0.7: objects + object libraries (-l/-i/-u/-x), default allocation,
 *        R_RELWORD/section relocs, COFF-v1 .out (-a/-r/-ar) + command file
 *        (MEMORY/SECTIONS, GROUP/UNION, overlay pages, load!=run,
 *        NOLOAD/COPY, link-time sym assignment) + map.
 * v0.9: DSECT (0x01, dummy: placed, not written, not allocated); -v0 (version-0 COFF,
 *        20-byte file header); -h (globals -> C_STAT); -b (clear debug-merge flag 0x1000).
 *        Build: cc -std=c99 -Wall -Wextra -O2 -o c5xlnk c5xlnk.c
 * Usage: c5xlnk obj... [-o out.out] [-m map.map] [-text ADDR] [-data ADDR] [-bss ADDR]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "portable.h"
#include "c5xcoff.h"
#include "c5xbanner.h"

/* COFF byte I/O comes from the shared module (c5xcoff/c5xcoff.c): the short local names
 * used throughout this file alias the shared primitives, so the byte layout lives
 * in exactly one place (no linker/assembler/disassembler drift). */
#define u32   c5xcoff_rd32
#define u16   c5xcoff_rd16
#define p16   c5xcoff_put16
#define p32   c5xcoff_put32
#define pname c5xcoff_w_name

/* Apply one relocation to an instruction/data word, driven by the shared reloc-kind
 * table (c5xcoff.h / M2). A full-word kind (R_RELWORD) stores the section-relative
 * offset in the word and the final value is word + base, so base is simply ADDED.
 * A partial kind (R_PARTLS7/R_PARTMS9/R_REL/R_RELBYTE) carries the full value as an
 * explicit addend in the reloc reserved field; the final value is base + addend and
 * only the kind's narrow bit-field (shifted) is replaced - never touching the opcode
 * bits above it. This makes the linker correctly link any relocation the assembler
 * emits, not just full words. */
/* Set when an R_RELBYTE relocation's final value overflows its 8-bit field. The reference linker
 * then clears F_EXEC in the output file header (the LACK/ADDK/SUBK/LARK constant was
 * truncated), while leaving the relocated data unchanged. Reverse-engineered: only
 * R_RELBYTE triggers it (R_REL 13-bit overflow does not), threshold final > 0xFF. */
static int g_reloc_overflow = 0;
static uint16_t c5xcoff_apply(uint16_t w, uint32_t base, uint16_t addend, uint16_t type){
    const C5xCoffRelocKind *k = c5xcoff_reloc_kind(type);
    if(!k || !k->uses_addend) return (uint16_t)(w + base);
    uint32_t fin = base + addend;
    if(type == R_RELBYTE && fin > 0xFFu) g_reloc_overflow = 1;
    uint32_t mask = (k->field_bits >= 16) ? 0xFFFFu : ((1u << k->field_bits) - 1u);
    return (uint16_t)((w & ~mask) | (((fin >> k->shift)) & mask));
}

typedef struct { char name[9]; uint32_t size, scnptr, relptr; uint32_t nreloc; uint32_t flags;
                 uint32_t vaddr; uint16_t *data; uint32_t base; const uint8_t*rel; } Scn;
typedef struct { char name[9]; long value; int scnum; int cls; long final; } Sym;

typedef struct {
    const char*path; uint8_t*buf; long len;
    int nscns; Scn scn[16];
    int nsyms; Sym *sym;
} Obj;

/* Parse a COFF object whose bytes are already in o->buf (length o->len, name o->path).
 * Hardened against malformed/truncated input: every field read from the file is
 * bounds-checked against o->len before use, and the file is rejected cleanly (no
 * out-of-bounds read, no oversized allocation) rather than crashing. The success
 * path for a well-formed object is unchanged. */
static int c5xcoff_parse(Obj*o){
    uint8_t*d=o->buf; long len=o->len;
    if(len<2){ fprintf(stderr,"%s: too short for a COFF header\n",o->path); return 0; }
    uint16_t magic=u16(d); if(magic!=COFF_MAGIC_V2 && magic!=COFF_MAGIC_V1){ fprintf(stderr,"%s: not TI COFF (magic %04x)\n",o->path,magic); return 0; }
    int v2=(magic==COFF_MAGIC_V2);
    int fhsz=v2?22:20, shsz=v2?48:40;
    if(len<fhsz){ fprintf(stderr,"%s: truncated file header\n",o->path); return 0; }
    o->nscns=u16(d+2); uint32_t symptr=u32(d+8); long nsyms=(long)u32(d+12); uint16_t opthdr=u16(d+16);
    if(o->nscns<0 || o->nscns>(int)(sizeof o->scn/sizeof o->scn[0])){ fprintf(stderr,"%s: bad section count %d\n",o->path,o->nscns); return 0; }
    long base=(long)fhsz+opthdr;
    if(base<0 || base + (long)shsz*o->nscns > len){ fprintf(stderr,"%s: truncated section headers\n",o->path); return 0; }
    for(int i=0;i<o->nscns;i++){ uint8_t*s=d+base+(long)shsz*i; Scn*sc=&o->scn[i];
        memcpy(sc->name,s,8); sc->name[8]=0; for(char*c=sc->name+7;c>=sc->name&&*c==0;c--)*c=0;
        sc->vaddr=u32(s+12); sc->size=u32(s+16); sc->scnptr=u32(s+20); sc->relptr=u32(s+24);
        if(v2){ sc->nreloc=u32(s+32); sc->flags=u32(s+40); } else { sc->nreloc=u16(s+32); sc->flags=u32(s+36); }
        if(!sc->relptr) sc->nreloc=0;   /* no relocation table -> no entries to walk */
        if(sc->scnptr && (long)sc->scnptr + 2*(long)sc->size > len){ fprintf(stderr,"%s: section %d data out of range\n",o->path,i); return 0; }
        if(sc->relptr && (long)sc->relptr + 12*(long)sc->nreloc > len){ fprintf(stderr,"%s: section %d relocations out of range\n",o->path,i); return 0; }
        sc->data=sc->size?malloc((size_t)sc->size*2):NULL;
        if(sc->size && !sc->data){ fprintf(stderr,"%s: out of memory (section %d, %u words)\n",o->path,i,sc->size); return 0; }
        for(uint32_t w=0;w<sc->size;w++) sc->data[w]=sc->scnptr?u16(d+sc->scnptr+2*w):0;
        sc->rel=sc->relptr?d+sc->relptr:NULL;
    }
    /* symbol table: must lie within the file; otherwise treat as absent */
    if(nsyms<0 || symptr<(uint32_t)fhsz || (long)symptr + 18*nsyms > len) nsyms=0;
    o->nsyms=(int)nsyms;
    long strbase=(long)symptr + 18*nsyms;      /* string table follows the symbols */
    o->sym=calloc(nsyms>0?(size_t)nsyms:1,sizeof(Sym));
    for(int i=0;i<o->nsyms;i++){ uint8_t*s=d+symptr+18*i; Sym*sy=&o->sym[i];
        if(u32(s)==0){ long soff=strbase+(long)u32(s+4); int k=0;   /* name in the string table */
            if(soff>=0 && soff<len){ const char*str=(const char*)(d+soff); long maxn=len-soff;
                while(k<8 && k<maxn && str[k]){ sy->name[k]=str[k]; k++; } }
            sy->name[k]=0; }
        else { memcpy(sy->name,s,8); sy->name[8]=0; }
        sy->value=(long)u32(s+8); sy->scnum=(int16_t)u16(s+12); sy->cls=s[16];
        int naux=s[17]; i+=naux; /* skip aux entries (bounded by the loop condition) */
    }
    return 1;
}

static int load_obj(const char*path, Obj*o){
    FILE*f=fopen(path,"rb"); if(!f){perror(path);return 0;}
    fseek(f,0,SEEK_END); o->len=ftell(f); fseek(f,0,SEEK_SET);
    if(o->len<=0){ fprintf(stderr,"%s: empty or unreadable\n",path); fclose(f); return 0; }
    o->buf=malloc((size_t)o->len); if(!o->buf){ fprintf(stderr,"%s: out of memory\n",path); fclose(f); return 0; }
    if(fread(o->buf,1,o->len,f)!=(size_t)o->len){fclose(f);return 0;} fclose(f);
    o->path=path;
    return c5xcoff_parse(o);
}

/* ---------------- object libraries (COFF archives): -l / -i / -x / -u ---------- */
/* A TI COFF archive is a Unix "!<arch>\n" archive. c5xlnk resolves library members by
 * scanning each member's own COFF symbol table for a definition of a currently
 * undefined symbol (the archive symbol directory is only an index and is not required
 * for correctness). A pulled member becomes an ordinary linked object. */
#define MAXLIB 32
static const char *g_lib[MAXLIB]; static int g_nlib=0;     /* -l names */
static const char *g_idir[MAXLIB]; static int g_nidir=0;   /* -i search dirs */
static const char *g_usym[MAXLIB]; static int g_nusym=0;   /* -u forced undefineds */
static int g_exhaustive=0;                                 /* -x */
static int g_v0=0;        /* -v0: emit version-0 COFF (20-byte file header, no version word) */
static int g_bnomerge=0;  /* -b: disable symbolic-debug merge (clear file flag 0x1000) */
static int g_hstatic=0;   /* -h: make all global (C_EXT) output symbols static (C_STAT) */

/* one archive member: a byte slice of the archive buffer */
typedef struct { char name[17]; const uint8_t*data; long size; int pulled; } Member;

/* open a -l library, trying the name as given then each -i dir, then a .lib suffix */
static uint8_t* lib_open(const char*name, long*lenp, char*foundpath, size_t foundpathsz){
    char cand[1024]; FILE*f=NULL;
    const char*suf[2]={"",".lib"};
    for(int s=0;s<2 && !f;s++){
        snprintf(cand,sizeof cand,"%s%s",name,suf[s]); f=fopen(cand,"rb");
        for(int d=0; !f && d<g_nidir; d++){ snprintf(cand,sizeof cand,"%s/%s%s",g_idir[d],name,suf[s]); f=fopen(cand,"rb"); }
    }
    if(!f){ fprintf(stderr,"c5xlnk: cannot find library '%s'\n",name); return NULL; }
    snprintf(foundpath,foundpathsz,"%s",cand);
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if(n<=0){ fprintf(stderr,"c5xlnk: library '%s' empty or unreadable\n",name); fclose(f); return NULL; }
    uint8_t*b=malloc((size_t)n); if(!b){ fclose(f); return NULL; }
    if(fread(b,1,n,f)!=(size_t)n){fclose(f);free(b);return NULL;} fclose(f);
    *lenp=n; return b;
}

/* enumerate archive members into m[] (skipping the "/" symbol table and "//" string table) */
static int ar_members(const uint8_t*b, long len, Member*m, int maxm){
    if(len<8 || memcmp(b,"!<arch>\n",8)) { fprintf(stderr,"c5xlnk: not an archive\n"); return -1; }
    long p=8; int n=0;
    while(p+60<=len && n<maxm){
        const uint8_t*h=b+p;
        char nm[17]; memcpy(nm,h,16); nm[16]=0;
        /* member name ends at '/' (GNU) or trailing spaces */
        for(int i=0;i<16;i++){ if(nm[i]=='/'){ nm[i]=0; break; } }
        for(int i=15;i>=0 && (nm[i]==' '||nm[i]==0);i--) nm[i]=0;
        char szs[11]; memcpy(szs,h+48,10); szs[10]=0; long sz=strtol(szs,NULL,10);
        long body=p+60;
        /* a member's declared size must be non-negative and fit the remaining
         * bytes - otherwise c5xcoff_parse would read past the archive buffer, and a
         * negative size would stall the p advance below (DoS). Reject and stop. */
        if(sz<0 || body>len || sz>len-body){ fprintf(stderr,"c5xlnk: archive member out of range\n"); break; }
        if(nm[0] && strcmp(nm,"/") && strcmp(nm,"//") && strcmp(nm,"__.SYMDEF")){
            snprintf(m[n].name,sizeof m[n].name,"%s",nm); m[n].data=b+body; m[n].size=sz; m[n].pulled=0; n++;
        }
        p=body+sz+(sz&1);   /* members are padded to an even boundary */
    }
    return n;
}

/* does object o define a global symbol named nm (C_EXT, scnum>0)? */
static int obj_defines(const Obj*o, const char*nm){
    for(int j=0;j<o->nsyms;j++){ const Sym*sy=&o->sym[j];
        if(sy->cls==2 && sy->scnum>0 && !strcmp(sy->name,nm)) return 1; }
    return 0;
}

/* true if NM is defined by none of the nobj linked objects */
static int syms_undefined(const Obj *O, int nobj, const char *nm){
    for(int i=0;i<nobj;i++) if(obj_defines(&O[i], nm)) return 0;
    return 1;
}

/* local global-symbol map: symbol name -> final address */
typedef struct { char name[9]; long final; } LnkGmap;

/* final address of NM in G[0..nG), or dflt if NM is not present */
static long gmap_final(const LnkGmap *G, int nG, const char *nm, long dflt){
    for(int k=0;k<nG;k++) if(!strcmp(G[k].name, nm)) return G[k].final;
    return dflt;
}

/* ---------------- linker command file: MEMORY / SECTIONS ---------------- */
typedef struct { char name[16]; int page; uint32_t origin, length, cursor; } Region;
static Region g_reg[32]; static int g_nreg=0;
typedef struct { char sec[16]; char region[16]; } SecMap;
static SecMap g_smap[64]; static int g_nsmap=0;
/* ordered SECTIONS directives (for GROUP/UNION/overlay-page placement) */
typedef struct { int kind; char member[16][9]; int nmem; char region[16]; char loadreg[16]; char runreg[16];
                 int page; int havepage; int type; } SecDir;
static SecDir g_dir[64]; static int g_ndir=0;   /* kind: 0 plain, 1 GROUP, 2 UNION; type: 0 normal, 1 NOLOAD, 2 COPY */
/* link-time symbol assignment: name = term (+|- term)* ; (terms: number, symbol, or '.') */
typedef struct { char name[33]; char term[16][33]; char sign[16]; int nterm; long value; int done; } Assign;
static Assign g_asym[64]; static int g_nasym=0;
static Region* reg_find(const char*n){ for(int i=0;i<g_nreg;i++) if(!strcmp(g_reg[i].name,n)) return &g_reg[i]; return NULL; }
static const char* smap_region(const char*sec){ for(int i=0;i<g_nsmap;i++) if(!strcmp(g_smap[i].sec,sec)) return g_smap[i].region; return NULL; }

/* tokenizer over a command file: whitespace/punctuation-separated, C comments stripped,
 * with one-token pushback. */
static char *g_cmdbuf, *g_ctok, *g_pushed;
static char* ctok(void){
    static char t[256];
    if(g_pushed){ char*r=g_pushed; g_pushed=NULL; return r; }
    for(;;){ while(*g_ctok==' '||*g_ctok=='\t'||*g_ctok=='\n'||*g_ctok=='\r')g_ctok++;
        if(g_ctok[0]=='/'&&g_ctok[1]=='*'){ g_ctok+=2; while(*g_ctok&&!(g_ctok[0]=='*'&&g_ctok[1]=='/'))g_ctok++; if(*g_ctok)g_ctok+=2; continue; }
        break; }
    if(!*g_ctok) return NULL;
    if(strchr("{}:,=>();",*g_ctok)){ t[0]=*g_ctok++; t[1]=0; return t; }
    int n=0; while(*g_ctok&&!strchr(" \t\n\r{}:,=>();",*g_ctok)&&n<255) t[n++]=*g_ctok++; t[n]=0; return t;
}
static void cpush(char*t){ g_pushed=t; }
static uint32_t cnum(const char*s){ if(!s)return 0; if(s[0]=='0'&&(s[1]=='x'||s[1]=='X'))return (uint32_t)strtoul(s+2,NULL,16); return (uint32_t)strtoul(s,NULL,0); }
static int iskey(const char*s,const char*full,const char*ab){ return !strcasecmp(s,full)||(ab&&!strcasecmp(s,ab)); }

/* parse the RHS of "name = expr ;" into a signed-term list (terms are numbers, symbols,
 * or '.'). Tokens are concatenated (whitespace-insensitive) then split on +/-. */
static void parse_assign(const char*name){
    char buf[512]; int bn=0; char*t;
    while((t=ctok()) && strcmp(t,";")){ for(const char*p=t;*p&&bn<511;p++) buf[bn++]=*p; }
    buf[bn]=0;
    if(g_nasym>=64) return; Assign*A=&g_asym[g_nasym++]; memset(A,0,sizeof *A);
    strncpy(A->name,name,32);
    int i=0; char sign='+';
    while(buf[i]){
        if(buf[i]=='+'||buf[i]=='-'){ sign=buf[i]; i++; continue; }
        char term[33]; int tn=0; while(buf[i]&&buf[i]!='+'&&buf[i]!='-'&&tn<32) term[tn++]=buf[i++]; term[tn]=0;
        if(tn&&A->nterm<16){ snprintf(A->term[A->nterm],sizeof A->term[A->nterm],"%s",term); A->sign[A->nterm]=sign; A->nterm++; sign='+'; }
    }
}

static int parse_cmd(const char*path, const char*objv[], int*nobj, const char**outp, const char**mapp, const char**entry){
    FILE*f=fopen(path,"rb"); if(!f){perror(path);return 0;}
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    g_cmdbuf=malloc(n+1); if(fread(g_cmdbuf,1,n,f)!=(size_t)n){fclose(f);return 0;} g_cmdbuf[n]=0; fclose(f);
    g_ctok=g_cmdbuf; g_pushed=NULL; char*t;
    while((t=ctok())){
        if(!strcasecmp(t,"MEMORY")){
            char*o=ctok(); if(!o||strcmp(o,"{"))continue;
            int page=0;
            for(;;){ char*k=ctok(); if(!k||!strcmp(k,"}"))break;
                if(!strcasecmp(k,"PAGE")){ page=cnum(ctok()); char*c=ctok(); (void)c; continue; } /* PAGE n : */
                /* region: NAME [(attrs)] : key=val[, key=val ...] */
                Region*r = (g_nreg<32)? &g_reg[g_nreg] : &g_reg[0]; memset(r,0,sizeof *r);
                strncpy(r->name,k,15); r->page=page;
                char*p=ctok();
                if(p&&!strcmp(p,"(")){ while((p=ctok())&&strcmp(p,")")){} p=ctok(); } /* skip (attrs) */
                if(p&&strcmp(p,":")) cpush(p);                                        /* optional ':' */
                for(;;){ char*key=ctok(); if(!key)break;
                    if(iskey(key,"origin","o")){ char*eq=ctok(); if(eq&&strcmp(eq,"="))cpush(eq); r->origin=cnum(ctok()); }
                    else if(iskey(key,"length","l")||iskey(key,"len",NULL)){ char*eq=ctok(); if(eq&&strcmp(eq,"="))cpush(eq); r->length=cnum(ctok()); }
                    else if(iskey(key,"fill","f")){ char*eq=ctok(); if(eq&&strcmp(eq,"="))cpush(eq); ctok(); }
                    else if(!strcmp(key,",")){ }
                    else { cpush(key); break; }    /* next region name / PAGE / } */
                }
                r->cursor=r->origin; if(g_nreg<32)g_nreg++;
            }
        } else if(!strcasecmp(t,"SECTIONS")){
            char*o=ctok(); if(!o||strcmp(o,"{"))continue;
            for(;;){ char*s0=ctok(); if(!s0||!strcmp(s0,"}"))break;
                char s[256]; strncpy(s,s0,255); s[255]=0;   /* ctok reuses a static buffer: copy before peeking */
                /* symbol assignment inside SECTIONS: name = expr ; */
                { char*nx=ctok(); if(nx&&!strcmp(nx,"=")){ parse_assign(s); continue; } if(nx)cpush(nx); }
                SecDir*D = (g_ndir<64)? &g_dir[g_ndir] : &g_dir[0]; memset(D,0,sizeof *D);
                int grouped = !strcasecmp(s,"GROUP")||!strcasecmp(s,"UNION");
                D->kind = !strcasecmp(s,"GROUP")?1 : !strcasecmp(s,"UNION")?2 : 0;
                char sec[16]={0};
                if(grouped){
                    /* GROUP|UNION [:] { m1 m2 ... } > REGION [PAGE n] */
                    char*p=ctok(); if(p&&!strcmp(p,":"))p=ctok();
                    if(p&&!strcmp(p,"{")){ for(;;){ char*m=ctok(); if(!m||!strcmp(m,"}"))break;
                        if(D->nmem<16) strncpy(D->member[D->nmem++],m,8); } }
                } else {
                    strncpy(sec,s,15); strncpy(D->member[0],s,8); D->nmem=1;
                }
                /* trailing: optional ':' , an input-spec '{...}', '> REGION', 'PAGE n' */
                char region[16]={0};
                for(;;){ char*p=ctok(); if(!p)break;
                    if(!strcmp(p,"{")){ int d=1; while(d){ char*q=ctok(); if(!q)break; if(!strcmp(q,"{"))d++; else if(!strcmp(q,"}"))d--; } }
                    else if(!strcmp(p,">")){ char*rg=ctok(); if(rg){ strncpy(region,rg,15); strncpy(D->region,rg,15);} }
                    else if(!strcasecmp(p,"load")){ char*eq=ctok(); if(eq&&strcmp(eq,"="))cpush(eq); char*rg=ctok(); if(rg){ if(!strcmp(rg,">"))rg=ctok(); if(rg)strncpy(D->loadreg,rg,15);} }
                    else if(!strcasecmp(p,"run")){ char*eq=ctok(); if(eq&&strcmp(eq,"="))cpush(eq); char*rg=ctok(); if(rg){ if(!strcmp(rg,">"))rg=ctok(); if(rg)strncpy(D->runreg,rg,15);} }
                    else if(!strcasecmp(p,"type")){ char*eq=ctok(); if(eq&&strcmp(eq,"="))cpush(eq); char*ty=ctok(); if(ty&&!strcasecmp(ty,"NOLOAD"))D->type=1; else if(ty&&!strcasecmp(ty,"COPY"))D->type=2; else if(ty&&!strcasecmp(ty,"DSECT"))D->type=3; }
                    else if(!strcasecmp(p,"PAGE")){ D->page=cnum(ctok()); D->havepage=1; }
                    else if(!strcmp(p,":")||!strcmp(p,",")){ }
                    else { cpush(p); break; }
                }
                if(g_ndir<64) g_ndir++;
                if(!grouped && g_nsmap<64){ strncpy(g_smap[g_nsmap].sec,sec,15); strncpy(g_smap[g_nsmap].region,region,15); g_nsmap++; }
            }
        } else if(!strcmp(t,"-o")){ *outp=strdup(ctok()); }
        else if(!strcmp(t,"-m")){ *mapp=strdup(ctok()); }
        else if(!strcmp(t,"-e")){ *entry=strdup(ctok()); }
        else if(!strcmp(t,"-x")){ g_exhaustive=1; }
        else if(!strncmp(t,"-l",2)){ const char*v=t[2]?strdup(t+2):strdup(ctok()); if(*v&&g_nlib<MAXLIB)g_lib[g_nlib++]=v; }
        else if(!strncmp(t,"-i",2)){ const char*v=t[2]?strdup(t+2):strdup(ctok()); if(*v&&g_nidir<MAXLIB)g_idir[g_nidir++]=v; }
        else if(!strncmp(t,"-u",2)){ const char*v=t[2]?strdup(t+2):strdup(ctok()); if(*v&&g_nusym<MAXLIB)g_usym[g_nusym++]=v; }
        else if(t[0]=='-'){ }
        else { /* bare name: either "name = expr;" (link-time symbol) or an object file */
            char nt[256]; strncpy(nt,t,255); nt[255]=0;
            char*nx=ctok();
            if(nx&&!strcmp(nx,"=")){ parse_assign(nt); }
            else { if(nx)cpush(nx); if(*nobj<64) objv[(*nobj)++]=strdup(nt); }
        }
    }
    return 1;
}

/* evaluate the link-time assignments against a name->value lookup (objects' globals and
 * other assignments), iterating so assignments can reference each other. dotval is the
 * value of '.' (best-effort current location). Returns resolved assignments. */
static long asym_lookup(const char*nm, long (*glook)(const char*,int*), long dotval, int*ok){
    if(!strcmp(nm,".")){ if(ok)*ok=1; return dotval; }
    char*end; long v=strtol(nm,&end,0); if(end!=nm&&*end==0){ if(ok)*ok=1; return v; }
    for(int i=0;i<g_nasym;i++) if(g_asym[i].done&&!strcmp(g_asym[i].name,nm)){ if(ok)*ok=1; return g_asym[i].value; }
    int gok=0; long gv=glook(nm,&gok); if(ok)*ok=gok; return gv;
}
static void asym_eval(long (*glook)(const char*,int*), long dotval){
    for(int pass=0;pass<8;pass++){ int progress=0;
        for(int a=0;a<g_nasym;a++){ Assign*A=&g_asym[a]; if(A->done)continue;
            long val=0; int allok=1;
            for(int k=0;k<A->nterm;k++){ int ok=0; long tv=asym_lookup(A->term[k],glook,dotval,&ok);
                if(!ok){ allok=0; break; } val += (A->sign[k]=='-')? -tv : tv; }
            if(allok){ A->value=val; A->done=1; progress=1; } }
        if(!progress) break; }
}

/* file-scope global-symbol map, so the link-time assignment evaluator can look up
 * object globals without threading the per-path map through function pointers */
static struct { char name[33]; long val; } g_gmap[8192]; static int g_ngmap=0;
static void gmap_add(const char*nm,long v){ if(g_ngmap<8192){ strncpy(g_gmap[g_ngmap].name,nm,32); g_gmap[g_ngmap].val=v; g_ngmap++; } }
static long gmap_look(const char*nm,int*ok){ for(int i=0;i<g_ngmap;i++) if(!strcmp(g_gmap[i].name,nm)){ if(ok)*ok=1; return g_gmap[i].val; } if(ok)*ok=0; return 0; }

/* COFF file header. v1 (default): 22 bytes = [version 0x00c1][18 common][target 0x0092].
 * v0 (-v0): 20 bytes = [magic 0x0092][18 common], no version word, no trailing target.
 * -b clears the 0x1000 (symbolic-debug-merged) flag. */
static long c5xcoff_fhsz(void){ return g_v0?20:22; }
static void c5xcoff_write_fhdr(FILE*f,unsigned nscns,unsigned long symptr,unsigned long nsyms,unsigned opthdr,unsigned flags){
    if(g_bnomerge) flags &= ~0x1000u;
    p16(f, g_v0?COFF_TARGET_C5X:COFF_MAGIC_V1);
    p16(f,nscns); p32(f,0); p32(f,symptr); p32(f,nsyms);
    p16(f,opthdr); p16(f,flags);
    if(!g_v0) p16(f,COFF_TARGET_C5X);
}

/* linked output: flat (addr,word) pairs, page-tagged */
typedef struct { int page; uint32_t addr; uint16_t word; } OW;
static OW g_ow[1<<16]; static int g_now=0;
static void ow(int page,uint32_t a,uint16_t w){ if(g_now<(1<<16)){g_ow[g_now].page=page;g_ow[g_now].addr=a;g_ow[g_now].word=w;g_now++;} }

/* ---------------- general section placement: GROUP / UNION / overlay pages -----
 * A dedicated path used when the SECTIONS script contains GROUP/UNION or any
 * non-standard (named) output section. It models an arbitrary ordered list of
 * output sections, places them per the directives, applies relocations, and writes
 * the COFF .out. Layout: GROUP members contiguous in order;
 * UNION members share a run address (paddr) with sequential load addresses (vaddr);
 * page>0 sections carry the 0x01000000 flag; unallocated .data/.bss default to page 1.
 * The loadable image (headers+data) is the validated surface; the trailing symbol
 * table is emitted minimally (nsyms=0), which does not affect that image. */
typedef struct {
    char name[9]; unsigned tflag; int page, allocated, order, noload, copy, dsect;
    uint32_t paddr, vaddr, size; uint16_t *data; int has_data;
    int oc_obj[64], oc_scn[64], noc;   /* contributing (object,section) pieces, in link order */
} GSec;

static int gsec_find(GSec*g,int ng,const char*nm){ for(int i=0;i<ng;i++) if(!strcmp(g[i].name,nm))return i; return -1; }

static int link_general(Obj*O,int nobj,const char**objs,const char*outp,const char*mapp,const char*entry,int quiet,int verbose){
    static GSec G[64]; int ng=0; (void)objs;
    /* 1. collect distinct output sections in first-seen (object) order, with contributions */
    for(int i=0;i<nobj;i++) for(int s=0;s<O[i].nscns;s++){ Scn*sc=&O[i].scn[s]; if(!sc->name[0])continue;
        int gi=gsec_find(G,ng,sc->name);
        if(gi<0){ gi=ng++; memset(&G[gi],0,sizeof G[gi]); strncpy(G[gi].name,sc->name,8);
            unsigned f=sc->flags; G[gi].tflag = (f&0x20)?0x20u : (f&0x80)?0x80u : 0x40u;
            if(!strcmp(sc->name,".text"))G[gi].tflag=0x20; else if(!strcmp(sc->name,".bss"))G[gi].tflag=0x80;
            /* default page when a command file is present: .data/.bss -> page 1, else page 0 */
            G[gi].page = (!strcmp(sc->name,".data")||!strcmp(sc->name,".bss"))?1:0;
            G[gi].order = 1000+gi; }
        if(G[gi].noc<64){ G[gi].oc_obj[G[gi].noc]=i; G[gi].oc_scn[G[gi].noc]=s; G[gi].noc++; }
        G[gi].size += sc->size;
    }
    /* 2. placement from the ordered directives */
    int seq=0;
    for(int d=0;d<g_ndir;d++){ SecDir*D=&g_dir[d];
        Region*R = D->region[0]? reg_find(D->region):NULL;
        int page = D->havepage? D->page : (R?R->page:0);
        uint32_t cur = R? R->cursor : 0;
        if(D->kind==2){ /* UNION: shared run addr (paddr), sequential load addr (vaddr) */
            uint32_t ubase=cur, usize=0, pre=0;
            for(int m=0;m<D->nmem;m++){ int gi=gsec_find(G,ng,D->member[m]); if(gi<0)continue; if(G[gi].size>usize)usize=G[gi].size; }
            for(int m=0;m<D->nmem;m++){ int gi=gsec_find(G,ng,D->member[m]); if(gi<0)continue;
                G[gi].paddr=ubase; G[gi].vaddr=ubase+usize+pre; G[gi].page=page; G[gi].allocated=1; G[gi].order=seq++;
                pre+=G[gi].size; }
            cur = ubase+usize+pre;
        } else { /* plain or GROUP: contiguous in listed order, with optional load!=run */
            Region*runR  = D->runreg[0]?  reg_find(D->runreg)  : (D->loadreg[0]?reg_find(D->loadreg):R);
            Region*loadR = D->loadreg[0]? reg_find(D->loadreg) : (D->runreg[0]?reg_find(D->runreg):R);
            int pg = D->havepage? D->page : (runR?runR->page:page);
            for(int m=0;m<D->nmem;m++){ int gi=gsec_find(G,ng,D->member[m]); if(gi<0)continue;
                if(D->type==2||D->type==3){ /* COPY/DSECT: not allocated - placed at the region cursor, cursor unchanged.
                    * COPY keeps its data (flag 0x10); DSECT is a dummy - no data in the file (scnptr=0, flag 0x01). */
                    uint32_t org = runR? runR->origin : cur;
                    G[gi].paddr=org; G[gi].vaddr=org; G[gi].page=pg; G[gi].allocated=1; G[gi].order=seq++;
                    if(D->type==2) G[gi].copy=1; else G[gi].dsect=1;
                    continue; }
                uint32_t pa = runR? runR->cursor : cur;        /* paddr = run address */
                uint32_t va = loadR? loadR->cursor : pa;       /* vaddr = load address */
                G[gi].paddr=pa; G[gi].vaddr=va; G[gi].page=pg; G[gi].allocated=1; G[gi].order=seq++;
                G[gi].noload=(D->type==1);
                if(runR) runR->cursor += G[gi].size;
                if(loadR && loadR!=runR) loadR->cursor += G[gi].size;   /* NOLOAD still advances run; load region only if distinct */
            }
            R=NULL;  /* cursors already advanced on runR/loadR */
        }
        if(R) R->cursor=cur;
    }
    /* 2b. default allocation for unallocated, non-empty, initialized sections: continue
     * the cursor of the first region on the section's page (places e.g. an
     * unmentioned named .sect right after the last allocated section in that region).
     * They keep their first-seen order key, so they still follow in the section list. */
    for(int gi=0;gi<ng;gi++){ GSec*g=&G[gi]; if(g->allocated||g->size==0||g->tflag==0x80)continue;
        Region*R=NULL; for(int r=0;r<g_nreg;r++) if(g_reg[r].page==g->page){ R=&g_reg[r]; break; }
        if(!R && g_nreg>0) R=&g_reg[0];
        if(R){ g->paddr=g->vaddr=R->cursor; R->cursor+=g->size; g->page=R->page; } }
    /* 2c. no command file at all: default allocation. Initialized sections ->
     * PROG page 0 from 0x0800 in link order; uninitialized (.bss/.usect) -> DATA page 1
     * from 0x0800; empty sections stay at origin 0 but keep their natural page (page 1
     * only for uninitialized). Mirrors the standard .text/.data/.bss path for programs
     * that also carry a named section (which routes through this general path). */
    if(g_nreg==0 && g_ndir==0){
        uint32_t prog=0x0800, data=0x0800;
        for(int gi=0;gi<ng;gi++){ GSec*g=&G[gi]; int uninit=(g->tflag==0x80);
            g->page = uninit?1:0; g->allocated=1;
            if(g->size==0){ g->paddr=g->vaddr=0; continue; }
            if(uninit){ g->paddr=g->vaddr=data; data+=g->size; }
            else      { g->paddr=g->vaddr=prog; prog+=g->size; } }
    }
    /* 3. set section bases + symbol finals, merge data, apply relocations */
    for(int gi=0;gi<ng;gi++){ GSec*g=&G[gi]; uint32_t off=0;
        for(int c=0;c<g->noc;c++){ Scn*sc=&O[g->oc_obj[c]].scn[g->oc_scn[c]]; sc->base=g->paddr+off; off+=sc->size; } }
    for(int i=0;i<nobj;i++) for(int j=0;j<O[i].nsyms;j++){ Sym*sy=&O[i].sym[j];
        sy->final = (sy->scnum>=1&&sy->scnum<=O[i].nscns)? O[i].scn[sy->scnum-1].base+sy->value : sy->value; }
    static struct { char name[9]; long final; } GL[4096]; int nGL=0;
    for(int i=0;i<nobj;i++) for(int j=0;j<O[i].nsyms;j++){ Sym*sy=&O[i].sym[j];
        if(sy->name[0]&&sy->scnum>0){ int f=0; for(int k=0;k<nGL;k++) if(!strcmp(GL[k].name,sy->name)){f=1;break;}
            if(!f&&nGL<4096){ snprintf(GL[nGL].name,sizeof GL[nGL].name,"%s",sy->name); GL[nGL].final=sy->final; nGL++; } } }
    /* link-time symbol assignments: resolve against object globals, add to the map */
    if(g_nasym>0){ g_ngmap=0; for(int k=0;k<nGL;k++) gmap_add(GL[k].name,GL[k].final);
        uint32_t dotv=0; for(int gi=0;gi<ng;gi++) if(G[gi].paddr+G[gi].size>dotv) dotv=G[gi].paddr+G[gi].size;
        asym_eval(gmap_look,(long)dotv);
        for(int a=0;a<g_nasym;a++) if(g_asym[a].done && nGL<4096){ strncpy(GL[nGL].name,g_asym[a].name,8); GL[nGL].final=g_asym[a].value; nGL++; } }
    for(int i=0;i<nobj;i++) for(int s=0;s<O[i].nscns;s++){ Scn*sc=&O[i].scn[s];
        for(uint32_t r=0;r<sc->nreloc;r++){ const uint8_t*re=sc->rel+12*r;
            uint32_t vaddr=u32(re), symndx=u32(re+4); uint16_t radd=u16(re+8), rtype=u16(re+10); uint32_t add;
            if(symndx==0xFFFFFFFFu) add=sc->base;
            else if(symndx<(uint32_t)O[i].nsyms){ Sym*ts=&O[i].sym[symndx];
                if(ts->scnum>0) add=(uint32_t)ts->final; else { add=(uint32_t)ts->final; for(int k=0;k<nGL;k++) if(!strcmp(GL[k].name,ts->name)){add=(uint32_t)GL[k].final;break;} } }
            else add=sc->base;
            uint32_t idx=vaddr-sc->vaddr; if(idx<sc->size) sc->data[idx]=c5xcoff_apply(sc->data[idx],add,radd,rtype); } }
    /* merge each output section's words from its pieces (post-relocation) */
    for(int gi=0;gi<ng;gi++){ GSec*g=&G[gi]; if(g->tflag==0x80||g->size==0||g->noload||g->dsect){ g->has_data=0; continue; }
        g->has_data=1; g->data=calloc(g->size?g->size:1,2); uint32_t off=0;
        for(int c=0;c<g->noc;c++){ Scn*sc=&O[g->oc_obj[c]].scn[g->oc_scn[c]];
            for(uint32_t w=0;w<sc->size;w++) g->data[off+w]= sc->data?sc->data[w]:0; off+=sc->size; } }
    /* 4. output order: allocated sections by placement sequence, then the rest first-seen */
    int ord[64]; for(int i=0;i<ng;i++)ord[i]=i;
    for(int a=0;a<ng;a++)for(int b=a+1;b<ng;b++) if(G[ord[b]].order<G[ord[a]].order){ int t=ord[a];ord[a]=ord[b];ord[b]=t; }
    /* 5. write COFF .out */
    const char*op=outp?outp:"a.out"; FILE*f=fopen(op,"wb"); if(!f){perror(op);return 1;}
    long opthdr=28, fhsz=c5xcoff_fhsz(), shsz=40, hdrs=fhsz+opthdr+(long)ng*shsz;
    /* assign scnptr offsets in output order */
    long scp[64]; long cur=hdrs;
    for(int k=0;k<ng;k++){ GSec*g=&G[ord[k]]; if(g->has_data&&g->size){ scp[ord[k]]=cur; cur+=(long)g->size*2; } else scp[ord[k]]=0; }
    long symptr=cur;
    /* standard-section aggregates for the optional header */
    int it=gsec_find(G,ng,".text"), id=gsec_find(G,ng,".data"), ib=gsec_find(G,ng,".bss");
    uint32_t tsize=it>=0?G[it].size:0, dsize=id>=0?G[id].size:0, bsize=ib>=0?G[ib].size:0;
    uint32_t tstart=(it>=0&&G[it].size)?G[it].paddr:0, dstart=(id>=0&&G[id].size)?G[id].paddr:0;
    uint32_t ep = entry? (uint32_t)0:0; if(entry){ for(int k=0;k<nGL;k++) if(!strcmp(GL[k].name,entry)){ep=(uint32_t)GL[k].final;break;} }
    c5xcoff_write_fhdr(f,(unsigned)ng,(unsigned long)symptr,0,(unsigned)opthdr,0x1143u & ~(g_reloc_overflow?F_EXEC:0u));
    p16(f,COFF_OPT_MAGIC); p16(f,COFF_OPT_VSTAMP); p32(f,tsize); p32(f,dsize); p32(f,bsize); p32(f,ep); p32(f,tstart); p32(f,dstart);
    for(int k=0;k<ng;k++){ GSec*g=&G[ord[k]]; unsigned fl=g->tflag | (g->page?0x01000000u:0u) | (g->noload?0x02u:0u) | (g->copy?0x10u:0u) | (g->dsect?0x01u:0u);
        pname(f,g->name); p32(f,g->paddr); p32(f,g->vaddr); p32(f,g->size);
        p32(f,(unsigned long)scp[ord[k]]); p32(f,0); p32(f,0); p16(f,0); p16(f,0); p32(f,fl); }
    for(int k=0;k<ng;k++){ GSec*g=&G[ord[k]]; if(g->has_data&&g->size) for(uint32_t w=0;w<g->size;w++) p16(f,g->data[w]); }
    fclose(f);
    if(!quiet) fprintf(stderr,"c5xlnk: wrote %s (%d sections, general placement)\n",op,ng);
    if(verbose&&!quiet) for(int k=0;k<ng;k++){ GSec*g=&G[ord[k]];
        fprintf(stderr,"  %-8s page %d paddr=0x%04x vaddr=0x%04x size=%u%s\n",g->name,g->page,g->paddr,g->vaddr,g->size,g->allocated?"":" (unallocated)"); }
    /* map */
    if(mapp){ FILE*mf=fopen(mapp,"w"); if(mf){ fprintf(mf,"c5xlnk map (general placement)\n\nSECTION ALLOCATION\n");
        for(int k=0;k<ng;k++){ GSec*g=&G[ord[k]]; fprintf(mf,"  %-8s page %d  paddr 0x%04x  vaddr 0x%04x  size 0x%04x  %s\n",
            g->name,g->page,g->paddr,g->vaddr,g->size,g->allocated?"":"(unallocated)"); }
        fclose(mf); } }
    return 0;
}

#define C5XLNK_ROLE  "TMS320 fixed-point COFF linker (C5x)"
#define C5XLNK_SCOPE "TI COFF " C5X_DOT " C1x/C2x/C2xx/C5x (0x0092)"

/* Identity banner + full usage. Shown for --help/-h (to stdout) and for no-args
 * or an unknown option (to stderr), so the tool always explains itself. */
static void lnk_help(FILE *f){
    c5x_banner(f,"c5xlnk",C5XLNK_ROLE,C5XLNK_SCOPE);
    fputs("usage: c5xlnk <obj...|cmdfile> [options]\n"
          "  -o <file>    executable COFF .out (default: a.out)\n"
          "  -m <file>    write a link map\n"
          "  -e <sym>     set the entry point symbol\n"
          "  -s           strip the symbol table from the output\n"
          "  -l <lib>     search object library <lib> for unresolved externals\n"
          "  -i <dir>     add <dir> to the library search path (for -l)\n"
          "  -u <sym>     force <sym> undefined (pull the library member that defines it)\n"
          "  -x           exhaustively re-read libraries until nothing more resolves\n"
          "  -r | -ar     relocatable output (-ar also marks it executable)\n"
          "  -v0          emit version-0 COFF (20-byte file header)\n"
          "  --static     make all global symbols static in the output symbol table\n"
          "  -b           disable symbolic-debug merge (clear file flag 0x1000)\n"
          "  -flat        emit the flat 'PAGE AAAA WWWW' image instead of COFF\n"
          "  -text/-data/-bss <addr>   override the default section origins (PROG/DATA 0x0800)\n"
          "  -V, --verbose  report placement and symbol resolution\n"
          "  -q, --quiet    one-line identity instead of the full banner\n"
          "  -h, --help     show this help and exit\n", f);
}

int main(int argc,char**argv){
    const char*objs[64]; int nobj=0; const char*outp=NULL,*mapp=NULL,*entry=NULL;
    uint32_t org_text=0x0800, org_data=0, org_bss=0x0800; int have_data=0, flat=0, strip=0, quiet=0, verbose=0;
    int relocatable=0, execout=0;   /* -r: retain relocations; -a/-ar: executable (default -a) */
    for(int i=1;i<argc;i++){ const char*a=argv[i];
        if(!strcmp(a,"-o")&&i+1<argc)outp=argv[++i];
        else if(!strcmp(a,"-m")&&i+1<argc)mapp=argv[++i];
        else if(!strcmp(a,"-e")&&i+1<argc)entry=argv[++i];
        else if(!strcmp(a,"-text")&&i+1<argc)org_text=strtol(argv[++i],NULL,0);
        else if(!strcmp(a,"-data")&&i+1<argc){org_data=strtol(argv[++i],NULL,0);have_data=1;}
        else if(!strcmp(a,"-bss")&&i+1<argc)org_bss=strtol(argv[++i],NULL,0);
        else if(!strcmp(a,"-flat"))flat=1;
        else if(!strcmp(a,"-r"))relocatable=1;
        else if(!strcmp(a,"-ar")||!strcmp(a,"-ra")){relocatable=1;execout=1;}
        else if(!strcmp(a,"-a"))execout=1;
        else if(!strcmp(a,"-s"))strip=1;
        else if(!strcmp(a,"-x"))g_exhaustive=1;
        else if(!strcmp(a,"-v0"))g_v0=1;
        else if(!strcmp(a,"-b"))g_bnomerge=1;
        else if(!strcmp(a,"--static"))g_hstatic=1;   /* make all globals static (classic -h) */
        else if(!strncmp(a,"-l",2)){ const char*v=a[2]?a+2:(i+1<argc?argv[++i]:""); if(*v&&g_nlib<MAXLIB)g_lib[g_nlib++]=v; }
        else if(!strncmp(a,"-i",2)){ const char*v=a[2]?a+2:(i+1<argc?argv[++i]:""); if(*v&&g_nidir<MAXLIB)g_idir[g_nidir++]=v; }
        else if(!strncmp(a,"-u",2)){ const char*v=a[2]?a+2:(i+1<argc?argv[++i]:""); if(*v&&g_nusym<MAXLIB)g_usym[g_nusym++]=v; }
        else if(!strcmp(a,"-q")||!strcmp(a,"--quiet"))quiet=1;
        else if(!strcmp(a,"-V")||!strcmp(a,"--verbose"))verbose=1;
        else if(!strcmp(a,"--version")){ c5x_banner(stdout,"c5xlnk",C5XLNK_ROLE,C5XLNK_SCOPE); return 0; }
        else if(!strcmp(a,"--help")||!strcmp(a,"-h")||!strcmp(a,"-?")){ lnk_help(stdout); return 0; }
        else if(a[0]!='-'&&nobj<64){
            /* a non-COFF input file is a linker command file (auto-detected) */
            FILE*pf=fopen(a,"rb"); int isc5xcoff=0; if(pf){ unsigned char mg[2]={0,0}; if(fread(mg,1,2,pf)==2){ unsigned m=mg[0]|(mg[1]<<8); isc5xcoff=(m==0x00c1||m==0x00c2); } fclose(pf); }
            if(isc5xcoff) objs[nobj++]=a;
            else { const char*e2=entry; parse_cmd(a,objs,&nobj,&outp,&mapp,&e2); entry=e2; }
        }
        else { fprintf(stderr,"c5xlnk: unknown option '%s'\n",a); lnk_help(stderr); return 2; }
    }
    if(!relocatable) execout=1;   /* default -a (absolute executable) */
    if(!nobj){ lnk_help(stderr); return 2; }
    /* Identity on startup: the full banner by default, one line under -q. */
    if(quiet) c5x_oneline(stderr,"c5xlnk"); else c5x_banner(stderr,"c5xlnk",C5XLNK_ROLE,C5XLNK_SCOPE);
    static Obj O[64]; for(int i=0;i<nobj;i++) if(!load_obj(objs[i],&O[i])) return 1;

    /* ---- object-library resolution (-l/-i/-u/-x) ---------------------------------
     * Pull a library member iff it defines a symbol that is referenced but not yet
     * defined by the objects loaded so far (plus any -u forced symbols). Repeat within
     * each library until it yields nothing; -x re-sweeps all libraries until a full
     * pass pulls no member (resolves cross-library back references). */
    if(g_nlib>0){
        /* load every archive once */
        static uint8_t* libbuf[MAXLIB]; static long liblen[MAXLIB];
        static Member libm[MAXLIB][256]; static int libnm[MAXLIB]; char fp[1024];
        for(int L=0;L<g_nlib;L++){ libbuf[L]=lib_open(g_lib[L],&liblen[L],fp,sizeof fp); if(!libbuf[L])return 1;
            libnm[L]=ar_members(libbuf[L],liblen[L],libm[L],256); if(libnm[L]<0)return 1;
            if(verbose&&!quiet) fprintf(stderr,"c5xlnk: library %s: %d member(s)\n",fp,libnm[L]); }
        int sweep=1;
        while(sweep){ sweep=0;
            for(int L=0;L<g_nlib;L++){ int again=1;
                while(again){ again=0;
                    for(int mi=0; mi<libnm[L]; mi++){ Member*mm=&libm[L][mi]; if(mm->pulled) continue;
                        /* parse member header enough to test its defined globals */
                        Obj probe; memset(&probe,0,sizeof probe);
                        probe.buf=malloc(mm->size); memcpy(probe.buf,mm->data,mm->size); probe.len=mm->size; probe.path=mm->name;
                        if(!c5xcoff_parse(&probe)){ free(probe.buf); continue; }
                        int want=0;
                        for(int u=0;u<g_nusym&&!want;u++) if(syms_undefined(O,nobj,g_usym[u])&&obj_defines(&probe,g_usym[u])) want=1;
                        for(int i2=0;i2<nobj&&!want;i2++) for(int j=0;j<O[i2].nsyms&&!want;j++){ Sym*sy=&O[i2].sym[j];
                            if(sy->cls==2 && sy->scnum==0 && sy->name[0] && obj_defines(&probe,sy->name) && syms_undefined(O,nobj,sy->name)) want=1; }
                        if(!want){ free(probe.buf); continue; }
                        if(nobj>=64){ fprintf(stderr,"c5xlnk: too many objects (library pull)\n"); return 1; }
                        O[nobj]=probe; objs[nobj]=strdup(mm->name); nobj++; mm->pulled=1; again=1; sweep=g_exhaustive;
                        if(verbose&&!quiet) fprintf(stderr,"c5xlnk: pulled member %s from %s\n",mm->name,g_lib[L]);
                    }
                }
            }
            if(!g_exhaustive) break;
        }
    }

    /* General placement path: used when the SECTIONS script has GROUP/UNION or any
     * non-standard (named) output section. Produces the arbitrary multi-section COFF
     * .out with GROUP/UNION/overlay-page layout. (-flat/-r keep the standard path.) */
    if(!flat && !relocatable){
        int general=0;
        for(int d=0;d<g_ndir;d++) if(g_dir[d].kind!=0 || g_dir[d].type!=0) general=1;  /* GROUP/UNION or COPY/NOLOAD/DSECT */
        for(int i=0;i<nobj && !general;i++) for(int s=0;s<O[i].nscns;s++){ const char*n=O[i].scn[s].name;
            if(n[0] && strcmp(n,".text") && strcmp(n,".data") && strcmp(n,".bss")){ general=1; break; } }
        if(general) return link_general(O,nobj,objs,outp,mapp,entry,quiet,verbose);
    }

    /* command-file MEMORY/SECTIONS: drive the section origins from the named regions.
     * (.text/.data in PROG, .bss in DATA is the common layout; a distinct .data region
     *  becomes an explicit -data origin.) */
    if(g_nreg>0){
        const char*rt=smap_region(".text"), *rd=smap_region(".data"), *rb=smap_region(".bss");
        Region *Rt=rt?reg_find(rt):NULL, *Rd=rd?reg_find(rd):NULL, *Rb=rb?reg_find(rb):NULL;
        if(Rt) org_text=Rt->origin;
        if(Rb) org_bss=Rb->origin;
        if(Rd && (!Rt || strcmp(rd,rt))){ org_data=Rd->origin; have_data=1; }
        if(verbose&&!quiet) fprintf(stderr,"c5xlnk: command file: %d region(s), %d section mapping(s)\n",g_nreg,g_nsmap);
    }

    /* place sections: PROG cursor for .text then .data; DATA cursor for .bss.
       the default keeps .text and .data contiguous in PROG from org_text. */
    uint32_t prog=org_text, dcur=org_bss;
    /* first pass: .text of every object, then .data, then .bss (COFF link order) */
    const char*order[3]={".text",".data",".bss"};
    uint32_t text_end=org_text, data_start=0; int have_dseg=0;
    for(int k=0;k<3;k++) for(int i=0;i<nobj;i++) for(int s=0;s<O[i].nscns;s++){ Scn*sc=&O[i].scn[s];
        if(strcmp(sc->name,order[k]))continue;
        if(!strcmp(sc->name,".bss")){ sc->base=dcur; dcur+=sc->size; }
        else { if(!strcmp(sc->name,".data")&&have_data&&prog<org_data) prog=org_data; sc->base=prog;
               if(!strcmp(sc->name,".data")&&!have_dseg){ data_start=prog; have_dseg=1; }
               prog+=sc->size; if(!strcmp(sc->name,".text"))text_end=prog; }
    }
    if(!have_dseg) data_start=text_end;
    uint32_t ntext=text_end-org_text, ndata=prog-data_start, nbss=dcur-org_bss;
    if(verbose&&!quiet) fprintf(stderr,"c5xlnk: %d object(s); .text=%u@0x%04x .data=%u@0x%04x .bss=%u@0x%04x\n",
        nobj,ntext,org_text,ndata,data_start,nbss,org_bss);

    /* per-object finals: for a symbol defined in a section, final = base + value */
    for(int i=0;i<nobj;i++) for(int j=0;j<O[i].nsyms;j++){ Sym*sy=&O[i].sym[j];
        if(sy->scnum>=1 && sy->scnum<=O[i].nscns) sy->final=O[i].scn[sy->scnum-1].base + sy->value; else sy->final=sy->value; }
    /* global name->final map from DEFINED symbols (scnum>0), for cross-object externals */
    static LnkGmap G[4096]; int nG=0;
    for(int i=0;i<nobj;i++) for(int j=0;j<O[i].nsyms;j++){ Sym*sy=&O[i].sym[j];
        if(sy->name[0] && sy->scnum>0){ int f=0; for(int k=0;k<nG;k++) if(!strcmp(G[k].name,sy->name)){f=1;break;}
            if(!f && nG<4096){ snprintf(G[nG].name,sizeof G[nG].name,"%s",sy->name); G[nG].final=sy->final; nG++; } } }
    /* link-time symbol assignments (sym = expr;): resolve and add to the global map so
     * external relocations and the entry point can reference them. */
    if(g_nasym>0){ g_ngmap=0; for(int k=0;k<nG;k++) gmap_add(G[k].name,G[k].final);
        asym_eval(gmap_look,(long)prog);
        for(int a=0;a<g_nasym;a++) if(g_asym[a].done && nG<4096){ strncpy(G[nG].name,g_asym[a].name,8); G[nG].final=g_asym[a].value; nG++; } }
    /* apply relocations and emit words */
    for(int i=0;i<nobj;i++) for(int s=0;s<O[i].nscns;s++){ Scn*sc=&O[i].scn[s];
        int page=(!strcmp(sc->name,".bss"))?1:0;
        for(uint32_t r=0;r<sc->nreloc;r++){ const uint8_t*re=sc->rel+12*r;
            uint32_t vaddr=u32(re), symndx=u32(re+4); uint16_t radd=u16(re+8), rtype=u16(re+10);
            uint32_t add;
            if(symndx==0xFFFFFFFFu) add=sc->base;                         /* section-relative */
            else if(symndx<(uint32_t)O[i].nsyms){ Sym*ts=&O[i].sym[symndx];
                add = (ts->scnum>0) ? (uint32_t)ts->final : (uint32_t)gmap_final(G,nG,ts->name,ts->final); } /* defined here, else resolve external by name */
            else add=sc->base;
            uint32_t idx=vaddr - sc->vaddr;
            if(idx<sc->size) sc->data[idx]=c5xcoff_apply(sc->data[idx],add,radd,rtype);
        }
        for(uint32_t w=0;w<sc->size;w++) ow(page, sc->base+w, sc->data? sc->data[w]:0);
    }

    /* assemble the linked .text/.data images from the page-0 word list */
    static uint16_t T[1<<16],D[1<<16];
    for(int i=0;i<g_now;i++){ if(g_ow[i].page!=0)continue;
        if(g_ow[i].addr>=data_start){ uint32_t x=g_ow[i].addr-data_start; if(x<ndata)D[x]=g_ow[i].word; }
        else { uint32_t x=g_ow[i].addr-org_text; if(x<ntext)T[x]=g_ow[i].word; } }
    uint32_t ep = entry? (uint32_t)gmap_final(G,nG,entry,0) : 0;

    if(flat){
        FILE*of=outp?fopen(outp,"wb"):stdout; if(!of){perror(outp);return 1;}
        for(int i=0;i<g_now;i++) fprintf(of,"%d %04X %04X\n",g_ow[i].page,g_ow[i].addr,g_ow[i].word);
        if(of!=stdout)fclose(of);
    } else {
        /* COFF v1 executable .out. */
        const char*op = outp?outp:"a.out";
        FILE*f=fopen(op,"wb"); if(!f){perror(op);return 1;}
        /* user globals: unique DEFINED C_EXT symbols (cls=2, scnum>0, non-section) */
        static struct { char name[9]; long final; int scnum; } UG[4096]; int nug=0;
        for(int i=0;i<nobj;i++) for(int j=0;j<O[i].nsyms;j++){ Sym*sy=&O[i].sym[j];
            if(sy->cls==2 && sy->scnum>0 && sy->name[0] && sy->name[0]!='.'){
                int f2=0; for(int k=0;k<nug;k++) if(!strcmp(UG[k].name,sy->name)){f2=1;break;}
                if(!f2 && nug<4096){ snprintf(UG[nug].name,sizeof UG[nug].name,"%s",sy->name); UG[nug].final=sy->final; UG[nug].scnum=sy->scnum; nug++; } } }
        int nsyms = strip?0 : (8*nobj + nug + (execout?6:0));  /* boundary globals only in an executable */
        /* retained relocations (-r/-ar): count input .text/.data relocs */
        int rtext=0,rdata=0; if(relocatable) for(int i=0;i<nobj;i++) for(int s=0;s<O[i].nscns;s++){ Scn*sc=&O[i].scn[s];
            if(!strcmp(sc->name,".text"))rtext+=sc->nreloc; else if(!strcmp(sc->name,".data"))rdata+=sc->nreloc; }
        long opthdr=28, fhsz=c5xcoff_fhsz(), shsz=40;
        long scn_text=fhsz+opthdr+3*shsz;
        long scn_data=scn_text+(long)ntext*2;
        long rel_text=scn_data+(long)ndata*2;
        long rel_data=rel_text+(long)rtext*12;
        long symptr = strip?0 : (rel_data+(long)rdata*12);
        unsigned fflags = 0x1140u | (relocatable?0:0x01u) | ((execout && !g_reloc_overflow)?0x02u:0u); /* RELFLG set only when stripped; EXEC for executables, cleared on R_RELBYTE truncation */
        /* file header (v1 0x00c1 / v0 0x0092; -b clears 0x1000) */
        c5xcoff_write_fhdr(f,3,(unsigned long)symptr,nsyms,(unsigned)opthdr,fflags);
        /* empty sections report address 0, unless a command file assigned them a region
         * (the region origin is reported even when empty) */
        int ex_t=g_nreg>0&&smap_region(".text"), ex_d=g_nreg>0&&smap_region(".data"), ex_b=g_nreg>0&&smap_region(".bss");
        uint32_t dstart_h = (ndata||ex_d)? data_start : 0;
        uint32_t tstart_h = (ntext||ex_t)? org_text  : 0;
        uint32_t bstart_h = (nbss ||ex_b)? org_bss   : 0;
        /* optional header (28B): magic, vstamp, tsize, dsize, bsize, entry, text_start, data_start */
        p16(f,COFF_OPT_MAGIC); p16(f,COFF_OPT_VSTAMP); p32(f,ntext); p32(f,ndata); p32(f,nbss);
        p32(f,ep); p32(f,tstart_h); p32(f,dstart_h);
        /* section headers (40B v1): name,paddr,vaddr,size,scnptr,relptr=0,lnnoptr=0,nreloc=0,nlnno=0,flags */
        pname(f,".text"); p32(f,tstart_h); p32(f,tstart_h); p32(f,ntext); p32(f,ntext?scn_text:0); p32(f,rtext?rel_text:0); p32(f,0); p16(f,(unsigned)rtext); p16(f,0); p32(f,0x20);
        pname(f,".data"); p32(f,dstart_h); p32(f,dstart_h); p32(f,ndata); p32(f,ndata?scn_data:0); p32(f,rdata?rel_data:0); p32(f,0); p16(f,(unsigned)rdata); p16(f,0); p32(f,0x40);
        pname(f,".bss");  p32(f,bstart_h); p32(f,bstart_h); p32(f,nbss);  p32(f,0);               p32(f,0); p32(f,0); p16(f,0); p16(f,0); p32(f,0x01000080);
        /* section data */
        for(uint32_t w=0;w<ntext;w++) p16(f,T[w]);
        for(uint32_t w=0;w<ndata;w++) p16(f,D[w]);
        /* retained relocations (-r/-ar): rebase each input reloc's vaddr to its placed address */
        if(relocatable){ const char*onm[2]={".text",".data"};
            for(int which=0;which<2;which++) for(int i=0;i<nobj;i++) for(int s=0;s<O[i].nscns;s++){ Scn*sc=&O[i].scn[s];
                if(strcmp(sc->name,onm[which]))continue;
                for(uint32_t r=0;r<sc->nreloc;r++){ const uint8_t*re=sc->rel+12*r;
                    uint32_t vaddr=u32(re), symndx=u32(re+4); uint16_t type=u16(re+10);
                    p32(f, sc->base + (vaddr - sc->vaddr)); p32(f, symndx); p16(f,0); p16(f,type); } } }
        /* symbol table (unless -s): per object {.file+aux, .text/.data/.bss C_STAT+aux},
         * then user globals (C_EXT), then the 6 boundary globals. */
        if(!strip){
            for(int i=0;i<nobj;i++){
                /* per-object section base/size/reloc */
                uint32_t tb=0,db=0,bb=0,tsz=0,dsz=0,bsz=0,trl=0;
                for(int s=0;s<O[i].nscns;s++){ Scn*sc=&O[i].scn[s];
                    if(!strcmp(sc->name,".text")){tb=sc->base;tsz=sc->size;trl=sc->nreloc;}
                    else if(!strcmp(sc->name,".data")){db=sc->base;dsz=sc->size;}
                    else if(!strcmp(sc->name,".bss")){bb=sc->base;bsz=sc->size;} }
                const char*base=objs[i]; for(const char*p=objs[i];*p;p++) if(*p=='/'||*p=='\\')base=p+1;
                if(O[i].nsyms>0 && O[i].sym[0].name[0] && O[i].sym[0].scnum==-2) base=O[i].sym[0].name;
                /* .file + aux(name). The .file value chains to the next file's
                 * first symbol index: object i occupies 8 symbols, so (i+1)*8. */
                pname(f,".file"); p32(f,(unsigned long)((i+1)*8)); p16(f,0xfffe); p16(f,0); fputc(103,f); fputc(1,f);
                { char nm[8]={0}; strncpy(nm,base,8); fwrite(nm,1,8,f); char z[10]={0}; fwrite(z,1,10,f); }
                /* .text/.data/.bss C_STAT + aux(size,reloc) */
                pname(f,".text"); p32(f,tsz?tb:0); p16(f,1); p16(f,0); fputc(3,f); fputc(1,f); p32(f,tsz); p16(f,(unsigned)trl); { char z[12]={0}; fwrite(z,1,12,f);}
                pname(f,".data"); p32(f,dsz?db:0); p16(f,2); p16(f,0); fputc(3,f); fputc(1,f); p32(f,dsz); { char z[14]={0}; fwrite(z,1,14,f);}
                pname(f,".bss");  p32(f,bsz?bb:0); p16(f,3); p16(f,0); fputc(3,f); fputc(1,f); p32(f,bsz); { char z[14]={0}; fwrite(z,1,14,f);}
            }
            /* globals' storage class: C_EXT(2) normally, C_STAT(3) under -h (make statics) */
            int gcls = g_hstatic?3:2;
            /* user globals (n_type 0x0004, C_EXT user symbols are stamped and
             * carried through; the boundary globals below keep n_type 0). */
            for(int k=0;k<nug;k++){ pname(f,UG[k].name); p32(f,(unsigned long)UG[k].final); p16(f,(unsigned)UG[k].scnum); p16(f,0x0004); fputc(gcls,f); fputc(0,f); }
            /* 6 boundary globals - only in an executable output */
            if(execout){
                pname(f,".text"); p32(f,org_text);          p16(f,1); p16(f,0); fputc(gcls,f); fputc(0,f);
                pname(f,"etext"); p32(f,org_text+ntext);    p16(f,1); p16(f,0); fputc(gcls,f); fputc(0,f);
                pname(f,".data"); p32(f,ndata?data_start:0);p16(f,2); p16(f,0); fputc(gcls,f); fputc(0,f);
                pname(f,"edata"); p32(f,ndata?data_start+ndata:0); p16(f,2); p16(f,0); fputc(gcls,f); fputc(0,f);
                pname(f,".bss");  p32(f,bstart_h);          p16(f,3); p16(f,0); fputc(gcls,f); fputc(0,f);
                pname(f,"end");   p32(f,bstart_h+nbss);     p16(f,3); p16(f,0); fputc(gcls,f); fputc(0,f);
            }
        }
        fclose(f);
        if(!quiet) fprintf(stderr,"c5xlnk: wrote %s (.text=%u .data=%u .bss=%u @0x%04x)\n",op,ntext,ndata,nbss,org_text);
    }

    /* map file */
    if(mapp){ FILE*mf=fopen(mapp,"w"); if(mf){
        fprintf(mf,"c5xlnk map\n\nMEMORY CONFIGURATION\n  PAGE 0 PROG origin 0x%04x\n  PAGE 1 DATA origin 0x%04x\n\nSECTION ALLOCATION\n",org_text,org_bss);
        for(int k=0;k<3;k++) for(int i=0;i<nobj;i++) for(int s=0;s<O[i].nscns;s++){ Scn*sc=&O[i].scn[s];
            if(strcmp(sc->name,order[k]))continue;
            fprintf(mf,"  %-6s page %d  origin 0x%04x  length 0x%04x  %s\n",sc->name,(!strcmp(sc->name,".bss"))?1:0,sc->base,sc->size,objs[i]); }
        fprintf(mf,"\nGLOBAL SYMBOLS\n");
        for(int i=0;i<nobj;i++) for(int j=0;j<O[i].nsyms;j++){ Sym*sy=&O[i].sym[j]; if(sy->name[0]&&sy->name[0]!='.'&&sy->scnum>0) fprintf(mf,"  0x%04lx  %s\n",sy->final,sy->name); }
        fclose(mf);
    } }
    return 0;
}
