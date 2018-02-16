# See LICENSE file for copyright and license details.

# Project configuration
include config.mk

MODULES :=
SRC := main.c sys.c

# Project modules
include $(patsubst %, %/module.mk, $(MODULES))

OBJ := $(patsubst %.c, %.o, $(filter %.c, $(SRC)))

# Standard targets
all: yorha

options:
	@echo "Build options:"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "CC      = $(CC)"

clean:
	@echo "Cleaning"
	@rm -rf build

# Object Build Rules
%.o: %.c config.mk
	@echo "CC [R] $@"
	@mkdir -p $(shell dirname $@)
	@$(CC) $(CFLAGS) -c -o $@ $<

# Targets
yorha: $(OBJ)
	@echo "CC $@"
	@$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: all options clean
