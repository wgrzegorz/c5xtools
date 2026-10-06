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
 * c5xdis - TMS320C5x disassembler (native, POSIX C99).
 *
 * Decodes a raw little-endian 16-bit DSP image into c5xasm-compatible source.
 * The decoder is the exact inverse of c5xasm's encoder, driven by the SAME
 * instruction table (include/c5x_isa.h), so the
 * emitted source re-assembles to the original bytes. That round-trip
 * (c5xdis -> c5xasm -> cmp) is the correctness oracle; see test/roundtrip.sh.
 *
 * This is the processor/instruction-model layer (the equivalent of a Ghidra
 * SLEIGH module / IDA processor module). On top of it the analysis passes that
 * make a mature disassembler "smart" are implemented in --trace mode:
 * recursive-descent traversal from entry vectors with a fixpoint, a call graph
 * and per-function xref/calls/refs headers, code/data separation, DP tracking
 * with direct-access resolution, computed-dispatch resolution (both the
 * SPLK-constant and the range-checked TBLR jump-table idioms), and
 * symbolization seeded from an external symbol file. None of them can weaken the
 * byte-exact guarantee: a mis-decode simply fails the round-trip and falls back
 * to a .word, and code/data classification only changes labels and .word vs
 * instruction, never the bytes. The default (no --trace) is a linear sweep.
 *
 * Build:  make   (see ../build.sh)
 * Usage:  c5xdis image.bin [start_word [count_words]] [--base ADDR] [-o out.asm]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "c5xbanner.h"
#include "c5x_isa.h"

#define C5XDIS_ROLE  "TMS320 fixed-point disassembler (C5x)"
#define C5XDIS_SCOPE "raw DSP image " C5X_DOT " round-trip via c5xasm"

/* C5x memory-mapped registers (page 0), for the symbolization pass; unused in
 * the round-trip-safe numeric rendering below but kept for the next phase. */
/* (intentionally omitted here until the symbolization pass uses it) */

/* ---- operand rendering: inverse of c5xasm field_at()/indirect_byte() ---- */
/* Render the low-byte addressing field. Returns 0 on an unknown indirect mode
 * (caller then falls back to .word to preserve the round-trip guarantee). */
/* Split the low-byte addressing field into its mode token and an optional
 * next-ARP index (*arp = -1 if none). Returns 0 on a byte the encoder cannot
 * reproduce (unknown mode, or a non-zero AR index with no update bit). */
static int field_parts(unsigned low, char *mode, size_t msz, int *arp){
    *arp = -1;
    if(!(low & 0x80)){ snprintf(mode, msz, "0x%02X", low & 0x7F); return 1; }  /* direct dma */
    const char *m;
    switch(low & 0x70){
        case 0x00: m="*";      break;
        case 0x10: m="*-";     break;
        case 0x20: m="*+";     break;
        case 0x40: m="*BR0-";  break;
        case 0x50: m="*0-";    break;
        case 0x60: m="*0+";    break;
        case 0x70: m="*BR0+";  break;
        default: return 0;                     /* 0x30: not a valid C5x mode */
    }
    snprintf(mode, msz, "%s", m);
    if(low & 0x08) *arp = (int)(low & 7);      /* next-ARP update */
    else if(low & 0x07) return 0;              /* non-canonical (likely data) */
    return 1;
}
/* Field rendered with the next-ARP inline (",ARn"), as most encoders expect. */
static int render_field(unsigned low, char *out, size_t osz){
    char mode[16]; int arp;
    if(!field_parts(low, mode, sizeof mode, &arp)) return 0;
    if(arp>=0) snprintf(out, osz, "%s,AR%d", mode, arp);
    else       snprintf(out, osz, "%s", mode);
    return 1;
}
/* Emit "NAME mode[,shift][,ARn]". The shift is a value operand that must PRECEDE
 * the next-ARP: c5xasm encodes a shift written after the ARP as 0,
 * so "LACC *+,AR2,4" would lose the shift - the canonical form is "LACC *+,4,AR2".
 * Returns 0 on an un-reproducible addressing byte (caller falls back to .word). */
static int emit_mem_shift(char *out, size_t osz, const char *name, unsigned low, unsigned s){
    char md[16]; int arp;
    if(!field_parts(low, md, sizeof md, &arp)) return 0;
    if(arp>=0){ if(s) snprintf(out,osz,"%s %s,%u,AR%d",name,md,s,arp); else snprintf(out,osz,"%s %s,AR%d",name,md,arp); }
    else      { if(s) snprintf(out,osz,"%s %s,%u",name,md,s);          else snprintf(out,osz,"%s %s",name,md); }
    return 1;
}

/* ---- instruction-table lookups (inverse of the encoder's forward map) ---- */
/* Name of the (fmt, base-opcode) whose fixed bits under `mask` equal word's,
 * requiring the table opcode to be a clean base for that mask (operand bits 0). */
static const char *find_fmt(uint16_t w, unsigned char fmt, uint16_t mask){
    for(int i=0;i<C5X_ISA_N;i++){
        if(c5x_isa[i].fmt==fmt && c5x_isa[i].opcode && (c5x_isa[i].opcode & ~mask)==0
           && (w & mask)==c5x_isa[i].opcode)
            return c5x_isa[i].name;
    }
    return NULL;
}
static const char *find_exact1(uint16_t w){             /* no-operand single word */
    for(int i=0;i<C5X_ISA_N;i++){
        unsigned char f=c5x_isa[i].fmt;
        if((f==0x00||f==0x1c) && c5x_isa[i].opcode && c5x_isa[i].opcode==w)
            return c5x_isa[i].name;
    }
    return NULL;
}
static const char *find_memop(uint16_t w){              /* high-byte + low-8 field */
    for(int i=0;i<C5X_ISA_N;i++){
        if(c5x_isa[i].fmt==0x01 && (c5x_isa[i].opcode & 0x00FF)==0 && c5x_isa[i].opcode==(w & 0xFF00))
            return c5x_isa[i].name;
    }
    return NULL;
}

static const char *CTRL[8]={"INTM","OVM","CNF","SXM","HM","TC","XF","C"};

/* Render the condition tokens of a BCND/CC/RETC word (inverse of parse_cond).
 * The Z/C/V groups occupy disjoint bit positions, so the byte decomposes
 * uniquely. Returns 0 if any bits are not a valid condition (caller -> .word). */
