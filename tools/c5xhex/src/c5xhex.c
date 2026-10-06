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
 * c5xhex - TMS320C5x COFF-to-hex converter (native, POSIX C99).
 *
 * A modern COFF->hex converter: reads an executable TI COFF .out
 * (as produced by c5xlnk) and writes a boot/ROM image in Intel-HEX,
 * Motorola S-record, TI-Tagged (SDSMAC), ASCII-Hex, Tektronix, or raw binary.
 *
 * Features:
 *   - default emits the FULL 16-bit image (both bytes), not an LSB-only split
 *     that forces two passes; -romwidth 8 + -order lsb|msb selects the classic 8-bit layout.
 *   - one tool, several formats selectable per run; modern -h/-V CLI.
 *
 * Build: cc -std=c99 -Wall -Wextra -O2 -o c5xhex c5xhex.c
 * Usage: c5xhex <in.out> -f <fmt> [-o out] [-romwidth 8|16] [-order lsb|msb] [-e n]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "c5xcoff.h"
#include "c5xbanner.h"
/* COFF reads come from the shared module: these names alias its primitives. */
#define u32 c5xcoff_rd32
#define u16 c5xcoff_rd16


/* word image: (addr, 16-bit word, section id), gathered from .text/.data */
typedef struct { uint32_t addr; uint16_t word; int sec; } WD;
static WD g_wd[1<<17]; static int g_nwd=0;
static int cmpwd(const void*a,const void*b){ const WD*x=a,*y=b; if(x->addr!=y->addr)return x->addr<y->addr?-1:1; return 0; }

static int load_out(const char*path, uint32_t*entry){
    FILE*f=fopen(path,"rb"); if(!f){perror(path);return 0;}
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if(n<2){ fprintf(stderr,"%s: too short for a COFF header\n",path); fclose(f); return 0; }
    uint8_t*d=malloc(n); if(!d){fclose(f);return 0;} if(fread(d,1,n,f)!=(size_t)n){free(d);fclose(f);return 0;} fclose(f);
    uint16_t magic=u16(d); if(magic!=COFF_MAGIC_V1 && magic!=COFF_MAGIC_V2){ fprintf(stderr,"%s: not TI COFF (%04x)\n",path,magic); free(d); return 0; }
    int v2=(magic==COFF_MAGIC_V2); int fhsz=22, shsz=v2?48:40;  /* TI C5x COFF: 22-byte file header (target id) for both v1/v2 */
    if(n<fhsz){ fprintf(stderr,"%s: truncated file header\n",path); free(d); return 0; }
    int nscns=u16(d+2); uint16_t opthdr=u16(d+16);
    long base=(long)fhsz+opthdr;
    if(entry){ *entry = (opthdr>=28 && (long)fhsz+24<=n)? u32(d+fhsz+20) : 0; }   /* optional header entry field */
    if(nscns<0 || base<0 || base+(long)shsz*nscns>n){ fprintf(stderr,"%s: truncated section headers\n",path); free(d); return 0; }
    for(int i=0;i<nscns;i++){ const uint8_t*s=d+base+(long)shsz*i;
        char nm[9]; memcpy(nm,s,8); nm[8]=0;
        uint32_t vaddr=u32(s+12), size=u32(s+16), scnptr=u32(s+20);
        unsigned flags = v2? u32(s+40) : u32(s+36);
        if(!scnptr || !size) continue;                 /* skip .bss / empty */
        if(flags & 0x80) continue;                      /* STYP_BSS: no data */
        if((long)scnptr + 2*(long)size > n){ fprintf(stderr,"%s: section %d data out of range\n",path,i); free(d); return 0; }
        for(uint32_t w=0; w<size && g_nwd<(1<<17); w++) { g_wd[g_nwd].addr=vaddr+w; g_wd[g_nwd].word=u16(d+scnptr+2*w); g_wd[g_nwd].sec=i; g_nwd++; }
    }
    free(d); qsort(g_wd,g_nwd,sizeof g_wd[0],cmpwd); return 1;
}

