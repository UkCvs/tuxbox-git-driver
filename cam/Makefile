ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/..
include $(DRIVER_TOPDIR)/kernel.make
else

obj-m	:= cam.o

EXTRA_CFLAGS	+= -I$(DRIVER_TOPDIR)/include -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb

ifeq ($(CONFIG_CAM_DEBUG),y)
EXTRA_CFLAGS	+= -DDEBUG
endif

endif
