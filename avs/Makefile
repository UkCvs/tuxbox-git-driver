ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(PWD)/..
include $(DRIVER_TOPDIR)/kernel.make
else
obj-m		+= avs.o
avs-objs	:= stv6412.o cxa2092.o cxa2126.o avs_core.o

EXTRA_CFLAGS	+= -I$(DRIVER_TOPDIR)/include
endif