/* produce the output byte stream per romwidth/order; returns a (addr,byte) list.
 * romwidth 8: one byte per word address (LSB or MSB). romwidth 16: two bytes
 * (lo,hi or hi,lo) at byte address 2*word_addr. */
typedef struct { uint32_t addr; uint8_t b; int sec; } AB;
static AB g_ab[1<<18]; static int g_nab=0;
static void build_bytes(int romwidth,int msbfirst){
    g_nab=0;
    for(int i=0;i<g_nwd;i++){ uint16_t w=g_wd[i].word; uint32_t a=g_wd[i].addr; int sc=g_wd[i].sec;
        if(romwidth==8){ g_ab[g_nab].addr=a; g_ab[g_nab].b=(uint8_t)(msbfirst?(w>>8):(w&0xff)); g_ab[g_nab].sec=sc; g_nab++; }
        else { uint8_t lo=w&0xff, hi=w>>8;
            g_ab[g_nab].addr=2*a;   g_ab[g_nab].b=msbfirst?hi:lo; g_ab[g_nab].sec=sc; g_nab++;
            g_ab[g_nab].addr=2*a+1; g_ab[g_nab].b=msbfirst?lo:hi; g_ab[g_nab].sec=sc; g_nab++; } }
}
/* contiguous ROM image over [org, org+len) words (ROM image + fill): every word
 * in range is emitted, taken from a section when present else the fill value. romwidth 16
 * packs 2 bytes/word with the byte stream based at the word origin, so
 * the record address is org and bytes run linearly from there. All one "section" (0) so
 * records join across former section boundaries and fill gaps. */
static void build_image(uint32_t org,uint32_t len,uint16_t fill,int romwidth,int msbfirst){
    g_nab=0; uint32_t byte=org;
    for(uint32_t a=org;a<org+len;a++){
        uint16_t w=fill; for(int i=0;i<g_nwd;i++) if(g_wd[i].addr==a){ w=g_wd[i].word; break; }
        if(romwidth==8){ g_ab[g_nab].addr=byte++; g_ab[g_nab].b=(uint8_t)(msbfirst?(w>>8):(w&0xff)); g_ab[g_nab].sec=0; g_nab++; }
        else { uint8_t lo=w&0xff, hi=w>>8;
            g_ab[g_nab].addr=byte++; g_ab[g_nab].b=msbfirst?hi:lo; g_ab[g_nab].sec=0; g_nab++;
            g_ab[g_nab].addr=byte++; g_ab[g_nab].b=msbfirst?lo:hi; g_ab[g_nab].sec=0; g_nab++; } }
}

/* a record continues only within one section and contiguous addresses */
#define SAME_RUN(i,start,n) ((i)<g_nab && (n)<16 && g_ab[i].addr==(start)+(n) && g_ab[i].sec==g_ab[(i)-(n)].sec)

