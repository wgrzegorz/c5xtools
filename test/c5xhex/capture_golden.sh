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
# capture_golden.sh - ONE-TIME golden capture of the reference hex tool output
# (under wine), so check_golden.sh can confront c5xhex offline. For each case it
# builds the linked .out with c5xasm+c5xlnk (deterministic) and freezes the
# the reference hex tool Intel-HEX and Motorola-S conversions (CR-stripped):
#   plain  -> golden/<name>-intel.hex, <name>-srec.hex   (the reference hex tool -i / -m)
#   image  -> same, via a the reference hex tool ROMS{origin,length,fill} command file
# Needs the wine wrapper named in $C5X_TI_WRAPPER; check_golden.sh never does.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../lib.sh"      # sha256_line
asm="$here/../../tools/c5xasm/c5xasm"
lnk="$here/../../tools/c5xlnk/c5xlnk"
wrap="${C5X_TI_WRAPPER:-}"
if [ -z "$wrap" ] || [ ! -f "$wrap" ]; then echo "capture: set C5X_TI_WRAPPER to the TI wrapper" >&2; exit 1; fi
wrap=$(cd "$(dirname "$wrap")" && pwd)/$(basename "$wrap")
FILL=0xBEEF

mkdir -p "$here/golden"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

build_out() {  # <src-stem> -> $tmp/m.out
    "$asm" "$here/$1.asm" -coff -o "$tmp/m.obj" -v50 -q
    "$lnk" "$tmp/m.obj" -o "$tmp/m.out" -q
}
data_range() {  # prints "org len+3" of the .data section (COFF v1 headers)
    od -A n -v -t u1 "$1" | awk '
    { for (i=1;i<=NF;i++) b[n++]=$i }
    function u16(o){ return b[o]+b[o+1]*256 }
    function u32(o){ return b[o]+b[o+1]*256+b[o+2]*65536+b[o+3]*16777216 }
    END { nscn=u16(2); opt=u16(16); base=22+opt; SCN=40
        for (i=0;i<nscn;i++){ o=base+SCN*i; nm=""
            for (j=0;j<8;j++){ c=b[o+j]; if(c==0)break; nm=nm sprintf("%c",c) }
            if (nm==".data"){ printf "%d %d\n", u32(o+8), u32(o+16)+3; exit } } }'
}

for spec in "hex:hex_sample:plain" "image:hole_sample:image"; do
    name=$(echo "$spec" | cut -d: -f1); src=$(echo "$spec" | cut -d: -f2); kind=$(echo "$spec" | cut -d: -f3)
    build_out "$src"
    cp "$tmp/m.out" "$tmp/work.out"
    if [ "$kind" = plain ]; then
        ( cd "$tmp" && sh "$wrap" dsphex -i work.out -o d.i >/dev/null 2>&1 )
        ( cd "$tmp" && sh "$wrap" dsphex -m work.out -o d.m >/dev/null 2>&1 )
    else
        set -- $(data_range "$tmp/m.out"); org=$1; ln=$2
        for pair in "-i:d.i" "-m:d.m"; do
            flag=$(echo "$pair" | cut -d: -f1); outf=$(echo "$pair" | cut -d: -f2)
            printf 'work.out\n-o %s\n%s\n-image\n-memwidth 16\n-romwidth 16\nROMS {\n  EPROM: origin = 0x%x, length = 0x%x, fill = 0x%X\n}\n' \
                "$outf" "$flag" "$org" "$ln" "$FILL" > "$tmp/$outf.cmd"
            ( cd "$tmp" && sh "$wrap" dsphex "$outf.cmd" >/dev/null 2>&1 )
        done
    fi
    for pair in "d.i:intel" "d.m:srec"; do
        f=$(echo "$pair" | cut -d: -f1); tag=$(echo "$pair" | cut -d: -f2)
        [ -f "$tmp/$f" ] || { echo "capture: the reference hex tool produced no $f for $name" >&2; exit 1; }
        tr -d '\r' < "$tmp/$f" > "$here/golden/$name-$tag.hex"
        echo "capture: golden/$name-$tag.hex"
    done
done

( cd "$here/golden" && : > SHA256SUMS; for f in *.hex; do sha256_line "$f"; done >> SHA256SUMS )
echo "capture_golden: done"
