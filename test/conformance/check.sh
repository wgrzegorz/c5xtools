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
# check.sh - wine-free ISA conformance gate. Builds the committed corpus.asm with
# c5xtools (c5xasm -> c5xlnk -> out2img.sh) and compares the 16-bit image against
# the committed golden (corpus.img). A mismatch means c5xasm and the TI
# toolset disagree on some mnemonic or operand form - a compatibility regression.
#
# corpus.asm (one representative form of every mnemonic + an operand-form matrix)
# is committed alongside its golden; see README.md. No python, no wine.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
asm="$here/../../tools/c5xasm/c5xasm"
lnk="$here/../../tools/c5xlnk/c5xlnk"
[ -x "$asm" ] || { echo "build c5xasm first: $asm" >&2; exit 1; }
[ -f "$here/corpus.asm" ] || { echo "no corpus.asm" >&2; exit 1; }
[ -f "$here/corpus.img" ] || { echo "no golden corpus.img" >&2; exit 1; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
"$asm" "$here/corpus.asm" -coff -o "$tmp/c.obj" -v50 -q
"$lnk" "$tmp/c.obj" -o "$tmp/c.out" -q
sh "$here/../out2img.sh" "$tmp/c.out" "$tmp/c.img"
if cmp -s "$tmp/c.img" "$here/corpus.img"; then
    n=$(($(wc -c < "$here/corpus.img") / 2))
    echo "PASS conformance: c5xasm matches the golden across $n corpus words (per-mnemonic + operand-form matrix)"
else
    echo "FAIL conformance: c5xasm image differs from the golden" >&2
    echo "  c5xasm: $tmp/c.img   golden: $here/corpus.img" >&2
    exit 1
fi
