<p align="center">
  <img src="cult-of-the-modem.png" alt="CULT OF THE MODEM - DSC" width="720">
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

# Documentation

This directory holds the original Texas Instruments manuals that c5xtools is
built from. The build and cross-compile guide is
[`BUILDING.md`](https://github.com/wgrzegorz/c5xtools/blob/main/BUILDING.md) in
the repository root.

## Contents

| Path | What it is |
|------|------------|
| [`SPRU056D-tms320c5x-users-guide.pdf`](https://github.com/wgrzegorz/c5xtools/raw/main/docs/SPRU056D-tms320c5x-users-guide.pdf) | TI TMS320C5x User's Guide (see below). |
| [`SPRU018D-assembly-language-tools.pdf`](https://github.com/wgrzegorz/c5xtools/raw/main/docs/SPRU018D-assembly-language-tools.pdf) | TI Assembly Language Tools User's Guide (see below). |
| [`SPRZ113A-tms320c5x-users-guide-errata.pdf`](https://github.com/wgrzegorz/c5xtools/raw/main/docs/SPRZ113A-tms320c5x-users-guide-errata.pdf) | TI TMS320C5x User's Guide errata (corrections on top of SPRU056D). |

## Texas Instruments manuals

These two documents are the normative sources for everything c5xtools produces:
the instruction encodings, the COFF object format, and the hex record formats.
They are kept here as offline copies for convenience; both are Texas Instruments
copyrighted documents and are freely available from TI at the links below.

### SPRU056D - TMS320C5x User's Guide (1998)

The processor reference: the architecture and buses, the CPU (CALU, multiplier,
PLU and the auxiliary-register unit), the status and control registers, program
control, the six addressing modes, and the complete instruction set with a
per-instruction reference (chapter 6).

Download [the local copy](https://github.com/wgrzegorz/c5xtools/raw/main/docs/SPRU056D-tms320c5x-users-guide.pdf), or get it
from TI at https://www.ti.com/lit/ug/spru056d/spru056d.pdf

### SPRU018D - TMS320C1x/C2x/C2xx/C5x Assembly Language Tools User's Guide (1995)

The toolchain reference: the assembler source syntax, the directives and macro
language, sections and the linker command language (`MEMORY` / `SECTIONS`), and
the hex-conversion utility and its record formats. Its Appendix A gives the
byte-level COFF layout (file, section, symbol and relocation structures, plus the
relocation-type table A-9) that `lib/c5xcoff/` is built from.

Download [the local copy](https://github.com/wgrzegorz/c5xtools/raw/main/docs/SPRU018D-assembly-language-tools.pdf), or get it
from TI at https://www.ti.com/lit/ug/spru018d/spru018d.pdf

### SPRZ113A - TMS320C5x User's Guide Errata / Update (1998)

The published corrections to SPRU056D. It is small but normative: where the
errata changes a detail of the User's Guide, the errata is what holds.

Download [the local copy](https://github.com/wgrzegorz/c5xtools/raw/main/docs/SPRZ113A-tms320c5x-users-guide-errata.pdf), or get it
from TI at https://www.ti.com/lit/er/sprz113a/sprz113a.pdf

A plain-text explanation of all of the above, cross-checked against these
manuals, is in the project
[wiki](https://github.com/wgrzegorz/c5xtools/wiki); the full list of sources is
on its [References](https://github.com/wgrzegorz/c5xtools/wiki/References) page.

## License

MIT - (c) 2026 Grzegorz Worona <grzegorz@worona.pl>. See `LICENSE`.
