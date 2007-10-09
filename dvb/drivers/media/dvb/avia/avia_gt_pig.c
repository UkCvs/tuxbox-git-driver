/*
 * $Id: avia_gt_pig.c,v 1.40.4.3 2007/10/09 01:03:38 carjay Exp $
 *
 * pig driver for AViA eNX/GTX (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2001-2002 Florian Schirmer <jolt@tuxbox.org>
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

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "avia_gt.h"
#include "avia_gt_capture.h"	/* TODO: does pig have to know about capture? -> delegate knowledge to v4l2 */
#include "avia_gt_pig.h"

#define MAX_PIG_COUNT 2

#define CAPTURE_WIDTH 720
#define CAPTURE_HEIGHT 576

static struct avia_gt_pig_info pig_info[2] = {
	{.width = 720,
	.height = 576},
	{.width = 720,
	.height = 576}
};

static sAviaGtInfo *gt_info;
static unsigned char pig_busy[MAX_PIG_COUNT];	// TODO: hide in struct
static unsigned long pig_buffer[MAX_PIG_COUNT];
static unsigned long pig_oddoffset[MAX_PIG_COUNT];
static unsigned char pig_count;
static unsigned long pig_stride[MAX_PIG_COUNT];

int avia_gt_pig_hide(u8 pig_nr)
{
	if (pig_nr >= pig_count)
		return -ENODEV;

	if (pig_busy[pig_nr]) {
		avia_gt_reg_set(VPSA1, E, 0);
		avia_gt_capture_stop();
		pig_busy[pig_nr] = 0;
	}

	return 0;
}

int avia_gt_pig_set_pos(u8 pig_nr, u16 x, u16 y)
{
	if (pig_nr >= pig_count)
		return -ENODEV;

 	pig_info[pig_nr].left = x;
 	pig_info[pig_nr].top = y;

	if (avia_gt_chip(ENX)) {
		enx_reg_set(VPP1, HPOS, 63 + (x / 2));
		enx_reg_set(VPP1, VPOS, 21 + (y / 2));
	}
	else if (avia_gt_chip(GTX)) {
		gtx_reg_set(VPP1, HPOS, 36 + (x / 2)); //- (gtx_reg_s(VPS)->S ? 3 : 0);
		gtx_reg_set(VPP1, VPOS, 20 + (y / 2));
	}

	return 0;
}

int avia_gt_pig_set_stack(u8 pig_nr, u8 stack_order)
{
	if (pig_nr >= pig_count)
		return -ENODEV;

	if (avia_gt_chip(ENX))
		enx_reg_set(VPSO1, SO, stack_order & 0x03);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(VPSZ1, P, !!stack_order);

	return 0;
}

int avia_gt_pig_set_size(u8 pig_nr, u16 width, u16 height, u8 stretch)
{
	struct avia_gt_capture_params capparams;

	if (pig_nr >= pig_count)
		return -ENODEV;

	if (pig_busy[pig_nr])
		return -EBUSY;

	avia_gt_capture_get_params (&capparams);		// TODO: this must go to v4l2
	capparams.captured_width = width;
	capparams.captured_height = height;
	if (avia_gt_capture_apply_params(&capparams,NULL)<0)
		return 1;
 	pig_info[pig_nr].width = width = capparams.captured_width;
 	pig_info[pig_nr].width = height = capparams.captured_height;
 	
 
	avia_gt_reg_set(VPSZ1, WIDTH, width/2);			// pixel pairs
	avia_gt_reg_set(VPSZ1, S, stretch);
	avia_gt_reg_set(VPSZ1, HEIGHT, ((height+1)&~1)/2);	// scan lines per field

	return 0;
}

