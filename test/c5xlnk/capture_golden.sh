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
# capture_golden.sh - ONE-TIME golden capture with the ORIGINAL TI toolchain
# (the reference assembler + linker, under wine). Freezes the TI output at BOTH stages so
# check_golden.sh can confront c5xtools offline:
#   pre-link  -> golden/<src>.obj   : the reference assembler .obj (timestamp masked)
#   post-link -> golden/<name>.limg : the reference linker linked image (addr word lines)
# Needs the wine wrapper named in $C5X_TI_WRAPPER; check_golden.sh never does.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../lib.sh"      # mask_coff_ts, sha256_line/hash
wrap="${C5X_TI_WRAPPER:-}"
if [ -z "$wrap" ] || [ ! -f "$wrap" ]; then echo "capture: set C5X_TI_WRAPPER to the TI wrapper" >&2; exit 1; fi
wrap=$(cd "$(dirname "$wrap")" && pwd)/$(basename "$wrap")

SRCS="reloc_sample xref_a xref_b"
GRPS="reloc:reloc_sample xref:xref_a,xref_b"

mkdir -p "$here/golden"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# pre-link: a the reference assembler object per source, 4-byte COFF timestamp zeroed.
for s in $SRCS; do
    cp "$here/$s.asm" "$tmp/$s.asm"
    ( cd "$tmp" && sh "$wrap" dspa "$s.asm" -v50 >/dev/null 2>&1 )
    [ -f "$tmp/$s.obj" ] || { echo "capture: the reference assembler produced no $s.obj" >&2; exit 1; }
    mask_coff_ts "$tmp/$s.obj"
    cp "$tmp/$s.obj" "$here/golden/$s.obj"
    echo "capture: pre-link  golden/$s.obj"
done

# post-link: the reference linker linked image per group, as sorted "addr word" lines.
for g in $GRPS; do
    name=${g%%:*}; objs=$(echo "${g#*:}" | tr ',' ' ' | sed 's/\([^ ][^ ]*\)/\1.obj/g')
    # shellcheck disable=SC2086
    ( cd "$tmp" && sh "$wrap" dsplnk $objs -o "$name.out" >/dev/null 2>&1 )
    [ -f "$tmp/$name.out" ] || { echo "capture: the reference linker produced no $name.out" >&2; exit 1; }
    od -A n -v -t u1 "$tmp/$name.out" | awk '
    { for (i=1;i<=NF;i++) b[n++]=$i }
    function u16(o){ return b[o]+b[o+1]*256 }
    function u32(o){ return b[o]+b[o+1]*256+b[o+2]*65536+b[o+3]*16777216 }
    END {
        nscn=u16(2); opt=u16(16); base=22+opt; SCN=40
        for (i=0;i<nscn;i++){ o=base+SCN*i; va=u32(o+12); sz=u32(o+16); sp=u32(o+20)
            if (sp>0) for (w=0;w<sz;w++) printf "%04x %04x\n", va+w, u16(sp+2*w)
        }
    }' | sort > "$here/golden/$name.limg"
    echo "capture: post-link golden/$name.limg"
done

( cd "$here/golden" && : > SHA256SUMS; for f in *.obj *.limg; do sha256_line "$f"; done >> SHA256SUMS )
echo "capture_golden: done"