static int render_cond(uint16_t w, char *out, size_t osz){
    const char *z=NULL,*c=NULL,*v=NULL,*tp=NULL;
    switch(w & 0xCC){ case 0x00:break; case 0x88:z="EQ";break; case 0x08:z="NEQ";break;
        case 0x44:z="LT";break; case 0xCC:z="LEQ";break; case 0x04:z="GT";break;
        case 0x8C:z="GEQ";break; default:return 0; }
    switch(w & 0x11){ case 0x00:break; case 0x11:c="C";break; case 0x01:c="NC";break; default:return 0; }
    switch(w & 0x22){ case 0x00:break; case 0x22:v="OV";break; case 0x02:v="NOV";break; default:return 0; }
    switch((w>>8)&3){ case 3:break; case 1:tp="TC";break; case 2:tp="NTC";break; case 0:tp="BIO";break; }
    out[0]=0; size_t L=0;
    const char *parts[4]={z,c,v,tp};
    for(int i=0;i<4;i++) if(parts[i]){ int k=snprintf(out+L,osz-L,"%s%s",L?",":"",parts[i]); if(k<0)return 0; L+=(size_t)k; }
    return L>0;                                  /* empty => unconditional (handled elsewhere) */
}

/* Decode one instruction at words[i]; write the source line into out.
 * Returns the number of words consumed (1 or 2). Ordered specific -> general;
 * every rendered form is the inverse of a c5xasm encoder path, so it
 * re-assembles byte-exact (the round-trip gate rejects anything that does not). */
static int decode_one(const uint16_t *words, int i, int n, char *out, size_t osz){
    uint16_t w = words[i];
    uint16_t w2 = (i+1<n) ? words[i+1] : 0;
    int have2 = (i+1<n);
    const char *name;
    char fld[24], cnd[32];

    /* 1. no-operand single word (fmt 0x00 / 0x1c) */
    if((name=find_exact1(w))){ snprintf(out,osz,"%s",name); return 1; }

    /* 2. composite immediate/control forms not matchable by a table opcode */
    if((w & 0xFFF0)==0xBE40){ snprintf(out,osz,"%s %s",(w&1)?"SETC":"CLRC",CTRL[(w>>1)&7]); return 1; }
    if((w & 0xFE00)==0xBC00){ snprintf(out,osz,"LDP #0x%03X",w&0x1FF); return 1; }
    if((w & 0xF000)==0x2000){ if(emit_mem_shift(out,osz,"ADD",w&0xFF,(w>>8)&0xF)) return 1; }
    if((w & 0xF000)==0x3000){ if(emit_mem_shift(out,osz,"SUB",w&0xFF,(w>>8)&0xF)) return 1; }
    if((w & 0xF000)==0x1000){ if(emit_mem_shift(out,osz,"LACC",w&0xFF,(w>>8)&0xF)) return 1; }

    /* 3. two-word branch/call and repeat-block (exact opcode + 16-bit word) */
    if(have2 && ((name=find_fmt(w,0x02,0xFFFF)) || (name=find_fmt(w,0x16,0xFFFF)))){
        snprintf(out,osz,"%s 0x%04X",name,w2); return 2;
    }
    /* 3b. 7-family branch/call with an indirect next-ARP modifier (low byte has
     * bit7); target is still the 16-bit second word. */
    if(have2 && (w & 0x80)){
        const char *bn=NULL; unsigned hi=w>>8;
        if(hi==0x79)bn="B"; else if(hi==0x7A)bn="CALL"; else if(hi==0x7B)bn="BANZ";
        else if(hi==0x7D)bn="BD"; else if(hi==0x7E)bn="CALLD"; else if(hi==0x7F)bn="BANZD";
        if(bn){ char f2[24]; if(render_field(w & 0xFF,f2,sizeof f2)){ snprintf(out,osz,"%s 0x%04X,%s",bn,w2,f2); return 2; } }
    }

    /* 4. conditional branch / call / return (fmt 0x18 / 0x1a) */
    switch(w & 0xFC00){
        case 0xE000: case 0xE800: case 0xF000: case 0xF800:
            if(have2){ const char*m=(w&0xFC00)==0xE000?"BCND":(w&0xFC00)==0xE800?"CC":(w&0xFC00)==0xF000?"BCNDD":"CCD";
                if(render_cond(w,cnd,sizeof cnd)){ snprintf(out,osz,"%s 0x%04X,%s",m,w2,cnd); return 2; } }
            break;
        case 0xEC00: case 0xFC00: {
            const char*m=(w&0xFC00)==0xEC00?"RETC":"RETCD";
            if(((w>>8)&3)==3 && (w&0xFF)==0){ snprintf(out,osz,"%s",m); return 1; }
            if(render_cond(w,cnd,sizeof cnd)){ snprintf(out,osz,"%s %s",m,cnd); return 1; }
            break; }
    }

    /* 4b. XC n,cond (fmt 0x19): conditionally execute the next 1 (bit12=0) or
     * 2 (bit12=1) words. Identifier bits 15:13=111, 11=0, 10=1; the condition
     * (TP at 9:8, ZLVC at 7:4, mask at 3:0) is the same encoding as BCND. */
    if((w & 0xEC00)==0xE400){ if(render_cond(w,cnd,sizeof cnd)){ snprintf(out,osz,"XC %u,%s",((w>>12)&1)+1,cnd); return 1; } }

    /* 5. class ladder (ordered most-specific mask first) */
    if((name=find_fmt(w,0x0A,0xFFF8))){ snprintf(out,osz,"%s %u",name,w&7); return 1; }         /* LARP */
    if((name=find_fmt(w,0x0F,0xFFF8)) && have2){ snprintf(out,osz,"%s AR%u,#0x%04X",name,w&7,w2); return 2; } /* LRLK */
    if((name=find_fmt(w,0x1B,0xFFE0))){ snprintf(out,osz,"%s %u",name,w&0x1F); return 1; }        /* INTR */
    if((name=find_fmt(w,0x15,0xFFF0))){ snprintf(out,osz,"%s %u",name,(w&0xF)+1); return 1; }      /* BSAR */
    if((name=find_fmt(w,0x12,0xFFF0)) && have2){ unsigned s=w&0xF; if(s) snprintf(out,osz,"%s #0x%04X,%u",name,w2,s); else snprintf(out,osz,"%s #0x%04X",name,w2); return 2; } /* ADLK/ANDK/ORK/SBLK/XORK/LALK */
    if((name=find_fmt(w,0x03,0xF800))){ if(emit_mem_shift(out,osz,name,w&0xFF,(w>>8)&7)) return 1; } /* SACL/SACH (shift before ARP) */
    if((name=find_fmt(w,0x05,0xF800))){ if(render_field(w&0xFF,fld,sizeof fld)){ snprintf(out,osz,"%s AR%u,%s",name,(w>>8)&7,fld); return 1; } } /* SAR */
    if((name=find_fmt(w,0x07,0xF800))){ snprintf(out,osz,"%s AR%u,#0x%02X",name,(w>>8)&7,w&0xFF); return 1; } /* LARK */
    if((name=find_fmt(w,0x0E,0xF000))){ unsigned b=(w>>8)&0xF; char md[16]; int arp;  /* BIT: the bit code is a required value that must precede the next-ARP */
        if(field_parts(w&0xFF,md,sizeof md,&arp)){ if(arp>=0) snprintf(out,osz,"%s %s,%u,AR%d",name,md,b,arp); else snprintf(out,osz,"%s %s,%u",name,md,b); return 1; } }
    if((name=find_fmt(w,0x08,0xE000))){ snprintf(out,osz,"%s #0x%04X",name,w&0x1FFF); return 1; } /* MPYK */
    if((name=find_fmt(w,0x09,0xFF00))){ snprintf(out,osz,"%s #0x%02X",name,w&0xFF); return 1; }   /* ADRK/SBRK/RPTK */
    if((name=find_fmt(w,0x10,0xFF00))){ snprintf(out,osz,"%s #0x%02X",name,w&0xFF); return 1; }   /* ADDK/LACK/SUBK */
    if((name=find_fmt(w,0x04,0xFF00)) && have2){ char md[16]; int arp;  /* IN/OUT: the port is a required value that must precede the next-ARP */
        if(field_parts(w&0xFF,md,sizeof md,&arp)){ if(arp>=0) snprintf(out,osz,"%s %s,0x%04X,AR%d",name,md,w2,arp); else snprintf(out,osz,"%s %s,0x%04X",name,md,w2); return 2; } } /* IN/OUT */
    if((name=find_fmt(w,0x11,0xFF00)) && have2){ if(render_field(w&0xFF,fld,sizeof fld)){ snprintf(out,osz,"%s 0x%04X,%s",name,w2,fld); return 2; } } /* MAC/MACD/BLKD/BLKP */
    if((name=find_fmt(w,0x14,0xFF00)) && have2){ if(render_field(w&0xFF,fld,sizeof fld)){ snprintf(out,osz,"%s #0x%04X,%s",name,w2,fld); return 2; } } /* SPLK/APLK/... */
    if((name=find_fmt(w,0x17,0xFF00)) && have2){ char md[16]; int arp;   /* LMMR/SMMR: imm is A(1), so next-ARP must follow the immediate */
        if(field_parts(w&0xFF,md,sizeof md,&arp)){ if(arp>=0) snprintf(out,osz,"%s %s,#0x%04X,AR%d",name,md,w2,arp); else snprintf(out,osz,"%s %s,#0x%04X",name,md,w2); return 2; } }
    if((w & 0xFF00)==0xA000){ if(render_field(w&0xFF,fld,sizeof fld) && (w&0x80)){ snprintf(out,osz,"NORM %s",fld); return 1; } } /* NORM (indirect only) */
    if((name=find_fmt(w,0x0C,0xFFFC))){ snprintf(out,osz,"%s %u",name,w&3); return 1; }          /* CMPR/SPM */
    if((w & 0x7E00)==0x0E00){ if(render_field(w&0xFF,fld,sizeof fld)){ snprintf(out,osz,"%s #%u,%s",(w&0x8000)?"SST":"LST",(w>>8)&1,fld); return 1; } } /* LST/SST */

    /* 5b. LAR ARn,dma - the table opcode is 0x0000 (so find_fmt skips it), and
     * nothing else is encoded in 0x0000-0x07FF; the AR is bits 8-10. */
    if((w & 0xF800)==0x0000){ if(render_field(w&0xFF,fld,sizeof fld)){ snprintf(out,osz,"LAR AR%u,%s",(w>>8)&7,fld); return 1; } }

    /* 6. single-word memory op (fmt 0x01) */
    if((name=find_memop(w))){ if(render_field(w&0xFF,fld,sizeof fld)){ snprintf(out,osz,"%s %s",name,fld); return 1; } }

    /* 7. not provably code */
    snprintf(out,osz,".word 0x%04X",w);
    return 1;
}

