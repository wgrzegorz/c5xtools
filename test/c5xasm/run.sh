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
# Seed test: byte-exact validated C5x words. Output format: "<P|D> ADDR WORD".

set -e
here=$(dirname "$0")
bin="$here/../../tools/c5xasm/c5xasm"
got=$("$bin" "$here/seed.asm" | sort)
exp=$(printf 'P 0000 B94A\nP 0001 B801\nP 0002 EF00\nP 0003 1234\nP 0004 0000\n' | sort)

echo "--- c5xasm ---"; echo "$got"
if [ "$got" = "$exp" ]; then
    echo "PASS seed: lacl #4ah=B94A, add #1h=B801, ret=EF00, .word, label"
    exit 0
else
    echo "--- expected ---"; echo "$exp"; echo "FAIL seed"; exit 1
fi
