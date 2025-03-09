BIN ?= mkfs.ouichefs
IMG ?= test.img
IMGSIZE ?= 50
CC ?= gcc
CFLAGS ?= -Wall

prefix ?= /usr/local
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin

all: ${BIN}

${BIN}: mkfs-ouichefs.c
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $<

install: ${BIN}
	install -Dm 755 ${BIN} $(DESTDIR)$(bindir)/mkfs.ouichefs

img: ${BIN}
	rm -rf ${IMG}
	dd if=/dev/zero of=${IMG} bs=1M count=${IMGSIZE}
	./${BIN} ${IMG}
	sync

clean:
	rm -rf *~

mrproper: clean
	rm -rf ${BIN}

.PHONY: all install clean mrproper img
