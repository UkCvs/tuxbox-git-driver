ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/../../../../..
include $(DRIVER_TOPDIR)/kernel.make
else
# .config is used to create the avia_gt_config.h-header
CONFIG_FILE:=$(src)/.config
AVIA_GT_CONFIG_FILE:=$(src)/avia_gt_config.h

# if config file doesn't exist a default will get generated
include $(CONFIG_FILE)

# objects always generated, 
#     obj-m are always standalone,
#     *-misc-objs only if configured that way
obj-m		:= avia_av.o avia_gt.o

avia_av-objs	:= avia_av_core.o avia_av_ksyms.o avia_av_debug.o
av-misc		:= avia_av_event.o avia_av_proc.o
avia_gt-objs	:= avia_gt_core.o avia_gt_enx.o avia_gt_gtx.o\
		    avia_gt_ucode.o avia_gt_ucode_firmware.o
avia_gt_fb-objs	:= avia_gt_fb_core.o

gt-misc		:= avia_gt_accel.o

# objects generated according to .config
ifeq ($(CONFIG_AVIA_GT_CAPTURE),y)
gt-misc		+= avia_gt_capture.o avia_gt_pig.o
obj-m		+= avia_gt_v4l2.o
endif
ifeq ($(CONFIG_AVIA_GT_DMX),y)
gt-misc		+= avia_gt_dmx.o avia_gt_vbi.o
endif
ifeq ($(CONFIG_AVIA_GT_GV),y)
gt-misc		+= avia_gt_gv.o
obj-m		+= avia_gt_fb.o
endif
ifeq ($(CONFIG_AVIA_GT_IR),y)
gt-misc		+= avia_gt_ir.o
avia_gt_ir_rc-objs = avia_gt_ir_rc_core.o avia_gt_ir_rc_nokia.o
obj-m		+= avia_gt_ir_samsung.o avia_gt_lirc.o avia_gt_ir_rc.o
endif
ifeq ($(CONFIG_AVIA_GT_PCM),y)
gt-misc		+= avia_gt_pcm.o
obj-m		+= avia_gt_oss.o
endif

CC		:= $(CROSS_COMPILE)gcc -I$(TOPDIR)/dvb/include

EXTRA_CFLAGS	+= -I$(DRIVER_TOPDIR)/include -I$(DRIVER_TOPDIR)/dvb/include \
		    -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb

ifeq ($(CONFIG_AVIA_DEBUG),y)
EXTRA_CFLAGS	+= -DDEBUG
endif

ifeq ($(CONFIG_AVIA_STANDALONE),y)
obj-m		+= $(av-misc) $(gt-misc)
EXTRA_CFLAGS	+= -DSTANDALONE
else
avia_av-objs	+= $(av-misc)
avia_gt-objs	+= $(gt-misc)
endif

#
# all avia_gt objects depend on the configuration file
#
$(addprefix $(obj)/,$(obj-m) $(avia_av-objs) $(avia_gt-objs)): $(AVIA_GT_CONFIG_FILE)

#
# create a custom .config if it doesn't exist
#
$(CONFIG_FILE):
	@if [ ! -e $(CONFIG_FILE) ]; then				\
	echo "          Creating default .config-file";			\
	echo \# CONFIG_AVIA_DEBUG is not set	   > $(CONFIG_FILE);	\
	echo CONFIG_AVIA_GT_CAPTURE=y		  >> $(CONFIG_FILE);	\
	echo CONFIG_AVIA_GT_DMX=y		  >> $(CONFIG_FILE);	\
	echo CONFIG_AVIA_GT_GV=y		  >> $(CONFIG_FILE);	\
	echo CONFIG_AVIA_GT_IR=y		  >> $(CONFIG_FILE);	\
	echo CONFIG_AVIA_GT_PCM=y		  >> $(CONFIG_FILE);	\
	echo \# CONFIG_AVIA_STANDALONE is not set >> $(CONFIG_FILE);	\
	fi

#
# recreate the configuration header every time .config changes
#     also remove all avia_*.mod-files to avoid pure object files being 
#     treated as modules (*.ko)
#
$(AVIA_GT_CONFIG_FILE): $(CONFIG_FILE)
	@if [ -d $(MODVERDIR) ]; then 		\
		rm -f $(MODVERDIR)/avia_*.mod; 	\
		rm -f $(obj)/avia_*.ko; 	\
	fi
	@echo "          Creating $(AVIA_GT_CONFIG_FILE)"
	@echo \#ifndef __avia_gt_config_h__		>  $(AVIA_GT_CONFIG_FILE)
	@echo \#define __avia_gt_config_h__		>> $(AVIA_GT_CONFIG_FILE)
	@if [ "$(CONFIG_AVIA_GT_CAPTURE)" = "y" ]; then					\
		echo \#define CONFIG_AVIA_GT_CAPTURE 1	>> $(AVIA_GT_CONFIG_FILE);	\
	else										\
		echo \#undef CONFIG_AVIA_GT_CAPTURE	>> $(AVIA_GT_CONFIG_FILE);	\
	fi
	@if [ "$(CONFIG_AVIA_GT_DMX)" = "y" ]; then					\
		echo \#define CONFIG_AVIA_GT_DMX 1	>> $(AVIA_GT_CONFIG_FILE);	\
	else										\
		echo \#undef CONFIG_AVIA_GT_DMX		>> $(AVIA_GT_CONFIG_FILE);	\
	fi
	 @if [ "$(CONFIG_AVIA_GT_GV)" = "y" ]; then					\
		echo \#define CONFIG_AVIA_GT_GV 1	>> $(AVIA_GT_CONFIG_FILE);	\
	else										\
		echo \#undef CONFIG_AVIA_GT_GV		>> $(AVIA_GT_CONFIG_FILE);	\
	fi
	@if [ "$(CONFIG_AVIA_GT_IR)" = "y" ]; then					\
		echo \#define CONFIG_AVIA_GT_IR 1	>> $(AVIA_GT_CONFIG_FILE);	\
	else										\
		echo \#undef CONFIG_AVIA_GT_IR		>> $(AVIA_GT_CONFIG_FILE);	\
	fi
	@if [ "$(CONFIG_AVIA_GT_PCM)" = "y" ]; then					\
		echo \#define CONFIG_AVIA_GT_PCM 1	>> $(AVIA_GT_CONFIG_FILE);	\
	else										\
		echo \#undef CONFIG_AVIA_GT_PCM		>> $(AVIA_GT_CONFIG_FILE);	\
	fi
	@echo \#endif					>> $(AVIA_GT_CONFIG_FILE)

clean-files	:= avia_gt_config.h

endif
