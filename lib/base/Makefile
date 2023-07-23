RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

.DEFAULT_GOAL := build

build: $(TARGETS)
ifeq ($(RELEASE_DIR_EXIST),yes)
	$(Q)cp $(TARGETS) $(RELEASE_LIB_DIR)/
else
	$(warning "$(RELEASE_LIB_DIR)" not exist!)
endif

clean:
	$(Q)-rm $(TARGETS)

distclean: clean

.PHONY: build clean distclean
