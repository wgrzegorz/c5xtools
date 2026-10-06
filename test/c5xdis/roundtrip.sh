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
# roundtrip.sh - prove c5xdis output re-assembles byte-exact.
#
#   fixture.asm --c5xasm--> words --> image.bin --c5xdis--> dis.asm --c5xasm--> words2
#   PASS iff words2 == words (the whole point of the toolchain closing the loop).
#
# Usage: sh test/roundtrip.sh [fixture.asm]

set -eu
here=$(cd "$(dirname "$0")" && pwd)
dis="$here/../../tools/c5xdis/c5xdis"
asm="$here/../../tools/c5xasm/c5xasm"
fix="${1:-$here/fixtures/sample.asm}"

[ -x "$dis" ] || { echo "SKIP: c5xdis not built"; exit 0; }
[ -x "$asm" ] || { echo "SKIP: c5xasm not built ($asm)"; exit 0; }
[ -f "$fix" ] || { echo "SKIP: fixture not found ($fix)"; exit 0; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# 1. fixture -> reference word stream (flat "SEC AAAA WWWW")
"$asm" "$fix" -q > "$work/ref.flat"
# 2. flat -> raw little-endian image. awk parses the hex and prints octal escape
#    text (\NNN); the shell printf renders the bytes - portable across awk flavours.
esc=$(awk 'NF==3 {
    h = tolower($3); v = 0
    for (i = 1; i <= length(h); i++) v = v*16 + index("0123456789abcdef", substr(h,i,1)) - 1
    printf "\\%03o\\%03o", v % 256, int(v/256) % 256
}' "$work/ref.flat")
printf "$esc" > "$work/img.bin"
# 3. image -> disassembly
"$dis" "$work/img.bin" -q > "$work/dis.asm"
# 4. disassembly -> word stream again
"$asm" "$work/dis.asm" -q > "$work/rt.flat"

# compare the WWWW columns
awk '{print $3}' "$work/ref.flat" > "$work/ref.w"
awk '{print $3}' "$work/rt.flat"  > "$work/rt.w"
total=$(wc -l < "$work/ref.w" | tr -d ' ')
decoded=$(grep -cvE '^\s*(\.word|;|$)' "$work/dis.asm" || true)
disc=$(grep -cE '^\s*\.word' "$work/dis.asm" || true)

name=$(basename "$fix")
if cmp -s "$work/ref.w" "$work/rt.w"; then
    pct=0; [ "$total" -gt 0 ] && pct=$(( decoded * 100 / total ))
    echo "PASS roundtrip ($name): $total words, re-assembled byte-exact; decoded=$decoded (.word=$disc) coverage=${pct}%"
else
    echo "FAIL roundtrip ($name): word streams differ"
    diff "$work/ref.w" "$work/rt.w" | head -20
    exit 1
fi
