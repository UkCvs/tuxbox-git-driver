ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(PWD)/..
include $(DRIVER_TOPDIR)/kernel.make
else
obj-m		:= dvb2eth.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb -I$(DRIVER_TOPDIR)/include
endif