/* ================= Phase 2: recursive-descent analysis ================= *
 * Follow control flow from entry points to separate code from data (a linear
 * sweep decodes data as plausible instructions). Reached instructions are code;
 * unreached words are data (.word). Branch/call targets become labels. Anything
 * the analysis cannot resolve (computed targets, out-of-image branches, reached
 * yet undecodable words) is reported in detail and marked inline in the source,
 * following the two-level uncertainty discipline. Round-trip byte-exact holds:
 * code re-encodes via labels (.ps sets the origin), data re-emits as .word.
 * ----------------------------------------------------------------------- */

static unsigned char fmt_of(const char *name){
    for(int i=0;i<C5X_ISA_N;i++) if(!strcmp(c5x_isa[i].name,name)) return c5x_isa[i].fmt;
    return 0xFF;
}

typedef struct { int len, fall, has_t, computed, is_call; uint16_t target; char mn[12]; } Flow;

/* Classify the control flow of the instruction at words[i]. */
static Flow classify(const uint16_t *words, int i, int n){
    char line[80]; int len=decode_one(words,i,n,line,sizeof line);
    Flow f; memset(&f,0,sizeof f); f.len=len; f.fall=1;
    const char *p=line; while(*p==' ')p++;
    int k=0; while(*p && *p!=' ' && k<11) f.mn[k++]=*p++; f.mn[k]=0;
    while(*p==' ')p++; const char *ops=p;
    unsigned t; int gott=(sscanf(ops,"0x%x",&t)==1); int comma=(strchr(ops,',')!=NULL);
    const char *m=f.mn;
    if(!strcmp(m,"B")||!strcmp(m,"BD")){ f.fall=0; if(gott){f.has_t=1;f.target=(uint16_t)t;} }
    else if(!strcmp(m,"BACC")||!strcmp(m,"BACCD")){ f.fall=0; f.computed=1; }        /* branch to ACC */
    else if(!strcmp(m,"CALA")||!strcmp(m,"CALAD")){ f.computed=1; f.is_call=1; }     /* call ACC, returns */
    else if(!strcmp(m,"CALL")||!strcmp(m,"CALLD")||!strcmp(m,"CC")||!strcmp(m,"CCD")){ f.is_call=1; if(gott){f.has_t=1;f.target=(uint16_t)t;} }
    else if(!strcmp(m,"BCND")||!strcmp(m,"BCNDD")){ f.fall=comma?1:0; if(gott){f.has_t=1;f.target=(uint16_t)t;} }
    else if(!strncmp(m,"RET",3)){ f.fall=(*ops)?1:0; }                               /* uncond return stops */
    else if(fmt_of(m)==0x02){ if(gott){f.has_t=1;f.target=(uint16_t)t;} }            /* conditional branch */
    return f;
}

/* Generic ("WHAT", not "WHY") one-line gloss of an instruction's effect, derived
 * mechanically from the mnemonic/operands. No semantic analysis - just a literal
 * reading, to make the listing skimmable like a hand disassembly. */
