RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

.DEFAULT_GOAL := build

build:
	$(Q)make -C sample $@
	$(Q)make -C unittest $@

clean:
	$(Q)make -C sample $@
	$(Q)make -C unittest $@

distclean:
	$(Q)make -C sample $@
	$(Q)make -C unittest $@

.PHONY: build clean distclean
