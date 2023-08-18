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
	$(Q)mkdir -p $(OUTPUT_DIR)/libs
	$(Q)cp $(SCRIPT_DIR)/insmod.sh $(OUTPUT_DIR)/

modules:
ifneq ($(CONFIG_DNVME),)
	$(Q)make -C modules/dnvme
endif
ifneq ($(CONFIG_U_DMA_BUF),)
	$(Q)make -C modules/udmabuf
endif
PHONY += modules

lib:
ifneq ($(CONFIG_LIB),)
	$(Q)make -C lib
endif
PHONY += lib

build: pre modules lib
ifneq ($(CONFIG_UNVME),)
	$(Q)make -C app
endif
PHONY += build

clean:
	$(Q)make -C modules/dnvme $@
ifneq ($(CONFIG_U_DMA_BUF),)
	$(Q)make -C modules/udmabuf $@
endif
ifneq ($(CONFIG_LIB),)
	$(Q)make -C lib $@
endif
ifneq ($(CONFIG_UNVME),)
	$(Q)make -C app $@
endif
	$(Q)make CC=gcc HOSTCC=gcc -C scripts/kconfig $@
PHONY += clean

distclean:
	$(Q)make -C modules/dnvme $@
	$(Q)make -C modules/udmabuf $@
	$(Q)make -C lib $@
	$(Q)make -C app $@
	$(Q)make CC=gcc HOSTCC=gcc -C scripts/kconfig $@
	$(Q)rm -rf $(TOP_DIR)/include/config
	$(Q)rm -rf $(OUTPUT_DIR)
PHONY += distclean

.PHONY: $(PHONY)
