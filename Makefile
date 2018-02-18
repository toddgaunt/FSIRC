# See LICENSE file for copyright and license details.

# Project configuration
include config.mk

MODULES :=
SRC := main.c sys.c

CFLAGS+="-DVERSION=\"$(VERSION)\""

# Project modules
include $(patsubst %, %/module.mk, $(MODULES))

OBJ := $(patsubst %.c, %.o, $(filter %.c, $(SRC)))

# Standard targets
all: options fsircc

options:
	@echo "Build options:"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "CC      = $(CC)"

clean:
	@echo "Cleaning"
	@rm -f $(OBJ) fsircc

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f fsircc $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/fsircc
	mkdir -p $(DESTDIR)$(PREFIX)/man/man1
	sed "s/VERSION/$(VERSION)/g" < fsircc.1 > $(DESTDIR)$(PREFIX)/man/man1/fsirc.1
	chmod 644 $(DESTDIR)$(PREFIX)/man/man1/fsircc.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/fsircc
	rm -f $(DESTDIR)$(PREFIX)/man/man1/fsircc.1

# Object Build Rules
%.o: %.c config.mk config.h
	@echo "CC [R] $@"
	@mkdir -p $(shell dirname $@)
	@$(CC) $(CFLAGS) -c -o $@ $<

# Targets
config.h:
	@cp config.def.h config.h

fsircc: $(OBJ)
	@echo "CC $@"
	@$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: all options clean install uninstall