static void emit_intel(FILE*f){
    int i=0; while(i<g_nab){ uint32_t start=g_ab[i].addr; int sc=g_ab[i].sec; int n=0; uint8_t buf[16];
        while(i<g_nab && n<16 && g_ab[i].addr==start+n && g_ab[i].sec==sc){ buf[n++]=g_ab[i].b; i++; }
        unsigned sum=n+((start>>8)&0xff)+(start&0xff)+0;
        fprintf(f,":%02X%04X00",n,start&0xffff);
        for(int k=0;k<n;k++){ fprintf(f,"%02X",buf[k]); sum+=buf[k]; }
        fprintf(f,"%02X\n",(unsigned)((-sum)&0xff)); }
    fprintf(f,":00000001FF\n");
}
static void emit_srec(FILE*f){
    fprintf(f,"S00600004844521B\n");   /* header record, data "HDR" */
    int i=0; while(i<g_nab){ uint32_t start=g_ab[i].addr; int sc=g_ab[i].sec; int n=0; uint8_t buf[16];
        while(i<g_nab && n<16 && g_ab[i].addr==start+n && g_ab[i].sec==sc){ buf[n++]=g_ab[i].b; i++; }
        unsigned len=n+4; unsigned sum=len+((start>>16)&0xff)+((start>>8)&0xff)+(start&0xff); /* count = 3 addr + n data + 1 cksum */
        fprintf(f,"S2%02X%06X",len,start&0xffffff);     /* 3-byte address */
        for(int k=0;k<n;k++){ fprintf(f,"%02X",buf[k]); sum+=buf[k]; }
        fprintf(f,"%02X\n",(unsigned)((~sum)&0xff)); }
    fprintf(f,"S804000000FB\n");
}
static void emit_ascii(FILE*f){
    int i=0; while(i<g_nab){ uint32_t start=g_ab[i].addr; int sc=g_ab[i].sec; fprintf(f,"$A%04X,\n",start&0xffff);
        int col=0; while(i<g_nab && g_ab[i].addr==start+col && g_ab[i].sec==sc){ fprintf(f,"%02X ",g_ab[i].b); i++; col++; if(col%16==0)fprintf(f,"\n"); }
        if(col%16)fprintf(f,"\n"); }
}
/* TI-Tagged (SDSMAC), 16-bit words, word addresses. Tags: K<id><name> (module start),
 * 9<addr> (load addr), B<word> (data), 7<cksum> (running sum of tag+payload bytes),
 * F (end of line), : (end of file). Breaks per section and per 8 data words. */
static void emit_tagged(FILE*f){
    fprintf(f,"K0000c5xhex  ");                 /* module start (8-char id) */
    int i=0; while(i<g_nwd){ uint32_t start=g_wd[i].addr; int sc=g_wd[i].sec; unsigned sum=0; int col=0;
        fprintf(f,"9%04X",start&0xffff); sum+=0x39+((start>>8)&0xff)+(start&0xff);
        while(i<g_nwd && g_wd[i].addr==start+col && g_wd[i].sec==sc && col<8){ uint16_t w=g_wd[i].word;
            fprintf(f,"B%04X",w); sum+=0x42+(w>>8)+(w&0xff); i++; col++; }
        fprintf(f,"7%04XF\n",(unsigned)((-(int)sum)&0xffff)); }
    fprintf(f,":\n");
}
static void emit_bin(FILE*f){ for(int i=0;i<g_nab;i++) fputc(g_ab[i].b,f); }

#define C5XHEX_ROLE  "TMS320 fixed-point COFF->hex"
#define C5XHEX_SCOPE "TI COFF 0x0092 " C5X_DOT " Intel/Motorola/SREC"

static void usage(FILE*f){
    c5x_banner(f,"c5xhex",C5XHEX_ROLE,C5XHEX_SCOPE);
    fputs(
"usage: c5xhex <in.out> -f <fmt> [options]\n"
"  -f intel|srec|tagged|ascii|bin   output format (required)\n"
"  -o <file>       output file (default: stdout)\n"
"  -romwidth 8|16  8 = one byte per word (classic 8-bit), 16 = full word as 2 bytes (default)\n"
"  -order lsb|msb  byte order for the ROM stream (default lsb)\n"
"  -e <addr>       record an entry address where the format supports it\n"
"  -image          emit a contiguous ROM image over -org/-len (fill gaps)\n"
"  -org <addr>      image start word address (with -image)\n"
"  -len <n>         image length in words (with -image)\n"
"  -fill <val>      fill value for words not covered by a section (default 0)\n"
"  -V, --verbose   report sections and byte counts\n"
"  -q, --quiet     one-line identity instead of the full banner\n"
"  -h, --help      show this help\n"
"Default (romwidth 16) emits the complete image; use -romwidth 8 -order lsb for the classic 8-bit layout.\n", f);
}

