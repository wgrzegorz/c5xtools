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
 * c5xasm - TMS320C5x assembler (native, POSIX C99), data-driven encoder.
 *
 * Dependency-free, portable (macOS clang / Linux gcc). The instruction set is
 * driven by c5x_isa.h (name -> {opcode, format-class}). Each format class has an
 * encoder that reproduces the canonical C5x bit placement and short/long form
 * selection.
 *
 * Build:  make   (multi-TU: src/c5xasm.c + src/operands.c; see ../build.sh)
 * Usage:  c5xasm input.asm [-o output.txt] [-v50]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portable.h"
#include "c5xcoff.h"
#include <ctype.h>
#include <stdint.h>
#include "c5x_isa.h"
#include "c5xbanner.h"
#include "c5xasm.h"
#include "operands.h"

static const char *g_file = "<stdin>";
static int g_line = 0, g_errors = 0, g_final = 0;
/* exported to operands.c via c5xasm.h */
void err(const char *m){ if(!g_final) return; fprintf(stderr,"%s:%d: error: %s\n",g_file,g_line,m); g_errors++; }

/* ---------------- symbols ---------------- */
typedef struct { char name[64]; long value; int defined; int is_label; int sec;
    int global;        /* named in .global/.def/.ref (candidate for the symbol table) */
    int referenced;    /* used in an expression/operand somewhere */
    int symidx;        /* final index in the emitted COFF symbol table (write_c5xcoff) */
} Sym;
/* A symbol produces a relocation when it is a locally defined label OR an
 * external reference (declared global but not defined here). */
static int sym_reloc(const Sym*s){ return s && (s->is_label || (s->global && !s->defined)); }
/* An external reference resolves against its own symbol-table entry (symndx =
 * the symbol index), not a section symbol. */
static int sym_external(const Sym*s){ return s && s->global && !s->defined; }
static Sym g_syms[8192]; static int g_nsyms = 0;
static Sym *g_evsym=NULL; static int g_evnsym=0;   /* eval symbol tracking (for relocations) */
static Sym *sym_find(const char *n){ for(int i=0;i<g_nsyms;i++) if(!strcasecmp(g_syms[i].name,n)) return &g_syms[i]; return NULL; }
static Sym *sym_intern(const char *n){ Sym*s=sym_find(n); if(s)return s;
    if(g_nsyms>=(int)(sizeof g_syms/sizeof g_syms[0])){err("symbol table full");return &g_syms[0];}
    s=&g_syms[g_nsyms++]; strncpy(s->name,n,sizeof s->name-1); s->name[sizeof s->name-1]=0; s->value=0; s->defined=0; return s; }
static void sym_set(const char *n,long v){ Sym*s=sym_intern(n); s->value=v; s->defined=1; }
/* A signature of all symbol values/sections, used to detect when the multi-pass
 * layout has reached a fixed point (symbol addresses stop changing). */
static unsigned long sym_sig(void){
    unsigned long h=2166136261ul;
    for(int i=0;i<g_nsyms;i++){
        unsigned long v=(unsigned long)g_syms[i].value;
        h=(h^(v&0xffff))*16777619ul; h=(h^((v>>16)&0xffff))*16777619ul;
        h=(h^(unsigned long)(g_syms[i].sec&0xff))*16777619ul;
    }
    return h;
}

static void define_mmregs(void){
    /* Full C5x MMR map (.mmregs). */
    static const struct{const char*n;long v;} m[]={
        {"IMR",0x04},{"GREG",0x05},{"IFR",0x06},{"PMST",0x07},
        {"RPTC",0x08},{"BRCR",0x09},{"PASR",0x0A},{"PAER",0x0B},
        {"TREG0",0x0C},{"TREG1",0x0D},{"TREG2",0x0E},{"DBMR",0x0F},
        {"AR0",0x10},{"AR1",0x11},{"AR2",0x12},{"AR3",0x13},
        {"AR4",0x14},{"AR5",0x15},{"AR6",0x16},{"AR7",0x17},
        {"INDX",0x18},{"ARCR",0x19},{"CBSR1",0x1A},{"CBER1",0x1B},
        {"CBSR2",0x1C},{"CBER2",0x1D},{"CBCR",0x1E},{"BMAR",0x1F},
        {"DRR",0x20},{"DXR",0x21},{"SPC",0x22},{"TIM",0x24},
        {"PRD",0x25},{"TCR",0x26},{"PDWSR",0x28},{"IOWSR",0x29},
        {"CWSR",0x2A},{"TRCV",0x30},{"TDXR",0x31},{"TSPC",0x32},
        {"TCSR",0x33},{"TRTA",0x34},{"TRAD",0x35},
    };
    for(size_t i=0;i<sizeof m/sizeof m[0];i++) sym_set(m[i].n,m[i].v);
}

/* ---------------- expressions ---------------- */
static long g_loc=0; static int g_pass=0;
#define C5X_MAXPASS 64   /* fixed-point relaxation cap; typical inputs converge in < 8 */
static const char *ep;
static void sw(void){ while(*ep==' '||*ep=='\t') ep++; }
static long pnum(void){ char b[64]; int n=0;
    if(ep[0]=='0'&&(ep[1]=='x'||ep[1]=='X')){ ep+=2; while(isxdigit((unsigned char)*ep)&&n<63)b[n++]=*ep++; b[n]=0; return strtol(b,NULL,16);}
    while(isalnum((unsigned char)*ep)&&n<63){b[n++]=*ep++;} b[n]=0;
    if(n>0){ char s=(char)tolower((unsigned char)b[n-1]);
        if(s=='h'){b[n-1]=0;return strtol(b,NULL,16);} if(s=='b'){b[n-1]=0;return strtol(b,NULL,2);} if(s=='q'){b[n-1]=0;return strtol(b,NULL,8);} }
    return strtol(b,NULL,10); }
/* Expression grammar with C-like operator precedence (low -> high):
 *   | , ^ , & , << >> , + - , * / % , unary + - ~ , primary.
 * Only + - * / were supported before; adding bitwise/shift ops follows the reference assembler
 * (e.g. FMT | RATE, MASK & x, 1<<n) and leaves all existing expressions
 * unchanged (same results for + - * /). */
static long p_or(void);
static long pprim(void){ sw();
    if(*ep=='('){ep++;long v=p_or();sw();if(*ep==')')ep++;else err("missing ')'");return v;}
    if(*ep=='$'){ep++;return g_loc;}
    if(*ep=='~'){ep++;return ~pprim();}
    if(*ep=='-'){ep++;return -pprim();} if(*ep=='+'){ep++;return pprim();}
    if(isdigit((unsigned char)*ep)) return pnum();
    if(isalpha((unsigned char)*ep)||*ep=='_'){ char nm[64];int n=0;
        while((isalnum((unsigned char)*ep)||*ep=='_')&&n<63){nm[n++]=*ep++;} nm[n]=0;
        Sym*s=sym_find(nm);
        if(s&&s->global) s->referenced=1;
        if(!s||!s->defined){
            if(s&&s->global){ g_evsym=s; g_evnsym++; return 0; } /* external reference: relocated, not an error */
            if(g_final){err("undefined symbol");} return 0;}
        if(s->is_label){ g_evsym=s; g_evnsym++; }   /* track label refs for relocation */
        return s->value;}
    err("bad expression"); return 0; }
static long p_mul(void){ long v=pprim(); for(;;){ sw();
    if(*ep=='*'){ep++;v*=pprim();} else if(*ep=='/'){ep++;long d=pprim();v=d?v/d:0;}
    else if(*ep=='%'){ep++;long d=pprim();v=d?v%d:0;} else break;} return v; }
static long p_add(void){ long v=p_mul(); for(;;){ sw();
    if(*ep=='+'){ep++;v+=p_mul();} else if(*ep=='-'){ep++;v-=p_mul();} else break;} return v; }
static long p_shift(void){ long v=p_add(); for(;;){ sw();
    if(ep[0]=='<'&&ep[1]=='<'){ep+=2;v<<=p_add();} else if(ep[0]=='>'&&ep[1]=='>'){ep+=2;v>>=p_add();}
    else break;} return v; }
static long p_and(void){ long v=p_shift(); for(;;){ sw(); if(*ep=='&'&&ep[1]!='&'){ep++;v&=p_shift();} else break;} return v; }
static long p_xor(void){ long v=p_and();   for(;;){ sw(); if(*ep=='^'){ep++;v^=p_and();} else break;} return v; }
static long p_or(void){  long v=p_xor();   for(;;){ sw(); if(*ep=='|'&&ep[1]!='|'){ep++;v|=p_xor();} else break;} return v; }
/* exported to operands.c via c5xasm.h */
long eval(const char*s){ g_evsym=NULL; g_evnsym=0; ep=s; long v=p_or(); sw(); if(*ep)err("trailing chars in expression"); return v; }

/* ---------------- sections ---------------- */
/* c5xasm tracks an ordered section table. The first three are always present and
 * always emitted: .text (0x20), .data (0x40) and .bss (0x80,
 * uninitialised). .sect "name" adds an initialised named section (flags 0x40);
 * .usect "name" and .bss add uninitialised ones (0x80). Each section carries its
 * own location counter; switching sections saves the current counter and restores
 * the target's. The flat (non-COFF) path only uses .text/.data (chars 'P'/'D');
 * named sections are a COFF-object concept. */
