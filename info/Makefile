ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/..
include $(DRIVER_TOPDIR)/kernel.make
else
obj-m				+= tuxbox.o

tuxbox-objs			:= tuxbox_core.o tuxbox_hardware_dbox2.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/include
endif
