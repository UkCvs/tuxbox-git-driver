/*
 * $Id: avia_gt_ir_rc_core.c,v 1.1.2.1 2005/02/09 05:09:57 carjay Exp $
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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/input.h>

#include <dbox/dbox2_fp_rc.h>

#include "avia_gt_ir.h"
#include "avia_gt_ir_rc_core.h"
#include "avia_gt_ir_rc_nokia.h"

/* we want to be able to use several remotes at the same time */
LIST_HEAD(ir_client_list);
static int ir_handle;
static void avia_gt_ir_rc_task(unsigned long);
static struct tasklet_struct avia_gt_ir_rc_tasklet = {
		.func = avia_gt_ir_rc_task
};

static void avia_gt_ir_rc_task(unsigned long arg)
{
	u32 period_high = 0;
	u32 period_low = 0;
	struct list_head *entry;
	
	while (!avia_gt_ir_receive_pulse(&period_low, &period_high, 0)){

		if (period_high > 0xffffff)
			period_high = 0xffffff;

		if (period_low > 0xffffff)
			period_low = 0xffffff;
	
		list_for_each(entry,&ir_client_list){
			struct ir_rc_client* irc;
			irc = container_of(entry, struct ir_rc_client, list);
			if (!irc->decode_func(irc,period_low,period_high))
				irc->input_func(irc);
		}
	}
}

/*
	adds an ir_rc_client structure to the list,
	structure must be static (there is no remove function)
*/
static int avia_gt_ir_rc_add(struct ir_rc_client *irc)
{
	init_timer(&irc->timeout);
	list_add_tail(&irc->list, &ir_client_list);
	return 0;
}

static int __init avia_gt_ir_rc_init(void)
{
	struct ir_client irc;
	
	printk(KERN_INFO "avia_gt_ir_rc: $Id: avia_gt_ir_rc_core.c,v 1.1.2.1 2005/02/09 05:09:57 carjay Exp $\n");

	avia_gt_ir_rc_add(&avia_gt_ir_rc_nokia_client);

	irc.flags = AVIA_GT_IR_RX;
	irc.rx_task = &avia_gt_ir_rc_tasklet;

	if ((ir_handle = avia_gt_ir_register(&irc)) < 0)
		return ir_handle;

	return 0;
}

static void __exit avia_gt_ir_rc_exit(void)
{
	/* turn off IR */
	avia_gt_ir_unregister(ir_handle);
}

module_init(avia_gt_ir_rc_init);
module_exit(avia_gt_ir_rc_exit);

MODULE_AUTHOR("Carsten Juttner <carjay@gmx.net>");
MODULE_DESCRIPTION("alternative rc input driver");
MODULE_LICENSE("GPL");
