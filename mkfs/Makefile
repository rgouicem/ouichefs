BIN ?= mkfs.ouichefs
IMG ?= test.img
IMGSIZE ?= 50

KERNSRC ?= ../../linux

all: ${BIN}

${BIN}: mkfs-ouichefs.c
	gcc -I${KERNSRC}  -Wall -o $@ $<

img: ${BIN}
	rm -rf ${IMG}
	dd if=/dev/zero of=${IMG} bs=1M count=${IMGSIZE}
	./${BIN} ${IMG}
	sync

clean:
	rm -rf *~

mrproper: clean
	rm -rf ${BIN}

.PHONY: all clean mrproper img
