#     Priority to include "Rules.mk" of the parent direcotry, then include
# "Rules.mk" of the current directory.
RULES_MK := $(addsuffix ../$(notdir $(RULES_MK)),$(dir $(RULES_MK)))
-include $(RULES_MK)

ifeq ($(RULES_LIST),)
RULES_LIST := $(MAKEFILE_LIST)
else
RULES_LIST := $(filter-out $(lastword $(RULES_LIST)),$(RULES_LIST))
endif

# --------------------------------------------------------------------------- #
# Directory
# --------------------------------------------------------------------------- #

# The directory path where the current file is located
CUR_DIR_PATH := $(shell dirname $(abspath $(lastword $(RULES_LIST))))
# The directory name where the current file is located
CUR_DIR_NAME := $(notdir $(CUR_DIR_PATH))

# --------------------------------------------------------------------------- #
# Targets
# --------------------------------------------------------------------------- #
LIB_STATIC := $(addprefix lib,$(addsuffix .a,$(CUR_DIR_NAME)))
LIB_DYNAMIC := $(addprefix lib,$(addsuffix .so,$(CUR_DIR_NAME)))

DIRS := . ./cmd
SRCS := $(foreach dir,$(DIRS),$(wildcard $(dir)/*.c))
OBJS := $(SRCS:.c=.o)
TARGETS := $(LIB_DYNAMIC)

# --------------------------------------------------------------------------- #
# Recipes in rules
# --------------------------------------------------------------------------- #
%.o: %.c
	@echo "  CC \t $@"
	$(Q)$(CC) $(CFLAGS) -fPIC -c $^ -o $@

%.so: $(OBJS)
	@echo "  CC \t $@"
	$(Q)$(CC) -shared $^ -o $@

%.a: $(OBJS)
	$(Q)$(AR) rcs $@ $^
