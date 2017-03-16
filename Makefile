include config.mk

TARGET=yorha

SRC = ${TARGET:=.c}
HDR = ${TARGET:=.h}
OBJ = ${TARGET:=.o}
MAN1 = ${TARGET:=.1} 
SRC_DIR = 

all: options ${TARGET}

options: config.mk
	@printf "${TARGET} build options:\n"
	@printf "CFLAGS  = ${CFLAGS}\n"
	@printf "LDFLAGS = ${LDFLAGS}\n"
	@printf "CC      = ${CC}\n"

%.o: ${SRC_DIR}/%.c ${SRC_DIR}/${HDR} config.mk
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