typedef struct { char name[64]; unsigned flags; int uninit; long loc; } Sect;
static Sect g_sect[64]; static int g_nsect=0; static int g_cursec=0;
static char sec_flatchar(int i){ return i==0?'P':'D'; }   /* .text->P, all else->D */
static void sect_reset(void){
    static const struct { const char*n; unsigned f; int u; } fixed[3] =
        { {".text",0x20,0}, {".data",0x40,0}, {".bss",0x80,1} };
    for(int i=0;i<3;i++){ strcpy(g_sect[i].name,fixed[i].n); g_sect[i].flags=fixed[i].f;
                          g_sect[i].uninit=fixed[i].u; g_sect[i].loc=0; }
    g_nsect=3; g_cursec=0;
}
static int sect_index(const char*nm){ for(int i=0;i<g_nsect;i++) if(!strcmp(g_sect[i].name,nm))return i; return -1; }
/* find-or-create a section; make_current switches to it, saving/restoring loc */
static int sect_use(const char*nm, unsigned flags, int uninit, int make_current){
    int i=sect_index(nm);
    if(i<0){ if(g_nsect>=(int)(sizeof g_sect/sizeof g_sect[0])){ err("too many sections"); return g_cursec; }
        i=g_nsect++; strncpy(g_sect[i].name,nm,63); g_sect[i].name[63]=0;
        g_sect[i].flags=flags; g_sect[i].uninit=uninit; g_sect[i].loc=0; }
    if(make_current && i!=g_cursec){ g_sect[g_cursec].loc=g_loc; g_cursec=i; g_loc=g_sect[i].loc; }
    return i;
}
/* operand parsing for section directives: a "quoted" or bare name/symbol, then the
 * following comma-separated size argument. sec_*_adv advance the cursor past the
 * token and an optional separating comma; sec_next_arg returns the next arg up to
 * the next comma (so trailing .usect/.bss flags are ignored). */
static char* sec_skip(char*s){ while(*s==' '||*s=='\t')s++; return s; }
static int sec_name_adv(char**ps, char*out, size_t n){
    char*s=sec_skip(*ps); size_t k=0;
    if(*s=='"'){ s++; while(*s && *s!='"' && k+1<n) out[k++]=*s++; if(*s=='"')s++; }
    else { while(*s && *s!=',' && *s!=' ' && *s!='\t' && k+1<n) out[k++]=*s++; }
    out[k]=0; s=sec_skip(s); if(*s==',')s++; *ps=s; return k>0;
}
static int sec_name(const char*s, char*out, size_t n){ char*t=(char*)s; return sec_name_adv(&t,out,n); }
static int sec_sym_adv(char**ps, char*out, size_t n){ return sec_name_adv(ps,out,n); }
static char* sec_next_arg(char**ps){ static char buf[128]; char*s=sec_skip(*ps); size_t k=0;
    while(*s && *s!=',' && k+1<sizeof buf) buf[k++]=*s++;
    while(k>0 && (buf[k-1]==' '||buf[k-1]=='\t')) k--;
    buf[k]=0; s=sec_skip(s); if(*s==',')s++; *ps=s; return buf; }

/* ---------------- relocations (COFF output) ---------------- */
static int g_c5xcoff=0;
/* internal relocation record, RELA-style: an explicit addend (the full section-
 * relative value) separate from the narrow instruction field. Lowered to the COFF
 * entry's reserved field by the writer. type is a shared R_* kind (c5xcoff.h). */
typedef struct { int sec; uint16_t off; uint32_t symndx; uint16_t type; uint16_t addend; Sym*ext; } Rel;
static Rel g_rel[1<<15]; static int g_nrel=0;
/* a pending direct-address (dma) relocation armed by field_at via dma_operand();
 * g_part_add is the full section-relative offset (addend) stored in the reloc
 * entry's otherwise-reserved field, while the word itself carries only the narrow
 * field (7-bit dma for R_PARTLS7 0x28, or the 9-bit DP page for R_PARTMS9 0x29). */
static int g_part_pending=0; static Sym *g_part_sym=NULL; static uint16_t g_part_add=0; static uint16_t g_part_type=0;

/* ---------------- output ---------------- */
typedef struct { int sec; uint16_t addr, word; } Out;
static Out g_out[1<<16]; static int g_nout=0;
static void emit(uint16_t w){
    if(g_final && g_nout<(int)(sizeof g_out/sizeof g_out[0])){ g_out[g_nout].sec=g_cursec; g_out[g_nout].addr=(uint16_t)g_loc; g_out[g_nout].word=w; g_nout++; }
    /* a partial-field operand that named a label relocates THIS instruction word in a
     * field narrower than the address (7-bit dma -> R_PARTLS7 0x28; 9-bit DP page ->
     * R_PARTMS9 0x29), relative to the label's section: self-section symndx 0xFFFFFFFF,
     * otherwise 1 + 2*section index; the full addend goes in the reserved field. */
    if(g_part_pending){
        if(g_c5xcoff && g_final && g_nrel<(int)(sizeof g_rel/sizeof g_rel[0])){
            g_rel[g_nrel].sec=g_cursec; g_rel[g_nrel].off=(uint16_t)g_loc; g_rel[g_nrel].type=g_part_type;
            g_rel[g_nrel].ext = sym_external(g_part_sym) ? g_part_sym : NULL;
            g_rel[g_nrel].symndx = sym_external(g_part_sym) ? 0u /* patched in write_c5xcoff */
                                   : (g_part_sym && g_part_sym->sec==g_cursec) ? 0xFFFFFFFFu
                                   : (uint32_t)(1+2*(g_part_sym?g_part_sym->sec:0));
            g_rel[g_nrel].addend=g_part_add;
            g_nrel++;
        }
        g_part_pending=0; g_part_sym=NULL;
    }
    g_loc++;
}
/* arm a partial-field relocation (type 0x28 dma / 0x29 page) against a label for the
 * next word emit() writes; addend is the full evaluated offset (stored in reserved). */
static void arm_part_reloc(Sym*s, long addend, uint16_t type){
    if(g_c5xcoff && g_final && sym_reloc(s)){ g_part_pending=1; g_part_sym=s; g_part_add=(uint16_t)addend; g_part_type=type; }
    else { g_part_pending=0; g_part_sym=NULL; g_part_add=0; }
}
/* field_at() calls this right after evaluating a DIRECT (dma) operand: if it named a
 * single relocatable label, arm a dma (R_PARTLS7) relocation for the next emit. */
void dma_operand(long addend){
    if(g_evnsym==1) arm_part_reloc(g_evsym, addend, R_PARTLS7);
    else { g_part_pending=0; g_part_sym=NULL; g_part_add=0; }
}

/* record a relocation for the full-word address about to be emitted at g_loc, when
 * the most recent eval() referenced exactly one label (a branch/call target or a
 * long-immediate address). The relocation is R_RELWORD (type 0x10) in both cases;
 * what differs is the symbol it is relative to: a label in the SAME section uses
 * the self-section form (symndx 0xFFFFFFFF), a label in ANOTHER section is
 * relocated against that section's symbol-table entry (symndx = 1 + 2*index). The
 * emitted word already holds the label's section-relative offset. (Direct-address
 * dma references use the DP-relative type-0x28 relocation instead - armed by
 * dma_operand() and recorded in emit().) */
static void reloc_here(void){
    if(!(g_c5xcoff && g_final && g_evnsym==1 && sym_reloc(g_evsym))) return;
    if(g_nrel>=(int)(sizeof g_rel/sizeof g_rel[0])) return;
    g_rel[g_nrel].sec=g_cursec; g_rel[g_nrel].off=(uint16_t)g_loc; g_rel[g_nrel].type=R_RELWORD;
    g_rel[g_nrel].ext = sym_external(g_evsym) ? g_evsym : NULL;
    g_rel[g_nrel].symndx = sym_external(g_evsym) ? 0u /* patched in write_c5xcoff */
                           : (g_evsym->sec==g_cursec) ? 0xFFFFFFFFu : (uint32_t)(1+2*g_evsym->sec);
    g_rel[g_nrel].addend=0;
    g_nrel++;
}
/* An immediate (captured rn=g_evnsym, rs=g_evsym right after its eval) is a
 * relocatable reference when, in COFF output, it names a label. The toolchain then uses
 * the long instruction form and emits a relocation on the immediate word. */
#define IS_RELOC(rn,rs) (g_c5xcoff && (rn)>=1 && sym_reloc(rs))
/* Emit an immediate/address word, recording its relocation first (restores the
 * eval state the caller captured so reloc_here() sees the right symbol). */
static void emit_sym(long k, int rn, Sym *rs){ g_evsym=rs; g_evnsym=rn; reloc_here(); emit((uint16_t)(k&0xFFFF)); }

/* operand parsing (split/indirect/field/shift/imm/cond/ctrl) lives in operands.c */

/* ---------------- ISA lookup ---------------- */
static const IsaEnt *isa_find(const char *n){
    for(int i=0;i<C5X_ISA_N;i++) if(!strcasecmp(c5x_isa[i].name,n)) return &c5x_isa[i];
    return NULL;
}

