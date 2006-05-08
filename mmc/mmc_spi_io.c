#include <linux/delay.h>
#include <linux/timer.h>

/*
 * $Id: mmc_spi_io.c,v 1.1.2.1 2006/05/08 23:21:27 carjay Exp $
 *
 * Block device driver for a MMC/SD card in SPI mode using GPIOs
 * MMC IO routines
 *
 * Linux 2.4 driver copyright Madsuk,Rohde,TaGana
 * Linux 2.6 driver changes added by Carsten Juttner <carjay@gmx.net>
 * This implementation does not use the 2.6 MMC subsystem (yet).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>

#include "8xx_mmc.h"
#include "mmc_spi_io.h"

int mmc_spi_write_block(unsigned int dest_addr, unsigned char *data)
{
	unsigned int address;
	unsigned char r = 0;
	unsigned char ab0, ab1, ab2, ab3;
	int i;

	address = dest_addr;

	ab3 = 0xff & (address >> 24);
	ab2 = 0xff & (address >> 16);
	ab1 = 0xff & (address >> 8);
	ab0 = 0xff & address;
	mmc_spi_cs_low();
	for (i = 0; i < 4; i++)
		mmc_spi_io(0xff);
	mmc_spi_io(0x58);
	mmc_spi_io(ab3);	/* msb */
	mmc_spi_io(ab2);
	mmc_spi_io(ab1);
	mmc_spi_io(ab0);	/* lsb */
	mmc_spi_io(0xff);
	for (i = 0; i < 8; i++) {
		r = mmc_spi_io(0xff);
		if (r == 0x00)
			break;
	}
	if (r != 0x00) {
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		return (1);
	}

	mmc_spi_io(0xfe);
	for (i = 0; i < 512; i++)
		mmc_spi_io(data[i]);
	for (i = 0; i < 2; i++)
		mmc_spi_io(0xff);

	for (i = 0; i < 1000000; i++) {
		r = mmc_spi_io(0xff);
		if (r == 0xff)
			break;
	}
	if (r != 0xff) {
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		return (3);
	}
	mmc_spi_cs_high();
	mmc_spi_io(0xff);
	return (0);
}

int mmc_spi_read_block(unsigned char *data, unsigned int src_addr)
{
	unsigned int address;
	unsigned char r = 0;
	unsigned char ab0, ab1, ab2, ab3;
	int i;

	address = src_addr;

	ab3 = 0xff & (address >> 24);
	ab2 = 0xff & (address >> 16);
	ab1 = 0xff & (address >> 8);
	ab0 = 0xff & address;

	mmc_spi_cs_low();
	for (i = 0; i < 4; i++)
		mmc_spi_io(0xff);
	mmc_spi_io(0x51);
	mmc_spi_io(ab3);	/* msb */
	mmc_spi_io(ab2);
	mmc_spi_io(ab1);
	mmc_spi_io(ab0);	/* lsb */

	mmc_spi_io(0xff);
	for (i = 0; i < 8; i++) {
		r = mmc_spi_io(0xff);
		if (r == 0x00)
			break;
	}
	if (r != 0x00) {
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		return (1);
	}
	for (i = 0; i < 100000; i++) {
		r = mmc_spi_io(0xff);
		if (r == 0xfe)
			break;
	}
	if (r != 0xfe) {
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		return (2);
	}
	for (i = 0; i < 512; i++) {
		r = mmc_spi_io(0xff);
		data[i] = r;
	}
	for (i = 0; i < 2; i++) {
		r = mmc_spi_io(0xff);
	}
	mmc_spi_cs_high();
	mmc_spi_io(0xff);

	return (0);
}

int mmc_spi_card_init(void)
{
	unsigned char r = 0;
	short i, j;
	unsigned long flags;

	local_irq_save(flags);

	mmc_spi_cs_high();
	for (i = 0; i < 1000; i++)
		mmc_spi_io(0xff);

	mmc_spi_cs_low();

	mmc_spi_io(0x40);
	for (i = 0; i < 4; i++)
		mmc_spi_io(0x00);
	mmc_spi_io(0x95);
	for (i = 0; i < 8; i++) {
		r = mmc_spi_io(0xff);
		if (r == 0x01)
			break;
	}
	mmc_spi_cs_high();
	mmc_spi_io(0xff);
	if (r != 0x01) {
		local_irq_restore(flags);
		return (1);
	}

	for (j = 0; j < 30000; j++) {
		mmc_spi_cs_low();

		mmc_spi_io(0x41);
		for (i = 0; i < 4; i++)
			mmc_spi_io(0x00);
		mmc_spi_io(0xff);
		for (i = 0; i < 8; i++) {
			r = mmc_spi_io(0xff);
			if (r == 0x00)
				break;
		}
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		if (r == 0x00) {
			local_irq_restore(flags);
			return (0);
		}
	}
	local_irq_restore(flags);
	return (2);
}

#if 0
int mmc_spi_card_config(void)
{
	unsigned char r = 0;
	short i;
	unsigned char csd[32];
	unsigned int c_size;
	unsigned int c_size_mult;
	unsigned int mult;
	unsigned int read_bl_len;
	unsigned int blocknr = 0;
	unsigned int block_len = 0;
	unsigned int size = 0;

	mmc_spi_cs_low();
	for (i = 0; i < 4; i++)
		mmc_spi_io(0xff);
	mmc_spi_io(0x49);
	for (i = 0; i < 4; i++)
		mmc_spi_io(0x00);
	mmc_spi_io(0xff);
	for (i = 0; i < 8; i++) {
		r = mmc_spi_io(0xff);
		if (r == 0x00)
			break;
	}
	if (r != 0x00) {
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		return (1);
	}
	for (i = 0; i < 8; i++) {
		r = mmc_spi_io(0xff);
		if (r == 0xfe)
			break;
	}
	if (r != 0xfe) {
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		return (2);
	}
	for (i = 0; i < 16; i++) {
		r = mmc_spi_io(0xff);
		csd[i] = r;
	}
	for (i = 0; i < 2; i++) {
		r = mmc_spi_io(0xff);
	}
	mmc_spi_cs_high();
	mmc_spi_io(0xff);
	if (r == 0x00)
		return (3);

	c_size = csd[8] + csd[7] * 256 + (csd[6] & 0x03) * 256 * 256;
	c_size >>= 6;
	c_size_mult = csd[10] + (csd[9] & 0x03) * 256;
	c_size_mult >>= 7;
	read_bl_len = csd[5] & 0x0f;
	mult = 1;
	mult <<= c_size_mult + 2;
	blocknr = (c_size + 1) * mult;
	block_len = 1;
	block_len <<= read_bl_len;
	size = block_len * blocknr;
	size >>= 10;

	for (i = 0; i < (1 << 6); i++) {
		hd_blocksizes[i] = 1024;
		hd_hardsectsizes[i] = block_len;
		hd_maxsect[i] = 256;
	}
	hd_sizes[0] = size;
	hd[0].nr_sects = blocknr;

	printk(KERN_INFO "Size = %d, hardsectsize = %d, sectors = %d\n", size, block_len, blocknr);

	return 0;
}
#endif
