ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/..
include $(DRIVER_TOPDIR)/kernel.make
else

obj-m	:= cam.o cam_napi.o

EXTRA_CFLAGS	+= -I$(DRIVER_TOPDIR)/include -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb

endif
