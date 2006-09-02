ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/..
include $(DRIVER_TOPDIR)/kernel.make
else

obj-m		+= ide-dbox2.o

ide-dbox2-objs	+= main.o trace.o transfer.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/include
endif
