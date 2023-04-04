include $(TOP_DIR)/modules/Rules.mk
CUR_DIR := $(TOP_DIR)/modules/dnvme

# tell define_trace.h where to find the nvme trace header
CFLAGS_trace.o := -I$(src)

TARGET					:= dnvme.ko
obj-m					:= dnvme.o
dnvme-y					:= core.o
dnvme-y					+= cmd.o
dnvme-y					+= io.o
dnvme-y					+= ioctl.o
dnvme-y					+= irq.o
dnvme-y					+= meta.o
dnvme-y					+= pci.o
dnvme-y					+= proc.o
dnvme-y					+= queue.o
dnvme-y					+= trace.o
ifneq ($(CONFIG_DNVME_CMB),)
dnvme-y					+= cmb.o
endif
ifneq ($(CONFIG_DNVME_DEBUG),)
dnvme-y					+= debug.o
endif

build:
	$(Q)make -C $(KERNEL_DIR) M=$(CUR_DIR) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules
	$(Q)cp $(TARGET) $(OUTPUT_DIR)/

clean:
	$(Q)make -C $(KERNEL_DIR) M=$(CUR_DIR) clean

distclean: clean

.PHONY: build clean distclean
