ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(PWD)/..
include $(DRIVER_TOPDIR)/kernel.make
else
obj-m		+= lcd.o
lcd-objs	:= lcd-console.o lcd-font.o lcd-ks0713.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/include
endif
