RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

.DEFAULT_GOAL := build

pre:
	$(Q)bash $(CUR_DIR_PATH)/prepare.sh

$(TARGET): $(OBJS)
	$(Q)$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

build: pre $(TARGET)
ifeq ($(RELEASE_DIR_EXIST),yes)
	$(Q)cp $(TARGET) $(OUTPUT_DIR)/
else
	$(warning "$(OUTPUT_DIR)" not exist!)
endif

clean:
	$(Q)rm -f $(OBJS)

distclean: clean
	$(Q)rm -f $(TARGET)

.PHONY: build clean distclean