/* ---------------- per-class encoders ---------------- */
#define A(i) ((i)<np?pa[i]:"")
static void enc_acc(const char*M,char*pa[],int np);   /* 0x1d ADD/LAC/LACC/SUB */
static void enc_logic(const char*M,char*pa[],int np); /* 0x1e */
static void enc_laclldp(const char*M,char*pa[],int np);
static void enc_mpyrpt(const char*M,char*pa[],int np);
static void enc_bldd(const char*M,char*pa[],int np);
static void enc_lstsst(const char*M,char*pa[],int np);

static void encode(const IsaEnt*e, const char*M, char*pa[], int np){
    uint16_t op=e->opcode;
    switch(e->fmt){
    case 0x00: emit(op); return;                                   /* no operand */
    case 0x1c: if(op==0){err("instruction not valid for C5x (-v50)");return;} emit(op); return;
    case 0x01: emit((uint16_t)(op|field_at(pa,np,0))); return;     /* {ind|dma} */
    case 0x13: emit((uint16_t)(op|field_at(pa,np,0))); return;     /* NORM {ind} */
    case 0x02: {                                                   /* branch/call */
        if((op>>12)==0x7){ uint16_t ib=(uint16_t)(op&0xFF); long tgt=0; int gott=0;
            for(int i=0;i<np;i++){ if(pa[i][0]=='*') ib=indirect_byte(pa,np,i); else if(!gott){tgt=eval(pa[i]);gott=1;} }
            emit((uint16_t)((op&0xFF00)|ib)); reloc_here(); emit((uint16_t)(tgt&0xFFFF)); }
        else { long t=eval(A(0)); emit(op); reloc_here(); emit((uint16_t)(t&0xFFFF)); }   /* pre-baked cond branch */
        return; }
    case 0x03: { long sh=shift_after(pa,np,0); if(sh<0||sh>7)err("shift out of range 0-7"); emit((uint16_t)(op|((sh&7)<<8)|field_at(pa,np,0))); return; } /* SACL/SACH */
    case 0x04: { emit((uint16_t)(op|field_at(pa,np,0)));                 /* IN/OUT: word2=port */
        int f; long port=value_before_arp(pa,np,0,&f); if(!f)err("IN/OUT requires a port"); emit((uint16_t)(port&0xFFFF)); return; }
    case 0x05: {                                                   /* SAR/LAR ARn, ... */
        int n; if(!is_arreg(A(0),&n)){err("needs ARn");return;}
        if(!strcasecmp(M,"SAR")){ emit((uint16_t)(0x8000|((n&7)<<8)|field_at(pa,np,1))); return; }
        /* LAR */
        if(imm_tok(A(1))){ long k=imm_val(A(1)); int rn=g_evnsym; Sym*rs=g_evsym;
            if(k>=0&&k<=0xFF&&!IS_RELOC(rn,rs)) emit((uint16_t)(0xB000|((n&7)<<8)|(k&0xFF)));
            else { emit((uint16_t)(0xBF08|(n&7))); emit_sym(k,rn,rs); } }
        else emit((uint16_t)(0x0000|((n&7)<<8)|field_at(pa,np,1)));
        return; }
    case 0x06: { long sh=shift_after(pa,np,0); emit((uint16_t)(op|((sh&0xF)<<8)|field_at(pa,np,0))); return; } /* C2x LACI/ADDI/SUBI */
    case 0x07: { int n; if(!is_arreg(A(0),&n)){err("needs ARn");return;} long k=imm_val(A(1)); int rn=g_evnsym; Sym*rs=g_evsym;
        if(IS_RELOC(rn,rs)){
            if(k>=0&&k<=0xFF) arm_part_reloc(rs,k,R_RELBYTE);
            else { emit((uint16_t)(0xBF08u|(n&7))); emit_sym(k,rn,rs); return; }  /* promote LARK -> LRLK + R_RELWORD */
        }
        emit((uint16_t)(op|((n&7)<<8)|(k&0xFF))); return; } /* LARK #k8 (R_RELBYTE, promotes to LRLK) */
    case 0x08: { long k=imm_val(A(0)); int rn=g_evnsym; Sym*rs=g_evsym;
        if(IS_RELOC(rn,rs)) arm_part_reloc(rs,k,R_REL);   /* 13-bit, never promoted */
        emit((uint16_t)(op|(k&0x1FFF))); return; } /* MPYK #k13 (R_REL) */
    case 0x09: { const char*t=A(0); long k=imm_tok(t)?imm_val(t):eval(t); emit((uint16_t)(op|(k&0xFF))); return; } /* ADRK/SBRK/RPTK */
    case 0x0a: { long n=eval(A(0)); if(n<0||n>7)err("ARP out of range 0-7"); emit((uint16_t)(op|(n&7))); return; } /* LARP */
    case 0x0b: { const char*t=A(0); long k=imm_tok(t)?imm_val(t):eval(t); emit((uint16_t)(op|(k&0x1FF))); return; } /* LDPK #k9 */
    case 0x0c: { if(op==0){err("instruction not valid for C5x (-v50)");return;} /* CONF: invalid on C5x */
                 const char*t=A(0); long k=imm_tok(t)?imm_val(t):eval(t); if(k<0||k>3)err("constant out of range 0-3"); emit((uint16_t)(op|(k&0xF))); return; } /* CMPR/SPM */
    case 0x0d: err("serial port instruction invalid on C5x (-v50)"); return; /* FORT - invalid on C5x */
    case 0x0e: { int f; long bit=value_before_arp(pa,np,0,&f); if(!f)err("BIT requires a bit code"); if(bit<0||bit>15)err("bit code out of range 0-15"); emit((uint16_t)(op|((bit&0xF)<<8)|field_at(pa,np,0))); return; } /* BIT dma,bit */
    case 0x0f: { int n=0; is_arreg(A(0),&n); emit((uint16_t)(op|(n&7))); long k=imm_val(A(1)); emit_sym(k,g_evnsym,g_evsym); return; } /* LRLK */
    case 0x10: { const char*t=A(0); long k=imm_tok(t)?imm_val(t):eval(t); int rn=g_evnsym; Sym*rs=g_evsym;
        if(IS_RELOC(rn,rs)){
            if(k>=0&&k<=0xFF){ arm_part_reloc(rs,k,R_RELBYTE); }
            else { /* offset too big for the 8-bit field: promote to the long
                    * mnemonic (ADLK/LALK/SBLK) + full word + R_RELWORD. Span-dependent;
                    * the fixed-point pass loop converges when the offset settles. */
                uint16_t lop = !strcasecmp(M,"ADDK")?0xBF90u : !strcasecmp(M,"LACK")?0xBF80u : !strcasecmp(M,"SUBK")?0xBFA0u : 0u;
                if(lop){ emit(lop); emit_sym(k,rn,rs); return; }
            }
        }
        emit((uint16_t)(op|(k&0xFF))); return; } /* ADDK/LACK/SUBK #k8 (R_RELBYTE, promotes to long) */
    case 0x11: { emit((uint16_t)(op|field_at(pa,np,1))); const char*t=A(0); long k=imm_tok(t)?imm_val(t):eval(t); emit_sym(k,g_evnsym,g_evsym); return; } /* MAC pma,field */
    case 0x12: { long sh=shift_after(pa,np,0); emit((uint16_t)(op|(sh&0xF))); long k=imm_val(A(0)); emit_sym(k,g_evnsym,g_evsym); return; } /* C2x long-imm ALK/ANDK.. */
    case 0x14: { emit((uint16_t)(op|field_at(pa,np,1))); long k=imm_val(A(0)); emit_sym(k,g_evnsym,g_evsym); return; } /* SPLK #imm,field */
    case 0x15: { long v=eval(A(0)); if(v<1||v>16)err("BSAR shift out of range 1-16"); emit((uint16_t)(op|((v-1)&0xF))); return; }   /* BSAR shift(1-16) */
    case 0x16: { const char*t=A(0); emit(op); long k=imm_tok(t)?imm_val(t):eval(t); emit_sym(k,g_evnsym,g_evsym); return; } /* RPTB/RPTZ/RPTR/MRKL */
    case 0x17: { emit((uint16_t)(op|field_at(pa,np,0))); long k=imm_val(A(1)); emit_sym(k,g_evnsym,g_evsym); return; } /* LMMR/SMMR field,#addr */
    case 0x18: { long t=eval(A(0)); int tp; int byte=parse_cond(pa,np,1,&tp); emit((uint16_t)((op&0xFC00)|(tp<<8)|byte)); reloc_here(); emit((uint16_t)(t&0xFFFF)); return; } /* BCND/CC + delayed */
    case 0x19: { long n=eval(A(0)); if(n!=1&&n!=2)err("XC count must be 1 or 2"); int tp; int byte=parse_cond(pa,np,1,&tp); emit((uint16_t)(0xE400|((n==2)?0x1000:0)|(tp<<8)|byte)); return; } /* XC */
    case 0x1a: { int tp; int byte=parse_cond(pa,np,0,&tp); emit((uint16_t)((op&0xFC00)|(tp<<8)|byte)); return; } /* RETC/RETCD */
    case 0x1b: { const char*t=A(0); long k=imm_tok(t)?imm_val(t):eval(t); if(k<0||k>31)err("interrupt number out of range 0-31"); emit((uint16_t)(op|(k&0x1F))); return; } /* INTR */
    case 0x1d: enc_acc(M,pa,np); return;
    case 0x1e: enc_logic(M,pa,np); return;
    case 0x1f: enc_laclldp(M,pa,np); return;
    case 0x20: enc_mpyrpt(M,pa,np); return;
    case 0x21: enc_bldd(M,pa,np); return;
    case 0x22: { int set=!strcasecmp(M,"SETC"); emit((uint16_t)(0xBE40|(ctrl_field(A(0))<<1)|(set?1:0))); return; }
    case 0x23: enc_lstsst(M,pa,np); return;
    default: err("format class not implemented"); return;
    }
}

