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
# roundtrip_corpus.sh - round-trip the full ISA conformance corpus and every example
# through c5xdis, proving the decoder is consistent with the encoder across the whole
# instruction space (assemble -> image -> c5xdis -> c5xasm -> byte-exact word stream).
# This is the single-source-ISA guard: c5xasm and c5xdis share one table
# (include/c5x_isa.h); round-tripping the corpus proves they cannot have drifted.
# Needs no TI tools / no wine.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$here/../.." && pwd)
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
rc=0

# 1. the conformance corpus (one reloc-free form of every mnemonic + operand matrix)
sh "$here/roundtrip.sh" "$top/test/conformance/corpus.asm" || rc=1

# 2. every example program
for src in "$top"/examples/src/*.asm; do
    sh "$here/roundtrip.sh" "$src" || rc=1
done
exit $rc
