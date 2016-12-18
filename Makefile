include config.mk

EXE=irccd

SRC=irccd.c
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

${EXE}: ${OBJ}
	${CC} -o ${EXE} ${OBJ} ${LDFLAGS}

#@@@@@@@@@@@@@@@@@@@@@@@#
# TESTING SECTION BELOW #
#########################

TESTS=

tests: ${TESTS}

run_tests: tests
	$(foreach test,${TESTS},./${test})

#
#
#

clean:
	rm -f ${EXE} ${OBJ}
	rm -f ${TESTS}

.PHONY: all settings clean