static void enc_acc(const char*M,char*pa[],int np){
    uint16_t base = !strcasecmp(M,"SUB")?0x3000 : (!strcasecmp(M,"ADD")?0x2000:0x1000); /* LAC/LACC->0x1000 */
    uint16_t base16= !strcasecmp(M,"SUB")?0x6500 : (!strcasecmp(M,"ADD")?0x6100:0x6A00);
    if(imm_tok(A(0))){ long k=imm_val(A(0)); int rn=g_evnsym; Sym*rs=g_evsym; long sh=(np>1)?eval(A(1)):0; int rel=IS_RELOC(rn,rs);
        if(!strcasecmp(M,"ADD")){ if(sh==0&&k>=0&&k<=0xFF&&!rel){emit((uint16_t)(0xB800|(k&0xFF)));return;} emit((uint16_t)(0xBF90|(sh&0xF))); emit_sym(k,rn,rs); return; }
        if(!strcasecmp(M,"SUB")){ if(sh==0&&k>=0&&k<=0xFF&&!rel){emit((uint16_t)(0xBA00|(k&0xFF)));return;} emit((uint16_t)(0xBFA0|(sh&0xF))); emit_sym(k,rn,rs); return; }
        emit((uint16_t)(0xBF80|(sh&0xF))); emit_sym(k,rn,rs); return; /* LACC/LAC: always long */
    }
    long sh=shift_after(pa,np,0); uint16_t fld=field_at(pa,np,0);
    if(sh==16) emit((uint16_t)(base16|fld)); else emit((uint16_t)(base|((sh&0xF)<<8)|fld));
}
static void enc_logic(const char*M,char*pa[],int np){
    if(!strcasecmp(M,"AND")||!strcasecmp(M,"OR")||!strcasecmp(M,"XOR")){
        uint16_t memb=!strcasecmp(M,"AND")?0x6E00:(!strcasecmp(M,"OR")?0x6D00:0x6C00);
        uint16_t immb=!strcasecmp(M,"AND")?0xBFB0:(!strcasecmp(M,"OR")?0xBFC0:0xBFD0);
        if(imm_tok(A(0))){ long k=imm_val(A(0)); int rn=g_evnsym; Sym*rs=g_evsym; long sh=(np>1)?eval(A(1)):0; emit((uint16_t)(immb|(sh&0xF))); emit_sym(k,rn,rs); }
        else emit((uint16_t)(memb|field_at(pa,np,0)));
        return;
    }
    /* APL/OPL/CPL/XPL */
    uint16_t nob=!strcasecmp(M,"APL")?0x5A00:(!strcasecmp(M,"OPL")?0x5900:(!strcasecmp(M,"CPL")?0x5B00:0x5800));
    uint16_t imb=!strcasecmp(M,"APL")?0x5E00:(!strcasecmp(M,"OPL")?0x5D00:(!strcasecmp(M,"CPL")?0x5F00:0x5C00));
    if(imm_tok(A(0))){ if(np<2){err("operand missing: #imm form needs a data-memory operand"); return;} emit((uint16_t)(imb|field_at(pa,np,1))); long k=imm_val(A(0)); emit_sym(k,g_evnsym,g_evsym); }
    else emit((uint16_t)(nob|field_at(pa,np,0)));
}
static void enc_laclldp(const char*M,char*pa[],int np){
    if(!strcasecmp(M,"LACL")){ if(imm_tok(A(0))){ long k=imm_val(A(0)); int rn=g_evnsym; Sym*rs=g_evsym; if(k>=0&&k<=0xFF&&!IS_RELOC(rn,rs)) emit((uint16_t)(0xB900|(k&0xFF))); else { emit((uint16_t)0xBF80); emit_sym(k,rn,rs); } } else emit((uint16_t)(0x6900|field_at(pa,np,0))); return; }
    /* LDP #page : a literal is the 9-bit page directly; a relocatable symbol is a
     * data ADDRESS whose page (addr>>7) goes in the field, with an R_PARTMS9 (0x29)
     * relocation carrying the full offset as addend. */
    if(imm_tok(A(0))){ long k=imm_val(A(0)); int rn=g_evnsym; Sym*rs=g_evsym;
        if(IS_RELOC(rn,rs)){ arm_part_reloc(rs,k,R_PARTMS9); emit((uint16_t)(0xBC00|((k>>7)&0x1FF))); }
        else emit((uint16_t)(0xBC00|(k&0x1FF))); }
    else emit((uint16_t)(0x0D00|field_at(pa,np,0)));
}
static void enc_mpyrpt(const char*M,char*pa[],int np){
    if(!strcasecmp(M,"MPY")){ if(imm_tok(A(0))){ long k=imm_val(A(0)); int rn=g_evnsym; Sym*rs=g_evsym; if(k>=-4096&&k<=4095&&!IS_RELOC(rn,rs)) emit((uint16_t)(0xC000|(k&0x1FFF))); else { emit((uint16_t)0xBE80); emit_sym(k,rn,rs); } } else emit((uint16_t)(0x5400|field_at(pa,np,0))); return; }
    /* RPT: #k<=0xFF short (0xBB00|k); else long RPTR (0xBEC4)+word */
    if(imm_tok(A(0))){ long k=imm_val(A(0)); if(k>=0&&k<=0xFF) emit((uint16_t)(0xBB00|(k&0xFF))); else { emit((uint16_t)0xBEC4); emit((uint16_t)(k&0xFFFF)); } }
    else emit((uint16_t)(0x0B00|field_at(pa,np,0)));
}
static void enc_bldd(const char*M,char*pa[],int np){
    if(!strcasecmp(M,"BLPD")){ /* source is program memory: #pma (long) or BMAR (single word) */
        if(np>1&&!strcasecmp(A(0),"BMAR")){ emit((uint16_t)(0xA400|field_at(pa,np,1))); return; }
        if(!imm_tok(A(0))){ err("BLPD source must be #pma or BMAR"); return; }
        emit((uint16_t)(0xA500|field_at(pa,np,1))); long k=imm_val(A(0)); emit_sym(k,g_evnsym,g_evsym); return; }
    /* BMAR-addressed single-word forms: BLDD BMAR,dst == BDSD dst (0xAC00),
     * BLDD src,BMAR == BDDD src (0xAD00); BMAR holds the other address. */
    if(np>1&&!strcasecmp(A(0),"BMAR")){ emit((uint16_t)(0xAC00|field_at(pa,np,1))); return; }
    if(np>1&&!strcasecmp(A(1),"BMAR")){ emit((uint16_t)(0xAD00|field_at(pa,np,0))); return; }
    /* BLDD: #src,dst -> A800|field(dst),src ; src,#dst -> A900|field(src),dst */
    if(imm_tok(A(0))){ emit((uint16_t)(0xA800|field_at(pa,np,1))); long k=imm_val(A(0)); emit_sym(k,g_evnsym,g_evsym); }
    else if(np>1&&imm_tok(A(1))){ emit((uint16_t)(0xA900|field_at(pa,np,0))); long k=imm_val(A(1)); emit_sym(k,g_evnsym,g_evsym); }
    else err("BLDD needs one immediate operand");
}
static void enc_lstsst(const char*M,char*pa[],int np){
    long n=imm_tok(A(0))?imm_val(A(0)):eval(A(0));
    uint16_t base=!strcasecmp(M,"SST")?0x8E00:0x0E00;
    emit((uint16_t)(base|((n&1)<<8)|field_at(pa,np,1)));
}

/* ---------------- line assembler ---------------- */
static void rstrip(char*s){ size_t n=strlen(s); while(n&&(s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]==' '||s[n-1]=='\t'))s[--n]=0; }

