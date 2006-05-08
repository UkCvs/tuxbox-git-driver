ifeq ($(KERNELRELEASE),)
DRIVER_TOPDIR:=$(shell pwd)/..
include $(DRIVER_TOPDIR)/kernel.make
else
obj-m					+= mmc_spi_block.o
mmc_spi_block-objs	+= mmc_spi_io.o 8xx_mmc.o mmc_spi_block_core.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/include

endif
