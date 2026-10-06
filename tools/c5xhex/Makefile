# c5xhex - native TMS320C5x COFF -> hex/ROM converter (TI-compatible).
include ../../common.mk

TARGET := c5xhex
BIN    := $(TARGET)$(EXEEXT)
OBJS   := $(OBJDIR)/c5xhex.o $(OBJDIR)/c5xcoff.o
DEPS   := $(OBJS:.o=.d)

.PHONY: all asan test capture-golden install clean
all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# Sanitizer build (ASan+UBSan) of the COFF reader, for the fuzzer
# (test/fuzz_reader.sh, driven by the package-level test/run-tests.sh).
asan: $(TARGET).asan
$(TARGET).asan: src/c5xhex.c $(ROOT)lib/c5xcoff/c5xcoff.c
	$(CC) $(ASANFLAGS) $(CPPFLAGS) $(VERFLAGS) -o $@ src/c5xhex.c $(ROOT)lib/c5xcoff/c5xcoff.c

# Offline gate (no wine, no python): confront c5xhex against the frozen golden
# Intel/Motorola-S golden (plain + image fill) + a binary round-trip self-check.
test: $(BIN)
	@sh ../../test/c5xhex/check_golden.sh

# One-time golden refresh with the real TI hex tool (needs wine); commit the result.
capture-golden: $(BIN)
	@sh ../../test/c5xhex/capture_golden.sh

install: $(BIN)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

clean:
	rm -rf $(TARGET) $(TARGET).exe $(TARGET).asan $(TARGET).asan.dSYM $(OBJDIR)

-include $(DEPS)
