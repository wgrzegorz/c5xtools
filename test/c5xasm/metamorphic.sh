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
# metamorphic.sh - oracle-free, wine-free metamorphic test for c5xasm.
#
# Relation (trivia-invariance): inserting blank lines and comment lines between
# source lines is semantically neutral, so the assembled object must not change.
# Applied to every self-contained case source, this catches lexer / comment /
# blank-line / layout regressions that a fixed golden might miss. Deterministic:
# the transform is a fixed rewrite, not random.

set -u
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../lib.sh"      # mask_coff_ts, sha256_line/hash
asm="$here/../../tools/c5xasm/c5xasm"
sha() { sha256_hash "$1"; }
mask() { mask_coff_ts "$1"; }

[ -x "$asm" ] || { echo "SKIP metamorphic: build c5xasm first"; exit 0; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
rc=0; n=0
for src in "$here"/cases/*.asm; do
    name=$(basename "$src" .asm)
    # Assemble both the original and the trivia variant from the SAME input name
    # (T.ASM), so the COFF .file symbol - the verbatim input name - matches and
    # only a genuine content difference can change the object.
    cp "$src" "$tmp/T.ASM"
    "$asm" "$tmp/T.ASM" -coff -o "$tmp/a.obj" -v50 -q >/dev/null 2>&1 || continue
    mask "$tmp/a.obj"
    # trivia variant: a comment and a blank line before every original line.
    awk '{ print "; metamorphic"; print ""; print }' "$src" > "$tmp/T.ASM"
    if ! "$asm" "$tmp/T.ASM" -coff -o "$tmp/b.obj" -v50 -q >/dev/null 2>&1; then
        echo "FAIL metamorphic: $name (trivia variant rejected)"; rc=1; continue
    fi
    mask "$tmp/b.obj"
    if [ "$(sha "$tmp/a.obj")" = "$(sha "$tmp/b.obj")" ]; then n=$((n+1))
    else echo "FAIL metamorphic: $name (trivia changed the object)"; rc=1; fi
done
[ $rc -eq 0 ] && echo "PASS metamorphic: $n cases trivia-invariant (blank/comment lines)"
exit $rc
