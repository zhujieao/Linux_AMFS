AMFS_VERSION="0.1"
obj-m := amfs.o
amfs-objs := dentry.o file.o inode.o main.o super.o lookup.o mmap.o
EXTRA_CFLAGS += -DAMFS_VERSION=\"$(AMFS_VERSION)\"
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -Wall -Werror amfsctl.c -o amfsctl
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f amfsctl
