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

# c5xdis - TMS320C5x disassembler

Decodes a raw little-endian 16-bit DSP image into c5xasm-compatible source. The
decoder is the inverse of c5xasm's encoder, driven by the same instruction table
(`../c5xasm/src/c5x_isa.h`), so the output re-assembles byte-exact - the
round-trip `c5xdis -> c5xasm -> cmp` is the correctness oracle (`make test`).

```
c5xdis image.bin [start [count]] [--base ADDR] [--trace] [--entry ADDR]... [--symbols FILE] [--addr] [-o out.asm]
```

Two modes are available. The default is a linear sweep with numeric operands,
round-trip-safe on any blob (`--addr` adds a trailing `; ADDR: HEX` comment).
The `--trace` mode performs recursive-descent analysis from the entry points
(default: the base) plus every explicit in-image branch/call target. It
separates code from data (unreached words become `.word`), assigns `L_ADDR`
labels and uses them in branch operands, sets the origin with `.ps`, lays the
source out in columns with a `; ADDR: HEX` comment, and marks every uncertainty
inline while streaming the detail to stderr:

Marker | Meaning
--- | ---
`@UNRESOLVED` | computed target (branch/call via ACC), flow not followed
`@EXT` | branch/call target outside the image
`@SUSPECT` | reached as code but not decodable (e.g. an indirect branch/call the decoder does not yet cover, or data reached through a false seed)

The output still re-assembles byte-exact (labels resolve against `.ps`).

Each function gets a header block with its callers (`xref`), the functions it
calls (`calls`), and the static data it touches (`refs`: table pointers of
MAC/BLKD/BLPD, I/O ports, named MMRs) - all derived mechanically from the code.
Instruction glosses are a literal reading of the mnemonic (WHAT), never inferred
intent (WHY).

Case-specific knowledge stays out of the tool: `--symbols FILE` reads
`<hexaddr> <name>` lines (and `entry <hexaddr>`) and the engine then prints those
names in place of `sub_`/`loc_`, in labels, operands and headers. `data <hexaddr>
<name>` names a data-memory location: where DP tracking resolves a direct access
to that full address, the `D:` note shows the name (operand stays numeric) - the usual
split between a symbol database and the disassembler engine. Nothing about any
particular firmware is compiled in; bases, entries and names are all inputs.

This is the instruction-model layer (like a Ghidra SLEIGH / IDA processor
module). It decodes the common C5x format classes and emits everything else as
`.word`, so correctness never depends on the decoder's completeness. Validated
byte-exact on a real-world TMS320C5x firmware image (resident + overlays,
~85% decoded; `make test`, which also round-trips a self-contained fixture).

The analysis passes that make it "smart" run in `--trace` and layer on top of
the decoder without ever weakening the byte-exact guarantee: recursive-descent
traversal with a fixpoint, a call graph with per-function xref/calls/refs, a
code/data cross-reference, DP tracking with direct-access resolution,
computed-dispatch resolution (the SPLK-constant idiom and the range-checked
`SUBK`/`ADLK`/`TBLR`/`BACC` jump-table - the latter recovers the handlers a tag
dispatcher reaches only through its table), basic-block structure (each function
split into blocks at branches/returns, counted in its header and separated by a
blank line in the listing), and symbolization from an external symbol file. The
default (no `--trace`) is a linear sweep; data interleaved with code is then
decoded as plausible instructions (byte-exact, but not semantically separated).

Part of c5xtools. MIT - (c) 2026 Grzegorz Worona <grzegorz@worona.pl>.

## License

MIT - (c) 2026 Grzegorz Worona <grzegorz@worona.pl>. See `LICENSE`.
