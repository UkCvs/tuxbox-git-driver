/*
 * $Id: avia_gt_ir_rc_core.h,v 1.1.2.1 2005/02/09 05:09:57 carjay Exp $
 *
 * alternative remote control driver
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2005 Carsten Juttner (carjay@gmx.net)
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
#ifndef __avia_gt_ir_rc_h__
#define __avia_gt_ir_rc_h__

/* simple struct keeping all data about one client */
struct ir_rc_client {
	struct list_head list;
	u32 received_code;		/* native code */
	int (*decode_func)(struct ir_rc_client *irc, u32 pulselow, u32 pulsehigh);	/* called after each pulse */
	int (*input_func)(struct ir_rc_client *irc);	/* translates codes for input event device */
	struct timer_list timeout;	/* for set/release of keys */
	void *priv;	/* for rc client, not used by avia_gt_ir_rc */
};

#endif
#endif
