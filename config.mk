# Program information
VERSION = 0.0.1

# System paths.
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# Linker configuration.
LDFLAGS = -s -loystr

# Compiler configuration.
CFLAGS = -g -std=gnu99 -O0 -Wall -Wextra -static
CC=cc

