#
# Makefile for the kernel DVB frontend device drivers.
#
ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/../../../../..
include $(DRIVER_TOPDIR)/kernel.make
else
CC	:= $(CROSS_COMPILE)gcc -I$(DRIVER_TOPDIR)/dvb/include
EXTRA_CFLAGS = -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb/dvb-core

obj-m	+= at76c651.o stv0297.o ves1820.o ves1x93.o tda80xx.o
endif
