RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

.DEFAULT_GOAL := build

build: $(TGTS)
ifneq ($(DIR_EXIST),yes)
	$(Q)mkdir -p $(RELEASE_SAMPLE_DIR)
endif
	$(Q)cp $(TGTS) $(RELEASE_SAMPLE_DIR)/

clean:
	$(Q)rm -f $(OBJS)

distclean: clean
	$(Q)rm -f $(TGTS)

.PHONY: build clean distclean
