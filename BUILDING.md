# Building c5xtools

c5xtools is deliberately dependency-light: four small C99 programs that share a
COFF module and a set of headers. This document covers building natively on each
platform, picking a compiler, cross-compiling, and the knobs you can turn.

## Requirements

To build the tools:

- A C99 compiler - `cc`, `clang` or `gcc`.
- `make` (GNU make or BSD make).

That is all. The code is ISO C99 plus the C standard library; there are no
third-party libraries and no `-l<lib>` link flags. `include/portable.h` supplies
`strcasecmp` / `strncasecmp` / `strdup` where a platform lacks them, so even the
common POSIX string helpers are self-contained.

To run the test suite (`make test`):

- A POSIX shell and the usual utilities only - `sh`, `awk`, `od`, `dd`, `cmp`,
  `mktemp`, `tr`, `grep`, and `sha256sum` or `shasum` (the suite auto-detects
  which is present). No python, no third-party tools.

Optional, only to *re-capture* the golden reference files from the original TI
tools (never needed for a normal build or `make test`):

- `wine` plus the TI Code Generation Tools 7.02, reached through a wrapper
  script named in `$C5X_TI_WRAPPER`.

## Build

```sh
make                 # build all four tools
make -C tools/c5xasm # build one tool
make test            # build, then run the offline suite (test/run-tests.sh)
make clean           # remove build artifacts
```

Each binary is produced in its own directory, e.g. `tools/c5xasm/c5xasm`.
Objects and auto-generated header dependencies live under `tools/<tool>/build/`.

## Install

```sh
make install                 # into /usr/local/bin
make install PREFIX=/opt     # binaries into /opt/bin, man pages into /opt/share/man/man1
make install DESTDIR=/tmp/pkg PREFIX=/usr   # staged (packaging)
```

## Building natively on each platform

The same sources build unchanged wherever a C99 compiler exists. In every case
the build is just `make`, and `make test` runs the offline suite against the
binaries you just built.

### Linux

Install a compiler and make, then build:

```sh
# Debian / Ubuntu
sudo apt-get install -y build-essential      # gcc + make  (or: clang)
# Fedora
sudo dnf install -y gcc make                 # or: clang

make
make test
```

### macOS

The compiler is clang, from the Xcode Command Line Tools
(`xcode-select --install`). Nothing else is needed:

```sh
make
make test
```

A universal (arm64 + x86-64) binary:

```sh
make CFLAGS="-std=c99 -O2 -arch arm64 -arch x86_64"
```

### Windows (MSYS2 + MinGW-w64)

