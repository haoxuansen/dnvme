include $(TOP_DIR)/modules/Rules.mk
CUR_DIR := $(TOP_DIR)/modules/dnvme

TARGET					:= dnvme.ko
obj-m					:= dnvme.o
dnvme-y					:= core.o
dnvme-y					+= io.o
dnvme-y					+= ioctl.o
dnvme-y					+= pci.o
dnvme-y					+= cmd.o
dnvme-y					+= log.o
dnvme-y					+= irq.o
dnvme-y					+= queue.o
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
