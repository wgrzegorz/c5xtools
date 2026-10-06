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
 * c5xbanner.h - shared c5xtools identity banner (wordmark + nameplate).
 *
 * One source of truth for the toolchain version/build stamp and the on-screen
 * identity used by c5xasm / c5xlnk / c5xhex. The whole c5xtools suite shares a
 * single version (see tools/VERSION); the Makefiles inject it together with the
 * build date (and optional git description) via -D, with safe fallbacks below.
 */

#ifndef C5XTOOLS_BANNER_H
#define C5XTOOLS_BANNER_H
#include <stdio.h>
#include <string.h>

#ifndef C5XTOOLS_VERSION
#define C5XTOOLS_VERSION "1.0.1"      /* fallback; Makefile injects from tools/VERSION */
#endif
#ifndef C5XTOOLS_BUILD
#define C5XTOOLS_BUILD __DATE__            /* fallback; Makefile injects ISO build date */
#endif
#ifndef C5XTOOLS_GIT
#define C5XTOOLS_GIT ""                    /* optional " +g<hash>" appended after the date */
#endif

#define C5X_TAG   "dSc"                    /* scene tag embedded in the top rule */
#define C5X_BAR   "="                      /* rule character */
#define C5X_DOT   "-"                      /* field separator */

/* Display width in columns = number of UTF-8 code points (all glyphs we use
 * are single-width), i.e. count every byte that is not a continuation byte. */
static int c5x__cols(const char *s){
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

static void c5x__bars(FILE *f, int n){ for (int i = 0; i < n; i++) fputs(C5X_BAR, f); }

/* A =-rule of width w columns; if tag != NULL it is embedded near the right end. */
static void c5x__rule(FILE *f, int w, const char *tag){
    if (tag) {
        int t = (int)strlen(tag);
        int left = w - t - 2;
        if (left < 0) left = 0;
        c5x__bars(f, left);
        fputs(tag, f);
        c5x__bars(f, 2);
    } else {
        c5x__bars(f, w);
    }
    fputc('\n', f);
}

static void c5x_wordmark(FILE *f){
    fputs(
"      ___|       |               |\n"
"  __| __ \\\\ \\  / __|  _ \\   _ \\  |  __|\n"
" (      ) |`  <  |   (   | (   | |\\__ \\\n"
"\\___|____/ _/\\_\\\\__|\\___/ \\___/ _|____/\n", f);
}

/* Full identity banner for --version / --help. Nameplate width follows the
 * widest content line, so it stays aligned at any version/build string. */
static void c5x_banner(FILE *f, const char *tool, const char *role, const char *scope){
    char L[7][128];
    int n = 0, i, w = 0, t;
    snprintf(L[n++], sizeof L[0], " c5xtools " C5X_DOT " %s " C5XTOOLS_VERSION
                                  " build " C5XTOOLS_BUILD C5XTOOLS_GIT, tool);
    snprintf(L[n++], sizeof L[0], " %s", role);
    snprintf(L[n++], sizeof L[0], " %s", scope);
    snprintf(L[n++], sizeof L[0], " Released under the MIT License");
    L[n++][0] = '\0';                                   /* blank spacer line */
    snprintf(L[n++], sizeof L[0], " Brought to you by + Cult of the Modem +");
    snprintf(L[n++], sizeof L[0], " 2026 Grzegorz Worona <grzegorz@worona.pl>");

    for (i = 0; i < n; i++){ int c = c5x__cols(L[i]); if (c > w) w = c; }
    t = (int)strlen(C5X_TAG) + 2;
    if (w < t) w = t;

    c5x_wordmark(f);
    c5x__rule(f, w, C5X_TAG);
    for (i = 0; i < n; i++){ fputs(L[i], f); fputc('\n', f); }
    c5x__rule(f, w, NULL);
}

/* One compact identity line for ordinary (non-quiet) runs. */
static void c5x_oneline(FILE *f, const char *tool){
    fprintf(f, "%s " C5XTOOLS_VERSION " " C5X_DOT " c5xtools " C5X_DOT " build "
               C5XTOOLS_BUILD C5XTOOLS_GIT " " C5X_DOT " Grzegorz Worona <grzegorz@worona.pl>\n", tool);
}
#endif /* C5XTOOLS_BANNER_H */
