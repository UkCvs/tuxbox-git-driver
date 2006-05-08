#ifndef _8XX_MMC_H_
#define _8XX_MMC_H_

/*
 * $Id: 8xx_mmc.h,v 1.1.2.1 2006/05/08 23:21:27 carjay Exp $
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

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>

/* on SD/MMC card pin 7 */
#define SD_DO	0x0040

/* on SD/MMC card pin 2 */
#define SD_DI  	0x0080

/* on SD/MMC card pin 1 */
#define SD_CS  	0x8000

#define CLK_PC4
/* if defined, SD_CLK is PC4, else it's PB16 */
#ifdef CLK_PC4
/* on SD/MMC card pin 5 */
#define SD_CLK 0x0800
#else
#define SD_CLK 0x4000
#endif

static inline void mmc_spi_cs_low(void)
{
	volatile immap_t *immap = (immap_t *) IMAP_ADDR;
	volatile cpm8xx_t *cp = &immap->im_cpm;
	cp->cp_pbdat &= ~SD_CS;
}

static inline void mmc_spi_cs_high(void)
{
	volatile immap_t *immap = (immap_t *) IMAP_ADDR;
	volatile cpm8xx_t *cp = &immap->im_cpm;
	cp->cp_pbdat |= SD_CS;
}

extern int mmc_spi_hardware_init(void);
extern unsigned char mmc_spi_io(u8 data);

#endif
