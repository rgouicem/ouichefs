BIN ?= mkfs.ouichefs
IMG ?= test.img
IMGSIZE ?= 50

all: ${BIN}

${BIN}: mkfs-ouichefs.c
	gcc -Wall -o $@ $<

${BIN}-static: mkfs-ouichefs.c
	gcc -Wall -static -o $@ $<

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
