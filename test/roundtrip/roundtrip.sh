#!/bin/sh
# roundtrip.sh - three automated tests over the real tool chain, run on the
# native CI runners against every bundled example and every hex format:
#
#   [functional] assemble -> link -> hex(5 formats) -> bin -> disassemble each
#                example and check every artifact is produced and non-empty.
#
#   [golden]     byte-exact determinism / cross-platform check: the SHA-256 of
#                every binary artifact (obj, out, and the intel/srec/tagged/
#                ascii/bin outputs) must match the recorded goldens. The tools
#                are deterministic and write binary outputs in binary mode, so
#                every platform (Linux, macOS, Windows, ...) produces identical
#                bytes.
#
#   [fidelity]   disassembler round-trip: disassemble the linked image, feed the
#                listing back through the assembler+linker, and the re-emitted
#                binary image must equal the original byte-for-byte. Proves the
#                c5xdis "re-assembles byte-exact" guarantee.
#
# Usage (env):
#   BIN=<dir>   directory with the built binaries (default: repo root, falling
#               back to tools/<tool>/)
#   EXE=<suffix>  executable suffix (e.g. .exe on Windows)
#   MODE=functional|golden|fidelity|all|update   which test(s) to run (default all)
#     update    regenerate golden.sha256 from the current tools

set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
BIN=${BIN:-$root}
EXE=${EXE:-}
MODE=${MODE:-all}
golden="$here/golden.sha256"

EXAMPLES="biquad circconv codec_loopback dftfilt dtmf fir goertzel linconv procinit"
FORMATS="intel srec tagged ascii bin"

resolve() {
    n=$1
    if [ -x "$BIN/c5x$n$EXE" ]; then echo "$BIN/c5x$n$EXE"
    elif [ -x "$root/tools/c5x$n/c5x$n$EXE" ]; then echo "$root/tools/c5x$n/c5x$n$EXE"
    else echo ""; fi
}
asm=$(resolve asm); lnk=$(resolve lnk); hex=$(resolve hex); dis=$(resolve dis)
for t in "$asm" "$lnk" "$hex" "$dis"; do
    [ -n "$t" ] || { echo "roundtrip: a tool binary was not found (BIN=$BIN EXE=$EXE)" >&2; exit 2; }
done

sha() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'
        else shasum -a 256 "$1" | awk '{print $1}'; fi; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Run the full chain (plus the disassembler round-trip) for one example.
# Tool banners go to the console; silence them (outputs go to -o files). set -e
# still aborts on any tool failure.
chain() {
    ex=$1
    "$asm" "$root/examples/src/$ex.asm" -coff -o "$work/$ex.obj"        >/dev/null 2>&1
    "$lnk" "$work/$ex.obj" -o "$work/$ex.out"                           >/dev/null 2>&1
    for fmt in $FORMATS; do "$hex" "$work/$ex.out" -f "$fmt" -o "$work/$ex.$fmt" >/dev/null 2>&1; done
    "$dis" "$work/$ex.bin" -o "$work/$ex.dis"                           >/dev/null 2>&1
    "$asm" "$work/$ex.dis" -coff -o "$work/$ex.r.obj"                   >/dev/null 2>&1
    "$lnk" "$work/$ex.r.obj" -o "$work/$ex.r.out"                       >/dev/null 2>&1
    "$hex" "$work/$ex.r.out" -f bin -o "$work/$ex.r.bin"                >/dev/null 2>&1
}
for ex in $EXAMPLES; do chain "$ex"; done

# The byte-compared (binary-mode) artifacts per example.
binart="obj out $FORMATS"

if [ "$MODE" = update ]; then
    : > "$golden"
    for ex in $EXAMPLES; do for a in $binart; do
        echo "$(sha "$work/$ex.$a")  $ex.$a" >> "$golden"
    done; done
    echo "updated $golden ($(wc -l < "$golden" | tr -d ' ') entries)"; exit 0
fi

rc=0
if [ "$MODE" = functional ] || [ "$MODE" = all ]; then
    echo "[functional] round-trip artifacts (obj,out,$FORMATS,dis) per example:"
    for ex in $EXAMPLES; do
        bad=""
        for a in $binart dis; do [ -s "$work/$ex.$a" ] || bad="$bad $a"; done
        if [ -z "$bad" ]; then echo "  PASS $ex"; else echo "  FAIL $ex (empty/missing:$bad)"; rc=1; fi
    done
fi
if [ "$MODE" = golden ] || [ "$MODE" = all ]; then
    echo "[golden] byte-exact determinism vs golden.sha256:"
    [ -s "$golden" ] || { echo "  FAIL golden.sha256 missing; run MODE=update" >&2; exit 1; }
    bad=0
    while read -r want name; do
        want=$(printf '%s' "$want" | tr -d '\r'); name=$(printf '%s' "$name" | tr -d '\r')
        [ -n "$name" ] || continue
        if [ "$(sha "$work/$name")" = "$want" ]; then :; else echo "  FAIL $name"; bad=1; rc=1; fi
    done < "$golden"
    [ "$bad" = 0 ] && echo "  PASS all $(wc -l < "$golden" | tr -d ' ') artifacts byte-exact"
fi
if [ "$MODE" = fidelity ] || [ "$MODE" = all ]; then
    echo "[fidelity] disassemble -> reassemble image equals the original:"
    for ex in $EXAMPLES; do
        if cmp -s "$work/$ex.bin" "$work/$ex.r.bin"; then echo "  PASS $ex"
        else echo "  FAIL $ex (round-trip image differs)"; rc=1; fi
    done
fi

exit $rc
