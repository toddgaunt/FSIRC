include config.mk

SRD_DIR=src/
SRC=${SRC_DIR}irccd.c
OBJ=${SRC:.c=.o}

all: settings ${EXE}

settings:
	@echo tsh build settings:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

irccd: ${OBJ} ${SRC_DIR}irccd.h
	${CC} -o ${EXE} ${OBJ} ${LDFLAGS}

clean:
	rm -f ${OBJ}
	rm -f ${EXE}

.PHONY: all settings clean
