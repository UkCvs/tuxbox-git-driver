#
# Makefile for the kernel DVB device drivers.
#
ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(PWD)/../../../../..
include $(DRIVER_TOPDIR)/kernel.make
else
dvb-core-objs = dvbdev.o dmxdev.o dvb_demux.o dvb_filter.o \
	        dvb_ca_en50221.o dvb_frontend.o \
		dvb_net.o dvb_ringbuffer.o

obj-m	+= dvb-core.o

# we do this to override the default "linux"-include dir
CC		:= $(CROSS_COMPILE)gcc -I$(DRIVER_TOPDIR)/dvb/include
endif
