# c5xasm - native TMS320C5x assembler (TI-compatible).
include ../../common.mk

TARGET := c5xasm
BIN    := $(TARGET)$(EXEEXT)
OBJS   := $(OBJDIR)/c5xasm.o $(OBJDIR)/operands.o $(OBJDIR)/c5xcoff.o
DEPS   := $(OBJS:.o=.d)

.PHONY: all test capture-golden install clean
all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# Offline suite (no wine / TI tools): seed smoke, operand-range strictness,
# a metamorphic consistency check, and the frozen golden compare.
test: $(BIN)
	@sh ../../test/c5xasm/run.sh
	@sh ../../test/c5xasm/difftest_operand_range.sh
	@sh ../../test/c5xasm/metamorphic.sh
	@sh ../../test/c5xasm/check_golden.sh

# One-time: (re)capture the golden objects. Needs the reference assembler via
# $C5X_TI_WRAPPER (e.g. a wine wrapper); `test` never does.
capture-golden: $(BIN)
	@sh ../../test/c5xasm/capture_golden.sh

install: $(BIN)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

clean:
	rm -rf $(TARGET) $(TARGET).exe $(OBJDIR)

-include $(DEPS)
