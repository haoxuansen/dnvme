RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

.DEFAULT_GOAL := build

$(TARGETS): $(OBJS)

build: $(TARGETS)
ifneq ($(RELEASE_DIR_EXIST),yes)
	$(Q)mkdir -p $(RELEASE_LIB_DIR)
endif
	$(Q)cp $(TARGETS) $(RELEASE_LIB_DIR)/

clean:
	$(Q)rm -f $(OBJS)

distclean:
	$(Q)rm -f $(OBJS) $(TARGETS)

.PHONY: build clean distclean
