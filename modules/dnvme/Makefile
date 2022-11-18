TOP_DIR ?= $(shell pwd)/../..
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
OUTPUT_DIR := $(TOP_DIR)/output
CUR_DIR := $(TOP_DIR)/modules/dnvme

EXTRA_CFLAGS := -I$(TOP_DIR)/include
EXTRA_CFLAGS += -I$(TOP_DIR)/include/uapi
EXTRA_CFLAGS += -include $(TOP_DIR)/include/uapi/kconfig.h

TARGET					:= dnvme.ko
obj-m					:= dnvme.o
dnvme-y					:= dnvme_cmb.o
dnvme-y					+= dnvme_cmds.o
dnvme-y					+= dnvme_ds.o
dnvme-y					+= dnvme_ioctls.o
dnvme-y					+= dnvme_irq.o
dnvme-y					+= dnvme_queue.o
dnvme-y					+= dnvme_reg.o
dnvme-y					+= dnvme_sts_chk.o
dnvme-y					+= core.o

build:
	$(Q)make -C $(KERNEL_DIR) M=$(CUR_DIR) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules
	$(Q)cp $(TARGET) $(OUTPUT_DIR)/

clean:
	$(Q)make -C $(KERNEL_DIR) M=$(CUR_DIR) clean

distclean: clean

.PHONY: build clean distclean
