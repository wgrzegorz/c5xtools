<p align="center">
  <img src="../docs/cult-of-the-modem.png" alt="CULT OF THE MODEM - DSC" width="720">
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

# c5xtools examples

Real, documented TMS320C5x DSP programs, written in TI assembly style. They are
worth reading on their own, and they double as the package smoke test: `make
test` assembles and links each one with c5xtools and compares the full 16-bit
image, byte-for-byte, to a golden captured once from the original TI toolset
(CGT 7.02). The TI tools are not shipped here.

## Programs (`src/`)

| file          | what it is                                                        |
|---------------|-------------------------------------------------------------------|
| `fir.asm`     | 16-tap FIR filter (MACD/RPT, coefficients in program memory)      |
| `biquad.asm`  | second-order IIR section, Direct Form I                           |
| `goertzel.asm`| single-tone Goertzel energy detector                             |
| `dtmf.asm`    | complete DTMF dialer: sine-table generator + 8-filter Goertzel bank|
| `dftfilt.asm` | Fourier-based filter (DFT analyse -> shape -> IDFT) - the heavy stress test |
| `codec_loopback.asm` | CS4231 stereo-codec PIO digital loopback (uses `.include "cs4231.inc"`, IN/OUT, a circular buffer and ACCB) |
| `procinit.asm` | TMS320C50 processor initialization (vector table, S/A RAM, block clear) |
| `linconv.asm` | linear convolution via MACD/TBLW |
| `circconv.asm`| circular convolution (ROTATE via ACCB + BMAR-addressed BLDD) |

All are written in TI style (`.title`, `*`-banner blocks, aligned comments) and
assemble identically under both c5xasm and the reference assembler.

## Testing

These programs are exercised by the offline test suite under
[`../test/`](../test/). The package smoke test in
[`test/examples/`](../test/examples/) builds each program and compares it to its
golden image; the broader ISA conformance gate in
[`test/conformance/`](../test/conformance/) checks c5xasm against the golden
across every mnemonic and a matrix of operand forms. Run everything with `make
test` from the repository root; the goldens are pre-captured, so no TI tools or
wine are needed.

## License

MIT - (c) 2026 Grzegorz Worona <grzegorz@worona.pl>. See `LICENSE`.
