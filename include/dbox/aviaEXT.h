/*
 * Extension device for non-API covered stuff for the Avia
 * (hopefully will disappear at some point)
 *
 * $Id: aviaEXT.h,v 1.2.2.1 2006/12/08 23:20:17 carjay Exp $
 *
 * Copyright (C) 2004 Carsten Juttner <carjay@gmx.met>
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

#ifndef AVIAEXT_H
#define AVIAEXT_H

/* 
	exported from avia_gt_napi.c, 
	but I don't like them to be there so they are not
	"officially" exported
*/
extern int avia_gt_get_playback_mode(void);
extern void avia_gt_set_playback_mode(int);

/* turns on/off the optical audio output (Avia600 only) */
#define AVIA_EXT_MAGIC	'o'
#define AVIA_EXT_IEC_SET	_IO(AVIA_EXT_MAGIC, 61) /* int */
#define AVIA_EXT_IEC_GET	_IO(AVIA_EXT_MAGIC, 62) /* int */

/* sets the avia decoding mode (dual pes or single ts) */
#define AVIA_EXT_AVIA_PLAYBACK_MODE_SET	_IO(AVIA_EXT_MAGIC, 63) /* int */
#define AVIA_EXT_AVIA_PLAYBACK_MODE_GET	_IO(AVIA_EXT_MAGIC, 64) /* int */

/* debug stuff */
#define AVIA_EXT_MEM_CMD _IO(AVIA_EXT_MAGIC, 65) /* unsigned int */

struct cmdheader {
	unsigned int cmd;
	unsigned int length;
};

#define AVIA_EXT_MEM_GBUS_READ	0x01
#define AVIA_EXT_MEM_GBUS_WRITE 0x02
struct cmd_gbus {
	struct cmdheader header;
	char start; /* first gbus register to read */
	char end;	/* last gbus register to read */
	unsigned int *buffer; /* buffer in userspace */
};

#endif /* AVIAEXT_H */
