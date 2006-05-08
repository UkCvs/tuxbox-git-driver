#ifndef _MMC_SPI_IO_H_
#define _MMC_SPI_IO_H_

/*
 * $Id: mmc_spi_io.h,v 1.1.2.1 2006/05/08 23:21:27 carjay Exp $
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

extern int mmc_spi_write_block(unsigned int dest_addr, unsigned char *data);
extern int mmc_spi_read_block(unsigned char *data, unsigned int src_addr);
extern int mmc_spi_card_init(void);
//static int mmc_card_config(void);
#endif
