/*
 * $Id: dbox2_fp_keyboard.c,v 1.7 2006/03/26 21:05:33 barf Exp $
 *
 * Copyright (C) 2002 by Florian Schirmer <jolt@tuxbox.org>
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>

#include <dbox/dbox2_fp_core.h>

static u16 keyboard_code_map[] = {

/* 000-009 */	KEY_RESERVED, KEY_ESC, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8,
/* 010-019 */	KEY_F9, KEY_F10, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 020-029 */	KEY_GRAVE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
/* 030-039 */	KEY_0, KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE, KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T,
/* 040-049 */	KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_BACKSLASH, KEY_CAPSLOCK, KEY_A,
/* 050-059 */	KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE,
/* 060-069 */	KEY_ENTER, KEY_LEFTSHIFT, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA,
/* 070-079 */	KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_RESERVED, KEY_LEFTCTRL, KEY_LEFTMETA, KEY_LEFTALT, KEY_SPACE, KEY_RIGHTALT, KEY_RESERVED,
/* 080-089 */	KEY_RIGHTMETA, KEY_RESERVED, KEY_SYSRQ, KEY_SCROLLLOCK, KEY_PAUSE, KEY_INSERT, KEY_RESERVED, KEY_RESERVED, KEY_DELETE, KEY_RESERVED,
/* 090-099 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_UP, KEY_102ND, KEY_LEFT, KEY_DOWN, KEY_RIGHT,
/* 100-109 */	KEY_NUMLOCK, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 110-119 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 120-127 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, BTN_RIGHT, BTN_LEFT,

};

static u16 fn_keyboard_code_map[] = {

/* 000-009 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 010-019 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 020-029 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_KP7, KEY_KP8, KEY_KP9,
/* 030-039 */	KEY_KPASTERISK, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 040-049 */	KEY_RESERVED, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KPMINUS, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 050-059 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KPPLUS, KEY_RESERVED,
/* 060-069 */	KEY_KPENTER, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_KP0, KEY_RESERVED,
/* 070-079 */	KEY_KPDOT, KEY_KPSLASH, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 080-089 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 090-099 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_PAGEUP, KEY_RESERVED, KEY_HOME, KEY_PAGEDOWN, KEY_END,
/* 100-109 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 110-119 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
/* 120-127 */	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,

};

#define KEYBOARD_CODE_COUNT	(sizeof(keyboard_code_map) / sizeof(keyboard_code_map[0]))

static struct input_dev *keyboard_input_dev;
static u8 fn_pressed;

//#define dprintk printk
#define dprintk if (0) printk

static void dbox2_fp_keyboard_queue_handler(u8 queue_nr)
{
	u16 scancode = 0xFFFF;

//	dprintk("event on queue %d\n", queue_nr);

	switch (mid) {

		case TUXBOX_DBOX2_MID_NOKIA:

			fp_cmd(fp_get_i2c(), 0x03, (u8 *)&scancode, 2);
	
			break;

		case TUXBOX_DBOX2_MID_PHILIPS:
		case TUXBOX_DBOX2_MID_SAGEM:
	
			fp_cmd(fp_get_i2c(), 0x28, (u8 *)&scancode, 2);
	
			break;
			
	}

	if ((scancode & 0x7F) == 0x49) {

		fn_pressed = !(scancode & 0x80);

		return;

	}

	dprintk("keyboard scancode: 0x%02x (%d) up/down: %s fn: %d\n", scancode & 0x7F, scancode & 0x7F, scancode & 0x80 ? "up" : "down", !!fn_pressed);

	if (keyboard_code_map[scancode & 0x7F] != KEY_RESERVED || fn_keyboard_code_map[scancode & 0x7F] != KEY_RESERVED) {

		if (!(scancode & 0x80))
			clear_bit(fn_pressed?fn_keyboard_code_map[scancode & 0x7F]:keyboard_code_map[scancode & 0x7F], keyboard_input_dev->key);
																				
		input_event(keyboard_input_dev, EV_KEY, fn_pressed?fn_keyboard_code_map[scancode & 0x7F]:keyboard_code_map[scancode & 0x7F], !(scancode & 0x80));

		/* need to clear KEY_PAGEUP, KEY_HOME, KEY_PAGEDOWN, KEY_END manually, because fn bit missing on release */
		if ((scancode & 0x80) && ((scancode & 0x7F) >= 95 && (scancode & 0x7F) <= 99))
			input_event(keyboard_input_dev, EV_KEY, fn_keyboard_code_map[scancode & 0x7F], !(scancode & 0x80));
	}

}

int __init dbox2_fp_keyboard_init(struct input_dev *input_dev)
{
	u16 code_nr;

	keyboard_input_dev = input_dev;

	set_bit(EV_KEY, keyboard_input_dev->evbit);

	for (code_nr = 0; code_nr < KEYBOARD_CODE_COUNT; code_nr++) {

		if (keyboard_code_map[code_nr] != KEY_RESERVED)
			set_bit(keyboard_code_map[code_nr], keyboard_input_dev->keybit);

		if (fn_keyboard_code_map[code_nr] != KEY_RESERVED)
			set_bit(fn_keyboard_code_map[code_nr], keyboard_input_dev->keybit);

	}

	dbox2_fp_queue_alloc(1, dbox2_fp_keyboard_queue_handler);

	return 0;

}

void __exit dbox2_fp_keyboard_exit(void)
{

	dbox2_fp_queue_free(1);

}
