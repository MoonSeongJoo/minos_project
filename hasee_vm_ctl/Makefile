obj-m += hasee_vm_ctl.o

KDIR ?= ../linux-4.19.238

all:
	$(MAKE) -C $(KDIR) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- M=$(PWD) clean