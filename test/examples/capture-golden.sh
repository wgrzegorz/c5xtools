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
# capture-golden.sh - ONE-TIME golden capture with the ORIGINAL TI toolset (wine).
# For each src/*.asm: the reference assembler -> the reference linker, reduce the linked .out to a canonical 16-bit
# word image (out2img.sh), and record golden/<name>.img + its SHA-256. The smoke
# test then needs only these goldens - never the TI tools. Requires the wine wrapper.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../lib.sh"      # sha256_line
wrap="${C5X_TI_WRAPPER:-}"
[ -f "$wrap" ] || { echo "need the wine TI-toolchain wrapper: $wrap" >&2; exit 1; }
mkdir -p "$here/golden"; : > "$here/golden/SHA256SUMS"
for src in "$here"/../../examples/src/*.asm; do
    name=$(basename "$src" .asm); up=$(echo "$name" | tr a-z A-Z)
    tmp=$(mktemp -d); cp "$src" "$tmp/$up.ASM"; cp "$here"/../../examples/src/*.inc "$tmp/" 2>/dev/null || true
    ( cd "$tmp" && sh "$wrap" dspa "$up.ASM" -v50 >/dev/null 2>&1 \
         && sh "$wrap" dsplnk "$name.obj" -o o.out >/dev/null 2>&1 \
         && true )
    sh "$here/../out2img.sh" "$tmp/o.out" "$here/golden/$name.img"
    ( cd "$here/golden" && sha256_line "$name.img" ) >> "$here/golden/SHA256SUMS"
    echo "captured $name  ($(wc -c < "$here/golden/$name.img" | tr -d ' ') bytes)"
    rm -rf "$tmp"
done
echo "goldens + SHA256SUMS written to $here/golden/"
