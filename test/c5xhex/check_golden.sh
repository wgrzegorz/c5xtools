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
# check_golden.sh - WINE-FREE confrontation of c5xhex against the frozen golden
# golden. For each case it rebuilds the linked .out with c5xasm+c5xlnk (same bytes
# as at capture, proven by the c5xlnk golden), converts it with c5xhex in
# the reference hex tool-compat mode, and compares the Intel-HEX and Motorola-S output
# (CR-stripped) to golden/. Then a wine-free self-check: the full 16-bit binary
# equals the linked image (out2img.sh). A mismatch means c5xhex and the reference hex tool diverge.

set -u
here=$(cd "$(dirname "$0")" && pwd)
asm="$here/../../tools/c5xasm/c5xasm"
lnk="$here/../../tools/c5xlnk/c5xlnk"
hex="$here/../../tools/c5xhex/c5xhex"
FILL=0xBEEF

if [ ! -x "$asm" ] || [ ! -x "$lnk" ] || [ ! -x "$hex" ]; then
    echo "build the tools first" >&2; exit 1
fi
if [ ! -d "$here/golden" ]; then echo "SKIP golden: not captured"; exit 0; fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
rc=0

build_out() {  # <src-stem> <dst.out>
    "$asm" "$here/$1.asm" -coff -o "$tmp/$1.obj" -v50 -q >/dev/null 2>&1
    "$lnk" "$tmp/$1.obj" -o "$2" -q >/dev/null 2>&1
}
# (origin length+3) of the .data section = the image-fill range (prints "org len").
data_range() {
    od -A n -v -t u1 "$1" | awk '
    { for (i=1;i<=NF;i++) b[n++]=$i }
    function u16(o){ return b[o]+b[o+1]*256 }
    function u32(o){ return b[o]+b[o+1]*256+b[o+2]*65536+b[o+3]*16777216 }
    END {
        nscn=u16(2); opt=u16(16); base=22+opt; SCN=40
        for (i=0;i<nscn;i++){ o=base+SCN*i; nm=""
            for (j=0;j<8;j++){ c=b[o+j]; if(c==0)break; nm=nm sprintf("%c",c) }
            if (nm==".data"){ printf "%d %d\n", u32(o+8), u32(o+16)+3; exit }
        }
    }'
}
strip_cr() { tr -d '\r' < "$1" > "$2"; }

# plain (whole image, 8-bit) + image (ROM fill over .data range, 16-bit)
for spec in "hex:hex_sample:plain" "image:hole_sample:image"; do
    name=$(echo "$spec" | cut -d: -f1)
    src=$(echo "$spec" | cut -d: -f2)
    kind=$(echo "$spec" | cut -d: -f3)
    build_out "$src" "$tmp/m.out"
    if [ "$kind" = plain ]; then
        "$hex" "$tmp/m.out" -f intel -romwidth 8 -order lsb -o "$tmp/c.i" >/dev/null 2>&1
        "$hex" "$tmp/m.out" -f srec  -romwidth 8 -order lsb -o "$tmp/c.m" >/dev/null 2>&1
    else
        set -- $(data_range "$tmp/m.out"); org=$1; ln=$2
        "$hex" "$tmp/m.out" -f intel -image -org "$org" -len "$ln" -fill "$FILL" -romwidth 16 -o "$tmp/c.i" >/dev/null 2>&1
        "$hex" "$tmp/m.out" -f srec  -image -org "$org" -len "$ln" -fill "$FILL" -romwidth 16 -o "$tmp/c.m" >/dev/null 2>&1
    fi
    for pair in "c.i:intel" "c.m:srec"; do
        f=$(echo "$pair" | cut -d: -f1); tag=$(echo "$pair" | cut -d: -f2)
        gold="$here/golden/$name-$tag.hex"
        if [ ! -f "$gold" ]; then continue; fi
        strip_cr "$tmp/$f" "$tmp/$f.nocr"
        if cmp -s "$tmp/$f.nocr" "$gold"; then
            echo "PASS $name-$tag: c5xhex == the reference hex tool"
        else
            echo "FAIL $name-$tag: c5xhex output differs from the golden"; rc=1
        fi
    done
done

# self-check: c5xhex -f bin equals the linked image (section words, LE, file
# order, skipping .bss) read straight from the .out with COFF v1 section headers.
build_out hex_sample "$tmp/m.out"
"$hex" "$tmp/m.out" -f bin -o "$tmp/c.bin" >/dev/null 2>&1
ref_esc=$(od -A n -v -t u1 "$tmp/m.out" | awk '
{ for (i=1;i<=NF;i++) b[n++]=$i }
function u16(o){ return b[o]+b[o+1]*256 }
function u32(o){ return b[o]+b[o+1]*256+b[o+2]*65536+b[o+3]*16777216 }
function bit7(x){ return int(x/128)%2 }
END {
    nscn=u16(2); opt=u16(16); base=22+opt; SCN=40
    for (i=0;i<nscn;i++){ o=base+SCN*i; sz=u32(o+16); sp=u32(o+20); fl=u32(o+36)
        if (sz>0 && sp>0 && bit7(fl)==0)
            for (w=0;w<sz;w++) printf "\\%03o\\%03o", b[sp+2*w], b[sp+2*w+1]
    }
}')
printf "$ref_esc" > "$tmp/ref.bin"
if cmp -s "$tmp/c.bin" "$tmp/ref.bin"; then
    echo "PASS hex-roundtrip: bin == .out words"
else
    echo "FAIL hex-roundtrip: bin != .out words"; rc=1
fi

if [ "$rc" -eq 0 ]; then
    echo "check_golden: c5xhex == original the reference hex tool (Intel + Motorola-S, plain + image fill), wine-free"
fi
exit "$rc"
