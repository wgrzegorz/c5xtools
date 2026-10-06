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
 * c5xasm.h - shared internal API across the c5xasm translation units.
 *
 * Only the handful of symbols that cross a module boundary live here. The bulk
 * of the assembler state (symbol table, location counter, output/relocation
 * buffers, expression parser internals) stays private to c5xasm.c; a module
 * that needs a value reaches it through eval(), and reports problems through
 * err(). Keeping this surface small is what lets each unit stay independent.
 */

#ifndef C5XASM_H
#define C5XASM_H

/* Report an assembly error at the current source line (pass 2 only). */
void err(const char *m);

/* Evaluate a C5x expression string to a long. Numeric bases: C (0x..),
 * TI suffix (h/b/q), decimal; symbols; '$'/'(' grouping. As a side effect it
 * records, for the most recent call, whether exactly one same-section label was
 * referenced, which c5xasm.c turns into a section-relative COFF relocation. */
long eval(const char *s);

/* Called by field_at() right after it evaluates a DIRECT (dma) memory operand:
 * if that operand named a single relocatable label, it arms a DP-relative dma
 * relocation that the next emitted word carries (COFF output only). */
void dma_operand(long addend);

#endif /* C5XASM_H */
