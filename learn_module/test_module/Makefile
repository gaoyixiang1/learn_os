obj-m +=test_module.o
CURRENT_PATH:=$(shell pwd)
LINUX_KERNEL:=$(shell uname -r)
LINUX_KERNEL_PATH:=/usr/src/linux-headers-$(LINUX_KERNEL)
all:
	make -C/lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C/lib/modules/$(shell uname -r)/build M=$(PWD) clean