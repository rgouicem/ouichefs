BIN ?= mkfs.ouichefs
IMG ?= test.img
IMGSIZE ?= 50

all: ${BIN}

${BIN}:
	cargo build
	cp target/debug/ouichefs_mkfs ${BIN}

img:
	rm -rf ${IMG}
	dd if=/dev/zero of=${IMG} bs=1M count=${IMGSIZE}
	cargo run -- ${IMG}
	sync

clean:
	cargo clean
	rm -rf *~

mrproper: clean
	rm -rf ${BIN}

.PHONY: all clean mrproper img
