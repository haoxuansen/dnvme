RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

.DEFAULT_GOAL := build

build: $(TGTS)
ifeq ($(RELEASE_DIR_EXIST),yes)
	$(Q)cp $(TGTS) $(OUTPUT_DIR)/
else
	$(warning "$(OUTPUT_DIR)" not exist!)
endif

clean:
	$(Q)rm -f $(OBJS)

distclean: clean
	$(Q)rm -f $(TGTS)

.PHONY: build clean distclean