static void asm_line(char *line){
    char *zz=strchr(line,0x1A); if(zz)*zz=0;
    if(line[0]=='*'){ return; }
    char *sc=strchr(line,';'); if(sc)*sc=0;
    rstrip(line); if(!*line) return;

    { char tmp[256]; strncpy(tmp,line,255); tmp[255]=0;
      char *f1=strtok(tmp," \t"); char *f2=f1?strtok(NULL," \t"):NULL;
      if(f1&&f2&&(!strcasecmp(f2,".set")||!strcasecmp(f2,".equ"))){
        char *val=strtok(NULL,"");
        int simple=1; for(char *c=f1;*c;c++) if(!(isalnum((unsigned char)*c)||*c=='_')){ simple=0; break; }
        if(simple) sym_set(f1, val?eval(val):0);
        return;
      } }

    char *p=line; char label[64]={0};
    if(!isspace((unsigned char)*p)&&(isalpha((unsigned char)*p)||*p=='_')){
        int n=0; char *q=p; while((isalnum((unsigned char)*q)||*q=='_')&&n<63)label[n++]=*q++;
        label[n]=0;
        if(*q=='+'||*q=='-'){ while(*q && !isspace((unsigned char)*q))q++; label[0]=0; }
        else if(*q==':')q++;
        p=q;
    }
    while(*p==' '||*p=='\t')p++;
    char mn[64]; int n=0; while(*p&&!isspace((unsigned char)*p)&&n<63)mn[n++]=*p++; mn[n]=0;
    while(*p==' '||*p=='\t')p++;
    char *operands=p;

    if(!strcasecmp(mn,".set")||!strcasecmp(mn,".equ")){ if(!label[0]){err(".set without label");return;} sym_set(label,eval(operands)); return; }
    if(label[0]){ sym_set(label,g_loc); Sym*ls=sym_find(label); if(ls){ ls->is_label=1; ls->sec=g_cursec; } }
    if(!mn[0]) return;

    /* directives */
    if(!strcasecmp(mn,".mmregs")){ if(g_pass==1)define_mmregs(); return; }
    if(!strcasecmp(mn,".ps")){ sect_use(".text",0x20,0,1); g_loc=eval(operands); return; }
    if(!strcasecmp(mn,".ds")){ sect_use(".data",0x40,0,1); g_loc=eval(operands); return; }
    if(!strcasecmp(mn,".text")){ sect_use(".text",0x20,0,1); return; }
    if(!strcasecmp(mn,".data")){ sect_use(".data",0x40,0,1); return; }
    /* .global/.def/.ref <name>[,<name>...]: mark symbols for the COFF symbol
     * table. A defined global is emitted (C_EXT, scnum = its section) or, when it
     * is only referenced and not defined here, an external (scnum 0). */
    if(!strcasecmp(mn,".global")||!strcasecmp(mn,".def")||!strcasecmp(mn,".ref")){
        const char*gp=operands;
        while(*gp){ while(*gp==' '||*gp=='\t'||*gp==',')gp++; if(!*gp)break;
            char gn[64]; int gi=0; while((isalnum((unsigned char)*gp)||*gp=='_'||*gp=='$')&&gi<63)gn[gi++]=*gp++; gn[gi]=0;
            if(gi){ Sym*s=sym_intern(gn); s->global=1; }
            while(*gp&&*gp!=',')gp++; }
        return; }
    if(!strcasecmp(mn,".entry")||!strcasecmp(mn,".end")||!strcasecmp(mn,".width")||
       !strcasecmp(mn,".length")||!strcasecmp(mn,".title")||!strcasecmp(mn,".version")||
       !strcasecmp(mn,".liston")||!strcasecmp(mn,".listoff")||!strcasecmp(mn,".include")) return;
    /* .sect "name": switch to a named INITIALISED section (STYP flags 0x40). */
    if(!strcasecmp(mn,".sect")){ char nm[64];
        if(!sec_name(operands,nm,sizeof nm)){ err(".sect needs a \"name\""); return; }
        sect_use(nm,0x40,0,1); return; }
    /* symbol .usect "name", words: reserve space in a named UNINITIALISED section
     * (0x80) and define the label there; the current section is NOT changed. */
    if(!strcasecmp(mn,".usect")){ char nm[64],*rest=operands;
        if(!sec_name_adv(&rest,nm,sizeof nm)){ err(".usect needs a \"name\""); return; }
        long words=eval(sec_next_arg(&rest));
        int j=sect_use(nm,0x80,1,0);
        if(label[0]){ sym_set(label,g_sect[j].loc); Sym*ls=sym_find(label); if(ls){ ls->is_label=1; ls->sec=j; } }
        g_sect[j].loc += words; return; }
    /* .bss symbol, words: reserve space in .bss and define the symbol there (the
     * symbol is the first operand, not a line label); current section unchanged. */
    if(!strcasecmp(mn,".bss")){ char sym[64],*rest=operands;
        if(!sec_sym_adv(&rest,sym,sizeof sym)){ err(".bss needs a symbol"); return; }
        long words=eval(sec_next_arg(&rest));
        int j=sect_index(".bss");
        sym_set(sym,g_sect[j].loc); Sym*ls=sym_find(sym); if(ls){ ls->is_label=1; ls->sec=j; }
        g_sect[j].loc += words; return; }
    if(!strcasecmp(mn,".space")){ long bits=eval(operands); long nw=(bits+15)/16; for(long i=0;i<nw;i++) emit(0x0000); return; }
    if(!strcasecmp(mn,".word")){ char buf[512]; strncpy(buf,operands,sizeof buf-1); buf[sizeof buf-1]=0;
        char *pp[64]; int np2=split(buf,pp,64);
        /* a .word whose value is a relocatable label carries a full-word (type 0x10)
         * relocation, exactly like a branch target - reloc_here() reads the eval. */
        for(int i=0;i<np2;i++){ long v=eval(pp[i]); reloc_here(); emit((uint16_t)(v&0xFFFF)); } return; }
    if(!strcasecmp(mn,".byte")||!strcasecmp(mn,".ubyte")){ char buf[512]; strncpy(buf,operands,sizeof buf-1); buf[sizeof buf-1]=0;
        char *pp[64]; int np2=split(buf,pp,64); for(int i=0;i<np2;i++) emit((uint16_t)(eval(pp[i])&0xFF)); return; }  /* one word per byte */
    if(!strcasecmp(mn,".long")||!strcasecmp(mn,".int")){ char buf[512]; strncpy(buf,operands,sizeof buf-1); buf[sizeof buf-1]=0;
        char *pp[64]; int np2=split(buf,pp,64); for(int i=0;i<np2;i++){ long v=eval(pp[i]); emit((uint16_t)(v&0xFFFF)); emit((uint16_t)((v>>16)&0xFFFF)); } return; } /* 32-bit, low word first */
    if(!strcasecmp(mn,".string")||!strcasecmp(mn,".pstring")){
        /* concatenate all quoted chars / bare values, pack 2 chars per word (first in high byte) */
        int hi=-1; const char*sp=operands;
        while(*sp){
            while(*sp==' '||*sp=='\t'||*sp==',')sp++;
            if(!*sp)break;
            if(*sp=='"'){ sp++; while(*sp&&*sp!='"'){ int ch=(unsigned char)*sp++;
                    if(hi<0)hi=ch; else { emit((uint16_t)((hi<<8)|ch)); hi=-1; } }
                if(*sp=='"')sp++; }
            else { char t[64];int m=0; while(*sp&&*sp!=','&&m<63)t[m++]=*sp++; t[m]=0; int ch=(int)(eval(t)&0xFF);
                    if(hi<0)hi=ch; else { emit((uint16_t)((hi<<8)|ch)); hi=-1; } }
        }
        if(hi>=0) emit((uint16_t)(hi<<8));   /* odd char: zero-pad low byte */
        return; }

    char ob[512]; strncpy(ob,operands,sizeof ob-1); ob[sizeof ob-1]=0;
    char *pa[16]; int np=split(ob,pa,16);

    const IsaEnt *e=isa_find(mn);
    if(!e){ err("unknown mnemonic"); return; }
    char M[64]; strncpy(M,e->name,63); M[63]=0;   /* canonical upper-case name */
    encode(e,M,pa,np);
}

/* ---------------- source loading (.include) ---------------- */
static char g_lines[1<<16][256]; static int g_nl=0;
static int load_src(const char *path){
    FILE*f=fopen(path,"rb"); if(!f) return 0;
    char dir[1024]; strncpy(dir,path,sizeof dir-1); dir[sizeof dir-1]=0;
    char *sl=strrchr(dir,'/'); if(sl)*sl=0; else strcpy(dir,".");
    char buf[256];
    while(fgets(buf,sizeof buf,f)){
        char *p=buf; while(*p==' '||*p=='\t')p++;
        if(!strncasecmp(p,".include",8)){
            char fn[256]={0}; char *q=strchr(p,'"');
            if(q){ char *q2=strchr(q+1,'"'); if(q2){ size_t len=(size_t)(q2-q-1); if(len>255)len=255; memcpy(fn,q+1,len); fn[len]=0; } }
            else sscanf(p+8,"%255s",fn);
            for(char *c=fn;*c;c++) if(*c=='\\') *c='/';
            char full[1400]; snprintf(full,sizeof full,"%s/%s",dir,fn);
            if(!load_src(full) && !load_src(fn)) { fprintf(stderr,"%s: cannot open include '%s'\n",path,fn); fclose(f); return 0; }
            continue;
        }
        { size_t L=strlen(buf); while(L&&(buf[L-1]=='\n'||buf[L-1]=='\r'))buf[--L]=0; }  /* strip EOL for the preprocessor */
        if(g_nl<(int)(sizeof g_lines/sizeof g_lines[0])) strcpy(g_lines[g_nl++],buf);
    }
    fclose(f); return 1;
}

/* ---------------- COFF object output (TI C5x COFF v1/v2) ----------------
 * Structure follows the TI COFF v2 object layout: file header
 * (22B v2 / 20B v1), three section headers (.text/.data/.bss; 48B v2 / 40B v1),
 * section data (little-endian words), then the symbol table (.file + a C_STAT
 * symbol per section, each with a 1 aux giving the section length). Names <= 8
 * chars are inline (no string table). v2 adds the C5x target id
 * 0x0092 to the file header and widens s_nreloc/s_nlnno to 4 bytes.  */
/* section header: a relocatable .obj zeroes paddr/vaddr (the linker fills real
 * addresses in the .out). Built from the shared c5xcoff primitives. v1 = 40 bytes
 * (16-bit nreloc/nlnno), v2 = 48 bytes (32-bit, plus a reserved word). */
