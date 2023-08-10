RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

DIRS := base nvme
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
	$(Q)make -C base $@
	$(Q)make -C nvme $@
ifeq ($(RELEASE_DIR_EXIST),yes)
	$(Q)cp $(TARGETS) $(RELEASE_LIB_DIR)/
else
	$(warning "$(RELEASE_LIB_DIR)" not exist!)
endif

clean:
	$(Q)make -C base $@
	$(Q)make -C nvme $@
	$(Q)-rm $(OBJS)

distclean:
	$(Q)make -C base $@
	$(Q)make -C nvme $@
	$(Q)-rm $(OBJS) $(TARGETS)

.PHONY: build clean distclean
