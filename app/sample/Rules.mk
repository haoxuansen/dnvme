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

RELEASE_SAMPLE_DIR := $(RELEASE_DIR)/sample
DIR_EXIST := $(call check_dir_exist,$(RELEASE_SAMPLE_DIR))

# --------------------------------------------------------------------------- #
# Targets
# --------------------------------------------------------------------------- #

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
TGTS := $(basename $(SRCS))

# --------------------------------------------------------------------------- #
# Recipes in rules
# --------------------------------------------------------------------------- #

%: %.c
	@echo "  CC \t $@"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
