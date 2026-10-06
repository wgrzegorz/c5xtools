# Contributing to c5xtools

Thank you for your interest in c5xtools. This is a small, single-maintainer
project, so the process here is deliberately light. Bug reports, reproducers and
focused patches are all welcome.

## Before you start

- For a security issue, do not open a public issue or pull request. Follow
  [SECURITY.md](SECURITY.md) instead.
- By participating you agree to the [Code of Conduct](CODE_OF_CONDUCT.md).
- Contributions are accepted under the project's MIT [LICENSE](LICENSE).

## Reporting a bug

Open an issue with the bug report template. The one thing that helps most is a
minimal reproducer: the smallest input file and the exact command line that
shows the problem. Please also include:

- the tool and its version banner (`c5xasm --version`, etc.),
- your platform (OS and compiler, or the prebuilt archive you downloaded),
- what you expected and what actually happened.

Output that is byte-wrong against the TI tools is a defect worth a report, even
when nothing crashes. If you can attach the TI reference bytes, all the better.

## Suggesting a change

Open an issue with the feature request template first, so the scope can be
agreed before you spend time on a patch. Keep in mind the deliberate limits:

- the assembler and disassembler target the C5x instruction set (with the
  C2x/C2xx-compatible subset); they are not meant to grow into other DSP
  families,
- the output must stay byte-compatible with the TI tools,
- the toolchain stays dependency-free and builds with a C99 compiler and make.

Cross-family support is unverified rather than refused; see the README if you
can help with period evaluation kits.

## Building and testing

```
make            # build the four tools
make test       # build, then run the full offline test suite
```

`make test` is the gate. It must pass before a change can be merged. See
[BUILDING.md](BUILDING.md) for make-free builds and cross-compilation.

When you change behaviour that the test suite checks, update the golden data in
the same commit (the per-tool `capture_golden.sh` / `capture.sh` scripts
regenerate it) and say in the pull request why the reference bytes changed.

## Pull requests

- Branch from `main` and keep each pull request to one logical change.
- Match the surrounding C style: C99, four-space indent, no external
  dependencies, no compiler warnings.
- All source and identifiers are in English and plain 7-bit ASCII.
- Make sure `make test` passes locally; CI runs the same gates, plus
  sanitizers, a reproducible-build check and a `-Werror` build.
- Describe what changed and how you verified it, using the pull request
  template.

## Questions

For anything that is not a bug or a proposed change, email
grzegorz@worona.pl.
