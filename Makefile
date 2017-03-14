include config.mk

TARGET=irccd
SRC=irccd.c
OBJ=${SRC:.c=.o}

all: settings ${TARGET}

settings:
	@echo ${TARGET} build settings:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

${TARGET}: ${OBJ}
	${CC} -o ${TARGET} ${OBJ} ${LDFLAGS}

clean:
	rm -f ${TARGET} ${OBJ}
	rm -f ${TESTS}

.PHONY: all settings clean
