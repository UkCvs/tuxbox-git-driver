ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(PWD)/..
include $(DRIVER_TOPDIR)/kernel.make
else
obj-m		:= dbox2_fp.o dbox2_fp_napi.o dbox2_fp_input.o

dbox2_fp-objs		:= dbox2_fp_core.o dbox2_fp_sec.o dbox2_fp_timer.o\
			     dbox2_fp_tuner.o dbox2_fp_reset.o

dbox2_fp_input-objs	:= dbox2_fp_input_core.o dbox2_fp_rc.o dbox2_fp_keyboard.o\
			     dbox2_fp_button.o dbox2_fp_mouse.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb -I$(DRIVER_TOPDIR)/include
endif
