#
# Makefile for the kernel DVB frontend device drivers.
#
ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(PWD)/../../../../..
include $(DRIVER_TOPDIR)/kernel.make
else
CC	:= $(CROSS_COMPILE)gcc -I$(DRIVER_TOPDIR)/dvb/include
EXTRA_CFLAGS = -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb/dvb-core

obj-m	+= stv0299.o
obj-m	+= at76c651.o
obj-m	+= ves1820.o
obj-m	+= ves1x93.o
endif
