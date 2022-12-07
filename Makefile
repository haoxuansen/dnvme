-include .config

TOP_DIR := $(shell pwd)
OUTPUT_DIR := $(TOP_DIR)/output
SCRIPT_DIR := $(TOP_DIR)/scripts

export TOP_DIR

ifeq ("$(origin V)","command line")
BUILD_VERBOSE := $(V)
endif
ifndef BUILD_VERBOSE
BUILD_VERBOSE := 0
endif

ifeq ($(BUILD_VERBOSE),1)
Q :=
else
Q := @
endif

export Q

PHONY :=

.DEFAULT_GOAL := build

MCONF := scripts/kconfig/mconf
$(MCONF):
	$(Q)make CC=gcc HOSTCC=gcc -C scripts/kconfig

menuconfig: $(MCONF)
	$(Q)rm -rf $(TOP_DIR)/include/config
	$(Q)rm -rf $(TOP_DIR)/include/generated
	$(Q)$(MCONF) Kconfig
PHONY += menuconfig

pre:
	$(Q)mkdir -p $(OUTPUT_DIR)
	$(Q)cp $(SCRIPT_DIR)/insmod.sh $(OUTPUT_DIR)/

modules:
ifneq ($(CONFIG_DNVME),)
	$(Q)make -C modules/dnvme
endif
ifneq ($(CONFIG_U_DMA_BUF),)
	$(Q)make -C modules/udmabuf
endif
PHONY += modules

build: pre modules
	$(Q)make -C app
PHONY += build

clean:
	$(Q)make -C modules/dnvme clean
	$(Q)make -C modules/udmabuf clean
	$(Q)make -C app clean
	$(Q)make CC=gcc HOSTCC=gcc -C scripts/kconfig clean
PHONY += clean

distclean:
	$(Q)make -C modules/dnvme distclean
	$(Q)make -C modules/udmabuf distclean
	$(Q)make -C app distclean
	$(Q)make CC=gcc HOSTCC=gcc -C scripts/kconfig distclean
	$(Q)rm -rf $(OUTPUT_DIR)
PHONY += distclean

.PHONY: $(PHONY)
