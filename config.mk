VERSION := 1.0.0

# System paths
PREFIX := /usr/local
BINPREFIX := $(PREFIX)/bin
INCLUDEPREFIX := $(PREFIX)/include
LIBPREFIX := $(PREFIX)/lib
MANPREFIX := $(PREFIX)/man

# Linking flags
LDFLAGS := -s

# C Compiler settings
CC := cc 
CFLAGS := -g -std=c99 -pedantic -O0 -Wall -Wextra
