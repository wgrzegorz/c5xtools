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
 * c5xcoff.c - shared TI C5x COFF primitives and record writers. See c5xcoff.h.
 */

#include "c5xcoff.h"
#include <string.h>

/* The relocation kinds we emit and understand, as data. R_RELWORD carries the
 * value in the full word (no separate addend); the partial kinds patch a narrow
 * field and carry the full section-relative value as an explicit addend in the
 * reserved field. R_REL is listed for completeness but is a stub until a
 * differential test exercises it. (SPRU018D Appendix A, Table A-9 Relocation
 * Types, for R_RELWORD/R_PARTLS7/R_PARTMS9.) */
static const C5xCoffRelocKind g_reloc_kinds[] = {
    /* type        name          bits shift uses_addend */
    { R_RELWORD,  "R_RELWORD",   16,  0,    0 },
    { R_RELBYTE,  "R_RELBYTE",    8,  0,    1 },  /* ADDK/LACK/SUBK/LARK #sym (if addend fits 8 bits) */
    { R_PARTLS7,  "R_PARTLS7",    7,  0,    1 },
    { R_PARTMS9,  "R_PARTMS9",    9,  7,    1 },
    { R_REL,      "R_REL",       13,  0,    1 },  /* MPYK #sym (13-bit, no promotion) */
};

const C5xCoffRelocKind *c5xcoff_reloc_kind(uint16_t type) {
    size_t i;
    for (i = 0; i < sizeof g_reloc_kinds / sizeof g_reloc_kinds[0]; i++)
        if (g_reloc_kinds[i].type == type) return &g_reloc_kinds[i];
    return NULL;
}

void c5xcoff_put16(FILE *f, unsigned v) {
    fputc((int)(v & 0xff), f);
    fputc((int)((v >> 8) & 0xff), f);
}

void c5xcoff_put32(FILE *f, unsigned long v) {
    c5xcoff_put16(f, (unsigned)(v & 0xffff));
    c5xcoff_put16(f, (unsigned)((v >> 16) & 0xffff));
}

uint16_t c5xcoff_rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

uint32_t c5xcoff_rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void c5xcoff_w_name(FILE *f, const char *s) {
    char b[8];
    memset(b, 0, sizeof b);
    /* COFF inline name: up to 8 bytes, null-padded, not necessarily terminated */
    strncpy(b, s, sizeof b);
    fwrite(b, 1, sizeof b, f);
}

void c5xcoff_w_reloc(FILE *f, uint32_t vaddr, uint32_t symndx, unsigned resv, unsigned type) {
    c5xcoff_put32(f, vaddr);
    c5xcoff_put32(f, symndx);
    c5xcoff_put16(f, resv);
    c5xcoff_put16(f, type);
}

void c5xcoff_w_sym(FILE *f, const char *nm, long val, int scnum, int sclass, int naux) {
    c5xcoff_w_name(f, nm);
    c5xcoff_put32(f, (unsigned long)val);
    c5xcoff_put16(f, (unsigned)(scnum & 0xffff));
    c5xcoff_put16(f, 0);                 /* n_type = 0 */
    fputc(sclass & 0xff, f);
    fputc(naux & 0xff, f);
}

void c5xcoff_w_aux_scn(FILE *f, long len, int nreloc) {
    char z[12];
    memset(z, 0, sizeof z);
    c5xcoff_put32(f, (unsigned long)len);
    c5xcoff_put16(f, (unsigned)nreloc);
    fwrite(z, 1, sizeof z, f);        /* rest of the 18-byte aux entry */
}
