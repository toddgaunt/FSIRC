include config.mk

EXE=irccd

SRC= irccd.c istrlib.c
OBJ=${SRC:.c=.o}

all: init settings ${EXE}

init:
	@mkdir -p build/

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

TESTS=test_istring


tests: ${TESTS}

run_tests: tests
	$(foreach test,${TESTS},./${test})

test_istring: test_istring.c istrlib.c
	${CC} -o test_istring test_istring.c istrlib.c

#
#
#

clean:
	rm -f ${EXE} ${OBJ}
	rm -f ${TESTS}

.PHONY: all settings clean
