/*
 * $Id: avia_av_debug.h,v 1.1.2.1 2006/12/05 20:54:52 carjay Exp $
 *
 * AViA 500/600 debug (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2006 Carsten Juttner (carjay@gmx.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, 
 * as published by the Free Software Foundation;
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


#ifndef AVIA_AV_DEBUG_H
#define AVIA_AV_DEBUG_H

#include <linux/module.h>

#define DBG_IRQ(format, ...)
//#define DBG_IRQ(format, ...)		printk(KERN_INFO format, ## __VA_ARGS__)
#define DBG_SETUP(format, ...)
//#define DBG_SETUP(format, ...)	printk(KERN_INFO format, ## __VA_ARGS__)

extern char *syncmode2string(unsigned int mode);
extern char *playstate2string(unsigned int state);
extern char *streamtype2string(unsigned int type);
extern char *aviacmd2string(unsigned int command);

#endif /* AVIA_AV_DEBUG_H */
