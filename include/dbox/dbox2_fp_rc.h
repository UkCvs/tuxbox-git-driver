/*
 * $Id: dbox2_fp_rc.h,v 1.1.2.1 2005/02/09 04:31:20 carjay Exp $
 *
 * Copyright (C) 2005 by Carsten Juttner <carjay@gmx.net>
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
#ifndef __dbox2_fp_rc_h__
#define __dbox2_fp_rc_h__

/* allows to insert remote control events from external modules */
extern void dbox2_fp_rc_input_event(unsigned int type, unsigned int code, int value);

#endif /* __dbox2_fp_rc_h__ */
#endif /* __KERNEL__ */
