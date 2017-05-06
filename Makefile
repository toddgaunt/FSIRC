include config.mk

TARGET = yorha

SRC_DIR = src
BUILD_DIR = build
DEBUG_DIR = debug

SRC = $(notdir $(wildcard ${SRC_DIR}/*.c))
OBJ = $(addprefix ${BUILD_DIR}/, ${SRC:.c=.o})
DEBUG_OBJ = $(addprefix ${DEBUG_DIR}/, ${SRC:.c=.o})

# Macro flags for injecting information into the program during compile time.
MFLAGS = \
       -DVERSION=\"${VERSION}\"\
       -DPREFIX=\"${PREFIX}\"\
       -DVARPREFIX=\"${VARPREFIX}\"\
       -DPRGM_NAME=\"${TARGET}\"

all: options ${TARGET}

options: config.mk
	@printf "${TARGET} build options:\n"
	@printf "CFLAGS  = ${CFLAGS}\n"
	@printf "LDFLAGS = ${LDFLAGS}\n"
	@printf "CC      = ${CC}\n"

${BUILD_DIR}/%.o: ${SRC_DIR}/%.c
	@mkdir -p $(dir $@)
	@printf "CC $@\n"
	@${CC} ${CFLAGS} ${MFLAGS} -c -o $@ $< ${LDFLAGS}

${OBJ}: config.mk

${TARGET}: ${OBJ}
	@printf "CC $@\n"
	@${CC} ${CFLAGS} ${MFLAGS} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@printf "Cleaning ... "
	rm -f ${TARGET} ${OBJ}
	@printf "done.\n"

.PHONY: all options clean dist install uninstall
