/*
 * $Id: dbox2_pll.c,v 1.1.2.1 2005/01/31 03:04:12 carjay Exp $
 *
 * Dbox2 PLL driver collection
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2005 Carsten Juttner <carjay@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>

/*
	PLLs are really a mess with the dbox2:
	FEs:
	Satellite:
		- VES1993 (Sagem and 'modded' Nokia)
			TSA5059 I2C-bus (CPU)
		- VES1893 (Nokia)
			SP5668 SPI (FP)

	Cable:
		- VES1820 (Nokia)
			SP5659 SPI (FP)
		- STV0297 (developer Philips)

	Terrestrial:
		- SQC6100 (developer)
	

	NB: in case the I2C-bus is used the demods "protect" the PLL from
		unnecessary bus traffic by disabling the bus. This is being
		taken care of in the demod driver, so doesn't need to be taken
		into account here.
*/

#include <dvb-core/dvb_frontend.h>

static struct i2c_adapter *adap;

/***************/
/* generic I2c */
/***************/

int dbox2_pll_i2c_write(int i2c_addr, char *buf, int len)
{
	int ret;
	struct i2c_msg msg = {
		.addr = i2c_addr,
		.flags = 0,
		.buf = buf,
		.len = len
	};
	if (!adap){
		printk(KERN_ERR "dbox2_napi_pll: no i2c_adapter set\n");
		return -EIO;
	}
	ret = i2c_transfer(adap,&msg,1);
	if (ret!=1){
		printk(KERN_ERR "dbox2_napi_pll: error writing to i2c client\n");
		return -EIO;
	}
	return 0;	
}

/************/
/* TSA 5059 */
/************/

#define TSA5059_I2C_ADDR	(0xc0>>1)
int dbox2_pll_tsa5059_set_freq (struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	u32 freq = p->frequency;
	u8 buf[2];
	freq /= 1000;
	buf[0] = (freq>>8) & 0x7f;
	buf[1] = freq & 0xff;
	return dbox2_pll_i2c_write(TSA5059_I2C_ADDR|1,buf,sizeof(buf));
}

int dbox2_pll_tsa5059_nokia_init (struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	char buf[]="\x06\x5c\x83\x60";
	return dbox2_pll_i2c_write(TSA5059_I2C_ADDR|1,buf,4);
}

int dbox2_pll_tsa5059_sagem_init (struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	char buf[]="\x25\x70\x92\x40";
	return dbox2_pll_i2c_write(TSA5059_I2C_ADDR|1,buf,4);
}

/******************/
/* Initialisation */
/******************/

int dbox2_pll_init(struct i2c_adapter *a)
{
	adap=a;
	return 0;
}
