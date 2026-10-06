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
# smoketest.sh - self-contained toolchain smoke test (NO original tools / wine).
# Build each src/*.asm with c5xtools (asm -> lnk), render the full 16-bit linked
# image with out2img.sh, and compare its sha256 to the golden captured once from
# the reference tools. Both bytes of every word are kept, so relocations and short/long
# form choices are checked, not just the low byte.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../lib.sh"      # mask_coff_ts, sha256_line/hash
asm="$here/../../tools/c5xasm/c5xasm"; lnk="$here/../../tools/c5xlnk/c5xlnk"

sha() { sha256_hash "$1"; }

[ -f "$here/golden/SHA256SUMS" ] || { echo "SKIP: no goldens"; exit 0; }
for t in "$asm" "$lnk"; do [ -x "$t" ] || { echo "SKIP: build c5xtools first ($t)"; exit 0; }; done
rc=0
for src in "$here"/../../examples/src/*.asm; do
    name=$(basename "$src" .asm); gold="$here/golden/$name.img"
    [ -f "$gold" ] || { echo "SKIP $name: no golden"; continue; }
    tmp=$(mktemp -d)
    "$asm" "$src" -coff -o "$tmp/o.obj" -v50 -q
    "$lnk" "$tmp/o.obj" -o "$tmp/o.out" -q
    sh "$here/../out2img.sh" "$tmp/o.out" "$tmp/o.img"
    if [ "$(sha "$gold")" = "$(sha "$tmp/o.img")" ]; then
        echo "PASS smoke: $name  ($(wc -c < "$tmp/o.img" | tr -d ' ') bytes)"
    else
        echo "FAIL smoke: $name  c5xtools != golden"; rc=1
    fi
    rm -rf "$tmp"
done
[ $rc -eq 0 ] && echo "ALL SMOKE TESTS PASSED" || echo "SMOKE TESTS FAILED"
exit $rc
