# See LICENSE file for copyright and license details.

# Project configuration
include config.mk

MODULES := arg sys channels strbuf
SRC := yorha.c

# Project modules
include $(patsubst %, %/module.mk, $(MODULES))

_OBJ := $(patsubst %.c, %.o, $(filter %.c, $(SRC)))
OBJ := $(patsubst %, build/%, $(_OBJ))
OBJ_DEBUG := $(patsubst %.o, build/%_d.o, $(_OBJ))

# Standard targets
all: release

options:
	@echo "Build options:"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "CC      = $(CC)"

dist: clean

clean:
	@echo "Cleaning"
	@rm -rf build

# Object Build Rules
build/%.o: %.c
	@echo "CC [R] $@"
	@mkdir -p $(shell dirname $@)
	@$(CC) $(CFLAGS) -c -o $@ $<

build/%_d.o: %.c
	@echo "CC [D] $@"
	@mkdir -p $(shell dirname $@)
	@$(CC) $(CFLAGS_DEBUG) -c -o $@ $< 

# Targets
build/yorha: $(OBJ)
	@echo "CC $@"
	@$(CC) -o $@ $^ $(LDFLAGS)

build/yorha_d: $(OBJ_DEBUG)
	@echo "CC $@"
	@$(CC) -o $@ $^ $(LDFLAGS_DEBUG)

release: build/yorha

debug: build/yorha_d

.PHONY: all options check clean dist install uninstall
