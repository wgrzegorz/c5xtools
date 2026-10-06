# Changelog - c5xtools

A modern, dependency-free TMS320C5x toolchain, compatible with TI's
toolchain (the TI CGT tools).

## [1.0.3] - 2026-10-06

First public release. A dependency-free TMS320C5x toolchain in portable C99 -
assembler, linker, hex/ROM converter and disassembler - whose output is
byte-compatible with the TI tools, with an offline test suite, manual pages and
an attested multi-platform release pipeline. (Supersedes the internal 1.0.0-1.0.2
iterations; their history is merged here.)

### c5xasm (assembler, TI-compatible)
- Full C5x instruction set from a shared instruction table (`src/c5x_isa.h`),
  with per-format-class operand encoding (all addressing modes, next-ARP, short/
  long immediate selection, shift and condition fields).
- Precedence-climbing expression evaluator; multi-pass symbol resolution that
  relaxes to a fixed point (forward references, span-dependent forms).
- Sections, directives, macros; COFF v1/v2 writer (headers, section data,
  symbol + string table, relocations) byte-identical to the reference assembler.
- Relocation kinds: `R_RELWORD`, `R_PARTLS7`, `R_PARTMS9`, `R_RELBYTE`, `R_REL`,
  with the reference assembler's short->long auto-promotion.

### c5xlnk (linker, TI-compatible)
- Reads TI COFF relocatable objects; places sections; resolves symbols including
  cross-object externals and libraries (`-l`/`-u`/`-x`/`-i`).
- Linker command files: `MEMORY`/`SECTIONS`, PAGE 0/1, `GROUP`/`UNION`,
  load!=run, `NOLOAD`/`COPY`/`DSECT`, symbol assignment and expressions.
- Data-driven relocation application per kind; writes the COFF `.out` executable
  (optional header, `F_EXEC`, sizes in words) and a flat image / map.

### c5xhex (hex/ROM converter, TI-compatible)
- COFF `.out` -> Intel-HEX, Motorola-S, TI-tagged, ASCII, or a full binary image.
- `-image` ROM fill over a range (`-org`/`-len`/`-fill`), `-romwidth`/`-order`
  byte control. Intel-HEX and Motorola-S byte-identical to the reference hex tool.

### c5xdis (disassembler)
- Decoder is the exact inverse of the shared instruction table, so its output
  re-assembles byte-exact (`c5xdis -> c5xasm -> cmp`).
- `--trace` analysis: recursive descent with a fixpoint, call graph with
  per-function xref/calls/refs, code/data separation, DP tracking, computed-
  dispatch resolution (constant and range-checked jump-table idioms), basic
  blocks, and symbolization from an external symbol file.

### Shared / infrastructure
- `c5xcoff/` - one COFF format module (record layout + primitives) shared by the
  assembler writer, the linker reader/writer and the hex reader, so they cannot
  drift.
- Correctness harness, pure POSIX shell: an ISA conformance corpus, operand-range
  strictness checks, an oracle-free metamorphic relation, a COFF-reader
  robustness fuzzer (memory safety on malformed/truncated input, under
  ASan/UBSan), and frozen TI golden objects/images with `SHA256SUMS` manifests
  confronted offline at every stage.
- Portable C99 (`portable.h`) targeting Linux/Unix/macOS, Windows and DOS, built
  with the Makefile or the make-free build scripts (`scripts/build.sh`,
  `scripts/build.bat`).

### Build, CI and release
- Each platform is built on its own native GitHub runner - Linux x86-64 and arm64
  (musl, static), macOS universal (arm64 + x86-64), Windows 64/32-bit (MSVC,
  static CRT) - with 32-bit DOS cross-built with DJGPP, and every archive is
  published with a signed build-provenance attestation.
- CI runs the conformance suite on Linux/macOS (gcc/clang) and FreeBSD, plus a
  functional round-trip and byte-exact cross-platform determinism over every
  example and hex format, disassembler fidelity, an AddressSanitizer + UBSan run
  of the suite, a reproducible-build check and a bug-class `-Werror` build.
- Clean version stamping: `VERSION` is the single source, injected into every
  tool's banner with the build date; a release build at the exact tag carries no
  extra git suffix.

## License

MIT - (c) 2026 Grzegorz Worona <grzegorz@worona.pl>. See `LICENSE`.
