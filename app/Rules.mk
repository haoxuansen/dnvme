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
# Compiler Options
# --------------------------------------------------------------------------- #

CFLAGS := $(RULE_CFLAGS)
LDFLAGS := $(RULE_LDFLAGS)
# * "backtrace_symbols()" depends on "-rdynamic" flag
LDFLAGS += -rdynamic
# !WARNNING: "-static" flag may affect "-rdynamic", don't enable together!
# LDFLAGS += -static
LDLIBS := -lall
# LDFLAGS += -dynamic
# LDLIBS := -lbase -lnvme

# --------------------------------------------------------------------------- #
# Recipes in rules
# --------------------------------------------------------------------------- #

%.o: %.c
	@echo "  CC \t $@"
	$(Q)$(CC) $(CFLAGS) -c $^ -o $@
