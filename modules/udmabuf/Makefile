include $(TOP_DIR)/modules/Rules.mk
CUR_DIR := $(TOP_DIR)/modules/udmabuf

TARGET					:= u-dma-buf.ko
obj-m					:= u-dma-buf.o

build:
	$(Q)make -C $(KERNEL_DIR) M=$(CUR_DIR) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules
	$(Q)cp $(TARGET) $(OUTPUT_DIR)/

clean:
	$(Q)make -C $(KERNEL_DIR) M=$(CUR_DIR) clean

distclean: clean

.PHONY: build clean distclean
