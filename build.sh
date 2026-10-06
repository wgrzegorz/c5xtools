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
# build.sh - thin convenience wrapper around the top-level Makefile.
#
#   ./build.sh                 build every tool
#   ./build.sh --test          build, then run the offline test suite
#   ./build.sh --clean         remove all build artifacts
#   ./build.sh --clean --test  clean, build, test
#   ./build.sh c5xasm          build a single tool (c5xasm/c5xlnk/c5xhex/c5xdis)
#
# Everything here is just `make`; see the Makefile / common.mk for the real build.

set -eu
here=$(cd "$(dirname "$0")" && pwd)
cd "$here"

do_clean=0; do_test=0; targets=""
for a in "$@"; do
    case "$a" in
        --clean) do_clean=1 ;;
        --test)  do_test=1 ;;
        -h|--help) sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        c5xasm|c5xlnk|c5xhex|c5xdis) targets="$targets $a" ;;
        *) echo "build.sh: unknown argument '$a'" >&2; exit 2 ;;
    esac
done

[ "$do_clean" = 1 ] && make clean
if [ -n "$targets" ]; then make $targets; else make all; fi
[ "$do_test" = 1 ] && make test
exit 0
