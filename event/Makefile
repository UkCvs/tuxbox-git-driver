ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(PWD)/..
include $(DRIVER_TOPDIR)/kernel.make
else
obj-m	+= event.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/include
endif
