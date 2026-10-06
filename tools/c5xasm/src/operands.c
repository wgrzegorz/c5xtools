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
 * operands.c - C5x operand parsing: field encoding, addressing modes,
 * condition field, control bits. Part of c5xtools. Version: see ../VERSION.
 *
 * Extracted verbatim from c5xasm.c; every field value here reproduces the reference assembler's
 * exact bit placement (see re/docs/). Depends only on eval()/err() (c5xasm.h).
 */

#include <string.h>
#include "portable.h"
#include <ctype.h>
#include <stdint.h>
#include "c5xasm.h"
#include "operands.h"

int split(char *s, char *parts[], int max){
    int n=0; char *p=s;
    while(*p && n<max){
        while(*p==' '||*p=='\t')p++;
        if(!*p) break;
        parts[n++]=p;
        while(*p && *p!=',')p++;
        if(*p==','){ *p=0; p++; }
        char *e=parts[n-1]+strlen(parts[n-1]); while(e>parts[n-1]&&(e[-1]==' '||e[-1]=='\t'))*--e=0;
    }
    return n;
}
int is_arreg(const char *t, int *n){
    /* C5x has eight auxiliary registers AR0-AR7 only; AR8/AR9 are not registers. */
    if((t[0]=='A'||t[0]=='a')&&(t[1]=='R'||t[1]=='r')&&t[2]>='0'&&t[2]<='7'&&t[3]==0){ *n=t[2]-'0'; return 1; } return 0;
}
/* indirect byte for parts[star_idx]; next-ARP scanned only AFTER the star token. */
uint16_t indirect_byte(char *parts[], int np, int star_idx){
    const char *t=parts[star_idx];
    uint16_t mode;
    if(!strcmp(t,"*")) mode=0x00;
    else if(!strcmp(t,"*-")) mode=0x10;
    else if(!strcmp(t,"*+")) mode=0x20;
    else if(!strcasecmp(t,"*0-")) mode=0x50;
    else if(!strcasecmp(t,"*0+")) mode=0x60;
    else if(!strcasecmp(t,"*br0-")) mode=0x40;
    else if(!strcasecmp(t,"*br0+")) mode=0x70;
    else { err("unsupported indirect mode"); mode=0x00; }
    uint16_t b=0x80|mode;
    for(int j=star_idx+1;j<np;j++){ int n; const char*t2=parts[j];
        if(is_arreg(t2,&n)){ b|=0x08|(n&7); break; }
        /* a token that looks like an auxiliary register but is not AR0-AR7 is an error,
         * not a silently-ignored operand (e.g. "*,AR9" is rejected). */
        if((t2[0]=='A'||t2[0]=='a')&&(t2[1]=='R'||t2[1]=='r')&&t2[2]&&t2[3]==0){ err("invalid auxiliary register (AR0-AR7)"); break; }
    }
    return b;
}
/* low byte for operand idx: indirect byte if it starts with '*', else direct dma7 */
uint16_t field_at(char *pa[], int np, int idx){
    if(pa[idx][0]=='*') return indirect_byte(pa,np,idx);
    long e=eval(pa[idx]);
    dma_operand(e);  /* arm a dma relocation if this direct operand named a label */
    return (uint16_t)(e&0x7F);
}
/* shift operand after idx: numeric, and - canonically - BEFORE any next-ARP.
 * the shift is taken from a token preceding the ARn; once the ARn appears the
 * shift slot is closed (shift 0) and a trailing number is ignored, so we stop at
 * the first ARn rather than scanning past it. Default 0. */
long shift_after(char *pa[], int np, int idx){
    for(int j=idx+1;j<np;j++){ int n; if(pa[j][0]=='*') continue; if(is_arreg(pa[j],&n)) break; return eval(pa[j]); }
    return 0;
}
/* like shift_after, but reports (via *found) whether a value operand was actually
 * present before the next-ARP. For instructions whose value is REQUIRED and must
 * precede the ARn (BIT bit code, IN/OUT port): it is rejected if missing or placed
 * after the ARP, e.g. "BIT *", "BIT *,AR5,4", "OUT *,AR6,063h". */
long value_before_arp(char *pa[], int np, int idx, int *found){
    for(int j=idx+1;j<np;j++){ int n; if(pa[j][0]=='*') continue; if(is_arreg(pa[j],&n)) break; *found=1; return eval(pa[j]); }
    *found=0; return 0;
}
int imm_tok(const char*t){ return t[0]=='#'; }
long imm_val(const char*t){ return eval(t+1); }

/* ---------------- condition field (chapter 3) ---------------- */
/* returns ZL/CV byte, sets *tp (3=none); validates one-per-group */
int parse_cond(char *pa[], int np, int start, int *tp){
    static const struct{const char*n;int byte;char grp;} C[]={
        {"EQ",0x88,'z'},{"NEQ",0x08,'z'},{"LT",0x44,'z'},{"LEQ",0xCC,'z'},
        {"GT",0x04,'z'},{"GEQ",0x8C,'z'},{"C",0x11,'c'},{"NC",0x01,'c'},
        {"OV",0x22,'v'},{"NOV",0x02,'v'},{"TC",-1,'t'},{"NTC",-2,'t'},{"BIO",-3,'t'},
    };
    *tp=3; int byte=0; int seen_z=0,seen_c=0,seen_v=0,seen_t=0;
    for(int i=start;i<np;i++){
        const char*t=pa[i];
        if(!strcasecmp(t,"UNC")) continue;
        int found=0;
        for(size_t k=0;k<sizeof C/sizeof C[0];k++) if(!strcasecmp(t,C[k].n)){
            found=1;
            if(C[k].grp=='t'){ if(seen_t){err("INVALID CONDITION VALUE");} seen_t=1; *tp=(C[k].byte==-1)?1:(C[k].byte==-2)?2:0; }
            else { int*s=(C[k].grp=='z')?&seen_z:(C[k].grp=='c')?&seen_c:&seen_v; if(*s){err("INVALID CONDITION VALUE");} *s=1; byte|=C[k].byte; }
            break;
        }
        if(!found) err("unknown condition");
    }
    return byte;
}
/* control bit index for SETC/CLRC */
int ctrl_field(const char *t){
    if(!strcasecmp(t,"intm")) return 0;
    if(!strcasecmp(t,"ovm"))  return 1;
    if(!strcasecmp(t,"cnf"))  return 2;
    if(!strcasecmp(t,"sxm"))  return 3;
    if(!strcasecmp(t,"hm"))   return 4;
    if(!strcasecmp(t,"tc"))   return 5;
    if(!strcasecmp(t,"xf"))   return 6;
    if(!strcasecmp(t,"c")||!strcasecmp(t,"carry")) return 7;
    err("unknown control bit"); return 0;
}
