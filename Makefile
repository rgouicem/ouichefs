obj-m += ouichefs.o
ouichefs-objs := fs.o super.o inode.o file.o dir.o snapshot.o

KERNELDIR ?= /home/bytemouse/Code/lkp/share/linux-stable
MAKE_ARGS := LLVM=1 CC="ccache clang" KBUILD_BUILD_TIMESTAMP=''

all:
	make -C $(KERNELDIR) M=$(PWD) $(MAKE_ARGS) -j$(nproc) modules

debug:
	make -C $(KERNELDIR) M=$(PWD) $(MAKE_ARGS) ccflags-y+="-DDEBUG -g" -j$(nproc) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf *~

.PHONY: all clean
