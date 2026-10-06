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
# fuzz_reader.sh - robustness gate for the COFF reader: feed it malformed and
# truncated objects and require clean rejection, never a crash. A valid seed
# object is mutated two ways - truncated at every 4-byte boundary, and with one
# byte's high bit flipped at sampled offsets - and the reader (preferably the
# ASan/UBSan build) is run on each. A crash (signal, i.e. exit >= 128), a memory
# error or undefined behaviour fails the gate; a graceful nonzero rejection is
# expected and OK.
#
# The seed's 4-byte COFF timestamp (bytes 4..7) is zeroed first so the mutation
# set is identical on every run and every platform (deterministic, reproducible).
#
# Leak detection is deliberately off (detect_leaks=0). This gate proves memory
# SAFETY on malformed input - no out-of-bounds access, no undefined behaviour,
# no crash - not leak-freedom. On a rejected input the tool exits straight away
# and lets the OS reclaim any names it had interned for the lifetime of the run,
# a normal idiom for a short-lived CLI. LeakSanitizer (Linux-only; absent on
# macOS) would otherwise flag those lifetime allocations and diverge by platform,
# so it is scoped out here while AddressSanitizer and UBSan stay fully active.
#
# Usage: fuzz_reader.sh <reader-binary> <seed-file>
set -u
bin=$1; seed=$2
if [ ! -x "$bin" ]; then echo "SKIP fuzz: reader not built ($bin)"; exit 0; fi
if [ ! -f "$seed" ]; then echo "SKIP fuzz: seed not found ($seed)"; exit 0; fi
here=$(cd "$(dirname "$0")" && pwd)
. "$here/../lib.sh"      # mask_coff_ts
ASAN_OPTIONS=${ASAN_OPTIONS:-abort_on_error=0:exitcode=99:detect_leaks=0}
UBSAN_OPTIONS=${UBSAN_OPTIONS:-halt_on_error=0:exitcode=99}
export ASAN_OPTIONS UBSAN_OPTIONS

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
cp "$seed" "$tmp/seed"
mask_coff_ts "$tmp/seed"   # determinize
seed="$tmp/seed"
size=$(wc -c < "$seed" | tr -d ' ')
crashes=0; runs=0

run_one() {  # $1 = mutated file
    ( cd "$tmp" && "$bin" "$1" >/dev/null 2>err )
    st=$?
    runs=$((runs+1))
    if [ "$st" -ge 128 ] || [ "$st" -eq 99 ] || grep -qi 'sanitizer\|runtime error' "$tmp/err"; then
        echo "CRASH on $(basename "$1") (exit $st)"
        sed -n '1,25p' "$tmp/err"      # surface the sanitizer report for diagnosis
        crashes=$((crashes+1))
    fi
}

# 1. truncations at every 4-byte boundary
len=4
while [ "$len" -lt "$size" ]; do
    head -c "$len" "$seed" > "$tmp/m"; run_one "$tmp/m"; len=$((len+4))
done
# 2. high-bit flip at every 8th offset
off=0
while [ "$off" -lt "$size" ]; do
    cp "$seed" "$tmp/m"
    b=$(od -A n -t u1 -j "$off" -N 1 "$seed" | tr -d ' ')
    nb=$(( b ^ 128 ))
    printf "$(printf '\\%03o' "$nb")" | dd of="$tmp/m" bs=1 seek="$off" count=1 conv=notrunc 2>/dev/null
    run_one "$tmp/m"; off=$((off+8))
done

if [ "$crashes" -eq 0 ]; then
    echo "PASS fuzz: $runs malformed inputs rejected cleanly (no crash)"; exit 0
else
    echo "FAIL fuzz: $crashes/$runs inputs crashed the reader"; exit 1
fi
