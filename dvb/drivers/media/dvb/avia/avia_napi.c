/*
 * $Id: avia_napi.c,v 1.10 2003/01/04 11:05:31 Jolt Exp $
 *
 * AViA GTX/eNX NAPI driver
 *
 * Homepage: http://dbox2.elxsi.de
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

#include <asm/bitops.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "../dvb-core/dvbdev.h"
#include "../dvb-core/dvb_i2c_bridge.h"

static struct dvb_adapter *adap;

struct dvb_adapter *avia_napi_get_adapter(void)
{

	return adap;
	
}

static int __init avia_napi_init(void)
{

	int result;

	printk("$Id: avia_napi.c,v 1.10 2003/01/04 11:05:31 Jolt Exp $\n");
	
	if ((result = dvb_register_adapter(&adap, "C-Cube AViA GTX/eNX with AViA 500/600")) < 0) {
	
		printk("avia_napi: dvb_register_adapter failed (errno = %d)\n", result);
		
		return result;
		
	}

	if ((result = dvb_i2c_bridge_register(adap)) < 0) {
	
		printk("avia_napi: dvb_register_adapter failed (errno = %d)\n", result);

		dvb_unregister_adapter(adap);
		
		return result;
		
	}

//FIXME	dvb_net_register();

	return 0;

}

static void __exit avia_napi_exit(void)
{

//FIXME	dvb_net_release(&net);

	dvb_i2c_bridge_unregister(adap);
	dvb_unregister_adapter(adap);

}

module_init(avia_napi_init);
module_exit(avia_napi_exit);

#if defined(MODULE)
MODULE_DESCRIPTION("AViA dvb adapter driver");
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
#if defined(MODULE_LICENSE)
MODULE_LICENSE("GPL");
#endif
#endif
