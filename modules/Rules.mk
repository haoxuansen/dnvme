include $(TOP_DIR)/.config

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
RELEASE_DIR := $(TOP_DIR)/release

EXTRA_CFLAGS := -I$(TOP_DIR)/include
EXTRA_CFLAGS += -I$(TOP_DIR)/include/generated
EXTRA_CFLAGS += -I$(TOP_DIR)/include/uapi
EXTRA_CFLAGS += -include $(TOP_DIR)/include/uapi/kconfig.h
