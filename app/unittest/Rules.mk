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

DIR_EXIST := $(call check_dir_exist,$(RELEASE_DIR))

# --------------------------------------------------------------------------- #
# Compiler Options
# --------------------------------------------------------------------------- #

CFLAGS += -include $(CUR_DIR_PATH)/auto_header.h
# CFLAGS += -DCONFIG_DEBUG_NO_DEVICE=1

# --------------------------------------------------------------------------- #
# Targets
# --------------------------------------------------------------------------- #

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
TARGET := nvmetool