static void gloss(const char *mn, const char *ops, char *out, size_t osz){
    out[0]=0;
    #define G(fmt,...) do{ snprintf(out,osz,fmt,__VA_ARGS__); return; }while(0)
    if(!strcmp(mn,"LDP")){ G("DP = %s",ops); }
    if(!strcmp(mn,"SPLK")){ const char*c=strchr(ops,','); if(c) G("[%s] = %.*s",c+1,(int)(c-ops),ops); }
    if(!strcmp(mn,"LACC")||!strcmp(mn,"LACL")||!strcmp(mn,"LACT")){ G("ACC = [%s]",ops); }
    if(!strcmp(mn,"ADD")||!strcmp(mn,"ADDS")||!strcmp(mn,"ADDH")||!strcmp(mn,"ADDC")){ G("ACC += [%s]",ops); }
    if(!strcmp(mn,"SUB")||!strcmp(mn,"SUBS")||!strcmp(mn,"SUBH")||!strcmp(mn,"SUBB")){ G("ACC -= [%s]",ops); }
    if(!strcmp(mn,"AND")||!strcmp(mn,"OR")||!strcmp(mn,"XOR")||!strcmp(mn,"ANDI")||!strcmp(mn,"ANDK")){ G("ACC %s [%s]",mn,ops); }
    if(!strcmp(mn,"SACL")){ const char*c=strchr(ops,','); G("[%.*s] = ACC(low)",c?(int)(c-ops):(int)strlen(ops),ops); }
    if(!strcmp(mn,"SACH")){ const char*c=strchr(ops,','); G("[%.*s] = ACC(high)",c?(int)(c-ops):(int)strlen(ops),ops); }
    if(!strcmp(mn,"SAMM")||!strcmp(mn,"SMMR")){ G("MMR[%s] = ACC",ops); }
    if(!strcmp(mn,"LAMM")||!strcmp(mn,"LMMR")){ G("ACC = MMR[%s]",ops); }
    if(!strcmp(mn,"LAR")){ G("%s",ops); }
    if(!strcmp(mn,"LARP")){ G("ARP = %s",ops); }
    if(!strcmp(mn,"MAR")){ G("AR update %s",ops); }
    if(!strcmp(mn,"CALL")||!strcmp(mn,"CALLD")){ G("call %s",ops); }
    if(!strcmp(mn,"CALA")||!strcmp(mn,"CALAD")){ G("%s","call (ACC)"); }
    if(!strcmp(mn,"B")||!strcmp(mn,"BD")){ G("goto %s",ops); }
    if(!strcmp(mn,"BACC")||!strcmp(mn,"BACCD")){ G("%s","goto (ACC)"); }
    if(!strcmp(mn,"BANZ")||!strcmp(mn,"BANZD")){ G("loop %s (AR != 0)",ops); }
    if(!strncmp(mn,"RET",3)){ G("%s","return"); }
    if(!strcmp(mn,"IDLE")){ G("%s","wait for interrupt"); }
    if(!strcmp(mn,"RPT")||!strcmp(mn,"RPTK")||!strcmp(mn,"RPTZ")){ G("repeat next %s+1",ops); }
    if(!strcmp(mn,"SETC")){ G("set %s",ops); }
    if(!strcmp(mn,"CLRC")){ G("clear %s",ops); }
    if(!strcmp(mn,"MPY")||!strcmp(mn,"MPYK")||!strcmp(mn,"MPYA")){ G("PREG = T * %s",ops); }
    if(!strcmp(mn,"OUT")){ G("port out %s",ops); }
    if(!strcmp(mn,"IN")){ G("port in %s",ops); }
    #undef G
}

/* C5x memory-mapped register names (page 0), for data-xref annotation. */
static const char *mmr_name(unsigned n){
    switch(n){
        case 0x04:return"IMR"; case 0x05:return"GREG"; case 0x06:return"IFR"; case 0x07:return"PMST";
        case 0x08:return"RPTC"; case 0x09:return"BRCR"; case 0x0A:return"PASR"; case 0x0B:return"PAER";
        case 0x0C:return"TREG0"; case 0x0D:return"TREG1"; case 0x0E:return"TREG2"; case 0x0F:return"DBMR";
        case 0x10:return"AR0"; case 0x11:return"AR1"; case 0x12:return"AR2"; case 0x13:return"AR3";
        case 0x14:return"AR4"; case 0x15:return"AR5"; case 0x16:return"AR6"; case 0x17:return"AR7";
        case 0x18:return"INDX"; case 0x19:return"ARCR"; case 0x1A:return"CBSR1"; case 0x1B:return"CBER1";
        case 0x1C:return"CBSR2"; case 0x1D:return"CBER2"; case 0x1E:return"CBCR"; case 0x1F:return"BMAR";
        case 0x20:return"DRR"; case 0x21:return"DXR"; case 0x22:return"SPC"; case 0x24:return"TIM";
        case 0x25:return"PRD"; case 0x26:return"TCR"; case 0x28:return"PDWSR"; case 0x29:return"IOWSR";
        case 0x2A:return"CWSR"; case 0x30:return"TRCV"; case 0x31:return"TDXR"; case 0x32:return"TSPC";
        case 0x33:return"TCSR"; case 0x34:return"TRTA"; case 0x35:return"TRAD"; default:return NULL;
    }
}

/* Data cross-reference summary for a function: scan from its entry to the next
 * function entry, collecting callees and static data/address references (table
 * pma of MAC/BLKD/BLPD..., I/O ports, named MMRs). Generic - straight from the
 * decoded code, no case-specific knowledge. Emits "; calls:" and "; refs:". */
static void emit_fn_refs(FILE *out, const uint16_t *words, long i, long end, uint16_t base, int total,
                         const unsigned char *iscode, const unsigned char *iscall){
    char ce[16][12]; int nce=0; char rf[24][16]; int nrf=0;
    #define ADD(arr,n,cap,...) do{ char _b[16]; snprintf(_b,sizeof _b,__VA_ARGS__); int _d=0; \
        for(int _k=0;_k<n;_k++) if(!strcmp(arr[_k],_b)){_d=1;break;} \
        if(!_d && n<cap) snprintf(arr[n++],sizeof arr[0],"%s",_b); }while(0)
    long j=i; int first=1;
    while(j<end){
        if(!first && iscall[j]) break;            /* next function */
        first=0;
        if(!iscode[j]){ j++; continue; }
        char line[80]; int len=decode_one(words,(int)j,(int)end,line,sizeof line);
        char mn[12]; { const char*p=line; while(*p==' ')p++; int k=0; while(*p&&*p!=' '&&k<11)mn[k++]=*p++; mn[k]=0; }
        uint16_t w2=(j+1<end)?words[j+1]:0;
        Flow f=classify(words,(int)j,(int)end);
        if(f.is_call && f.has_t){ long ti=(long)f.target-base; if(ti>=0&&ti<total) ADD(ce,nce,16,"sub_%04X",f.target); }
        if(!strcmp(mn,"MAC")||!strcmp(mn,"MACD")||!strcmp(mn,"BLKD")||!strcmp(mn,"BLKP")||!strcmp(mn,"BLPD"))
            ADD(rf,nrf,24,"tbl_%04X",w2);
        else if(!strcmp(mn,"IN")||!strcmp(mn,"OUT")) ADD(rf,nrf,24,"port_%04X",w2);
        else if(!strcmp(mn,"SAMM")||!strcmp(mn,"LAMM")||!strcmp(mn,"SMMR")||!strcmp(mn,"LMMR")){
            unsigned fld; if(sscanf(line,"%*s 0x%x",&fld)==1){ const char*nmr=mmr_name(fld); if(nmr) ADD(rf,nrf,24,"%s",nmr); }
        }
        j+=(len>0?len:1);
    }
    if(nce){ fprintf(out,";   calls:"); for(int k=0;k<nce;k++) fprintf(out," %s",ce[k]); fprintf(out,"\n"); }
    if(nrf){ fprintf(out,";   refs: "); for(int k=0;k<nrf;k++) fprintf(out," %s",rf[k]); fprintf(out,"\n"); }
    #undef ADD
}

