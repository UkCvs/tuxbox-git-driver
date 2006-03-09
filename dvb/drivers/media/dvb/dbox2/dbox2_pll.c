/*
 * $Id: dbox2_pll.c,v 1.1.2.7 2006/03/09 19:32:20 carjay Exp $
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

#include "dbox2_pll.h"

#ifdef DEBUG
#define dprintk(fmt, args...)	printk(fmt,##args)
#else
#define dprintk(fmt, args...)
#endif

/*
	PLLs are really a mess with the dbox2:
	FEs:
	Satellite:
		- VES1993 (Sagem and 'modded' Nokia)
			TSA5059 I2C-bus (CPU)
		- VES1893 (Nokia)
			SP5668 SPI (FP)
		- TDA8044 (Philips)
			TSA5059 I2C-bus (CPU)

	Cable:
		- AT76C651 (Sagem)
			TUA6010 I2C-bus (CPU)
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
/*********************/
/* generic functions */
/*********************/

static __inline__ u32 dbox2_pll_div(u32 a, u32 b)
{
	return (a + (b / 2)) / b;
}

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

int dbox2_pll_tsa5059_set_freq (struct pll_state *pll, struct dvb_frontend_parameters *p)
{
	u8 buf[4];
	u32 ref=0;
	u8 cp;
	u8 pe;
	u8 r=0;
	int diff;
	int i;
	u32 freq = p->frequency;
	u32 pll_clk = pll->clk;
	
	u16 ratio[]={
		 2, 4, 8,16,32,
		24, 5,10,20,40
	};
	
 	if (freq < 1100000)		/*  555uA */
 		cp = 2;
 	else if (freq < 1200000)	/*  260uA */
		cp = 1;
	else if (freq < 1600000)	/*  120uA */
		cp = 0;
	else if (freq < 1800000)	/*  260uA */
		cp = 1;
	else if (freq < 2000000)	/*  555uA */
		cp = 2;
	else				/* 1200uA */
		cp = 3;

	if (freq <= 2300000)
		pe = 0;
	else if (freq <= 2700000)
		pe = 1;
	else
		return -EINVAL;

	diff = INT_MAX;

	/* allow 2000kHz - 100kHz */
	for (i = 0; i < ARRAY_SIZE(ratio); i++) {
		u32 cfreq, tmpref;
		int tmpdiff;
		
		if (!ratio[i])
			continue;
		
		cfreq = dbox2_pll_div(pll_clk, ratio[i]);
		tmpref = dbox2_pll_div((freq * 1000), (cfreq << pe));
		tmpdiff = (freq * 1000) - (tmpref * (cfreq << pe));

		if (abs(tmpdiff) > abs(diff))
			continue;

		diff = tmpdiff;
		ref = tmpref;
		r = i;

		if (diff == 0)
			break;
	}
	if (r>4)
		r+=3;
	dprintk("dbox2_pll: tsa5059: Ref: %d kHz ofreq: %d kHz diff: %d Hz ref: %d kHz\n",
				((pll_clk/ratio[r>4?r-3:r]<<pe)/1000),freq,diff,ref*(pll_clk/ratio[r>4?r-3:r]<<pe)/1000);
	
	buf[0] = (ref >> 8) & 0x7f;
	buf[1] = ref & 0xff;
	buf[2] = 0x80 | ((ref >> 10) & 0x60) | (pe << 4) | r;
	buf[3] = (cp << 6) | ((pll->tsa5059_xc&0x03)<<4);

	return dbox2_pll_i2c_write(pll->i2c_addr,buf,sizeof(buf));
}

/************/
/* TUA 6010 */
/************/

int dbox2_pll_tua6010_set_freq(struct pll_state *pll, struct dvb_frontend_parameters *p)
{
	u32 div;
	u8 buf[4];
	u8 vu, p2, p1, p0;
	u32 freq = p->frequency;
	
	/* 47 MHz ... 862 MHz */
	if ((freq < 47000000) || (freq > 862000000))
		return -EINVAL;

	div = (freq + 36150000) / 62500;

	if (freq > 401250000)
		vu = 1;	/* UHF */
	else
		vu = 0; /* VHF */

	if (freq > 401250000)
		p2 = 1, p1 = 0, p0 = 1;
	else if (freq > 117250000)
		p2 = 1, p1 = 1, p0 = 0;
	else
		p2 = 0, p1 = 1, p0 = 1;

	buf[0] = (div >> 8) & 0x7f;
	buf[1] = (div >> 0) & 0xff;
	buf[2] = 0x8e;
	buf[3] = (vu << 7) | (p2 << 2) | (p1 << 1) | p0;

	return dbox2_pll_i2c_write(pll->i2c_addr, buf, sizeof(buf));
}

/******************/
/* Initialisation */
/******************/

int dbox2_pll_init(struct i2c_adapter *a)
{
	adap=a;
	return 0;
}
