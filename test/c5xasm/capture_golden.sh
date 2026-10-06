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
# capture_golden.sh - ONE-TIME golden capture with the reference assembler (wine).
# For every case in cases/cases.tsv it assembles cases/<name>.asm with the real
# the reference assembler, normalises the 4-byte COFF timestamp to zero, and writes golden/<name>.obj
# plus a SHA256SUMS manifest. check_golden.sh then confronts c5xasm offline.
# Needs the wine wrapper named in $C5X_TI_WRAPPER; re-run only when cases change.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../lib.sh"      # mask_coff_ts, sha256_line/hash
wrap="${C5X_TI_WRAPPER:-}"
[ -n "$wrap" ] && [ -f "$wrap" ] || { echo "capture: set C5X_TI_WRAPPER to the reference assembler wrapper" >&2; exit 1; }
wrap=$(cd "$(dirname "$wrap")" && pwd)/$(basename "$wrap")
sha() { sha256_line "$1"; }

mkdir -p "$here/golden"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
while IFS='	' read -r name flag; do
    [ -n "$name" ] || continue
    # the reference assembler keys the .file symbol off the input name; feed it the bare basename.
    cp "$here/cases/$name.asm" "$tmp/$name.asm"
    ( cd "$tmp" && sh "$wrap" dspa "$name.asm" -v50 >/dev/null 2>&1 )
    [ -f "$tmp/$name.obj" ] || { echo "capture: the reference assembler produced no $name.obj" >&2; exit 1; }
    mask_coff_ts "$tmp/$name.obj"
    cp "$tmp/$name.obj" "$here/golden/$name.obj"
    echo "capture: golden/$name.obj"
done < "$here/cases/cases.tsv"

( cd "$here/golden" && : > SHA256SUMS
  for f in *.obj; do sha "$f"; done >> SHA256SUMS )
echo "capture_golden: done ($(ls "$here"/golden/*.obj | wc -l | tr -d ' ') objects)"
