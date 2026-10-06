## Summary

What this changes and why.

## Related issue

Closes #... (open a feature request first for anything non-trivial).

## How it was verified

- [ ] `make test` passes locally.
- [ ] If behaviour that the test suite checks changed, the golden data was
      regenerated in this same change, and the reason the reference bytes
      changed is explained above.
- [ ] Builds clean with no new compiler warnings.

## Checklist

- [ ] One logical change, branched from `main`.
- [ ] C99, no new build dependency, output stays byte-compatible with the TI
      tools.
- [ ] Source and identifiers are English and plain 7-bit ASCII.
