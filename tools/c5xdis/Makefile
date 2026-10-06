# c5xdis - native TMS320C5x disassembler (re-assembles byte-exact via c5xasm).
include ../../common.mk

TARGET := c5xdis
BIN    := $(TARGET)$(EXEEXT)
OBJS   := $(OBJDIR)/c5xdis.o
DEPS   := $(OBJS:.o=.d)

.PHONY: all test install clean
all: $(BIN)

# The decoder reuses the shared ISA table (include/c5x_isa.h) - single source
# of the instruction set for the assembler and the disassembler alike.
$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# Round-trip gate: disassemble a fixture's image and re-assemble it byte-exact.
test: $(BIN)
	@sh ../../test/c5xdis/roundtrip.sh ../../test/c5xdis/fixtures/sample.asm
	@sh ../../test/c5xdis/roundtrip_corpus.sh

install: $(BIN)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

clean:
	rm -rf $(TARGET) $(TARGET).exe $(OBJDIR)

-include $(DEPS)
