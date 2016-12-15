# tshell version
VERSION=0.1

# Paths
PREFIX=/usr/local
MANPREFIX=${PREFIX}/share/man

# Compilation flags
CFLAGS=-g -std=c99 -pedantic -Wall -Wextra -static

# Extra flags below
#-Wfloat-equal -Wundef -Wstrict-prototypes -Wpointer-arith -Wstrict-overflow=5 -Waggregate-return -Wswitch-default -Wconversion -Wswitch-enum -Wunreachable-code

# Linking flags
LDFLAGS=

# Compiler and linker
CC=gcc