/* String table: COFF stores names longer than 8 chars out-of-line. A name field
 * is then {0,0,0,0, offset} pointing past the 4-byte size word at the table head.
 * offsets are assigned in symbol-table emission order (.file, then section names,
 * then user symbols), de-duplicated. */
typedef struct { const char*nm; long off; } StrEnt;
static long str_off(const StrEnt*st,int nst,const char*nm){
    for(int k=0;k<nst;k++){ if(!strcmp(st[k].nm,nm)) return st[k].off; } return -1; }
/* write an 8-byte name field: inline when off<0, else the long-name {0,offset} form */
static void cf_wname(FILE*f,const char*nm,long off){
    if(off>=0){ c5xcoff_put32(f,0); c5xcoff_put32(f,(unsigned long)off); } else c5xcoff_w_name(f,nm); }
/* a full symbol-table entry with an explicit type and an optional string offset */
static void cf_wsym(FILE*f,const char*nm,long off,long val,int scnum,int type,int sclass,int naux){
    cf_wname(f,nm,off); c5xcoff_put32(f,(unsigned long)val);
    c5xcoff_put16(f,(unsigned)(scnum&0xffff)); c5xcoff_put16(f,(unsigned)type);
    fputc(sclass&0xff,f); fputc(naux&0xff,f); }

static void cf_scnhdr(FILE*f,const char*nm,long noff,long size,long scnptr,long relptr,int nreloc,unsigned flags,int ver){
    cf_wname(f,nm,noff); c5xcoff_put32(f,0); c5xcoff_put32(f,0); c5xcoff_put32(f,(unsigned long)size);
    c5xcoff_put32(f,(unsigned long)scnptr); c5xcoff_put32(f,(unsigned long)relptr); c5xcoff_put32(f,0); /* relptr, lnnoptr */
    if(ver==2){ c5xcoff_put32(f,(unsigned)nreloc); c5xcoff_put32(f,0); c5xcoff_put32(f,flags); c5xcoff_put32(f,0); }
    else      { c5xcoff_put16(f,(unsigned)nreloc); c5xcoff_put16(f,0); c5xcoff_put32(f,flags); }
}

static void write_c5xcoff(const char*path,int ver,const char*srcfile){
    int ns=g_nsect;
    /* per-section: word count (initialised data), total size, relocation count */
    long nword[64]={0}, size[64]={0}, rcnt[64]={0};
    for(int i=0;i<g_nout;i++){ int s=g_out[i].sec; if(s>=0&&s<ns) nword[s]++; }
    for(int i=0;i<g_nrel;i++){ int s=g_rel[i].sec; if(s>=0&&s<ns) rcnt[s]++; }
    for(int s=0;s<ns;s++) size[s] = g_sect[s].uninit ? g_sect[s].loc : nword[s];
    int fhsz=(ver==2)?22:20, shsz=(ver==2)?48:40;
    /* lay out section data (initialised, non-empty sections) then relocation tables,
     * both in section-table order; uninitialised/empty get scnptr 0. */
    long cur=fhsz+(long)ns*shsz, scnptr[64]={0};
    for(int s=0;s<ns;s++){ if(!g_sect[s].uninit && nword[s]>0){ scnptr[s]=cur; cur+=nword[s]*2; } }
    long relptr[64]={0};
    for(int s=0;s<ns;s++){ if(rcnt[s]>0){ relptr[s]=cur; cur+=rcnt[s]*12; } }
    long symptr=cur;
    /* .file basename (without directory) */
    const char*base=srcfile; for(const char*p=srcfile;*p;p++) if(*p=='/'||*p=='\\') base=p+1;

    /* ---- user symbols named in .global/.def/.ref, emitted after the section
     * symbols. DEFINED globals are listed first (ascending by name, ASCII), then
     * UNDEFINED externals in order of first appearance; a global that is neither
     * defined nor referenced is dropped. ---- */
    static Sym* us[8192]; int nus=0;
    for(int i=0;i<g_nsyms;i++){ Sym*s=&g_syms[i]; if(s->global && s->defined) us[nus++]=s; }
    for(int i=1;i<nus;i++){ Sym*k=us[i]; int j=i-1; while(j>=0 && strcmp(us[j]->name,k->name)>0){ us[j+1]=us[j]; j--; } us[j+1]=k; }
    for(int i=0;i<g_nsyms;i++){ Sym*s=&g_syms[i]; if(s->global && !s->defined && s->referenced) us[nus++]=s; }
    int uidx0=1+2*ns;                                   /* first user-symbol index */
    for(int i=0;i<nus;i++) us[i]->symidx=uidx0+i;
    for(int i=0;i<g_nrel;i++) if(g_rel[i].ext) g_rel[i].symndx=(uint32_t)g_rel[i].ext->symidx;

    /* ---- string table: names > 8 chars, in emission order (.file, section names,
     * user symbols), de-duplicated; offsets counted from 4 (past the size word). ---- */
    static StrEnt st[8192+128]; int nst=0; long stlen=4;
    #define STADD(NM) do{ const char*_n=(NM); if(strlen(_n)>8 && str_off(st,nst,_n)<0){ \
        st[nst].nm=_n; st[nst].off=stlen; stlen+=(long)strlen(_n)+1; nst++; } }while(0)
    STADD(base);
    for(int s=0;s<ns;s++) STADD(g_sect[s].name);
    for(int i=0;i<nus;i++) STADD(us[i]->name);
    #undef STADD

    FILE*f=fopen(path,"wb"); if(!f){ perror(path); g_errors++; return; }
    /* file header: magic, nscns, timdat=0, symptr, nsyms = 1 file + 2/section + users */
    c5xcoff_put16(f,(ver==2)?COFF_MAGIC_V2:COFF_MAGIC_V1); c5xcoff_put16(f,(unsigned)ns); c5xcoff_put32(f,0);
    c5xcoff_put32(f,(unsigned long)symptr); c5xcoff_put32(f,(unsigned)(1+2*ns+nus));
    c5xcoff_put16(f,0); c5xcoff_put16(f,0x0140);                                       /* opthdr=0, flags */
    if(ver==2) c5xcoff_put16(f,COFF_TARGET_C5X);                                    /* target id C5x */
    /* section headers */
    for(int s=0;s<ns;s++)
        cf_scnhdr(f,g_sect[s].name,str_off(st,nst,g_sect[s].name),size[s],scnptr[s],relptr[s],(int)rcnt[s],g_sect[s].flags,ver);
    /* section data: each initialised section's words in address order */
    for(int s=0;s<ns;s++){ if(g_sect[s].uninit) continue;
        for(int i=0;i<g_nout;i++) if(g_out[i].sec==s) c5xcoff_put16(f,g_out[i].word); }
    /* relocation tables, in section order */
    for(int s=0;s<ns;s++)
        for(int i=0;i<g_nrel;i++) if(g_rel[i].sec==s) c5xcoff_w_reloc(f,g_rel[i].off,g_rel[i].symndx,g_rel[i].addend,g_rel[i].type);
    /* symbol table: .file, one C_STAT (+length/reloc aux) per section, then users */
    cf_wsym(f,base,str_off(st,nst,base),0,-2,0,C_FILE,0);                         /* C_FILE */
    for(int s=0;s<ns;s++){ cf_wsym(f,g_sect[s].name,str_off(st,nst,g_sect[s].name),0,s+1,0,C_STAT,1); c5xcoff_w_aux_scn(f,size[s],(int)rcnt[s]); }
    for(int i=0;i<nus;i++){ Sym*s=us[i];
        int scnum = s->is_label ? s->sec+1 : s->defined ? -1 : 0;   /* label: its section; .set: ABS; else external */
        long val  = s->defined ? s->value : 0;
        cf_wsym(f,s->name,str_off(st,nst,s->name),val,scnum,0x0004,C_EXT,0); }
    /* string table trailer: size word (incl. itself) then each long name, NUL-terminated */
    if(nst>0){ c5xcoff_put32(f,(unsigned long)stlen);
        for(int k=0;k<nst;k++) fwrite(st[k].nm,1,strlen(st[k].nm)+1,f); }
    fclose(f);
}

/* ================= preprocessor: macros, .loop, .if, .asg, .eval =================
 * Runs once over g_lines before assembly, expanding into g_lines. Text-level,
 * front end: .asg/.eval substitution symbols, .if/.elseif/.else/
 * .endif conditional assembly, .loop/.endloop repetition, and .macro/.endm with
 * positional parameter substitution. Does not affect the flat output path
 * (that source uses none of these). */
typedef struct { char name[32]; char val[160]; } Subst;
static Subst g_sub[512]; static int g_nsub=0;
static void sub_set(const char*n,const char*v){ for(int i=0;i<g_nsub;i++) if(!strcmp(g_sub[i].name,n)){ strncpy(g_sub[i].val,v,159);g_sub[i].val[159]=0; return;}
    if(g_nsub<512){ strncpy(g_sub[g_nsub].name,n,31);g_sub[g_nsub].name[31]=0; strncpy(g_sub[g_nsub].val,v,159);g_sub[g_nsub].val[159]=0; g_nsub++; } }
static const char* sub_get(const char*n){ for(int i=0;i<g_nsub;i++) if(!strcmp(g_sub[i].name,n)) return g_sub[i].val; return NULL; }

