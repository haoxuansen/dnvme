TOP_DIR ?= $(shell pwd)/..
include $(TOP_DIR)/.config

CUR_DIR := $(TOP_DIR)/app
OUTPUT_DIR := $(TOP_DIR)/output

srcs := $(foreach dir,$(CUR_DIR) $(CUR_DIR)/common,$(wildcard $(dir)/*.c))
objs := $(srcs:.c=.o)
target := nvmetool

CC ?= gcc
CFLAGS := -g -Wall
CFLAGS += -I$(TOP_DIR)/include/generated
CFLAGS += -I$(TOP_DIR)/include/uapi
CFLAGS += -I$(CUR_DIR)/common
CFLAGS += -include $(TOP_DIR)/include/uapi/kconfig.h

LDFLAGS := -L$(TOP_DIR)/output/libs
LDFLAGS += -static
LDLIBS := -lbase

Q ?= @

.DEFAULT_GOAL := build

%.o: %.c
	$(Q)$(CC) $(CFLAGS) -c $^ -o $@

pre:
	$(Q)bash $(CUR_DIR)/prepare.sh

$(target): $(objs)
	$(Q)$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

build: pre $(target)
	$(Q)cp $(target) $(OUTPUT_DIR)/

clean:
	$(Q)rm -f $(objs)

distclean: clean
	$(Q)rm -f $(target)

.PHONY: build clean distclean
