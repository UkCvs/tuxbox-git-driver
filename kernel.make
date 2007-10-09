# common build includes for the tuxbox
# change CDKPREFIX and CVSROOT 
# according to your setup if
# necessary

# directory containing the cdk
CDKDIR:=$(HOME)/tuxbox/target
# directory containing the cvs sources
CVSDIR:=$(HOME)/tuxbox/cvs

#######################################
ARCH=ppc
CROSS_COMPILE=powerpc-tuxbox-linux-gnu-
#######################################
PATH:=$(CDKDIR)/cdk/bin:$(PATH)
CDKROOT:=$(CDKDIR)/cdkroot
MAKE=/usr/bin/make
#######################################
KERNEL_LOCATION=$(CVSDIR)/cdk/linux
INSTALL_MOD_PATH=$(CDKDIR)/cdkroot
#######################################

# we need to get rid of the ".." at the end to avoid having to rebuild
# everything in case we switch directories :S
DRIVER_TOPDIR:=$(shell $(DRIVER_TOPDIR)/unify_path $(DRIVER_TOPDIR))

export PATH
export CVSROOT
export MAKE
export ARCH
export CROSS_COMPILE
export KERNEL_LOCATION
export INSTALL_MOD_PATH
export DRIVER_TOPDIR

CONFIGFILE := $(DRIVER_TOPDIR)/.config

include $(CONFIGFILE)


all:
	@$(MAKE) -C $(KERNEL_LOCATION) M=$(DRIVER_TOPDIR) KBUILD_VERBOSE=0 modules

install: all
	@$(MAKE) -C $(KERNEL_LOCATION) M=$(DRIVER_TOPDIR) KBUILD_VERBOSE=1 modules_install

clean:
	@$(MAKE) -C $(KERNEL_LOCATION) M=$(shell pwd) KBUILD_VERBOSE=0 clean

$(DRIVER_TOPDIR)/.config:
	@echo export CONFIG_AVS_DEBUG=n 	> $(CONFIGFILE); \
	echo export CONFIG_CAM_DEBUG=n		>> $(CONFIGFILE); \
	echo export CONFIG_AVIA_DEBUG=n	>> $(CONFIGFILE); \
	echo export CONFIG_NAPI_DEBUG=n	>> $(CONFIGFILE); \
	echo export CONFIG_FP_DEBUG=n		>> $(CONFIGFILE); \
	echo export CONFIG_LCD_DEBUG=n		>> $(CONFIGFILE); \
	echo export CONFIG_SAA_DEBUG=n		>> $(CONFIGFILE);

# for CDK compatibility, there is no useable distclean from here
distclean:	clean

.PHONY:	clean
