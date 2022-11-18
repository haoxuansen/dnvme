TOP_DIR := $(shell pwd)

export TOP_DIR

.DEFAULT_GOAL := build

pre:
	$(Q)mkdir -p $(TOP_DIR)/output

build: pre
	$(Q)make -C modules/dnvme
	$(Q)make -C modules/udmabuf
	$(Q)make -C app

clean:
	$(Q)make -C modules/dnvme clean
	$(Q)make -C modules/udmabuf clean
	$(Q)make -C app clean

distclean:
	$(Q)make -C modules/dnvme distclean
	$(Q)make -C modules/udmabuf distclean
	$(Q)make -C app distclean
	$(Q)rm -rf $(TOP_DIR)/output

.PHONY: build clean distclean
