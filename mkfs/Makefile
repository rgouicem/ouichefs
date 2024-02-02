BIN ?= mkfs.ouichefs

IMG ?= test.img
IMGSIZE ?= 50

SMOL_IMG ?= smol.img
SMOL_IMGSIZE ?= 400

MID_IMG ?= mid.img
MID_IMGSIZE ?= 1

KERNSRC ?= ../../linux

all: ${BIN}

${BIN}: mkfs-ouichefs.c
	gcc -I${KERNSRC}  -Wall -o $@ $<

img: ${BIN}
	rm -rf ${IMG}
	dd if=/dev/zero of=${IMG} bs=1M count=${IMGSIZE}
	./${BIN} ${IMG}
	sync

smol_img: ${BIN}
	rm -rf ${SMOL_IMG}
	dd if=/dev/zero of=${SMOL_IMG} bs=1K count=${SMOL_IMGSIZE}
	./${BIN} ${SMOL_IMG}
	sync

mid_img: ${BIN}
	rm -rf ${MID_IMG}
	dd if=/dev/zero of=${MID_IMG} bs=1M count=${MID_IMGSIZE}
	./${BIN} ${MID_IMG}
	sync

clean:
	rm -rf *~

mrproper: clean
	rm -rf ${BIN}

.PHONY: all clean mrproper img
