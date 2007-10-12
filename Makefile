# Tuxbox drivers Makefile
# there are only three targets
#
# make all     - builds all modules
# make install - installs the modules
# make clean   - deletes modules recursively
#
# note that "clean" only works in the current
# directory while "all" and "install" will
# execute from the top dir.

ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)
include $(DRIVER_TOPDIR)/kernel.make
else
obj-y	:= info/
obj-y	+= fp/
obj-y	+= cam/
obj-y	+= avs/
obj-y	+= dvb/
obj-y	+= i2c/
obj-y	+= event/
obj-y	+= lcd/
obj-y	+= mmc/
obj-y	+= saa7126/
#obj-y	+= dvb2eth/
obj-y	+= ide/
obj-y	+= ext/
endif
