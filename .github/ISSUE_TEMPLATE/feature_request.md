---
name: Feature request
about: Suggest an improvement or a new capability
title: ''
labels: enhancement
assignees: ''
---

## What you want

A clear description of the change and the problem it solves.

## Why

The use case. What are you trying to do that is awkward or impossible today?

## Scope check

Please confirm the change fits the project's deliberate limits:

- [ ] It keeps the output byte-compatible with the TI tools.
- [ ] It does not add a build dependency (C99 plus make stays enough).
- [ ] For the assembler/disassembler, it stays within the C5x instruction set
      (the C2x/C2xx-compatible subset included), rather than targeting another
      DSP family.

If your request is cross-family support, note that this is unverified rather
than refused; see the README about donating period evaluation kits.

## Alternatives

Anything you considered, or current workarounds.
