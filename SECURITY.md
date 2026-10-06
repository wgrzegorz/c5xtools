# Security Policy

## Reporting a vulnerability

Please report security vulnerabilities privately rather than in a public issue.

- Preferred: use GitHub's private vulnerability reporting. Open the repository's
  "Security" tab and choose "Report a vulnerability".
- Or email grzegorz@worona.pl.

Please include the affected version (the banner line from `--version`), the
platform, and the smallest input and steps that reproduce the problem. You will
get an acknowledgement, and a fix or an explanation, as quickly as is practical
for a single-maintainer project.

Do not open a public issue or pull request for a vulnerability until a fix is
released.

## Scope

c5xtools is a set of command-line tools that read and write files (assembly
source, TI COFF objects, hex/ROM records). The most relevant class of issue is a
malformed input file that causes a crash or memory-safety error in a reader. The
test suite fuzzes the COFF reader under AddressSanitizer and UBSan, but reports
of anything it misses are welcome.

## Supported versions

Security fixes are made against the latest release.

| Version | Supported          |
|---------|--------------------|
| 1.0.x   | yes                |
| < 1.0   | no                 |