/* Recursive-descent reachability from the entry indices. Fills iscode[] (an
 * instruction starts here) and islbl[] (a branch/call target or entry). */
static void trace(const uint16_t *words, int total, uint16_t base,
                  const long *entries, int nent, int seed,
                  unsigned char *iscode, unsigned char *islbl, unsigned char *iscall){
    int cap=2*total+64, sp=0; int *st=malloc(sizeof(int)*(size_t)cap);
    unsigned char *seen=calloc((size_t)(total?total:1),1);
    unsigned char *pushed=calloc((size_t)(total?total:1),1);
    if(!st||!seen||!pushed){ free(st); free(seen); free(pushed); return; }
    for(int e=0;e<nent;e++){ long idx=entries[e]-base; if(idx>=0&&idx<total){ islbl[idx]=1; if(!pushed[idx]){pushed[idx]=1; st[sp++]=(int)idx;} } }
    /* Seed from every explicit in-image branch/call target in the image (a
     * linear pre-scan): control reaches most functions only through the computed
     * dispatch, so single-entry descent would miss them. Over-seeding is bounded
     * - descent still stops at branches/returns - and stays round-trip-safe. */
    if(seed){
        for(int i=0;i<total;i++){
            Flow f=classify(words,i,total);
            if(f.has_t){ long ti=(long)f.target-base; if(ti>=0&&ti<total){ islbl[(int)ti]=1; if(f.is_call)iscall[(int)ti]=1; if(!pushed[ti]&&sp<cap){pushed[ti]=1; st[sp++]=(int)ti;} } }
        }
    }
    for(int round=0;;round++){
        while(sp){ int i=st[--sp];
            while(i>=0 && i<total && !seen[i]){
                seen[i]=1; iscode[i]=1;
                Flow f=classify(words,i,total);
                if(f.len==2 && i+1<total) seen[i+1]=1;          /* operand word consumed */
                if(f.has_t){ long ti=(long)f.target-base; if(ti>=0&&ti<total){ islbl[(int)ti]=1; if(f.is_call)iscall[(int)ti]=1; if(!pushed[ti]&&sp<cap){pushed[ti]=1; st[sp++]=(int)ti;} } }
                if(!f.fall) break;
                i += f.len;
            }
        }
        /* Dispatch resolution: a computed branch (BACC) jumps to an address held
         * in data memory, loaded by "LACC dma" just before it and written as a
         * constant elsewhere by "SPLK #addr,dma". First find the dispatch
         * variables (the dma read right before a computed branch); then harvest
         * only the in-image SPLK #addr constants written to THOSE variables as
         * code entries. Precise (avoids data-constant SPLKs) and generic - the
         * idiom, not case-specific. Iterate to a fixpoint. */
        if(!seed) break;
        unsigned char dispvar[128]={0};
        { int prev_load=0, prev_dma=-1;
          for(int i=0;i<total;){
            if(!iscode[i]){ prev_load=0; i++; continue; }
            char ln[80]; int len=decode_one(words,i,total,ln,sizeof ln);
            char mn[12]; { const char*p=ln; while(*p==' ')p++; int k=0; while(*p&&*p!=' '&&k<11)mn[k++]=*p++; mn[k]=0; }
            Flow f=classify(words,i,total);
            if(f.computed && prev_load && prev_dma>=0 && prev_dma<128) dispvar[prev_dma]=1;
            unsigned d; int isload=(!strcmp(mn,"LACC")||!strcmp(mn,"LACL")||!strcmp(mn,"LAMM")||!strcmp(mn,"ZALS")||!strcmp(mn,"LACT"))
                                   && sscanf(ln,"%*s 0x%x",&d)==1 && d<0x80;
            prev_load=isload; prev_dma=isload?(int)d:-1;
            i += (len>0?len:1);
          } }
        int added=0;
        for(int i=0;i<total;i++){
            if(!iscode[i]) continue;
            if((words[i]&0xFF00)==0xAE00 && !(words[i]&0x80) && (words[i]&0x7F)<128 && dispvar[words[i]&0x7F] && i+1<total){ /* SPLK #imm,dispvar */
                long im=(long)words[i+1]-base;
                if(im>=0 && im<total && !pushed[im]){ islbl[(int)im]=1; pushed[im]=1; if(sp<cap){ st[sp++]=(int)im; added++; } }
            }
        }
        /* Range-checked jump-table dispatch: a computed BACC/CALA reached via a
         * "TBLR dma" whose table base came from "ADD #K", usually bounded by a
         * preceding "SUB #N" range check (valid index 0..N => table [K-N .. K],
         * N+1 entries). Harvest the in-image table entries as handler entries
         * (code reached ONLY through the table - e.g. the mailbox tag dispatch).
         * The table words themselves stay data. Generic idiom, not case-knowledge.
         * K/N come from the long-immediate ADD/SUB (fmt 0x12), seen within a short
         * window before the computed transfer. */
        { int hk=-1, hn=-1, htblr=0, since=99;
          for(int i=0;i<total;){
            if(!iscode[i]){ hk=hn=-1; htblr=0; since=99; i++; continue; }
            char ln[80]; int len=decode_one(words,i,total,ln,sizeof ln);
            char mn[12]; { const char*p=ln; while(*p==' ')p++; int k=0; while(*p&&*p!=' '&&k<11)mn[k++]=*p++; mn[k]=0; }
            const char *p=ln; while(*p && *p!=' ')p++; while(*p==' ')p++;   /* operands */
            unsigned imm;
            if((!strcmp(mn,"SUBK")||!strcmp(mn,"SBLK")||!strcmp(mn,"SUB")) && *p=='#' && sscanf(p,"#0x%x",&imm)==1){ hn=(int)imm; since=0; }
            else if((!strcmp(mn,"ADLK")||!strcmp(mn,"ADDK")||!strcmp(mn,"ADD")) && *p=='#' && sscanf(p,"#0x%x",&imm)==1){ hk=(int)imm; since=0; }
            else if(!strcmp(mn,"TBLR")||!strcmp(mn,"TBLW")){ htblr=1; since=0; }
            else since++;
            int computed_dispatch = (!strcmp(mn,"BACC")||!strcmp(mn,"BACCD")||!strcmp(mn,"CALA")||!strcmp(mn,"CALAD"));
            if(computed_dispatch && htblr && hk>=0 && since<10){
                long tb, cnt;
                if(hn>=0 && hn<0x4000){ tb=(long)hk-hn; cnt=(long)hn+1; } else { tb=hk; cnt=256; }
                for(long e=0;e<cnt;e++){
                    long ti=(tb-base)+e;
                    if(ti<0||ti>=total) break;
                    long gi=(long)words[ti]-base;
                    if(gi<0||gi>=total){ if(hn>=0) continue; else break; }  /* padding / end of table */
                    if(!pushed[gi]){ islbl[(int)gi]=1; iscall[(int)gi]=1; pushed[gi]=1; if(sp<cap){ st[sp++]=(int)gi; added++; } }
                }
                hk=hn=-1; htblr=0; since=99;
            }
            i += (len>0?len:1);
          } }
        if(!added || round>16) break;
    }
    free(seen); free(st); free(pushed);
}

