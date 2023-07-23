RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

.DEFAULT_GOAL := build

build:
	$(Q)make -C base $@
	$(Q)make -C nvme $@

clean:
	$(Q)make -C base $@
	$(Q)make -C nvme $@

distclean:
	$(Q)make -C base $@
	$(Q)make -C nvme $@

.PHONY: build clean distclean
