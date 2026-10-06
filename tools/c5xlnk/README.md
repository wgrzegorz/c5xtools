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

# c5xlnk - native TMS320C5x COFF linker (TI-compatible)

A dependency-free, POSIX C99 linker for TI COFF v2 relocatable objects (as produced
by `c5xasm` or the reference assembler), the modern replacement for TI's `the reference linker.EXE`. It places
sections, resolves symbols (including cross-object externals by name), applies
relocations, and writes a linked image plus a map.

Part of the c5xtools toolchain: `c5xasm` (assembler) -> `c5xlnk` (linker) ->
`c5xhex` (hex/ROM converter); `c5xdis` disassembles back.

## Usage

```
c5xlnk obj... [-o out] [-m map] [-text ADDR] [-data ADDR] [-bss ADDR]
```

The default memory map mirrors the reference linker: PAGE 0 PROG origin `0x0800` (holds `.text`
then `.data`, contiguous), PAGE 1 DATA origin `0x0800` (holds `.bss`). The `-text/-data/-bss`
flags override the origins. Output (`-o`, else stdout) is currently the flat linked image
`PAGE AAAA WWWW`; `-m` writes a map (memory config, section allocation, global symbols).

## What it does (reversed from the reference linker)

It reads TI COFF v2 objects (magic `0x00c2`, 22-byte header + target id `0x0092`,
48-byte section headers, 10-byte relocation entries `{vaddr, symndx, type}`,
18-byte symbols + aux). COFF v1 objects (`0x00c1`) are also accepted.

It places `.text`/`.data` in PROG and `.bss` in DATA, concatenating like-named
input sections across objects in link order.

It builds a global symbol table from every defined symbol (`n_scnum > 0`); each
symbol's final address = its section's placed base + its section-relative value.

It applies relocations: a `symndx == 0xFFFFFFFF` entry relocates by the containing
section's base; a symbol entry relocates by that symbol's final address, resolving
undefined externals by name across objects. The stored word is section-relative
and the relocation base is added in place.

## Validation

`make test` is offline (no wine, no python): it confronts c5xtools with frozen
the reference tools golden at both stages - the pre-link object (sha256, timestamp
masked) and the post-link linked image. Covered:

`test/reloc_sample.asm` - a self-relative branch (`B start`): `7980 0800` after
relocation (section placed at 0x0800).

`test/xref_a.asm` + `test/xref_b.asm` - a cross-object `CALL sub1` resolved to the
definition's final address (`7a80 0803`).

Both link byte-identical to the reference linker.

## Scope (v1.0) and next steps

Implemented: COFF v2 parse, default + overridable allocation, section-relative and
symbol (incl. cross-object) relocations, flat image + map.

Next: a linker command file (`MEMORY`/`SECTIONS`), a full COFF `.out` writer
(executable: optional header + combined sections + merged symbol table), line-number
handling, and `-s` symbol stripping. c5xasm must also emit relocations for symbol
references so its own objects relink (today c5xasm resolves symbols absolutely);
that assembler-side change pairs with this linker.

## License

MIT - Copyright (c) 2026 Grzegorz Worona <grzegorz@worona.pl>. See `LICENSE`.
