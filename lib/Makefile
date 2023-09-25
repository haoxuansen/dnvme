RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

DIRS := base crc json nvme
SRCS := $(foreach dir,$(DIRS),$(wildcard $(CUR_DIR_PATH)/$(dir)/*.c))
OBJS := $(patsubst %.c,%.o,$(notdir $(SRCS)))

TARGETS := liball.a

.DEFAULT_GOAL := build

# %.o: %.c
$(OBJS): $(SRCS)
	$(Q)$(CC) $(CFLAGS) -fPIC -c $^

$(TARGETS): $(OBJS)
	$(Q)$(AR) rcs $@ $^

build: $(TARGETS)
	$(foreach dir,$(DIRS),-$(Q)$(shell make -C $(dir) $@))
ifneq ($(RELEASE_DIR_EXIST),yes)
	$(Q)mkdir -p $(RELEASE_LIB_DIR)
endif
	$(Q)cp $(TARGETS) $(RELEASE_LIB_DIR)/

clean:
	$(foreach dir,$(DIRS),-$(Q)$(shell make -C $(dir) $@))
	$(Q)rm -f $(OBJS)

distclean:
	$(foreach dir,$(DIRS),-$(Q)$(shell make -C $(dir) $@))
	$(Q)rm -f $(OBJS) $(TARGETS)

.PHONY: build clean distclean
