# linedict - simple youdao dictionary
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c linedict.c util.c lookup.c
OBJ = ${SRC:.c=.o}

all: options linedict

options:
	@echo linedict build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

config.h:config.def.h
	@echo creating $@ from config.def.h
	@cp config.def.h $@

${OBJ}: config.h config.mk drw.h

linedict: linedict.o drw.o util.o lookup.o
	@echo CC -o $@
	@${CC} -o $@ $^ ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f linedict config.h ${OBJ}

.PHONY: all options clean