int avia_gt_pig_show(u8 pig_nr)
{
	struct avia_gt_capture_params capparams;
	struct avia_gt_capture_info info;

	if (pig_nr >= pig_count)
		return -ENODEV;
	
	if (pig_busy[pig_nr])
		return -EBUSY;

	avia_gt_capture_get_params (&capparams);
	avia_gt_capture_get_info(&info);

	if (avia_gt_capture_start()<0)
		return -EBUSY;

	pig_buffer[pig_nr] = info.drambufstart;
	pig_stride[pig_nr] = info.bytesperline*((info.oddoffset)?1:2);	// if oddoffset is set, lines follow each other
	pig_oddoffset[pig_nr] = (info.oddoffset)?info.oddoffset:info.bytesperline;

	// TODO: do correct inversion of capture process
	if (avia_gt_chip(ENX)) {
		enx_reg_16(VPSTR1) = pig_stride[pig_nr]&~3 ;
		//enx_reg_16(VPSTR1) |= 0;				// Enable hardware double buffering
		enx_reg_set(VPSZ1, P, 0);
		enx_reg_set(VPSA1, Addr, pig_buffer[pig_nr] >> 2);	// Set buffer address (for non d-buffer mode)
		enx_reg_set(VPOFFS1, OFFSET, pig_oddoffset[pig_nr] >> 2);
		enx_reg_set(VPP1, U, 0);
	} else if (avia_gt_chip(GTX)) {
		gtx_reg_set(VPO, OFFSET, pig_stride[pig_nr] >> 1);
		gtx_reg_set(VPO, STRIDE, pig_stride[pig_nr] >> 1);
		gtx_reg_set(VPO, B, 0);					// Enable hardware double buffering
		gtx_reg_set(VPSA1, Addr, pig_buffer[pig_nr] >> 1);	// Set buffer address (for non d-buffer mode)
	}

	avia_gt_reg_set(VPP1, F, 0);
	avia_gt_reg_set(VPSA1, E, 1);
    
	pig_busy[pig_nr] = 1;
    
	return 0;
}

void avia_gt_pig_get_info(unsigned char pig, struct avia_gt_pig_info *info){
	*info = pig_info[pig];
}

int AVIA_GT_INIT avia_gt_pig_init(void)
{
	u8 pig_nr;

	printk(KERN_INFO "avia_gt_pig: $Id: avia_gt_pig.c,v 1.40.4.3 2007/10/09 01:03:38 carjay Exp $\n");

	gt_info = avia_gt_get_info();

	if (!avia_gt_supported_chipset(gt_info))
		return -ENODEV;

	if (avia_gt_chip(ENX))
		pig_count = 2;
	else if (avia_gt_chip(GTX))
		pig_count = 1;

	if (avia_gt_chip(ENX)) {
		enx_reg_set(RSTR0, PIG1, 1);
		enx_reg_set(RSTR0, PIG2, 1);
		enx_reg_set(RSTR0, PIG1, 0);
		enx_reg_set(RSTR0, PIG2, 0);
	} else if (avia_gt_chip(GTX)) {
		gtx_reg_set(RSTR0, PIG1, 1);
		gtx_reg_set(RSTR0, PIG1, 0);
	}
 
	for (pig_nr = 0; pig_nr < pig_count; pig_nr++) {
		avia_gt_pig_set_pos(pig_nr, 150, 50);
		avia_gt_pig_set_size(pig_nr, CAPTURE_WIDTH / 4, CAPTURE_HEIGHT / 4, 0);
	}

	return 0;
}

void AVIA_GT_EXIT avia_gt_pig_exit(void)
{
	avia_gt_pig_hide(0);

	avia_gt_reg_set(RSTR0, PIG1, 1);

	if (avia_gt_chip(ENX))
		enx_reg_set(RSTR0, PIG2, 1);
}

#if defined(STANDALONE)
module_init(avia_gt_pig_init);
module_exit(avia_gt_pig_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX PIG driver");
MODULE_LICENSE("GPL");
#endif

EXPORT_SYMBOL(avia_gt_pig_hide);
EXPORT_SYMBOL(avia_gt_pig_set_pos);
EXPORT_SYMBOL(avia_gt_pig_set_size);
EXPORT_SYMBOL(avia_gt_pig_set_stack);
EXPORT_SYMBOL(avia_gt_pig_show);
EXPORT_SYMBOL(avia_gt_pig_get_info);
