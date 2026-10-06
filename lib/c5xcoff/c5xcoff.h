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
 * c5xcoff.h - shared TI C5x COFF format module for c5xtools (single source of truth).
 *
 * One place for the COFF byte layout and constants so the assembler writer, the
 * linker reader/writer and the disassembler reader cannot drift. Field semantics
 * follow the TI C5x COFF format in SPRU018D Appendix A (file, section, symbol
 * and relocation structures), cross-checked byte-for-byte. All multi-byte values
 * are little-endian.
 *
 * The higher-level record COMPOSITION legitimately differs per tool (a relocatable
 * .obj zeroes section paddr/vaddr; an absolute .out carries real addresses; the
 * file header's target word and flags differ between .obj and .out), so those stay
 * in each tool - but built from the shared primitives and named constants here.
 */

#ifndef C5XCOFF_H
#define C5XCOFF_H

#include <stdio.h>
#include <stdint.h>

/* ---- file header version id (bytes 0-1) ---- */
#define COFF_MAGIC_V0   0x00c0u   /* 20-byte header, 10-byte reloc (legacy)       */
#define COFF_MAGIC_V1   0x00c1u   /* 22-byte header, 40-byte scnhdr, 12-byte reloc */
#define COFF_MAGIC_V2   0x00c2u   /* 22-byte header, 48-byte scnhdr, 12-byte reloc */
#define COFF_TARGET_C5X 0x0092u   /* f_target_id: TMS320C2x/C2xx/C5x               */

/* ---- optional (a.out) header ---- */
#define COFF_OPT_MAGIC  0x0108u
#define COFF_OPT_VSTAMP 0x02beu
#define COFF_OPT_SIZE   28

/* ---- file header flags (f_flags) ---- */
#define F_RELFLG 0x0001u          /* relocation info stripped                     */
#define F_EXEC   0x0002u          /* no unresolved references (linked executable) */
#define F_LNNO   0x0004u
#define F_LSYMS  0x0010u

/* ---- section flags (s_flags, STYP) ---- */
#define STYP_DSECT  0x0001u
#define STYP_NOLOAD 0x0002u
#define STYP_GROUP  0x0004u
#define STYP_PAD    0x0008u
#define STYP_COPY   0x0010u
#define STYP_TEXT   0x0020u
#define STYP_DATA   0x0040u
#define STYP_BSS    0x0080u

/* ---- relocation types (C5x, SPRU018D Appendix A, Table A-9) ---- */
#define R_ABS      0x0000u
#define R_RELBYTE  0x000fu
#define R_RELWORD  0x0010u        /* full 16-bit word                       */
#define R_PARTLS7  0x0028u        /* 7 LSBs: direct dma field (DP-relative)  */
#define R_PARTMS9  0x0029u        /* 9 MSBs: DP page                         */
#define R_REL      0x002au        /* 13-bit                                  */

/* ---- storage classes (c_sclass) ---- */
#define C_EXT  2
#define C_STAT 3
#define C_FILE 103

/* ---- record sizes ---- */
#define COFF_FILEHDR_V2  22
#define COFF_SCNHDR_V1   40
#define COFF_SCNHDR_V2   48
#define COFF_SYMENT      18
#define COFF_RELOC       12       /* v1/v2: vaddr(4) symndx(4) resv(2) type(2) */

/* ---- relocation kinds as data (single source of the field semantics) ----
 * One row per C5x relocation type: the width/shift of the instruction-word field
 * it patches, and whether the full section-relative value is carried as an explicit
 * addend in the reloc entry's reserved field (RELA-style) rather than in the word.
 * This is what an internal RELA record {section, offset, target, type, addend}
 * lowers through, and the one place the per-type bit layout is defined. */
typedef struct {
    uint16_t    type;        /* R_* type number stored in the reloc entry          */
    const char *name;        /* "R_RELWORD", ...                                   */
    uint8_t     field_bits;  /* patched field width in the 16-bit word (16 = full) */
    uint8_t     shift;       /* right-shift applied to the value before masking     */
    uint8_t     uses_addend; /* 1 = full value carried in the reserved field        */
} C5xCoffRelocKind;
/* Descriptor for a relocation type, or NULL if unknown/unsupported. */
const C5xCoffRelocKind *c5xcoff_reloc_kind(uint16_t type);

/* ---- little-endian byte primitives ---- */
void     c5xcoff_put16(FILE *f, unsigned v);
void     c5xcoff_put32(FILE *f, unsigned long v);
uint16_t c5xcoff_rd16(const uint8_t *p);
uint32_t c5xcoff_rd32(const uint8_t *p);

/* ---- shared record writers (identical bytes across .obj/.out) ---- */
void c5xcoff_w_name(FILE *f, const char *s);                 /* 8-byte name field, null-padded */
void c5xcoff_w_reloc(FILE *f, uint32_t vaddr, uint32_t symndx, unsigned resv, unsigned type);
void c5xcoff_w_sym(FILE *f, const char *nm, long val, int scnum, int sclass, int naux);
void c5xcoff_w_aux_scn(FILE *f, long len, int nreloc);       /* section symbol aux: scnlen, nreloc */

#endif /* C5XCOFF_H */
