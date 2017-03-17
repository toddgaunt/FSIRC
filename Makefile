include config.mk


TARGET = nerv
SRC = nerv.c libnerv.c
HDR = libnerv.h

all: options ${TARGET}

options: config.mk
	@printf "${TARGET} build options:\n"
	@printf "CFLAGS  = ${CFLAGS}\n"
	@printf "LDFLAGS = ${LDFLAGS}\n"
	@printf "CC      = ${CC}\n"

%.o: ${HDR} %.c config.mk
	@printf "CC $<\n"
	@${CC} ${CFLAGS} -c $<

${OBJ}: config.mk

${TARGET}: ${OBJ}
	@printf "CC $<\n"
	@${CC} -o ${TARGET} ${OBJ} ${LDFLAGS}

test: test.c ${TARGET}
	@printf "CC $<\n"
	@${CC} ${CFLAGS} ${LDFLAGS} -o $@ test.c ${TARGET}

check: test
	@./test

clean:
	@printf "Cleaning ... "
	@rm -f ${TARGET} ${OBJ} test
	@printf "done.\n"

.PHONY: all options clean dist install uninstall
