# test/lib.sh - shared helpers for the offline test scripts.
#
# Part of c5xtools (TMS320C5x toolchain). SPDX-License-Identifier: MIT
#
# Source it after setting up the shell, resolving the path relative to the
# script's own directory, for example:
#   here=$(cd "$(dirname "$0")" && pwd)
#   . "$here/../lib.sh"                # from test/<tool>/, test/examples/, test/conformance/
#   . "$here/lib.sh"                   # from test/ itself
#
# Everything here is plain POSIX sh: no bashisms, no external deps beyond the
# usual coreutils (dd, printf, cut) and sha256sum or shasum.

# Zero the 4-byte COFF timestamp (file-header bytes 4..7) so captured and
# rebuilt objects compare equal regardless of when they were produced.
mask_coff_ts() {            # <file>
    printf '\0\0\0\0' | dd of="$1" bs=1 seek=4 count=4 conv=notrunc 2>/dev/null
}

# Portable SHA-256: coreutils `sha256sum`, else BSD/macOS `shasum -a 256`.
sha256_line() {             # <file>  -> "HASH  name"
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"
    else shasum -a 256 "$1"; fi
}
sha256_hash() {             # <file>  -> HASH only
    sha256_line "$1" | cut -d' ' -f1
}

# Verify a SHA256SUMS manifest from the current directory. Returns 0 and prints
# a note (not a failure) when neither checker is installed.
sha256_check() {            # <manifest>
    if command -v sha256sum >/dev/null 2>&1; then sha256sum -c "$1" >/dev/null
    elif command -v shasum  >/dev/null 2>&1; then shasum -a 256 -c "$1" >/dev/null
    else echo "  (no sha256sum/shasum; skipping $1)"; return 0; fi
}