/* apply whole-identifier substitution from the subst table to a line */
static void apply_sub(const char*in,char*out,size_t osz){
    size_t o=0; const char*p=in;
    while(*p && o+1<osz){
        if(isalpha((unsigned char)*p)||*p=='_'){ char id[64];int n=0;
            while((isalnum((unsigned char)*p)||*p=='_')&&n<63){id[n++]=*p++;} id[n]=0;
            const char*v=sub_get(id); const char*s=v?v:id;
            while(*s&&o+1<osz)out[o++]=*s++;
        } else out[o++]=*p++;
    }
    out[o]=0;
}
/* minimal integer expression eval for .if/.loop (idents via subst table) */
static const char*pe; static long pp_e(void);
static void pp_sw(void){ while(*pe==' '||*pe=='\t')pe++; }
static long pp_prim(void){ pp_sw();
    if(*pe=='('){pe++;long v=pp_e();pp_sw();if(*pe==')')pe++;return v;}
    if(*pe=='!'){pe++;return !pp_prim();}
    if(*pe=='-'){pe++;return -pp_prim();} if(*pe=='+'){pe++;return pp_prim();}
    if(isdigit((unsigned char)*pe)){ char b[64];int n=0; while((isalnum((unsigned char)*pe))&&n<63)b[n++]=*pe++; b[n]=0;
        int L=n>0?tolower((unsigned char)b[n-1]):0; if(L=='h'){b[n-1]=0;return strtol(b,NULL,16);} if(b[0]=='0'&&(b[1]=='x'||b[1]=='X'))return strtol(b+2,NULL,16); return strtol(b,NULL,10); }
    if(isalpha((unsigned char)*pe)||*pe=='_'){ char id[64];int n=0; while((isalnum((unsigned char)*pe)||*pe=='_')&&n<63)id[n++]=*pe++; id[n]=0;
        const char*v=sub_get(id); if(v){ const char*save=pe; pe=v; long r=pp_e(); pe=save; return r;} return 0; }
    return 0; }
static long pp_mul(void){ long v=pp_prim(); for(;;){ pp_sw(); if(*pe=='*'){pe++;v*=pp_prim();} else if(*pe=='/'){pe++;long d=pp_prim();v=d?v/d:0;} else if(*pe=='%'){pe++;long d=pp_prim();v=d?v%d:0;} else break;} return v; }
static long pp_add(void){ long v=pp_mul(); for(;;){ pp_sw(); if(*pe=='+'){pe++;v+=pp_mul();} else if(*pe=='-'){pe++;v-=pp_mul();} else break;} return v; }
static long pp_rel(void){ long v=pp_add(); for(;;){ pp_sw();
    if(pe[0]=='<'&&pe[1]=='='){pe+=2;v=(v<=pp_add());} else if(pe[0]=='>'&&pe[1]=='='){pe+=2;v=(v>=pp_add());}
    else if(pe[0]=='='&&pe[1]=='='){pe+=2;v=(v==pp_add());} else if(pe[0]=='!'&&pe[1]=='='){pe+=2;v=(v!=pp_add());}
    else if(pe[0]=='<'&&pe[1]!='<'){pe++;v=(v<pp_add());} else if(pe[0]=='>'&&pe[1]!='>'){pe++;v=(v>pp_add());}
    else if(*pe=='='){pe++;v=(v==pp_add());} else break;} return v; }
static long pp_e(void){ long v=pp_rel(); for(;;){ pp_sw();
    if(pe[0]=='&'&&pe[1]=='&'){pe+=2;long r=pp_rel();v=(v&&r);} else if(pe[0]=='|'&&pe[1]=='|'){pe+=2;long r=pp_rel();v=(v||r);}
    else if(*pe=='&'){pe++;v&=pp_rel();} else if(*pe=='|'){pe++;v|=pp_rel();} else break;} return v; }
static long pp_eval(const char*s){ pe=s; return pp_e(); }

/* strip a leading quote/space and a trailing quote from an .asg value */
static void unquote(char*s){ char*e; while(*s==' '||*s=='\t')memmove(s,s+1,strlen(s));
    if(*s=='"'){ memmove(s,s+1,strlen(s)); e=s+strlen(s); while(e>s&&(e[-1]==' '||e[-1]=='\t'))*--e=0; if(e>s&&e[-1]=='"')e[-1]=0; } }

typedef struct { char name[32]; char params[8][32]; int nparams; int start,end; } Macro;
static Macro g_mac[128]; static int g_nmac=0;
static Macro* mac_find(const char*n){ for(int i=0;i<g_nmac;i++) if(!strcasecmp(g_mac[i].name,n)) return &g_mac[i]; return NULL; }

static char (*PP_OUT)[256]; static int pp_nout=0, pp_cap=0;
static void pp_emit(const char*l){ if(pp_nout>=pp_cap){ pp_cap=pp_cap?pp_cap*2:4096; PP_OUT=realloc(PP_OUT,pp_cap*256);} strncpy(PP_OUT[pp_nout],l,255); PP_OUT[pp_nout][255]=0; pp_nout++; }

/* first token (directive/mnemonic) and whether a leading label is present */
static void first_tok(const char*line,char*tok,int*after){ const char*p=line;
    /* a column-0 token that is not a directive is a label: skip it */
    if(*p && !isspace((unsigned char)*p) && *p!='.'){ while(*p&&!isspace((unsigned char)*p))p++; }
    while(*p==' '||*p=='\t')p++;
    int n=0; const char*q=p; while(*q&&!isspace((unsigned char)*q)&&n<63)tok[n++]=*q++; tok[n]=0; *after=(int)(q-line); }

static void pp_expand(int lo,int hi,int depth);  /* expand g_lines[lo..hi) */

/* expand a macro invocation given its args string */
static void pp_call_macro(Macro*m,const char*argstr,int depth){
    char buf[512]; strncpy(buf,argstr,511); buf[511]=0;
    char*av[8]; int na=split(buf,av,8);
    /* save/override param substitutions */
    char saved[8][160]; int had[8];
    for(int i=0;i<m->nparams;i++){ const char*cur=sub_get(m->params[i]); had[i]=cur!=NULL; if(had[i]){strncpy(saved[i],cur,159);saved[i][159]=0;} sub_set(m->params[i], i<na?av[i]:""); }
    pp_expand(m->start,m->end,depth+1);
    for(int i=0;i<m->nparams;i++){ if(had[i]) sub_set(m->params[i],saved[i]); else sub_set(m->params[i],m->params[i]); }
}

static void pp_expand(int lo,int hi,int depth){
    if(depth>32){ fprintf(stderr,"preprocessor: nesting too deep\n"); return; }
    int i=lo;
    while(i<hi){
        const char*raw=g_lines[i];
        char tok[64]; int after; first_tok(raw,tok,&after);
        /* skip a macro definition (bodies are expanded only on invocation) */
        if(!strcasecmp(tok,".macro")){ int d=1,j=i+1; for(;j<hi;j++){ char t2[64];int a2; first_tok(g_lines[j],t2,&a2);
                if(!strcasecmp(t2,".macro"))d++; else if(!strcasecmp(t2,".endm")){d--; if(!d)break;} } i=j+1; continue; }
        if(!strcasecmp(tok,".endm")||!strcasecmp(tok,".endloop")||!strcasecmp(tok,".endif")){ i++; continue; }
        /* .asg "v", NAME  |  .eval expr, NAME */
        if(!strcasecmp(tok,".asg")||!strcasecmp(tok,".eval")){
            char args[256]; strncpy(args,raw+after,255);args[255]=0;
            char ex[256]; apply_sub(args,ex,sizeof ex);
            char*comma=strrchr(ex,','); if(comma){ *comma=0; char*nm=comma+1; while(*nm==' '||*nm=='\t')nm++;
                char val[160];
                if(!strcasecmp(tok,".eval")){ snprintf(val,sizeof val,"%ld",pp_eval(ex)); }
                else { strncpy(val,ex,159);val[159]=0; unquote(val); }
                sub_set(nm,val); }
            i++; continue;
        }
        /* .loop N ... .endloop */
        if(!strcasecmp(tok,".loop")){ char ex[256]; apply_sub(raw+after,ex,sizeof ex); long cnt=pp_eval(ex);
            int d=1,j=i+1,body=i+1; for(;j<hi;j++){ char t2[64];int a2; first_tok(g_lines[j],t2,&a2);
                if(!strcasecmp(t2,".loop"))d++; else if(!strcasecmp(t2,".endloop")){d--; if(!d)break;} }
            for(long k=0;k<cnt;k++) pp_expand(body,j,depth+1);
            i=j+1; continue; }
        /* .if / .elseif / .else / .endif */
        if(!strcasecmp(tok,".if")){ char ex[256]; apply_sub(raw+after,ex,sizeof ex); int cond=pp_eval(ex)!=0; int taken=cond;
            int d=1,j=i+1; int seg_start=i+1; int emit_seg=cond;
            for(;j<hi;j++){ char t2[64];int a2; first_tok(g_lines[j],t2,&a2);
                if(!strcasecmp(t2,".if"))d++;
                else if(!strcasecmp(t2,".endif")){ d--; if(!d){ if(emit_seg)pp_expand(seg_start,j,depth); break; } }
                else if(d==1&&!strcasecmp(t2,".elseif")){ if(emit_seg)pp_expand(seg_start,j,depth); char e2[256];apply_sub(g_lines[j]+a2,e2,sizeof e2); int c2=(!taken)&&(pp_eval(e2)!=0); emit_seg=c2; if(c2)taken=1; seg_start=j+1; }
                else if(d==1&&!strcasecmp(t2,".else")){ if(emit_seg)pp_expand(seg_start,j,depth); emit_seg=!taken; taken=1; seg_start=j+1; }
            }
            i=j+1; continue; }
        /* macro invocation */
        { char nm[64]; int lab=(!isspace((unsigned char)*raw)&&*raw!='.')?1:0; const char*p=raw; char label[64]={0};
          if(lab){ int n=0; while(*p&&!isspace((unsigned char)*p)&&n<63)label[n++]=*p++; label[n]=0; }
          while(*p==' '||*p=='\t')p++;
          int n=0; while(*p&&!isspace((unsigned char)*p)&&n<63)nm[n++]=*p++; nm[n]=0; while(*p==' '||*p=='\t')p++;
          Macro*m=mac_find(nm);
          if(m){ if(lab){ char lbl[260]; snprintf(lbl,sizeof lbl,"%s:",label); pp_emit(lbl);} char ap[256]; apply_sub(p,ap,sizeof ap); pp_call_macro(m,ap,depth); i++; continue; }
        }
        /* ordinary line: apply substitution and emit */
        { char ex[256]; apply_sub(raw,ex,sizeof ex); pp_emit(ex); }
        i++;
    }
}

