ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/..
include $(DRIVER_TOPDIR)/kernel.make
else
obj-m		+= saa7126.o
saa7126-objs	:= saa7126_core.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/include
endif
