# c5xlnk - native TMS320C5x COFF linker (TI-compatible).
include ../../common.mk

TARGET := c5xlnk
BIN    := $(TARGET)$(EXEEXT)
OBJS   := $(OBJDIR)/c5xlnk.o $(OBJDIR)/c5xcoff.o
DEPS   := $(OBJS:.o=.d)

.PHONY: all asan test capture-golden install clean
all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# Sanitizer build (ASan+UBSan) of the object/archive reader, for the fuzzer
# (test/fuzz_reader.sh, driven by the package-level test/run-tests.sh).
asan: $(TARGET).asan
$(TARGET).asan: src/c5xlnk.c $(ROOT)lib/c5xcoff/c5xcoff.c
	$(CC) $(ASANFLAGS) $(CPPFLAGS) $(VERFLAGS) -o $@ src/c5xlnk.c $(ROOT)lib/c5xcoff/c5xcoff.c

# Offline gate (no wine, no python): confront c5xasm/c5xlnk against the frozen golden at both stages (pre-link .obj sha256, post-link flat image).
test: $(BIN)
	@sh ../../test/c5xlnk/check_golden.sh

# One-time golden refresh with the real TI tools (needs wine); commit the result.
capture-golden: $(BIN)
	@sh ../../test/c5xlnk/capture_golden.sh

install: $(BIN)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

clean:
	rm -rf $(TARGET) $(TARGET).exe $(TARGET).asan $(TARGET).asan.dSYM $(OBJDIR)

-include $(DEPS)
