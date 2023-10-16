RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

DIRS := base crc json nvme
SRCS := $(foreach dir,$(DIRS),$(wildcard $(CUR_DIR_PATH)/$(dir)/*.c))
OBJS := $(patsubst %.c,%.o,$(SRCS))

TARGETS := liball.a

.DEFAULT_GOAL := build

$(TARGETS): $(OBJS)
	@echo "  AR \t $@"
	$(Q)$(AR) rcs $@ $^

dynamic: 
	$(Q)make -C base
	$(Q)make -C crc
	$(Q)make -C json
	$(Q)make -C nvme

build: dynamic $(TARGETS)

ifneq ($(RELEASE_DIR_EXIST),yes)
	$(Q)mkdir -p $(RELEASE_LIB_DIR)
endif
	$(Q)cp $(TARGETS) $(RELEASE_LIB_DIR)/

clean:
	$(Q)make -C base $@
	$(Q)make -C crc $@
	$(Q)make -C json $@
	$(Q)make -C nvme $@
	$(Q)rm -f $(OBJS)

distclean:
	$(Q)make -C base $@
	$(Q)make -C crc $@
	$(Q)make -C json $@
	$(Q)make -C nvme $@
	$(Q)rm -f $(OBJS) $(TARGETS)

.PHONY: dynamic build clean distclean
