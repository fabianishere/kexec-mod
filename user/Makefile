ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
CC = gcc
CFLAGS ?= -O2 -Wall

.PHONY: all clean

all: redir.so

%.so: %.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -shared -fpic -o $@ $<

clean:
	rm redir.so
