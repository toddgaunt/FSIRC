# see license file for copyright and license details.

# Project configuration
include config.mk

LDFLAGS += -I.

MODULES := src
LIBS := -lstx
SRC := yorha.c

# Project modules
include $(patsubst %, %/module.mk, $(MODULES))

OBJ := \
	$(patsubst %.c, %.o, \
		$(filter %.c, $(SRC)))

%.o: %.c config.mk
	@printf "CC $<\n"
	@$(CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $<

# Standard targets
all: options yorha

options: config.mk
	@printf -- "yorha build options:\n"
	@printf -- "CFLAGS  = %s\n" "$(CFLAGS)"
	@printf -- "LDFLAGS = %s\n" "$(LDFLAGS)"
	@printf -- "CC      = %s\n" "$(CC)"

dist: clean
	#@printf "Creating dist tarball ... "
	#@mkdir -p yorha-$(VERSION)
	#TODO
	#@cp -rf ??? yorha-$(VERSION)
	#TODO
	#@tar -cf yorha-$(VERSION).tar yorha-$(VERSION)
	#@gzip yorha-$(VERSION).tar
	#@rm -rf ${DIST}
	#@printf "done.\n"

clean:
	@printf "Cleaning\n"
	@rm -f $(OBJ) yorha

# Main target
yorha: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LIBS)

.PHONY: all options check clean dist install uninstall
