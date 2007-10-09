/*
 * $Id: dbox2_pll.h,v 1.1.2.5 2007/10/09 01:03:45 carjay Exp $
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

#ifdef __KERNEL__
#ifndef DBOX2_PLL_H
#define DBOX2_PLL_H
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <dvb-core/dvb_frontend.h>
struct pll_state {
	u32 clk;
	u8 tsa5059_xc;
	u8 i2c_addr;
};
int dbox2_pll_tsa5059_set_freq (struct pll_state *pll, struct dvb_frontend_parameters *params);
int dbox2_pll_tua6010_set_freq (struct pll_state *pll, struct dvb_frontend_parameters *params);
int dbox2_pll_init(struct i2c_adapter *s);
#endif
#endif
