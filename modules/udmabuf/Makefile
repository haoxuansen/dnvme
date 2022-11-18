TOP_DIR ?= $(shell pwd)/../..
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
OUTPUT_DIR := $(TOP_DIR)/output
CUR_DIR := $(TOP_DIR)/modules/udmabuf

EXTRA_CFLAGS := -I$(TOP_DIR)/include
EXTRA_CFLAGS += -I$(TOP_DIR)/include/uapi

TARGET					:= u-dma-buf.ko
obj-m					:= u-dma-buf.o

build:
	$(Q)make -C $(KERNEL_DIR) M=$(CUR_DIR) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules
	$(Q)cp $(TARGET) $(OUTPUT_DIR)/

clean:
	$(Q)make -C $(KERNEL_DIR) M=$(CUR_DIR) clean

distclean: clean

.PHONY: build clean distclean