int main(int argc,char**argv){
    const char*in=NULL,*outp=NULL,*fmt=NULL; int romwidth=16, msbfirst=0, verbose=0, quiet=0; long eopt=-1;
    int image=0, order_set=0; long org=-1, len=-1; long fill=0;   /* -image + ROMS fill */
    for(int i=1;i<argc;i++){ const char*a=argv[i];
        if(!strcmp(a,"-f")&&i+1<argc)fmt=argv[++i];
        else if(!strcmp(a,"-o")&&i+1<argc)outp=argv[++i];
        else if(!strcmp(a,"-romwidth")&&i+1<argc)romwidth=(int)strtol(argv[++i],NULL,10);
        else if(!strcmp(a,"-order")&&i+1<argc){msbfirst=!strcmp(argv[++i],"msb");order_set=1;}
        else if(!strcmp(a,"-e")&&i+1<argc)eopt=strtol(argv[++i],NULL,0);
        else if(!strcmp(a,"-image"))image=1;
        else if(!strcmp(a,"-org")&&i+1<argc)org=strtol(argv[++i],NULL,0);
        else if(!strcmp(a,"-len")&&i+1<argc)len=strtol(argv[++i],NULL,0);
        else if(!strcmp(a,"-fill")&&i+1<argc)fill=strtol(argv[++i],NULL,0);
        else if(!strcmp(a,"-V")||!strcmp(a,"--verbose"))verbose=1;
        else if(!strcmp(a,"-q")||!strcmp(a,"--quiet"))quiet=1;
        else if(!strcmp(a,"-h")||!strcmp(a,"--help")){ usage(stdout); return 0; }
        else if(!strcmp(a,"--version")){ c5x_banner(stdout,"c5xhex",C5XHEX_ROLE,C5XHEX_SCOPE); return 0; }
        else if(a[0]!='-'&&!in)in=a;
        else { fprintf(stderr,"c5xhex: unknown option '%s'\n",a); usage(stderr); return 2; } }
    if(!in||!fmt){ usage(stderr); return 2; }
    /* Identity on startup: the full banner by default, one line under -q. */
    if(quiet) c5x_oneline(stderr,"c5xhex"); else c5x_banner(stderr,"c5xhex",C5XHEX_ROLE,C5XHEX_SCOPE);
    if(romwidth!=8&&romwidth!=16){ fprintf(stderr,"c5xhex: -romwidth must be 8 or 16\n"); return 2; }
    uint32_t entry=0; if(!load_out(in,&entry)) return 1; if(eopt>=0)entry=(uint32_t)eopt;
    if(image){
        if(org<0||len<0){ fprintf(stderr,"c5xhex: -image requires -org and -len\n"); return 2; }
        if(!order_set) msbfirst=1;   /* image mode emits MSB-first words */
        build_image((uint32_t)org,(uint32_t)len,(uint16_t)fill,romwidth,msbfirst);
    } else
    build_bytes(romwidth,msbfirst);
    if(verbose) fprintf(stderr,"c5xhex: %s -> %s (%s, romwidth=%d %s): %d words, %d bytes, entry=0x%x\n",
        in,outp?outp:"<stdout>",fmt,romwidth,msbfirst?"msb":"lsb",g_nwd,g_nab,entry);
    FILE*f=outp?fopen(outp,"wb"):stdout; if(!f){perror(outp);return 1;}
    if(!strcmp(fmt,"intel"))      emit_intel(f);
    else if(!strcmp(fmt,"srec"))  emit_srec(f);
    else if(!strcmp(fmt,"tagged"))emit_tagged(f);
    else if(!strcmp(fmt,"ascii")) emit_ascii(f);
    else if(!strcmp(fmt,"bin"))   emit_bin(f);
    else { fprintf(stderr,"c5xhex: unknown format '%s'\n",fmt); if(f!=stdout)fclose(f); return 2; }
    if(f!=stdout)fclose(f);
    return 0;
}
