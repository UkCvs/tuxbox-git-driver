/*
 * $Id: avia_napi.c,v 1.18.2.1 2005/01/15 02:35:09 carjay Exp $
 *
 * AViA GTX/eNX dvb api driver
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2002 Florian Schirmer <jolt@tuxbox.org>
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
#include <linux/module.h>
#include <linux/version.h>

#include "dvbdev.h"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#include "dvb_i2c_bridge.h"
#endif

static struct dvb_adapter *adap;

struct dvb_adapter *avia_napi_get_adapter(void)
{
	return adap;
}
EXPORT_SYMBOL(avia_napi_get_adapter);

static int __init avia_napi_init(void)
{
	int result;

	printk(KERN_INFO "$Id: avia_napi.c,v 1.18.2.1 2005/01/15 02:35:09 carjay Exp $\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	if ((result = dvb_register_adapter(&adap, "C-Cube AViA GTX/eNX with AViA 500/600",THIS_MODULE)) < 0) {
#else
	if ((result = dvb_register_adapter(&adap, "C-Cube AViA GTX/eNX with AViA 500/600")) < 0) {
#endif
		printk(KERN_ERR "avia_napi: dvb_register_adapter failed (errno = %d)\n", result);
		return result;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	if ((result = dvb_i2c_bridge_register(adap)) < 0) {
		printk(KERN_ERR "avia_napi: dvb_i2c_bridge_register failed (errno = %d)\n", result);
		dvb_unregister_adapter(adap);
		return result;
	}
#endif
	
	return 0;
}

static void __exit avia_napi_exit(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	dvb_i2c_bridge_unregister(adap);
#endif
	dvb_unregister_adapter(adap);
}

module_init(avia_napi_init);
module_exit(avia_napi_exit);

MODULE_DESCRIPTION("AViA dvb adapter driver");
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_LICENSE("GPL");
