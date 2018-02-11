VERSION := 1.0.0

# System paths
PREFIX := /usr/local
BINPREFIX := $(PREFIX)/bin
INCLUDEPREFIX := $(PREFIX)/include
LIBPREFIX := $(PREFIX)/lib
MANPREFIX := $(PREFIX)/man

# Linking flags
LDFLAGS :=
LDFLAGS_DEBUG :=

# C Compiler settings
CC := cc
CFLAGS := -O2 -Iinclude -std=c11 -pedantic -Wall -Wextra
CFLAGS_DEBUG := $(CFLAGS) -g -O0
