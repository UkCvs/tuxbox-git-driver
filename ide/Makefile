ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/..
include $(DRIVER_TOPDIR)/kernel.make
else

obj-m		+= dboxide.o

dboxide-objs	+= main.o trace.o transfer.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/include
endif
