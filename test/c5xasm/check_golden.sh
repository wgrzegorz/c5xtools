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
# check_golden.sh - WINE-FREE confrontation of c5xasm against the frozen TI
# golden. For every case in cases/cases.tsv it assembles cases/<name>.asm with
# c5xasm, normalises the 4-byte COFF timestamp to zero, and checks the object's
# sha256 equals the committed golden/<name>.obj (itself captured from the reference assembler with
# that one volatile field masked). The golden was recorded once by
# capture_golden.sh (needs wine); this gate never is.

set -u
here=$(cd "$(dirname "$0")" && pwd)
asm="$here/../../tools/c5xasm/c5xasm"
. "$here/../lib.sh"      # mask_coff_ts, sha256_hash

[ -x "$asm" ] || { echo "build c5xasm first: $asm" >&2; exit 1; }
[ -d "$here/golden" ] || { echo "SKIP golden: not captured"; exit 0; }
[ -f "$here/cases/cases.tsv" ] || { echo "SKIP golden: no cases"; exit 0; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
rc=0; n=0
while IFS='	' read -r name flag; do
    [ -n "$name" ] || continue
    gold="$here/golden/$name.obj"; src="$here/cases/$name.asm"
    [ -f "$gold" ] && [ -f "$src" ] || { echo "SKIP $name: missing case/golden"; continue; }
    obj="$tmp/$name.obj"
    if ! "$asm" "$src" "$flag" -o "$obj" -v50 -q >/dev/null 2>&1 || [ ! -f "$obj" ]; then
        echo "FAIL $name: c5xasm did not assemble it"; rc=1; continue
    fi
    # normalise the one legitimately-volatile field: COFF f_timdat (bytes 4..7).
    mask_coff_ts "$obj"
    if [ "$(sha256_hash "$obj")" = "$(sha256_hash "$gold")" ]; then
        echo "PASS $name  c5xasm $flag .obj == golden (timestamp masked)"; n=$((n+1))
    else
        echo "FAIL $name: c5xasm $flag .obj differs from the golden"; rc=1
    fi
done < "$here/cases/cases.tsv"
[ $rc -eq 0 ] && echo "check_golden: c5xasm == original the reference assembler across $n cases (wine-free)"
exit $rc
