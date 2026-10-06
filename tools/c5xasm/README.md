<p align="center">
  <img src="../../docs/cult-of-the-modem.png" alt="CULT OF THE MODEM - DSC" width="720">
</p>

<div align="center">
<pre>
        ___|    _  |               |       
    __| __ \ \  /   __|  _ \   _ \  |  __|   
   (      ) |`  \  |   (   | (   | |\__ \  
  \___|____/ _/\_\\__|\___/ \___/ _|____/  
======================================dSc==
</pre>
</div>

# c5xasm - native TMS320C5x assembler (TI-compatible)

A dependency-free, POSIX C99 assembler for the TMS320C5x, the modern replacement
for TI's `the reference assembler` (Code Generation Tools 7.02). It emits TI COFF objects
(`-coff`, v1/v2) byte-identical to the reference assembler, or a flat machine-word listing. Builds
unchanged on macOS (clang) and Linux (gcc).

Part of the c5xtools toolchain: `c5xasm` (assembler) -> `c5xlnk` (linker) ->
`c5xhex` (hex/ROM converter); `c5xdis` disassembles back.

## Usage

```
c5xasm input.asm [-o out] [-coff|-coff2] [-v50] [-q]
```

| Flag | Meaning |
| --- | --- |
| `-coff` / `-coff2` | write a COFF v1 / v2 relocatable object (default: a flat `P addr word` listing). |
| `-v50` | select the C5x (`'C50`) instruction set. |
| `-q` | quiet (suppress the identity banner). |

## What it does

The full C5x instruction set, driven by the shared table `src/c5x_isa.h`
(mnemonic -> opcode/mask/format class), with per-format-class operand encoding
(all addressing modes, next-ARP, short/long immediate selection, shift and
condition fields).

A precedence-climbing expression evaluator for operands and a multi-pass
symbol resolver that relaxes to a fixed point (forward references, span-
dependent short/long forms).

Sections, directives and macros; a COFF writer (headers, section data, symbol
table + string table for long names, relocations) shared with the linker via
`../c5xcoff/`.

The relocation kinds the reference assembler emits: `R_RELWORD`, `R_PARTLS7`, `R_PARTMS9`,
`R_RELBYTE`, `R_REL`, including the reference assembler's short->long auto-promotion.

## Tests

```
make && make test
```
`make test` is offline (no TI tools, no wine): a smoke check, the operand-range
strictness check, a metamorphic consistency check, and a differential-vs-the reference assembler
check against frozen golden objects. The golden (`../../test/c5xasm/golden/*.obj`,
one reference-assembler object per case) was captured once and committed; the
check assembles each case with c5xasm and confirms the object is byte-identical
to the reference assembler (timestamp masked), then verifies
`../../test/c5xasm/golden/SHA256SUMS`. The conformance corpus and the examples are
likewise confronted with frozen TI golden images (see `../../test/`).

To (re)capture the golden from the real TI assembler, set `C5X_TI_WRAPPER` to a wrapper
that runs the reference assembler (e.g. under wine) and run `make capture-golden`; `make test`
itself never needs it.

## License

MIT - (c) 2026 Grzegorz Worona <grzegorz@worona.pl>. See `LICENSE`.