/* ---- optional external symbol table (case-specific knowledge as data) ---- *
 * A plain-text file maps addresses to names and may add entry points, so the
 * generic engine can print meaningful names (e.g. from an external symbol file) without
 * any of that knowledge living in the tool. Format, one directive per line:
 *   <hexaddr> <name>     name the label at that DSP address (overrides sub_/loc_)
 *   entry <hexaddr>      add a code entry point for the traversal
 *   # comment
 * ------------------------------------------------------------------------- */
#define MAXSYM 8192
static struct { uint16_t addr; char name[32]; } g_sym[MAXSYM]; static int g_nsym=0;   /* program space */
static struct { uint16_t addr; char name[32]; } g_dsym[MAXSYM]; static int g_ndsym=0;  /* data space */
static const char *sym_lookup(uint16_t a){ for(int i=0;i<g_nsym;i++) if(g_sym[i].addr==a) return g_sym[i].name; return NULL; }
static const char *dsym_lookup(uint16_t a){ for(int i=0;i<g_ndsym;i++) if(g_dsym[i].addr==a) return g_dsym[i].name; return NULL; }
static int sym_valid(const char *s){ if(!(*s=='_'||(*s>='A'&&*s<='Z')||(*s>='a'&&*s<='z'))) return 0;
    for(const char*p=s;*p;p++) if(!(*p=='_'||(*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9'))) return 0; return 1; }
static void sym_load(const char *path, long *entries, int *nent, int maxent){
    FILE *f=fopen(path,"r"); if(!f){ fprintf(stderr,"c5xdis: cannot open symbols %s\n",path); return; }
    char ln[160]; int line=0;
    while(fgets(ln,sizeof ln,f)){ line++;
        char *p=ln; while(*p==' '||*p=='\t')p++;
        if(*p=='#'||*p=='\r'||*p=='\n'||*p==0) continue;
        char w1[64],w2[64],w3[64];
        int nf=sscanf(p,"%63s %63s %63s",w1,w2,w3);
        if(nf<2) continue;
        if(!strcmp(w1,"entry")){ if(*nent<maxent) entries[(*nent)++]=strtol(w2,NULL,0); continue; }
        if((!strcmp(w1,"data")||!strcmp(w1,"d")) && nf==3){      /* data-space symbol */
            if(!sym_valid(w3)){ fprintf(stderr,"c5xdis: symbols:%d: invalid name '%s' (skipped)\n",line,w3); continue; }
            if(g_ndsym<MAXSYM){ g_dsym[g_ndsym].addr=(uint16_t)strtol(w2,NULL,16); snprintf(g_dsym[g_ndsym].name,sizeof g_dsym[0].name,"%s",w3); g_ndsym++; }
            continue;
        }
        if(!sym_valid(w2)){ fprintf(stderr,"c5xdis: symbols:%d: invalid name '%s' (skipped)\n",line,w2); continue; }
        if(g_nsym<MAXSYM){ g_sym[g_nsym].addr=(uint16_t)strtol(w1,NULL,16); snprintf(g_sym[g_nsym].name,sizeof g_sym[0].name,"%s",w2); g_nsym++; }
    }
    fclose(f);
}

/* Split "MNEM ops" and, for a transfer with an in-image target, rewrite the
 * leading 0xADDR operand as a symbolic label (named symbol if known, else
 * sub_/loc_) so the source reads like code. */
static void split_label(const char *line, uint16_t base, int total, const unsigned char *linestart,
                         const unsigned char *iscall, char *mn, size_t mnsz, char *ops, size_t opsz){
    const char *p=line; while(*p==' ')p++;
    size_t k=0; while(*p && *p!=' ' && k+1<mnsz) mn[k++]=*p++; mn[k]=0;
    while(*p==' ')p++;
    unsigned char fm=fmt_of(mn); unsigned t;
    int transfer = fm==0x02 || fm==0x18 || !strcmp(mn,"CC") || !strcmp(mn,"CCD");
    /* Only turn the target into a label when it lands on a real emitted line
     * (never mid-instruction), so every reference resolves on re-assembly.
     * Call targets get a sub_ name, other branch targets a loc_ name. */
    if(transfer && sscanf(p,"0x%x",&t)==1){
        long ti=(long)t-base;
        if(ti>=0 && ti<total && linestart[ti]){
            const char *rest=p; while(*rest && *rest!=',') rest++;   /* keep trailing ,cond */
            const char *sn=sym_lookup((uint16_t)t);
            if(sn) snprintf(ops,opsz,"%s%s", sn, rest);
            else   snprintf(ops,opsz,"%s_%04X%s", iscall[ti]?"sub":"loc", t, rest);
            return;
        }
    }
    snprintf(ops,opsz,"%s",p);
}

static void usage(FILE *f){
    c5x_banner(f,"c5xdis",C5XDIS_ROLE,C5XDIS_SCOPE);
    fputs(
"usage: c5xdis <image.bin> [start_word [count_words]] [options]\n"
"  --base <addr>   load/base word address of the image (default 0)\n"
"  --trace         recursive-descent analysis: separate code from data, assign\n"
"                  labels, mark uncertainties (default entry = base)\n"
"  --entry <addr>  add a code entry point for --trace (repeatable)\n"
"  --symbols <f>   external symbol file: '<hexaddr> <name>' lines name labels,\n"
"                  'entry <hexaddr>' adds an entry point (case knowledge as data)\n"
"  --addr          (linear mode) emit address + hex as a trailing comment\n"
"  -o <file>       output file (default: stdout)\n"
"  -q, --quiet     one-line identity; suppress the diagnostics stream\n"
"  -h, --help      show this help and exit\n"
"Image is raw little-endian 16-bit words. Output is c5xasm source that\n"
"re-assembles byte-exact (recognized instructions decoded, the rest as .word).\n"
"In --trace mode each line carries '; ADDR: HEX [marks]'; markers: @UNRESOLVED\n"
"(computed target), @EXT (target outside image), @SUSPECT (reached, undecodable).\n", f);
}

#define MAXENT 64
int main(int argc,char**argv){
    const char *inpath=NULL,*outpath=NULL,*sympath=NULL; long start=0, count=-1, base=0;
    int quiet=0, addr=0, pos=0, do_trace=0;
    long entries[MAXENT]; int nent=0;
    for(int i=1;i<argc;i++){ const char*a=argv[i];
        if(!strcmp(a,"-o")&&i+1<argc) outpath=argv[++i];
        else if(!strcmp(a,"--base")&&i+1<argc) base=strtol(argv[++i],NULL,0);
        else if(!strcmp(a,"--trace")) do_trace=1;
        else if(!strcmp(a,"--symbols")&&i+1<argc){ sympath=argv[++i]; do_trace=1; }
        else if(!strcmp(a,"--entry")&&i+1<argc){ if(nent<MAXENT) entries[nent++]=strtol(argv[++i],NULL,0); do_trace=1; }
        else if(!strcmp(a,"--addr")) addr=1;
        else if(!strcmp(a,"-q")||!strcmp(a,"--quiet")) quiet=1;
        else if(!strcmp(a,"-h")||!strcmp(a,"--help")){ usage(stdout); return 0; }
        else if(!strcmp(a,"--version")){ c5x_banner(stdout,"c5xdis",C5XDIS_ROLE,C5XDIS_SCOPE); return 0; }
        else if(a[0]=='-'&&a[1]){ fprintf(stderr,"c5xdis: unknown option '%s'\n",a); usage(stderr); return 2; }
        else if(pos==0){ inpath=a; pos++; }
        else if(pos==1){ start=strtol(a,NULL,0); pos++; }
        else if(pos==2){ count=strtol(a,NULL,0); pos++; }
        else { fprintf(stderr,"c5xdis: too many arguments\n"); return 2; }
    }
    if(!inpath){ usage(stderr); return 2; }
    /* Identity on startup: the full banner by default, one line under -q. */
    if(quiet) c5x_oneline(stderr,"c5xdis"); else c5x_banner(stderr,"c5xdis",C5XDIS_ROLE,C5XDIS_SCOPE);

    FILE *inf=fopen(inpath,"rb"); if(!inf){ fprintf(stderr,"c5xdis: cannot open %s\n",inpath); return 1; }
    fseek(inf,0,SEEK_END); long bytes=ftell(inf); fseek(inf,0,SEEK_SET);
    long total=bytes/2;
    uint16_t *words=malloc((size_t)(total? total:1)*sizeof *words);
    if(!words){ fprintf(stderr,"c5xdis: out of memory\n"); fclose(inf); return 1; }
    for(long k=0;k<total;k++){ int lo=fgetc(inf), hi=fgetc(inf); words[k]=(uint16_t)((lo&0xFF)|((hi&0xFF)<<8)); }
    fclose(inf);

    if(start<0) start=0; if(start>total) start=total;
    long end = (count<0) ? total : start+count; if(end>total) end=total;

    FILE *out = outpath ? fopen(outpath,"w") : stdout;
    if(!out){ fprintf(stderr,"c5xdis: cannot write %s\n",outpath); free(words); return 1; }

    if(!do_trace){
        /* linear sweep (round-trip-safe on any blob) */
        for(long i=start;i<end;){
            char line[80]; int used=decode_one(words,(int)i,(int)end,line,sizeof line);
            if(addr){ char hex[16]; if(used==2) snprintf(hex,sizeof hex,"%04X %04X",words[i],words[i+1]); else snprintf(hex,sizeof hex,"%04X",words[i]);
                      fprintf(out,"        %-28s ; %04lX: %s\n", line, base+i, hex); }
            else      fprintf(out,"        %s\n", line);
            i+=used;
        }
        if(out!=stdout) fclose(out);
        free(words); return 0;
    }

    /* ---- recursive-descent analysis ---- */
    if(sympath) sym_load(sympath, entries, &nent, MAXENT);   /* names + extra entries (data) */
    if(nent==0) entries[nent++]=base;                 /* default entry: reset at base */
    unsigned char *iscode=calloc((size_t)(total?total:1),1);
    unsigned char *islbl =calloc((size_t)(total?total:1),1);
    unsigned char *iscall=calloc((size_t)(total?total:1),1);
    if(!iscode||!islbl||!iscall){ fprintf(stderr,"c5xdis: out of memory\n"); return 1; }
    trace(words,(int)total,(uint16_t)base,entries,nent,/*seed=*/1,iscode,islbl,iscall);

    /* cross-reference database: (call target -> caller address), from real code */
    int *xt=malloc(sizeof(int)*(size_t)(total?total:1));
    long *xc=malloc(sizeof(long)*(size_t)(total?total:1)); long nx=0;
    for(long i=start;i<end;){
        if(iscode[i]){ Flow f=classify(words,(int)i,(int)end);
            if(f.is_call && f.has_t){ long ti=(long)f.target-base; if(ti>=0&&ti<total && nx<total){ xt[nx]=(int)ti; xc[nx]=base+i; nx++; } }
            i+=(f.len>0?f.len:1);
        } else i++;
    }

    /* counts + detailed diagnostics (to stderr unless -q) */
    long ncode=0,ndata=0,nsub=0,nloc=0,nunres=0,next=0,nsusp=0;
    for(long i=start;i<end;){
        if(iscode[i]){ ncode++; if(islbl[i]){ if(iscall[i])nsub++; else nloc++; }
            Flow f=classify(words,(int)i,(int)end);
            char tmp[80]; int l2=decode_one(words,(int)i,(int)end,tmp,sizeof tmp);
            if(f.computed){ nunres++; if(!quiet) fprintf(stderr,"[unresolved] %04lX: %s computed target (flow not followed)\n",base+i,f.mn); }
            if(f.has_t){ long ti=(long)f.target-base; if(ti<0||ti>=total){ next++; if(!quiet) fprintf(stderr,"[ext]        %04lX: %s 0x%04X outside image\n",base+i,f.mn,f.target); } }
            if(!strncmp(tmp,".word",5)){ nsusp++; if(!quiet) fprintf(stderr,"[suspect]    %04lX: reached as code but not decodable (%04X)\n",base+i,words[i]); }
            i+=(f.len>0?f.len:l2);
        } else { ndata++; i++; }
    }

    /* which indices begin an emitted line (not consumed as a 2-word operand);
     * a label may only target one of these so every reference resolves. */
    unsigned char *linestart=calloc((size_t)(total?total:1),1);
    if(!linestart){ fprintf(stderr,"c5xdis: out of memory\n"); return 1; }
    for(long i=start;i<end;){ linestart[i]=1;
        if(iscode[i]){ char t2[80]; int u=decode_one(words,(int)i,(int)end,t2,sizeof t2); i+=u; } else i++; }

    /* Basic blocks: a leader starts a block, a block ends at a branch /
     * computed branch / return (a CALL does NOT end it - control returns). A
     * leader is a label, the instruction after a terminator, or code after
     * data. fnblocks[] holds the block count of the function starting at each
     * sub_ entry. Comment-only (a blank line at each boundary); round-trip
     * byte-exact is unaffected. */
    unsigned char *bb=calloc((size_t)(total?total:1),1);
    long *fnblocks=calloc((size_t)(total?total:1),sizeof(long));
    long nblk=0;
    if(!bb||!fnblocks){ fprintf(stderr,"c5xdis: out of memory\n"); return 1; }
    { int prev_term=1; long curfn=-1;
      for(long i=start;i<end;){
        if(iscode[i]){
            if(islbl[i] && iscall[i]) curfn=i;
            if(islbl[i] || prev_term){ bb[i]=1; nblk++; if(curfn>=0) fnblocks[curfn]++; }
            Flow f=classify(words,(int)i,(int)end);
            /* a block ends at a branch (incl. conditional - the fall-through is a
             * new block), a computed branch, or a return (incl. conditional RETC);
             * a CALL does not (control returns to the next instruction). */
            prev_term = (!f.is_call) && (f.computed || !f.fall || f.has_t || !strncmp(f.mn,"RET",3));
            i += (f.len>0?f.len:1);
        } else { prev_term=1; curfn=-1; i++; }
      } }

    /* header: origin, legend, summary */
    fprintf(out,"; c5xdis recursive-descent analysis\n");
    fprintf(out,"; labels: sub_ = call target (function), loc_ = branch target\n");
    fprintf(out,"; markers: @UNRESOLVED computed target  @EXT target outside image  @SUSPECT reached but undecodable\n");
    fprintf(out,"; summary: code=%ld data=%ld subs=%ld locs=%ld blocks=%ld unresolved=%ld ext=%ld suspect=%ld  (entries:",ncode,ndata,nsub,nloc,nblk,nunres,next,nsusp);
    for(int e=0;e<nent;e++) fprintf(out," 0x%04lX",entries[e]);
    fprintf(out,")\n;\n");
    fprintf(out,"        .ps 0x%04lX\n", base+start);

    /* emit: function headers, labels, decoded code (addr/hex/gloss/markers), data */
    int in_data=0; long cur_dp=-1;   /* DP tracking: known data page, -1 = unknown */
    for(long i=start;i<end;){
        /* basic-block boundary: blank line before a block leader (a sub_ already
         * has its own spaced header, so skip it there). */
        if(iscode[i] && bb[i] && i>start && !(islbl[i] && iscall[i])) fprintf(out,"\n");
        if(islbl[i]){
            if(in_data){ in_data=0; }
            const char *sn=sym_lookup((uint16_t)(base+i));
            char nm[40]; if(sn) snprintf(nm,sizeof nm,"%s",sn);
            if(iscall[i]){
                /* function header with its callers (precise xref) */
                if(!sn) snprintf(nm,sizeof nm,"sub_%04lX",base+i);
                long ncall=0; for(long x=0;x<nx;x++) if(xt[x]==i) ncall++;
                fprintf(out,"\n;%.*s\n", 62, "==============================================================");
                fprintf(out,"; %s", nm);
                if(ncall){ fprintf(out,"   xref:"); long shown=0; for(long x=0;x<nx && shown<8;x++) if(xt[x]==i){ fprintf(out," %04lX",xc[x]); shown++; } if(ncall>8) fprintf(out," +%ld",ncall-8); fprintf(out,"  (%ld)",ncall); }
                else fprintf(out,"   xref: none (reached by scan/flow)");
                fprintf(out,"\n");
                emit_fn_refs(out,words,i,end,(uint16_t)base,(int)total,iscode,iscall);
                if(fnblocks[i]) fprintf(out,";   blocks: %ld\n", fnblocks[i]);
                fprintf(out,";%.*s\n", 62, "==============================================================");
                fprintf(out,"%s:\n", nm);
            } else {
                if(!sn) snprintf(nm,sizeof nm,"loc_%04lX",base+i);
                fprintf(out,"%s:\n", nm);
            }
            cur_dp=-1;                          /* label = possible join: DP unknown */
        }
        if(iscode[i]){
            in_data=0;
            char line[80]; int used=decode_one(words,(int)i,(int)end,line,sizeof line);
            char mn[12], ops[48]; split_label(line,(uint16_t)base,(int)total,linestart,iscall,mn,sizeof mn,ops,sizeof ops);
            char hex[16]; if(used==2) snprintf(hex,sizeof hex,"%04X %04X",words[i],words[i+1]); else snprintf(hex,sizeof hex,"%04X     ",words[i]);
            char gl[48]; gloss(mn,ops,gl,sizeof gl);
            /* DP tracking: resolve a direct-dma access to its full data address
             * (DP<<7)|dma when the current page is statically known. */
            char dpn[40]; dpn[0]=0; unsigned dd;
            int mmr=(!strcmp(mn,"SAMM")||!strcmp(mn,"LAMM")||!strcmp(mn,"SMMR")||!strcmp(mn,"LMMR"));
            if(cur_dp>=0 && !mmr && ops[0]=='0'&&ops[1]=='x' && sscanf(ops,"0x%x",&dd)==1 && dd<0x80){
                unsigned long full=(cur_dp<<7)|dd; const char *dn=dsym_lookup((uint16_t)full);
                if(dn) snprintf(dpn,sizeof dpn," D:%s",dn); else snprintf(dpn,sizeof dpn," D:%04lX",full);
            }
            char mark[40]; mark[0]=0; Flow f=classify(words,(int)i,(int)end);
            if(f.computed) snprintf(mark,sizeof mark,"  @UNRESOLVED");
            else if(f.has_t){ long ti=(long)f.target-base; if(ti<0||ti>=total) snprintf(mark,sizeof mark,"  @EXT"); }
            if(!strncmp(line,".word",5)) snprintf(mark,sizeof mark,"  @SUSPECT");
            fprintf(out,"        %-7s %-20s ; %04lX: %-9s %s%s%s\n", mn, ops, base+i, hex, gl[0]?gl:"", dpn, mark);
            /* update DP state for subsequent instructions */
            { unsigned k;
              if(!strcmp(mn,"LDP") && ops[0]=='#' && sscanf(ops,"#0x%x",&k)==1) cur_dp=k&0x1FF;
              else if(!strcmp(mn,"LDP")||!strcmp(mn,"LST")||f.is_call||f.computed) cur_dp=-1; }
            i+=used;
        } else {
            if(!in_data){ fprintf(out,";\n; ---- data ----\n"); in_data=1; }
            fprintf(out,"        .word 0x%04X         ; %04lX\n", words[i], base+i);
            i++;
        }
    }
    free(iscode); free(islbl); free(iscall); free(linestart); free(xt); free(xc); free(bb); free(fnblocks);
    if(out!=stdout) fclose(out);
    free(words);
    return 0;
}
