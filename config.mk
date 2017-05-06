# Program information
VERSION = 0.0.1

# System paths.
PREFIX = /usr/local
BINPREFIX = ${PREFIX}/bin
MANPREFIX = ${PREFIX}/share/man
DOCPREFIX = ${PREFIX}/share/doc/
VARPREFIX = ${PREFIX}/var

# Linker configuration.
LDFLAGS = -lstx -larg

# Compiler configuration.
CFLAGS = -std=gnu99 -O2 -Wall -Wextra

# Compiler
CC = cc
