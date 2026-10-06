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
# check_golden.sh - WINE-FREE confrontation of c5xtools against the frozen TI
# golden at BOTH stages:
#   pre-link  : c5xasm .obj (timestamp masked) sha256 == golden/<src>.obj (the reference assembler)
#   post-link : c5xlnk -flat linked image      == golden/<name>.limg   (the reference linker)
# The golden was captured once by capture_golden.sh (needs wine); this never is.

set -u
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../lib.sh"      # mask_coff_ts, sha256_line/hash
asm="$here/../../tools/c5xasm/c5xasm"
lnk="$here/../../tools/c5xlnk/c5xlnk"

sha() { sha256_hash "$1"; }
mask() { mask_coff_ts "$1"; }

if [ ! -x "$asm" ] || [ ! -x "$lnk" ]; then echo "build c5xasm and c5xlnk first" >&2; exit 1; fi
if [ ! -d "$here/golden" ]; then echo "SKIP golden: not captured"; exit 0; fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
rc=0

# pre-link: our object (timestamp masked) == the golden object
for s in reloc_sample xref_a xref_b; do
    gold="$here/golden/$s.obj"
    if [ ! -f "$gold" ]; then continue; fi
    "$asm" "$here/$s.asm" -coff -o "$tmp/$s.obj" -v50 -q >/dev/null 2>&1
    mask "$tmp/$s.obj"
    if [ "$(sha "$tmp/$s.obj")" = "$(sha "$gold")" ]; then
        echo "PASS pre-link  $s  c5xasm .obj == golden (timestamp masked)"
    else
        echo "FAIL pre-link  $s: c5xasm .obj differs from the golden"
        rc=1
    fi
done

# post-link: our linked image contains every word of the golden image.
# reloc = [reloc_sample]; xref = [xref_a xref_b].
check_link() {
    name=$1; shift
    gold="$here/golden/$name.limg"
    if [ ! -f "$gold" ]; then return 0; fi
    objs=""
    for s in "$@"; do
        "$asm" "$here/$s.asm" -coff -o "$tmp/$s.obj" -v50 -q >/dev/null 2>&1
        objs="$objs $tmp/$s.obj"
    done
    "$lnk" $objs -flat -o "$tmp/$name.flat" -q >/dev/null 2>&1
    awk 'NF==3 { printf "%s %s\n", tolower($2), tolower($3) }' "$tmp/$name.flat" | sort > "$tmp/$name.ours"
    miss=0
    while IFS= read -r line; do
        if [ -z "$line" ]; then continue; fi
        if ! grep -Fxq "$line" "$tmp/$name.ours"; then miss=$((miss+1)); fi
    done < "$gold"
    tot=$(grep -c . "$gold")
    if [ "$miss" -eq 0 ]; then
        echo "PASS post-link $name  $tot linked words == the reference linker"
    else
        echo "FAIL post-link $name: $miss/$tot words differ from the golden"
        rc=1
    fi
}
check_link reloc reloc_sample
check_link xref  xref_a xref_b

if [ "$rc" -eq 0 ]; then
    echo "check_golden: c5xtools == original TI toolchain at both stages (wine-free)"
fi
exit "$rc"
