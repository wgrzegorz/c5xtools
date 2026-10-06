#!/bin/sh
# build.sh - compile the four c5xtools programs without make and without tests.
#
# Builds c5xasm, c5xlnk, c5xhex and c5xdis from source using a single C99
# compiler. No make, no test suite, no third-party libraries.
#
# Overridable via the environment:
#   CC   C compiler              (default: cc)
#   OPT  optimization/std flags  (default: -std=c99 -O2)
#   EXE  executable suffix       (default: empty; use .exe for MinGW/Cygwin)
#   OUT  output directory        (default: the repository root)
#
# Example:  CC=clang OUT=dist sh scripts/build.sh

set -eu

# Run from the repository root regardless of where the script is invoked.
cd "$(dirname "$0")/.."

CC=${CC:-cc}
OPT=${OPT:--std=c99 -O2}
EXE=${EXE:-}
OUT=${OUT:-.}
INC="-Iinclude -Ilib/c5xcoff"
COFF="lib/c5xcoff/c5xcoff.c"

# Stamp version, build date and git description into the banner, matching
# common.mk so a script build is identical to a make build. The single source
# for the version is VERSION; without this the binaries fall back to the
# hard-coded values in include/c5xbanner.h.
VER=$(cat VERSION 2>/dev/null || echo 0.0.0)
if [ -n "${SOURCE_DATE_EPOCH:-}" ]; then
    BUILD=$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y-%m-%d 2>/dev/null \
            || date -u -r "$SOURCE_DATE_EPOCH" +%Y-%m-%d 2>/dev/null \
            || echo reproducible)
else
    BUILD=$(date -u +%Y-%m-%d)
fi
GITDESC=$(git describe --tags --always --dirty --abbrev=8 2>/dev/null || true)
# Append the git description only for non-release builds. At the exact release
# tag (describe == v<VERSION>) the banner stays clean: "<tool> X.Y.Z build <date>".
if [ -n "$GITDESC" ] && [ "$GITDESC" != "v$VER" ]; then usegit=1; else usegit=0; fi
if [ "$usegit" = 1 ]; then echo "building c5xtools $VER ($BUILD +g$GITDESC)"
else echo "building c5xtools $VER ($BUILD)"; fi

VER_VER="-DC5XTOOLS_VERSION=\"$VER\""
VER_BUILD="-DC5XTOOLS_BUILD=\"$BUILD\""
VER_GIT="-DC5XTOOLS_GIT=\" +g$GITDESC\""

mkdir -p "$OUT"

build() {           # build <name> <tool source(s)...>
    name=$1
    shift
    echo "CC  $OUT/$name$EXE"
    # Word splitting of $OPT and $INC is intentional; the -D defines are passed
    # as single quoted arguments so their embedded quotes/spaces are preserved.
    if [ "$usegit" = 1 ]; then
        $CC $OPT $INC "$VER_VER" "$VER_BUILD" "$VER_GIT" "$@" "$COFF" -o "$OUT/$name$EXE"
    else
        $CC $OPT $INC "$VER_VER" "$VER_BUILD" "$@" "$COFF" -o "$OUT/$name$EXE"
    fi
}

build c5xasm tools/c5xasm/src/c5xasm.c tools/c5xasm/src/operands.c
build c5xlnk tools/c5xlnk/src/c5xlnk.c
build c5xhex tools/c5xhex/src/c5xhex.c
build c5xdis tools/c5xdis/src/c5xdis.c

echo "done -> $OUT"
