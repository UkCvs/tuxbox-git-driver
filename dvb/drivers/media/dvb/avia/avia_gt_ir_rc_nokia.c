/*
 * $Id: avia_gt_ir_rc_nokia.c,v 1.1.2.2 2005/02/09 18:46:16 carjay Exp $
 *
 * nokia (old) rc driver
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2005 Carsten Juttner (carjay@gmx.net)
 * Input event code copyright (C) 2002 Florian Schirmer <jolt@tuxbox.org>
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
#include <linux/module.h>
#include <linux/input.h>

#include <dbox/dbox2_fp_rc.h>

#include "avia_gt_ir_rc_nokia.h"

const static struct rc_key {
	unsigned long code;
	u8 value;
} rc_key_map[] = {
	{KEY_0,				0x00},
	{KEY_1,				0x01},
	{KEY_2,				0x02},
	{KEY_3,				0x03},
	{KEY_4,				0x04},
	{KEY_5,				0x05},
	{KEY_6,				0x06},
	{KEY_7,				0x07},
	{KEY_8,				0x08},
	{KEY_9,				0x09},
	{KEY_RIGHT, 		0x2E},
	{KEY_LEFT,			0x2F},
	{KEY_UP,			0x0E},
	{KEY_DOWN,			0x0F},
	{KEY_OK,			0x30},
	{KEY_MUTE,			0x28},
	{KEY_POWER,			0x0C},
	{KEY_GREEN,			0x55},
	{KEY_YELLOW,		0x52},
	{KEY_RED,			0x2d},
	{KEY_BLUE,			0x3b},
	{KEY_VOLUMEUP,		0x16},
	{KEY_VOLUMEDOWN,	0x17},
	{KEY_HELP,			0x82},
	{KEY_SETUP,			0x27},
	{KEY_HOME,			0x20},
	{KEY_PAGEDOWN,		0x53},
	{KEY_PAGEUP,		0x54},
};
#define RC_KEY_COUNT	ARRAY_SIZE(rc_key_map)

enum {
	KEY_RELEASED = 0,
	KEY_PRESSED,
	KEY_AUTOREPEAT
};

static int avia_gt_ir_rc_input_nokia(struct ir_rc_client *);
static int avia_gt_ir_rc_decode_nokia(struct ir_rc_client *, u32 pulselow, u32 pulsehigh);
struct ir_rc_client avia_gt_ir_rc_nokia_client = {
	.decode_func = avia_gt_ir_rc_decode_nokia,
	.input_func = avia_gt_ir_rc_input_nokia
};

static void rc_timeout(unsigned long data)
{
	struct ir_rc_client *irc = (struct ir_rc_client*) data;
	struct rc_key *last_key = (struct rc_key*) irc->priv;
	if (!last_key)
		return;
	/* "key released" event after timeout */
	dbox2_fp_rc_input_event(EV_KEY, last_key->code, KEY_RELEASED);
	last_key = NULL;
	irc->priv = (void *)last_key;
}

static int avia_gt_ir_rc_input_nokia(struct ir_rc_client *irc){
	const struct rc_key *key;
	const struct rc_key *last_key = (struct rc_key*)irc->priv;

	if (irc->received_code == 0xfffe){
		if (!last_key)	/* make code */
			return 0;
	} else {
		if ((irc->received_code&0xff00) != 0x5c00)
			return -EINVAL;
	}

	if ((irc->received_code==0xfffe)||(last_key && ((irc->received_code&0xff)!=last_key->value))) {
		if (timer_pending(&irc->timeout))
			del_timer_sync(&irc->timeout);
		if (last_key) {
			dbox2_fp_rc_input_event(EV_KEY, last_key->code, KEY_RELEASED);
			last_key = NULL;
			irc->priv = (void*)last_key;
		}
		if (irc->received_code == 0xfffe)
			return 0;
	}
	
	for (key = rc_key_map; key < &rc_key_map[RC_KEY_COUNT]; key++) {
		if (key->value == (irc->received_code & 0xff)) {
			if (timer_pending(&irc->timeout))
				del_timer_sync(&irc->timeout);
			if ((last_key) && (last_key->code == key->code)) {
				dbox2_fp_rc_input_event(EV_KEY, key->code, KEY_AUTOREPEAT);
			} else {
				dbox2_fp_rc_input_event(EV_KEY, key->code, KEY_PRESSED);
				last_key = key;
				irc->priv = (void *)last_key;
			}
			irc->timeout.function = rc_timeout;
			irc->timeout.expires = jiffies + (HZ / 4);	// in case the break code gets lost
			irc->timeout.data = (unsigned long)irc;
			add_timer(&irc->timeout);
			break;
		}
	}
	return 0;
}

static int avia_gt_ir_rc_decode_nokia(struct ir_rc_client *irc, u32 pulselow, u32 pulsehigh){
	static int skip;
	static int state;
	static u32 symbol;
	u32 filtered_low,filtered_high;

	filtered_low=pulselow+100;
	filtered_low/=430;
	filtered_high=pulsehigh+100;
	filtered_high/=550;

	/*
		gap between codes is 79500 but the Nokia RC sends:
			0xfffe - short pause - code - [gap] - [code] - ... - 0xfffe
	*/
	if (filtered_low>5){
		skip=0;state=0;
		return -EAGAIN;
	} else if (skip){
		return -EINVAL;
	}

	if (state > 32){	/* symb. too long */
		return -EINVAL;
	}
	
	if (filtered_low>90 && state){
		skip = 1;		/* data was lost */
		return -EINVAL;
	}
	
	if (!state && filtered_low != 5 ){
		skip = 1;
		return -EINVAL;
	} else if (!state && filtered_low == 5){
		state++;	/* symbol header */
		symbol=0;
		return -EAGAIN;
	}

	if (!filtered_high){
		skip = 1;	/* erraneous pulse */
		return -EINVAL;
	}

	/* collect the complete 8 symbol word (1 symbol = 2 bit) */
	state++;	/* first bit always low */
	if (filtered_low == 2)
		state++;
	symbol |= 1 << (33-state++);	/* state starts at 1 */
	if (filtered_high == 2){
		symbol |= 1 << (33-state++);
	}

	if (state > 32){
		/* symbol complete, now decode (phase shift) */
		int i;
		irc->received_code = 0;
		for (i=0;i<16;i++){
			irc->received_code <<=1;
			if ((symbol&0x3) == 2)
				irc->received_code |= 1;
			symbol >>=2;
		}
		return 0;
	}
	return -EAGAIN;
}
