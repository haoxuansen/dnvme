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

RELEASE_DIR_EXIST := $(call check_dir_exist,$(OUTPUT_DIR))

# --------------------------------------------------------------------------- #
# Compiler Options
# --------------------------------------------------------------------------- #

CFLAGS := $(RULE_CFLAGS)
LDFLAGS := $(RULE_LDFLAGS)
LDFLAGS += -static
LDLIBS := -lall
# LDFLAGS += -dynamic
# LDLIBS := -lbase -lnvme

# --------------------------------------------------------------------------- #
# Recipes in rules
# --------------------------------------------------------------------------- #

%.o: %.c
	$(Q)$(CC) $(CFLAGS) -c $^ -o $@
