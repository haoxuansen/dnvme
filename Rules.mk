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

TOP_DIR := $(CUR_DIR_PATH)
RELEASE_DIR := $(TOP_DIR)/release
RELEASE_LIB_DIR := $(RELEASE_DIR)/libs

# --------------------------------------------------------------------------- #
# Resource
# --------------------------------------------------------------------------- #

# Shall add global configuration file
include $(TOP_DIR)/.config

# --------------------------------------------------------------------------- #
# Compiler Options
# --------------------------------------------------------------------------- #

RULE_CFLAGS := -g -Wall
RULE_CFLAGS += -I$(TOP_DIR)/include/generated
RULE_CFLAGS += -I$(TOP_DIR)/include/uapi
RULE_CFLAGS += -I$(TOP_DIR)/lib/include
RULE_CFLAGS += -include $(TOP_DIR)/include/uapi/kconfig.h

RULE_LDFLAGS := -L$(RELEASE_LIB_DIR)

CC := gcc
LD := ld
OBJDUMP := objdump

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

# $1: The directory to be checked 
# Return "yes" if directory exist, otherwise returns "no"
define check_dir_exist
$(shell if [ -d $(1) ]; then echo "yes"; else echo "no"; fi)
endef
