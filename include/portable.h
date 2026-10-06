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
 * portable.h - portability shims for c5xtools (TMS320C5x toolchain).
 *
 * Goal: the tools compile unchanged under C99 on Linux, Unix, macOS, Windows
 * (MSVC / MinGW / clang, 32- and 64-bit) and 32-bit DOS (DJGPP / OpenWatcom).
 * The only non-ISO-C dependencies in the sources are the POSIX case-insensitive
 * string compares (<strings.h>, absent on Windows/MSVC and bare DOS) and strdup
 * (not ISO C). This header provides portable, ASCII-only, locale-independent
 * replacements and routes the standard names to them, so no call site changes.
 *
 * ASCII-only case folding is deliberate: assembler mnemonics, register names and
 * directives are ASCII, and a locale-independent compare keeps output identical
 * and reproducible across platforms and locales.
 *
 * Include this instead of <strings.h>. It pulls in <string.h> first, so the
 * macro redirections below never collide with a platform declaration.
 */

#ifndef C5X_PORTABLE_H
#define C5X_PORTABLE_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static inline int c5x_strcasecmp(const char *a, const char *b) {
    for (;;) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb) return ca - cb;
        if (ca == 0) return 0;
    }
}

static inline int c5x_strncasecmp(const char *a, const char *b, size_t n) {
    while (n--) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb) return ca - cb;
        if (ca == 0) return 0;
    }
    return 0;
}

static inline char *c5x_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Route the standard POSIX/non-ISO names to the portable versions. <string.h>
 * is already included above, so these macros cannot clash with a declaration. */
#undef strcasecmp
#undef strncasecmp
#undef strdup
#define strcasecmp  c5x_strcasecmp
#define strncasecmp c5x_strncasecmp
#define strdup      c5x_strdup

#endif /* C5X_PORTABLE_H */
