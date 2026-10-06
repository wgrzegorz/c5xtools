# c5xtools - top-level build orchestrator for the TMS320C5x toolchain.
#
# `make`          build every tool into tools/<tool>/<tool>
# `make test`     build, then run the full offline test suite (test/run-tests.sh)
# `make install`  install the four binaries into $(DESTDIR)$(PREFIX)/bin and
#                 the manual pages into $(DESTDIR)$(MANDIR)/man1
# `make clean`    remove every tool's build artifacts
#
# Per-tool settings (compiler, flags, version stamp, install dirs) live in
# common.mk; override from the environment, e.g. `make CC=gcc PREFIX=/opt`.
#
# Part of c5xtools. SPDX-License-Identifier: MIT
# Copyright (c) 2026 Grzegorz Worona <grzegorz@worona.pl>

TOOLS := c5xasm c5xlnk c5xhex c5xdis

# Install locations (GNU-style, DESTDIR-aware). Override, e.g. PREFIX=/opt.
PREFIX  ?= /usr/local
MANDIR  ?= $(PREFIX)/share/man
MAN1DIR ?= $(MANDIR)/man1
INSTALL ?= install

.PHONY: all $(TOOLS) test install install-man clean
.DEFAULT_GOAL := all

all: $(TOOLS)

$(TOOLS):
	@$(MAKE) --no-print-directory -C tools/$@

# The single test entry point: builds (if needed) and runs every gate, offline.
test: all
	@sh test/run-tests.sh

install: all install-man
	@for t in $(TOOLS); do $(MAKE) --no-print-directory -C tools/$$t install; done

# Section 1 manual pages into $(MAN1DIR); man-db reads them uncompressed.
install-man:
	@$(INSTALL) -d $(DESTDIR)$(MAN1DIR)
	@for m in man/*.1; do $(INSTALL) -m 0644 $$m $(DESTDIR)$(MAN1DIR)/; done
	@echo "installed man pages -> $(DESTDIR)$(MAN1DIR)"

clean:
	@for t in $(TOOLS); do $(MAKE) --no-print-directory -C tools/$$t clean; done
