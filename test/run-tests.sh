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
# run-tests.sh - the c5xtools offline test suite (no wine, no TI tools).
#
# 1. each tool's own gate      (make -C tools/<tool> test)
# 2. package-level end-to-end  (examples smoke: asm -> lnk -> image vs golden;
#                               ISA conformance: whole instruction space vs golden)
# 3. reader robustness fuzz    (ASan/UBSan over malformed/truncated COFF)
# 4. golden integrity          (verify every committed golden against its
#                               SHA256SUMS manifest with sha256sum / shasum)

set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
rc=0
TOOLS="c5xasm c5xlnk c5xhex c5xdis"

. "$here/lib.sh"               # mask_coff_ts, sha256_line/hash, sha256_check

# Verify a SHA256SUMS manifest living in <dir> (thin wrapper over sha256_check).
sha_check() { ( cd "$1" || return 1; sha256_check "$2" ); }

MAKE_CMD=${MAKE:-make}
echo "== tool gates =="
for t in $TOOLS; do
    echo "-- $t --"
    $MAKE_CMD --no-print-directory -C "$root/tools/$t" test || rc=1
done

echo "== package: examples smoke =="
sh "$root/test/examples/smoketest.sh" || rc=1

echo "== package: ISA conformance =="
sh "$root/test/conformance/check.sh" || rc=1

echo "== reader robustness fuzz =="
seeddir=$(mktemp -d)
asm="$root/tools/c5xasm/c5xasm"; lnk="$root/tools/c5xlnk/c5xlnk"
# c5xlnk reads objects: seed = a valid valid COFF .obj.
$MAKE_CMD --no-print-directory -C "$root/tools/c5xlnk" asan >/dev/null 2>&1 || true
"$asm" "$root/test/c5xlnk/reloc_sample.asm" -coff -o "$seeddir/seed.obj" -v50 -q >/dev/null 2>&1 || true
sh "$root/test/c5xlnk/fuzz_reader.sh" "$root/tools/c5xlnk/c5xlnk.asan" "$seeddir/seed.obj" || rc=1
# c5xhex reads linked executables: seed = a valid .out.
$MAKE_CMD --no-print-directory -C "$root/tools/c5xhex" asan >/dev/null 2>&1 || true
"$asm" "$root/test/c5xhex/hex_sample.asm" -coff -o "$seeddir/s.obj" -v50 -q >/dev/null 2>&1 \
  && "$lnk" "$seeddir/s.obj" -o "$seeddir/seed.out" -q >/dev/null 2>&1 || true
sh "$root/test/c5xhex/fuzz_reader.sh" "$root/tools/c5xhex/c5xhex.asan" "$seeddir/seed.out" || rc=1
rm -rf "$seeddir"

echo "== golden integrity (sha256) =="
found=0
for m in "$root"/test/*/golden/SHA256SUMS \
         "$root"/test/examples/golden/SHA256SUMS \
         "$root"/test/conformance/SHA256SUMS; do
    [ -f "$m" ] || continue
    found=$((found+1))
    d=$(dirname "$m")
    if sha_check "$d" SHA256SUMS; then
        echo "  PASS $(printf '%s' "$d" | sed "s|$root/||")"
    else
        echo "  FAIL $(printf '%s' "$d" | sed "s|$root/||") (sha256 mismatch)"; rc=1
    fi
done
[ "$found" -gt 0 ] || echo "  (no golden manifests found)"

echo
[ $rc -eq 0 ] && echo "run-tests: ALL GATES PASS (offline)" || echo "run-tests: FAILURES above"
exit $rc