static void preprocess(void){
    int has=0;
    /* pass 1: collect macro definitions (bodies stay in g_lines, referenced by range) */
    for(int i=0;i<g_nl;i++){ char tok[64];int after; first_tok(g_lines[i],tok,&after);
        if(!strcasecmp(tok,".macro")){ has=1;
            char nm[64]={0}; const char*p=g_lines[i]; if(!isspace((unsigned char)*p)){int n=0;while(*p&&!isspace((unsigned char)*p)&&n<63)nm[n++]=*p++;nm[n]=0;}
            if(*nm==0){ /* "name .macro" was parsed; fallback: token after label already consumed */ }
            if(g_nmac<128){ Macro*m=&g_mac[g_nmac++]; snprintf(m->name,sizeof m->name,"%s",nm); m->nparams=0;
                char args[256]; strncpy(args,g_lines[i]+after,255);args[255]=0; char*pp[8];int na=split(args,pp,8);
                for(int k=0;k<na&&k<8;k++){ strncpy(m->params[k],pp[k],31);m->params[k][31]=0;m->nparams++; }
                m->start=i+1; int d=1,j=i+1; for(;j<g_nl;j++){ char t2[64];int a2; first_tok(g_lines[j],t2,&a2); if(!strcasecmp(t2,".macro"))d++; else if(!strcasecmp(t2,".endm")){d--;if(!d)break;} } m->end=j;
            }
        }
        else if(!strcasecmp(tok,".asg")||!strcasecmp(tok,".eval")||!strcasecmp(tok,".loop")||!strcasecmp(tok,".if")) has=1;
    }
    if(!has) return;   /* no macro/preprocessor features: leave g_lines untouched (flat path) */
    PP_OUT=NULL; pp_nout=0; pp_cap=0;
    pp_expand(0,g_nl,0);
    g_nl=0; for(int i=0;i<pp_nout && g_nl<(int)(sizeof g_lines/sizeof g_lines[0]);i++) strcpy(g_lines[g_nl++],PP_OUT[i]);
    free(PP_OUT); PP_OUT=NULL;
}

/* ---------------- main ---------------- */
/* Flat ordering is by section CHARACTER ('D' before 'P', as before) then address,
 * so the flat stream is unchanged regardless of the new section
 * indices. Per-section COFF collection (write_c5xcoff) filters by index, which stays
 * address-ordered within each section because same-char sections keep their order. */
static int cmp_out(const void *a,const void *b){ const Out*x=a,*y=b;
    char cx=sec_flatchar(x->sec), cy=sec_flatchar(y->sec);
    if(cx!=cy) return cx<cy?-1:1;
    if(x->addr!=y->addr) return x->addr<y->addr?-1:1;
    return 0; }

#define C5XASM_ROLE  "TMS320 fixed-point assembler (C5x)"
#define C5XASM_SCOPE "C5x ISA 229 mnem. " C5X_DOT " COFF v1/v2 out"

static void usage(FILE*f){
    c5x_banner(f,"c5xasm",C5XASM_ROLE,C5XASM_SCOPE);
    fputs(
"usage: c5xasm <input.asm> [options]\n"
"  -o <file>     output file (default: stdout for flat, required for -coff)\n"
"  -coff2 | -coff   emit TI COFF v2 object (default COFF version)\n"
"  -coff1        emit classic TI COFF v1 object\n"
"  -v50          target TMS320C5x (accepted; C5x is the only target)\n"
"  -V, --verbose report passes, words emitted, relocations, output mode\n"
"  -q, --quiet   one-line identity instead of the full banner\n"
"  -h, --help    show this help and exit\n"
"Without a -coff flag the flat 'SEC AAAA WWWW' image is written (raw-image pipeline).\n", f);
}
int main(int argc,char**argv){
    const char*inpath=NULL,*outpath=NULL; int c5xcoff=0, verbose=0, quiet=0;  /* c5xcoff: 0=flat,1=v1,2=v2 */
    for(int i=1;i<argc;i++){ if(!strcmp(argv[i],"-o")&&i+1<argc)outpath=argv[++i];
        else if(!strcmp(argv[i],"-coff1"))c5xcoff=1;
        else if(!strcmp(argv[i],"-coff2")||!strcmp(argv[i],"-coff"))c5xcoff=2;
        else if(!strcmp(argv[i],"-h")||!strcmp(argv[i],"--help")){ usage(stdout); return 0; }
        else if(!strcmp(argv[i],"--version")){ c5x_banner(stdout,"c5xasm",C5XASM_ROLE,C5XASM_SCOPE); return 0; }
        else if(!strcmp(argv[i],"-V")||!strcmp(argv[i],"--verbose"))verbose=1;
        else if(!strcmp(argv[i],"-q")||!strcmp(argv[i],"--quiet"))quiet=1;
        else if(!strncmp(argv[i],"-v",2)) continue;        /* -v50 etc: accepted, C5x only */
        else if(argv[i][0]!='-')inpath=argv[i];
        else { fprintf(stderr,"c5xasm: unknown option '%s'\n",argv[i]); usage(stderr); return 2; } }
    if(!inpath){ usage(stderr); return 2; }
    /* Identity on startup: the full banner by default, one line under -q. */
    if(quiet) c5x_oneline(stderr,"c5xasm"); else c5x_banner(stderr,"c5xasm",C5XASM_ROLE,C5XASM_SCOPE);
    g_file=inpath; g_c5xcoff=c5xcoff;
    if(!load_src(inpath)){ perror(inpath); return 1; }
    preprocess();   /* macros, .loop, .if/.else, .asg/.eval (no-op if none present) */
    if(verbose&&!quiet) fprintf(stderr,"c5xasm: %s -> %s (%s), %d source lines\n",inpath,
        outpath?outpath:"<stdout>", c5xcoff==2?"COFFv2":c5xcoff==1?"COFFv1":"flat", g_nl);

    /* Relax to a fixed point: sizing passes (g_final=0) refine symbol addresses
     * until they stop changing (span-dependent short/long immediate selection and
     * forward references converge), then ONE final pass (g_final=1) emits. This
     * replaces a fixed 8-pass count: it adapts to the program's forward-reference
     * depth and errors loudly instead of silently truncating if it never settles.
     * The emitted output is identical to the old loop whenever that converged. */
    { unsigned long prevsig=0; int converged=0, done=0;
      for(g_pass=1; g_pass<=C5X_MAXPASS && !done; g_pass++){
          g_final=converged; g_loc=0; sect_reset(); g_nout=0; g_nrel=0;
          for(int i=0;i<g_nl;i++){ g_line=i+1; char t[256]; strcpy(t,g_lines[i]); asm_line(t); }
          if(g_final){ done=1; break; }
          { unsigned long sig=sym_sig(); if(g_pass>=2 && sig==prevsig) converged=1; prevsig=sig; }
      }
      if(!done){ fprintf(stderr,"c5xasm: assembly did not reach a fixed point in %d passes\n",C5X_MAXPASS); return 1; }
    }
    if(g_errors){ fprintf(stderr,"%d error(s)\n",g_errors); return 1; }
    if(verbose&&!quiet) fprintf(stderr,"c5xasm: ok - %d words emitted, %d relocation(s)\n",g_nout,g_nrel);

    qsort(g_out,g_nout,sizeof g_out[0],cmp_out);
    if(c5xcoff){
        if(!outpath){ fprintf(stderr,"c5xasm: -coff requires -o <file.obj>\n"); return 2; }
        write_c5xcoff(outpath,c5xcoff,inpath);
        return g_errors?1:0;
    }
    FILE*o=outpath?fopen(outpath,"wb"):stdout; if(!o){perror(outpath);return 1;}
    for(int i=0;i<g_nout;i++) fprintf(o,"%c %04X %04X\n",sec_flatchar(g_out[i].sec),g_out[i].addr,g_out[i].word);
    if(o!=stdout)fclose(o);
    return 0;
}
