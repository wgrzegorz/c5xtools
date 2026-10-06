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
 * operands.h - C5x operand parsing: field encoding, addressing modes,
 * condition field, control bits. Part of c5xtools. Version: see ../VERSION.
 *
 * The encoders in c5xasm.c build an instruction word from a tokenized operand
 * list; this module turns those tokens into the exact C5x bit fields.
 * It is pure with respect to assembler state - it only calls eval()/err() from
 * c5xasm.h - so it can be read, tested and changed on its own.
 */

#ifndef C5XASM_OPERANDS_H
#define C5XASM_OPERANDS_H
#include <stdint.h>

/* Split a comma-separated operand string in place into trimmed tokens. */
int split(char *s, char *parts[], int max);

/* True if token t is an auxiliary register ARn (0..7); *n gets the index. */
int is_arreg(const char *t, int *n);

/* Indirect-addressing low byte for parts[star_idx] (a '*'-prefixed token);
 * a following ARn in the list selects the next-ARP update. */
uint16_t indirect_byte(char *parts[], int np, int star_idx);

/* Low byte for operand idx: the indirect byte if it starts with '*', else the
 * 7-bit direct dma offset (low 7 bits of the evaluated expression). */
uint16_t field_at(char *pa[], int np, int idx);

/* First shift-like operand after idx (numeric, not ARn, not indirect); 0 if
 * none. */
long shift_after(char *pa[], int np, int idx);

/* Like shift_after, but *found reports whether a value operand was present
 * before the next-ARP (for REQUIRED values: BIT bit code, IN/OUT port). */
long value_before_arp(char *pa[], int np, int idx, int *found);

/* Immediate-operand helpers: '#'-prefix test and value (expression after '#'). */
int  imm_tok(const char *t);
long imm_val(const char *t);

/* Condition field (chapter 3): OR-combine ZLVC bits from the condition tokens
 * at [start, np); *tp gets the TP selector (0=TC,1=NTC,2=BIO,3=none). Reports
 * "INVALID CONDITION VALUE" on more than one condition per group. */
int parse_cond(char *pa[], int np, int start, int *tp);

/* Control-bit index for SETC/CLRC (intm/ovm/cnf/sxm/hm/tc/xf/c). */
int ctrl_field(const char *t);

#endif /* C5XASM_OPERANDS_H */
