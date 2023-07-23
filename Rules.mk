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

OUTPUT_DIR := $(CUR_DIR_PATH)/output
RELEASE_LIB_DIR := $(CUR_DIR_PATH)/output/libs

# --------------------------------------------------------------------------- #
# Resource
# --------------------------------------------------------------------------- #

# Shall add global configuration file
include $(CUR_DIR_PATH)/.config

# --------------------------------------------------------------------------- #
# Compiler Options
# --------------------------------------------------------------------------- #

RULE_CFLAGS := -g -Wall
RULE_CFLAGS += -I$(CUR_DIR_PATH)/include/generated
RULE_CFLAGS += -I$(CUR_DIR_PATH)/include/uapi
RULE_CFLAGS += -I$(CUR_DIR_PATH)/lib/include
# !TODO: adjust the position of "auto_header.h" reference
RULE_CFLAGS += -include $(CUR_DIR_PATH)/app/unittest/auto_header.h
RULE_CFLAGS += -include $(CUR_DIR_PATH)/include/uapi/kconfig.h

RULE_LDFLAGS := -L$(RELEASE_LIB_DIR)

CC := gcc

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

# $1: The directory to be checked 
# Return "yes" if directory exist, otherwise returns "no"
define check_dir_exist
$(shell if [ -d $(1) ]; then echo "yes"; else echo "no"; fi)
endef
