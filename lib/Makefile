TOP_DIR ?= $(shell pwd)/..
include $(TOP_DIR)/.config

CUR_DIR := $(TOP_DIR)/lib
OUTPUT_DIR := $(TOP_DIR)/output

srcs := $(wildcard *.c)
objs := $(srcs:.c=.o)
targets := libbase.so libbase.a

CC ?= gcc
CFLAGS := -g -Wall
CFLAGS += -I$(TOP_DIR)/include/generated
CFLAGS += -I$(TOP_DIR)/include/uapi
CFLAGS += -include $(TOP_DIR)/include/uapi/kconfig.h

.DEFAULT_GOAL := build

%.o: %.c
	$(Q)$(CC) $(CFLAGS) -fPIC -c $^

%.so: $(objs)
	$(Q)$(CC) -shared $^ -o $@

%.a: $(objs)
	$(Q)$(AR) rcs $@ $^

build: $(targets)
	$(Q)cp $(targets) $(TOP_DIR)/output/libs/

clean:
	$(Q)-rm $(targets)

distclean: clean

.PHONY: build clean distclean
