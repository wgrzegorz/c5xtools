# common.mk - shared build settings for every c5xtools tool.
#
# Each tool's Makefile does `include ../../common.mk` and then lists its own
# sources; everything portable (compiler, warnings, hardening, include paths,
# the version stamp, install rules, automatic header dependencies) lives here so
# the four tools cannot drift.
#
# Part of c5xtools (TMS320C5x toolchain). SPDX-License-Identifier: MIT
# Copyright (c) 2026 Grzegorz Worona <grzegorz@worona.pl>

# Repo root = the directory this file lives in (robust regardless of the caller).
ROOT := $(dir $(lastword $(MAKEFILE_LIST)))

# This file is included first and defines concrete object targets below, so pin
# the default goal to each tool's `all` rather than the first object rule.
.DEFAULT_GOAL := all

# Toolchain. All overridable from the environment / command line.
CC      ?= cc
CSTD    ?= -std=c99
WARN    ?= -Wall -Wextra -Wformat=2 -Wformat-security -Wshadow -Wpointer-arith
OPT     ?= -O2
# Release hardening (safe, portable; silently ignored where unsupported).
HARDEN  ?= -D_FORTIFY_SOURCE=2 -fstack-protector-strong
CFLAGS  ?= $(CSTD) $(WARN) $(OPT) $(HARDEN)

# Shared headers (include/) and the COFF library (lib/c5xcoff/).
CPPFLAGS += -I$(ROOT)include -I$(ROOT)lib/c5xcoff
# Automatic header-dependency tracking: no hand-maintained prerequisite lists.
DEPFLAGS  = -MMD -MP

# Version/build stamp (single source: VERSION). Reproducible: when
# SOURCE_DATE_EPOCH is set the date is derived from it, not the wall clock.
VERSION := $(shell cat $(ROOT)VERSION 2>/dev/null || echo 0.0.0)
ifeq ($(strip $(SOURCE_DATE_EPOCH)),)
BUILD := $(shell date -u +%Y-%m-%d)
else
BUILD := $(shell date -u -d @$(SOURCE_DATE_EPOCH) +%Y-%m-%d 2>/dev/null \
                 || date -u -r $(SOURCE_DATE_EPOCH) +%Y-%m-%d 2>/dev/null \
                 || echo reproducible)
endif
GITDESC  := $(shell git -C $(ROOT) describe --tags --always --dirty --abbrev=8 2>/dev/null)
VERFLAGS := -DC5XTOOLS_VERSION='"$(VERSION)"' -DC5XTOOLS_BUILD='"$(BUILD)"'
# Append the git description only for non-release builds. At the exact release
# tag (git describe == v<VERSION>) the version stamp is already the identity,
# so the banner stays clean: "<tool> X.Y.Z build <date>".
ifneq ($(GITDESC),)
ifneq ($(GITDESC),v$(VERSION))
VERFLAGS += -DC5XTOOLS_GIT='" +g$(GITDESC)"'
endif
endif

# Sanitizer flags for the robustness fuzzers (opt-in `make asan`).
ASANFLAGS ?= $(CSTD) -Wall -Wextra -g -O1 \
             -fsanitize=address,undefined -fno-omit-frame-pointer

# Install locations (GNU-style, DESTDIR-aware).
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
INSTALL ?= install

# Cross-compiling. Set CC to a cross toolchain (e.g. x86_64-w64-mingw32-gcc for
# Windows, i586-pc-msdosdjgpp-gcc / OpenWatcom for 32-bit DOS). EXEEXT=.exe adds
# the executable suffix those targets use; LDFLAGS=-static makes a standalone
# binary. All default to empty, so a native build is unchanged.
EXEEXT  ?=
LDFLAGS ?=

# Per-tool object directory, so the shared c5xcoff.c never collides between tools.
OBJDIR ?= build

# Compile any tool source or the shared COFF source into $(OBJDIR).
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS) $(VERFLAGS) -c $< -o $@
$(OBJDIR)/c5xcoff.o: $(ROOT)lib/c5xcoff/c5xcoff.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEPFLAGS) $(VERFLAGS) -c $< -o $@

$(OBJDIR):
	@mkdir -p $@
