/*
 * $Id: dbox2_fp_napi.c,v 1.1.2.5 2007/10/09 01:03:45 carjay Exp $
 *
 * Copyright (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/dvb/frontend.h>

#include <dbox2/dbox2_napi_core.h>
#include <dbox/dbox2_fp_core.h>
#include <dbox/dbox2_fp_sec.h>
#include <dbox/dbox2_fp_tuner.h>
#include <dvb-core/dvb_frontend.h>

#include "dbox2_fp_napi.h"

static struct dvb_adapter *dvb_adapter;

static inline
u32 unsigned_round_div(u32 n, u32 d)
{
	return (n + (d / 2)) / d;
}

/*
 * mitel sp5659
 * http://assets.zarlink.com/products/datasheets/zarlink_SP5659_MAY_02.pdf
 */

int dbox2_fp_napi_qam_set_freq(struct pll_state *pll, struct dvb_frontend_parameters *p)
{
	u32 freq = p->frequency;
	u32 div;
	u8 buf[4];

	div = unsigned_round_div(freq + 36125000, 62500);

	buf[0] = (div >> 8) & 0x7f;
	buf[1] = div & 0xff;
	buf[2] = 0x85 | ((div >> 10) & 0x60);
	buf[3] = (freq < 174000000 ? 0x02 :
		  freq < 455000000 ? 0x01 : 0x04);
			 
	//printk("freq = %d buf[3] = %x\n",freq,buf[3]);
	if (dbox2_fp_tuner_write_qam(buf, sizeof(buf)) < 0)
		return -EREMOTEIO;

	return 0;
}

/*
 * mitel sp5668
 * http://assets.zarlink.com/products/datasheets/zarlink_SP5668_JAN_01.pdf
 * 
 * [31:27] (= 0)
 * [26:24] port control bits (= 1)
 * [23:23] test mode enable (= 0)
 * [22:22] drive output disable switch (= 0)
 * [21:21] charge pump current select (= 1)
 * [20:18] reference divider ratio
 * [17:17] prescaler enable
 * [16:00] divider ratio
 */

int dbox2_fp_napi_qpsk_set_freq(struct pll_state *pll, struct dvb_frontend_parameters *p)
{
	static const u32 sp5668_ratios[] =
		{ 2000000, 1000000, 500000, 250000, 125000, 62500, 31250, 15625 };

	u32 freq = (p->frequency + 479500) * 1000;
	u32 div;
	u8 buf[4];

	int sel, pe;

	if (p->frequency > 3815467) {
		printk(KERN_ERR "frequency would lead to an integer overflow and is too high anyway\n");
		return -EINVAL;
	}

	if (freq >= 2000000000)
		pe = 1;
	else
		pe = 0;

	/*
	 * even the highest possible frequency which fits
	 * into 32 bit divided by the highest ratio
	 * is as low as 0x863, so no check for a negative
	 * sel value is needed below.
	 */
	for (sel = 7; sel >= 0; sel--)
		if ((div = unsigned_round_div(freq, sp5668_ratios[sel])) <= 0x3fff)
			break;

	printk(KERN_DEBUG "freq=%u ratio=%u div=%x pe=%hu\n",
			freq, sp5668_ratios[sel], div, pe);

	/* port control */
	buf[0] = 0x01;
	/* charge pump, ref div ratio, prescaler, div[16] */
	buf[1] = 0x20 | ((sel + pe) << 2) | (pe << 1) | ((div >> 16) & 0x01);
	/* div[15:8] */
	buf[2] = (div >> 8) & 0xff;
	/* div[7:0] */
	buf[3] = div & 0xff;

	if (dbox2_fp_tuner_write_qpsk(buf, sizeof(buf)) < 0)
		return -EREMOTEIO;

	return 0;
}

int dbox2_fp_napi_diseqc_send_master_cmd(struct dvb_frontend *fe, struct dvb_diseqc_master_cmd *cmd){
	return dbox2_fp_sec_diseqc_cmd(cmd->msg, cmd->msg_len);
}

int dbox2_fp_napi_diseqc_send_burst(struct dvb_frontend *fe, fe_sec_mini_cmd_t minicmd){
	int ret;
	switch (minicmd) {
	case SEC_MINI_A:
		ret = dbox2_fp_sec_diseqc_cmd("\x00\x00\x00\x00", 4);
		break;
	case SEC_MINI_B:
		ret = dbox2_fp_sec_diseqc_cmd("\xff", 1);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

int dbox2_fp_napi_set_tone(struct dvb_frontend *fe, fe_sec_tone_mode_t tone){
	int ret;
	switch (tone) {
	case SEC_TONE_OFF:
		ret = dbox2_fp_sec_set_tone(0);
		break;
	case SEC_TONE_ON:
		ret = dbox2_fp_sec_set_tone(1);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

int dbox2_fp_napi_set_voltage(struct dvb_frontend *fe, fe_sec_voltage_t voltage){
	int ret;
	switch (voltage) {
	case SEC_VOLTAGE_13:
		ret = dbox2_fp_sec_set_voltage(0);
		break;
	case SEC_VOLTAGE_18:
		ret = dbox2_fp_sec_set_voltage(1);
		break;
	case SEC_VOLTAGE_OFF:
		ret = dbox2_fp_sec_set_power(0);
		break;		
	default:
		ret = -EINVAL;
	}
	return ret;
}

int dbox2_fp_napi_enable_high_lnb_voltage(struct dvb_frontend *fe, long arg){
	return dbox2_fp_sec_set_high_voltage(arg ? 1 : 0);
}

int dbox2_fp_napi_get_sec_ops(struct dvb_frontend_ops *ops)
{
	ops->diseqc_send_master_cmd = dbox2_fp_napi_diseqc_send_master_cmd;
	ops->diseqc_send_burst = dbox2_fp_napi_diseqc_send_burst;
	ops->set_tone = dbox2_fp_napi_set_tone;
	ops->set_voltage = dbox2_fp_napi_set_voltage;
	ops->enable_high_lnb_voltage = dbox2_fp_napi_enable_high_lnb_voltage;
	return 0;
}

int dbox2_fp_napi_init(void)
{
	dvb_adapter = dbox2_napi_get_adapter();

	if (!dvb_adapter)
		return -EINVAL;

	return 0;
}

void dbox2_fp_napi_exit(void)
{
}

#if 0
module_init(dbox2_fp_napi_init);
module_exit(dbox2_fp_napi_exit);

EXPORT_SYMBOL(dbox2_fp_napi_get_sec_ops);
EXPORT_SYMBOL(dbox2_fp_napi_qam_set_freq);
EXPORT_SYMBOL(dbox2_fp_napi_qpsk_set_freq);

MODULE_AUTHOR("Andreas Oberritter <obi@saftware.de>");
MODULE_DESCRIPTION("dbox2 fp dvb api driver");
MODULE_LICENSE("GPL");
#endif
