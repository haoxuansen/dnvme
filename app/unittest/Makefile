RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

.DEFAULT_GOAL := build

pre:
	$(Q)bash $(CUR_DIR_PATH)/prepare.sh
	$(Q)bash $(CUR_DIR_PATH)/lds.sh

$(TARGET): $(OBJS)
	$(Q)$(CC) $(LDFLAGS) $^ $(LDLIBS) -T test.lds -Wl,-Map,$(TARGET).map -o $@

$(TARGET).asm: $(TARGET)
	@echo "  OBJDUMP \t $@"
	$(Q)$(OBJDUMP) -S $(TARGET) > $(TARGET).asm

build: pre $(TARGET) $(TARGET).asm
ifneq ($(DIR_EXIST),yes)
	$(Q)mkdir -p $(RELEASE_DIR)
endif
	$(Q)cp $(TARGET) $(RELEASE_DIR)/

clean:
	$(Q)rm -f $(OBJS)
	$(Q)bash $(CUR_DIR_PATH)/lds.sh $@

distclean: clean
	$(Q)rm -f $(TARGET) $(TARGET).map $(TARGET).asm

.PHONY: build clean distclean
