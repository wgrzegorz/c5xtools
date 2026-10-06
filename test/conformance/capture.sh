#!/bin/sh
#         ___|       |               |
#     __| __ \ \  / __|  _ \   _ \  |  __|
#    (      ) |`  <  |   (   | (   | |\__ \ 
#   \___|____/ _/\_\\__|\___/ \___/ _|____/
# ======================================dSc==
#   Brought to you by + Cult of the Modem +
#
# TMS320C5x assembler/linker/disassembler/hex
#
# SPDX-License-Identifier: MIT
# Author:  Grzegorz Worona <grzegorz@worona.pl>
# Version: 1.0.1
#
# capture.sh - ONE-TIME golden capture for the conformance corpus with the
# reference toolset (under wine). Builds the committed corpus.asm,
# it with the reference assembler, reduces the linked .out to a 16-bit word image (out2img.sh), and
# records corpus.img + its SHA-256. Needs the wine wrapper; check.sh never does.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../lib.sh"      # sha256_line
wrap="${C5X_TI_WRAPPER:-}"
[ -f "$wrap" ] || { echo "need the wine TI-toolchain wrapper: $wrap" >&2; exit 1; }

tmp=$(mktemp -d)
cp "$here/corpus.asm" "$tmp/CORPUS.ASM"
# NB: the reference assembler returns rc=0 even when it aborts on an error, so detect by output file.
( cd "$tmp" && sh "$wrap" dspa CORPUS.ASM -v50 >dspa.log 2>&1
               sh "$wrap" dsplnk CORPUS.obj -o o.out >lnk.log 2>&1 ) || true
[ -f "$tmp/o.out" ] || {
  echo "the reference tools produced no o.out:" >&2
  grep -iE 'error|aborted' "$tmp/dspa.log" "$tmp/lnk.log" 2>/dev/null >&2 || sed -n '1,40p' "$tmp/dspa.log" >&2
  exit 1; }
sh "$here/../out2img.sh" "$tmp/o.out" "$here/corpus.img"
( cd "$here" && sha256_line corpus.img ) > "$here/SHA256SUMS"
echo "captured corpus.img ($(wc -c < "$here/corpus.img" | tr -d ' ') bytes)"
rm -rf "$tmp"
