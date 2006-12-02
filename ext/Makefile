ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/..
include $(DRIVER_TOPDIR)/kernel.make
else

obj-m		+= aviaEXT.o

EXTRA_CFLAGS := -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb \
				-I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb/avia \
				-I$(DRIVER_TOPDIR)/include
endif
