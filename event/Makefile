ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/..
include $(DRIVER_TOPDIR)/kernel.make
else
obj-m	+= event.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/include
endif
