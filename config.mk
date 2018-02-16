VERSION := 1.0.0

# System paths
PREFIX := /usr/local
BINPREFIX := $(PREFIX)/bin
INCLUDEPREFIX := $(PREFIX)/include
LIBPREFIX := $(PREFIX)/lib
MANPREFIX := $(PREFIX)/man

# Linking flags
LDFLAGS :=

# C Compiler settings
CC := clang
CFLAGS := -O0 -Iinclude -pedantic -Wall -Wextra
CFLAGS += $(CFLAGS) -g -O0
