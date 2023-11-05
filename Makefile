-include .config

TOP_DIR := $(shell pwd)
RELEASE_DIR := $(TOP_DIR)/release
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
	$(Q)mkdir -p $(RELEASE_DIR)/libs
	$(Q)cp $(SCRIPT_DIR)/insmod.sh $(RELEASE_DIR)/

modules:
ifneq ($(CONFIG_DNVME),)
	$(Q)make -C modules/dnvme
endif
PHONY += modules

lib:
ifneq ($(CONFIG_LIBRARY),)
	$(Q)make -C lib
endif
PHONY += lib

build: pre modules lib
ifneq ($(CONFIG_APPLICATION),)
	$(Q)make -C app
endif
PHONY += build

clean:
	$(Q)make -C modules/dnvme $@
ifneq ($(CONFIG_LIBRARY),)
	$(Q)make -C lib $@
endif
ifneq ($(CONFIG_APPLICATION),)
	$(Q)make -C app $@
endif
	$(Q)make CC=gcc HOSTCC=gcc -C scripts/kconfig $@
PHONY += clean

distclean:
ifneq ($(CONFIG_DNVME),)
	$(Q)make -C modules/dnvme $@
endif
ifneq ($(CONFIG_LIBRARY),)
	$(Q)make -C lib $@
endif
ifneq ($(CONFIG_APPLICATION),)
	$(Q)make -C app $@
endif
	$(Q)make CC=gcc HOSTCC=gcc -C scripts/kconfig $@
	$(Q)rm -rf $(TOP_DIR)/include/config
	$(Q)rm -rf $(RELEASE_DIR)
PHONY += distclean

.PHONY: $(PHONY)
