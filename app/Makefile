TOP_DIR ?= $(shell pwd)/..
OUTPUT_DIR := $(TOP_DIR)/output

srcs := $(wildcard *.c)
objs := $(srcs:.c=.o)
target := nvmetool

CC ?= gcc
CFLAGS := -g -Wall
CFLAGS += -I$(TOP_DIR)/include/uapi

.DEFAULT_GOAL := build

%.o: %.c
	$(Q)$(CC) $(CFLAGS) -c $^

$(target): $(objs)
	$(Q)$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

build: $(target)
	$(Q)cp $(target) $(OUTPUT_DIR)/

clean:
	$(Q)rm -f $(objs)

distclean: clean
	$(Q)rm -f $(target)

.PHONY: build clean distclean
