ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/../../../../..
include $(DRIVER_TOPDIR)/kernel.make
else
#STANDALONE:=1

obj-m		:= dbox2_napi.o
dbox2_napi-objs	:= dbox2_napi_core.o dbox2_pll.o dbox2_fp_napi.o
cam		:= dbox2_cam_napi.o
avia_av		:= dbox2_avia_av_napi.o
avia_gt		:= dbox2_avia_gt_napi.o
standalone	:= $(cam) $(avia_av) $(avia_gt)

# if STANDALONE is undefined, simply link them all into a big module

ifeq ($(STANDALONE),)
dbox2_napi-objs	+= $(standalone)
else
EXTRA_CFLAGS	+= -DSTANDALONE
obj-m		+= $(standalone)
endif

EXTRA_CFLAGS	+= -I$(DRIVER_TOPDIR)/include \
		    -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb

# we do this to override the default "linux"-include dir
CC		:= $(CROSS_COMPILE)gcc -I$(DRIVER_TOPDIR)/dvb/include
endif
