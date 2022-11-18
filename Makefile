TOP_DIR := $(shell pwd)
OUTPUT_DIR := $(TOP_DIR)/output
SCRIPT_DIR := $(TOP_DIR)/scripts

export TOP_DIR

.DEFAULT_GOAL := build

pre:
	$(Q)mkdir -p $(OUTPUT_DIR)
	$(Q)cp $(SCRIPT_DIR)/insmod.sh $(OUTPUT_DIR)/

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
	$(Q)rm -rf $(OUTPUT_DIR)

.PHONY: build clean distclean
