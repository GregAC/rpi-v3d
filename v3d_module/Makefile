obj-m := v3d_mod.o
v3d_mod-objs := v3d_module.o vc_support.o

KDIR ?= /home/greg/rpi-repos/linux/

export ARCH:=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

default:
	$(MAKE) -C $(KDIR) M=$$PWD modules

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
