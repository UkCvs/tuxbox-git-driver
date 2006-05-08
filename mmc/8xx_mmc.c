/*
 * $Id: 8xx_mmc.c,v 1.1.2.1 2006/05/08 23:21:27 carjay Exp $
 *
 * PPC 8xx GPIO SPI driver for MMC
 *
 * Copyright (C) 2006 Carsten Juttner <carjay@gmx.net>
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
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>

#include "8xx_mmc.h"

int mmc_spi_hardware_init(void)
{
	volatile immap_t *immap = (immap_t*)IMAP_ADDR;
	volatile cpm8xx_t *cp = (cpm8xx_t *) & immap->im_cpm;
	volatile iop8xx_t *cpi = (iop8xx_t *) & immap->im_ioport;

	cpi->iop_papar &= ~(SD_DO | SD_DI);
	cpi->iop_paodr &= ~(SD_DO);
	cpi->iop_padir |= SD_DI;
	cpi->iop_padir &= ~SD_DO;
	cpi->iop_padat &= ~SD_DI;
	cpi->iop_padat &= ~SD_DI;

#ifdef CLK_PC4
	cp->cp_pbpar &= ~SD_CS;
	cp->cp_pbodr &= ~SD_CS;
	cp->cp_pbdir |= SD_CS;
	cp->cp_pbdat &= ~SD_CS;

	cpi->iop_pcpar &= ~SD_CLK;	/* GPIO                */
	cpi->iop_pcdir |= SD_CLK;	/* output!    */
	cpi->iop_pcso &= ~SD_CLK;	/* for clarity */
	cpi->iop_pcdat &= ~SD_CLK;
#else
	cp->cp_pbpar &= ~(SD_CLK | SD_CS);
	cp->cp_pbodr &= ~(SD_CLK | SD_CS);
	cp->cp_pbdir |= (SD_CLK | SD_CS);

	// Clock + CS low
	cp->cp_pbdat &= ~(SD_CLK | SD_CS);
#endif
	return 0;
}

unsigned char mmc_spi_io(unsigned char data_out)
{
	volatile immap_t *immap = (immap_t*)IMAP_ADDR;
#ifndef CLK_PC4
	volatile cpm8xx_t *cp = (cpm8xx_t *) & immap->im_cpm;
#endif
	volatile iop8xx_t *cpi = (iop8xx_t *) & immap->im_ioport;
	unsigned char result = 0;
	unsigned char i;

	for (i = 0x80; i != 0; i >>= 1) {
		if (data_out & i)
			cpi->iop_padat |= SD_DI;
		else
			cpi->iop_padat &= ~SD_DI;

#ifdef CLK_PC4
		cpi->iop_pcdat |= SD_CLK;
#else
		cp->cp_pbdat |= SD_CLK;
#endif
		if (cpi->iop_padat & SD_DO) {
			result |= i;
		}
#ifdef CLK_PC4
		cpi->iop_pcdat &= ~SD_CLK;
#else
		cp->cp_pbdat &= ~SD_CLK;
#endif
	}

	return result;
}