The native way to build on Windows is [MSYS2](https://www.msys2.org/). Install
it, open the **MSYS2 MinGW x64** shell, and install the toolchain:

```sh
pacman -S --needed make mingw-w64-x86_64-gcc
make
make test
```

That produces native Windows `.exe` files (MinGW-w64, no extra DLLs). For 32-bit
binaries, use the **MSYS2 MinGW x86** shell with `mingw-w64-i686-gcc`. Cygwin and
WSL work too; under WSL you simply follow the Linux instructions above.

### Other Unix (BSD, illumos, ...)

```sh
make        # or: gmake, if the system make is not GNU make
make test
```

## Choosing a compiler

Set `CC` to pick the compiler:

```sh
make CC=gcc
make CC=clang
```

The Makefile uses a gcc/clang-style command line (`-std=c99`, `-Wall`, `-c -o`,
`-MMD -MP`), so any driver with that convention works through `CC=`: the system
`gcc` or `clang`, a MinGW-w64 `gcc`, a musl `gcc`, or a DJGPP `gcc`.

For a compiler that does not speak gcc/clang flags (for example MSVC), skip the
Makefile and compile the sources directly. Each tool is its own `src/*.c` plus
the shared `lib/c5xcoff/c5xcoff.c`, built with `-Iinclude -Ilib/c5xcoff`:

| Tool     | Sources |
|----------|---------|
| `c5xasm` | `tools/c5xasm/src/c5xasm.c tools/c5xasm/src/operands.c lib/c5xcoff/c5xcoff.c` |
| `c5xlnk` | `tools/c5xlnk/src/c5xlnk.c lib/c5xcoff/c5xcoff.c` |
| `c5xhex` | `tools/c5xhex/src/c5xhex.c lib/c5xcoff/c5xcoff.c` |
| `c5xdis` | `tools/c5xdis/src/c5xdis.c lib/c5xcoff/c5xcoff.c` |

With MSVC, from a Developer Command Prompt. Pass `/std:c11`: the sources are C99
(declarations in a `for` initializer, mixed declarations and statements), which
the default MSVC C mode (C89) rejects.

```bat
cl /std:c11 /I include /I lib\c5xcoff tools\c5xasm\src\c5xasm.c tools\c5xasm\src\operands.c lib\c5xcoff\c5xcoff.c /Fe:c5xasm.exe
```

### Compiling all four without make (no tests)

If you do not have (or do not want) make, compile each tool directly. This is
packaged as two ready scripts that build all four programs, run no tests, and
are exercised in CI on Linux, macOS and Windows (see
`.github/workflows/build-scripts.yml`). Run one from the top of the source tree:

```sh
sh scripts/build.sh          # Unix / macOS / MinGW; honors CC, OPT, EXE, OUT
```

```bat
scripts\build.bat            :: Windows; uses cl (MSVC), else gcc (MinGW)
```

The exact compiler commands the scripts run are below, if you would rather drive
the compiler yourself.

Linux, macOS or any other Unix (`cc`, `gcc` or `clang`):

```sh
INC="-Iinclude -Ilib/c5xcoff"
OPT="-std=c99 -O2"
cc $OPT $INC tools/c5xasm/src/c5xasm.c tools/c5xasm/src/operands.c lib/c5xcoff/c5xcoff.c -o c5xasm
cc $OPT $INC tools/c5xlnk/src/c5xlnk.c lib/c5xcoff/c5xcoff.c -o c5xlnk
cc $OPT $INC tools/c5xhex/src/c5xhex.c lib/c5xcoff/c5xcoff.c -o c5xhex
cc $OPT $INC tools/c5xdis/src/c5xdis.c lib/c5xcoff/c5xcoff.c -o c5xdis
```

Windows with MSVC, from a Developer Command Prompt (produces `.exe` files):

```bat
set INC=/I include /I lib\c5xcoff
cl /std:c11 /O2 %INC% tools\c5xasm\src\c5xasm.c tools\c5xasm\src\operands.c lib\c5xcoff\c5xcoff.c /Fe:c5xasm.exe
cl /std:c11 /O2 %INC% tools\c5xlnk\src\c5xlnk.c lib\c5xcoff\c5xcoff.c /Fe:c5xlnk.exe
cl /std:c11 /O2 %INC% tools\c5xhex\src\c5xhex.c lib\c5xcoff\c5xcoff.c /Fe:c5xhex.exe
cl /std:c11 /O2 %INC% tools\c5xdis\src\c5xdis.c lib\c5xcoff\c5xcoff.c /Fe:c5xdis.exe
```

Windows with MinGW-w64 but without make, using `gcc` from `cmd` (its `bin`
directory on `PATH`):

```bat
gcc -std=c99 -O2 -Iinclude -Ilib\c5xcoff tools\c5xasm\src\c5xasm.c tools\c5xasm\src\operands.c lib\c5xcoff\c5xcoff.c -o c5xasm.exe
gcc -std=c99 -O2 -Iinclude -Ilib\c5xcoff tools\c5xlnk\src\c5xlnk.c lib\c5xcoff\c5xcoff.c -o c5xlnk.exe
gcc -std=c99 -O2 -Iinclude -Ilib\c5xcoff tools\c5xhex\src\c5xhex.c lib\c5xcoff\c5xcoff.c -o c5xhex.exe
gcc -std=c99 -O2 -Iinclude -Ilib\c5xcoff tools\c5xdis\src\c5xdis.c lib\c5xcoff\c5xcoff.c -o c5xdis.exe
```

The version banner falls back to a built-in default when `C5XTOOLS_VERSION` is
not defined, so no `-D` flags are required for a direct build.

## Overridable variables

Set on the command line or in the environment (see `common.mk`):

| Variable            | Default                                   | Purpose                              |
|---------------------|-------------------------------------------|--------------------------------------|
| `CC`                | `cc`                                      | C compiler                           |
| `CFLAGS`            | `-std=c99 -O2 -Wall -Wextra ...` + hardening| compile flags                        |
| `OPT`               | `-O2`                                     | optimization level                   |
| `HARDEN`            | `-D_FORTIFY_SOURCE=2 -fstack-protector-strong` | release hardening               |
| `PREFIX`            | `/usr/local`                              | install prefix                       |
| `BINDIR`            | `$(PREFIX)/bin`                           | install location for binaries        |
| `DESTDIR`           | (empty)                                   | staged-install root (packaging)      |
| `SOURCE_DATE_EPOCH` | (empty)                                   | set for a reproducible build stamp   |
| `LDFLAGS`           | (empty)                                   | extra link flags (e.g. `-static`)    |
| `EXEEXT`            | (empty)                                   | executable suffix (`.exe` for Windows/DOS) |

A reproducible release build with gcc:

```sh
make CC=gcc SOURCE_DATE_EPOCH=$(git log -1 --format=%ct)
```

## Cross-compiling and portable binaries

The prebuilt release archives are produced by cross-compiling on a Linux host
(that is what the release workflow uses); the same recipes work from macOS. The
pattern is always: pick a cross `CC`, add `EXEEXT=.exe` for Windows/DOS, and
`LDFLAGS=-static` for a self-contained binary.

A small helper that builds one target and gathers the binaries into
`dist/<name>/` (run from the top of the tree):

```sh
pkg() {                      # usage: pkg <name> CC=... [EXEEXT=.exe] [LDFLAGS=...]
  name=$1; shift
  make clean
  make all "$@"
  mkdir -p "dist/$name"
  cp tools/*/c5x* "dist/$name/" 2>/dev/null || true   # c5x<tool> or c5x<tool>.exe
  make clean
}
```

`make test` runs the host's binaries, so run it only on a native build; a cross
build just produces the target binaries. `dist/` is a build output (git-ignored).

### Static Linux (x86-64 and arm64)

```sh
# Debian / Ubuntu host
sudo apt-get install -y musl-tools gcc-aarch64-linux-gnu libc6-dev-arm64-cross

pkg linux-x86_64 CC=musl-gcc              "LDFLAGS=-static -s"
pkg linux-arm64  CC=aarch64-linux-gnu-gcc "LDFLAGS=-static -s"
```

musl gives a cleanly self-contained `-static` binary for x86-64; the arm64
archive uses the glibc cross toolchain with `-static`. On macOS a musl cross
toolchain is available with `brew install FiloSottile/musl-cross/musl-cross`
(add `--with-aarch64` for arm64).

### Windows 32-bit and 64-bit (MinGW-w64 cross)

```sh
# Debian / Ubuntu host
sudo apt-get install -y gcc-mingw-w64-x86-64 gcc-mingw-w64-i686
# macOS host: brew install mingw-w64

pkg win64 CC=x86_64-w64-mingw32-gcc EXEEXT=.exe "LDFLAGS=-static -s"
pkg win32 CC=i686-w64-mingw32-gcc   EXEEXT=.exe "LDFLAGS=-static -s"
```

The results are standalone `.exe` files (no runtime DLLs) that run on Windows
from `cmd` or PowerShell, e.g. `c5xasm.exe fir.asm -coff -o fir.obj`.

### 32-bit DOS (DJGPP)

Produces a 32-bit protected-mode DOS executable; it runs under a DPMI host (for
example CWSDPMI.EXE in bare DOS, or a DPMI environment such as DOSBox or a
Windows 9x DOS box):

```sh
pkg dos32 CC=i586-pc-msdosdjgpp-gcc EXEEXT=.exe HARDEN=
```

A ready-made DJGPP cross compiler is available from the
[build-djgpp](https://github.com/andrewwutw/build-djgpp) releases (extract it
and put its `bin/` on `PATH`). `HARDEN=` drops the stack protector and
`_FORTIFY_SOURCE`, which the DJGPP C library does not provide. 16-bit real-mode
DOS is not supported: reading an object file needs a flat 32-bit address space,
so a 32-bit DOS target (DJGPP, or an OpenWatcom DOS/4GW build) is required.

## License

MIT - (c) 2026 Grzegorz Worona <grzegorz@worona.pl>. See `LICENSE`.
