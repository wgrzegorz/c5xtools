---
name: Bug report
about: A crash, a wrong result, or output that does not match the TI tools
title: ''
labels: bug
assignees: ''
---

## What happened

A clear description of the problem. If the output is wrong rather than a crash,
say how it differs from what you expected (and from the TI tools, if known).

## Reproducer

The smallest input and the exact command line that shows it. Paste the input
inline if it is short, or attach it.

```
c5xasm fir.asm -coff -o fir.obj
```

## Expected vs actual

- Expected:
- Actual:

## Environment

- Tool and version (from `--version`):
- How you got it (prebuilt archive, or built from source with which compiler):
- OS and version:

## Anything else

Logs, a stack trace from a sanitizer build, or TI reference bytes if you have
them.
