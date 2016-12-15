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

clean:
	@rm -rf build/

.PHONY: all settings clean

#@@@@@@@@@@@@@@@@@@@@@@@#
# TESTING SECTION BELOW #
#########################


tests: test_istring

test_IString: istrlib.o
	${CC} -o test_istring istrlib.c test_istring.c
	./test_istring
