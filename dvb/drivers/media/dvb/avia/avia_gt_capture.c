/*
 * $Id: avia_gt_capture.c,v 1.32.4.5 2007/10/09 01:03:38 carjay Exp $
 * 
 * capture driver for eNX/GTX (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2001-2002 Florian Schirmer <jolt@tuxbox.org>
 * Copyright (C) 2003 Carsten Juttner (carjay@gmx.net)
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
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include "avia_gt.h"
#include "avia_gt_capture.h"

/* TODO: - eNX can handle capture unsquashed, exploit this
	 - double buffering if possible */

static sAviaGtInfo *gt_info;
/* hardware parameters (for module) */
static struct {	/* capture_hw_par information (fixed and directly dependent on capture_params) */
	unsigned long addr;	/* address of buffer in GTX/eNX-DRAM (offset within DRAM!) */
	unsigned long size;	/* size of complete area reserved for capturing */
	unsigned long oddoffset;/* offset to odd data, if present, 0 otherwise */
	u16 line_stride;	/* offset between the first pixels of two consecutive lines in one field in bytes (eNX) */
} capture_hw_par = {
	.addr = AVIA_GT_MEM_CAPTURE_OFFS,
	.size = AVIA_GT_MEM_CAPTURE_SIZE,
};

static struct avia_gt_capture_params current_params ={
	.input_width = 720,
	.input_height = 576,
	.captured_width = 360,
	.captured_height = 288
};
static unsigned int line_irqs;
static unsigned char capture_busy;
static unsigned long capture_framesize;	/* for quicker access */

DECLARE_WAIT_QUEUE_HEAD(capture_wait);

/* wake up the sleeping process only after this irq handler has been called at least
	twice to avoid a situation where we start at the end of a field, enable
	the capturing and the next line irq brings us here before we captured anything */
void avia_gt_capture_interrupt(unsigned short irq)
{
/*	unsigned char field = 0;
	if (avia_gt_chip(ENX))
		field = enx_reg_s(VLC)->F;
	else if (avia_gt_chip(GTX))
		field = gtx_reg_s(VLC)->F;
*/
	/* GTX does not seem to care about field irq settings, hmm... */
	if (avia_gt_chip(GTX) && gtx_reg_s(VLC)->F)
		return;
	line_irqs++;
	if (line_irqs>1)
		wake_up (&capture_wait);
}

/* Called to get the (capture) framebuffer content,
	it sleeps until at least one or more complete frames have been received
		@buffer : where to copy the frame
		@count : how much to copy
		@userspace : 0 = kernel space, 1 = user space */
int avia_gt_capture_copybuffer (unsigned char *buffer, unsigned long count, char userspace){
	if ((!capture_busy) || (!buffer))
			return -EINVAL;
	if (!count) return 0;
	if (count > capture_framesize)
		count = capture_framesize;
	if (wait_event_interruptible (capture_wait, (line_irqs>1)))
		return -ERESTARTSYS;
	if (userspace){
		unsigned long ret;
		ret = copy_to_user(buffer,gt_info->mem_addr+capture_hw_par.addr,count);
		if (ret)
			return -EFAULT;
	} else {
		memcpy (buffer,gt_info->mem_addr+capture_hw_par.addr,count);
	}
	return 0;
}

int avia_gt_capture_start(void)
{
	if (capture_busy)
		return -EBUSY;

	if (avia_gt_alloc_irq(gt_info->irq_vl1, avia_gt_capture_interrupt) < 0)
		return -EIO;

	if (avia_gt_chip(ENX))
		enx_reg_set(VCSA1, Addr, capture_hw_par.addr >> 2);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(VCSA1, Addr, capture_hw_par.addr >> 1);

	capture_busy = 1;
	line_irqs = 0;

	avia_gt_reg_set(VCSA1, E, 1);
	return 0;
}

void avia_gt_capture_stop(void)
{
	if (capture_busy) {
		avia_gt_reg_set(VCSA1, E, 0);
		avia_gt_free_irq(gt_info->irq_vl1);
		capture_busy = 0;
	} /*else {
		printk (KERN_WARNING "avia_gt_capture: capture_stop called unstarted\n");
	}*/
}


/* apply current params to the hardware */
static void avia_gt_capture_setup_params(void){
	u8 scale_x, scale_y;

#define BLANK_TIME 132		/* TODO: NTSC */
#define VIDCAP_PIPEDELAY 2

	if (avia_gt_chip(ENX))
		enx_reg_set(VCP, HPOS, ((BLANK_TIME - VIDCAP_PIPEDELAY) + current_params.input_x) / 2);
	else
		gtx_reg_set(VCP, HPOS, (96 + current_params.input_x) / 2);	/* FIXME: Verify VIDCAP_PIPEDELAY for GTX */

	scale_x = current_params.input_width/current_params.captured_width;
	scale_y = current_params.input_height/current_params.captured_height;
	avia_gt_reg_set(VCP, OVOFFS, (scale_y - 1) / 2);
	avia_gt_reg_set(VCP, EVPOS, 21 + (current_params.input_y / 2));
	avia_gt_reg_set(VCSZ, HDEC, scale_x - 1);
	avia_gt_reg_set(VCSZ, HSIZE, current_params.input_width / 2);
	avia_gt_reg_set(VCSZ, VSIZE, ((current_params.input_height+1)&~1)>>1);
	if (scale_y&1){
		avia_gt_reg_set(VCSZ, VDEC, scale_y - 1);	/* odd = both fields */
	} else {
		avia_gt_reg_set(VCSZ, VDEC, (scale_y/2)-1);	/* even = only even field */
	}

	/* If scale_y is even, capture only even fields (better results)
		If scale_y is odd,  capture both */
	avia_gt_reg_set(VCSZ, B, (scale_y&1)?1:0);
	if (avia_gt_chip(ENX)){
		if (current_params.eNX_extras&ENX_CAPT_STORE_INTERLACED) 	/* offset odd to even */
			enx_reg_set(VCOFFS, Offset, capture_hw_par.line_stride >> 2);
		else  
			enx_reg_set(VCOFFS, Offset, capture_hw_par.oddoffset >> 2);

		enx_reg_set(VCSTR, STRIDE, capture_hw_par.line_stride >> 2);	/* offset line to line */
	}	

}

/* apply sets the params and returns both params as they are applied and info (if requested)
	is also able to simply test the configuration */
static int avia_gt_capture_test_apply_params(struct avia_gt_capture_params *params, struct avia_gt_capture_info *info, char test){
	int retval = 0;
	struct avia_gt_capture_params tmpparams;
	u8 scale_x,scale_y;
	unsigned long framesize, linesize;
	unsigned char bytesperpix;
	unsigned short max_height;

	if (!params) return -EINVAL;

	/* we accept and substitute invalid values, user must check them
		since we assume user always wants a sizefilling image we fall back to bigger resolutions */

	tmpparams = *params;
	if (!avia_gt_chip(ENX)) 
		tmpparams.eNX_extras = 0;	/* nice try... ;) */

	bytesperpix = (tmpparams.eNX_extras&ENX_CAPT_UNSQUASHED)?2:1;
	max_height = /*avia_gt_gv_height_sth()*/576;	/* TODO: NTSC */

	/* first we correct the position */
	tmpparams.input_x=(tmpparams.input_x+1)&~1;	/* can only start on an even position */
	tmpparams.input_y=(tmpparams.input_y+1)&~1;	/* we are restricted to start with the even field */

	/* check we don't hit a boundary */
	if (avia_gt_chip(GTX)){
		if (tmpparams.input_x>718)
			tmpparams.input_x=718;	/* actually we could use only this line, it would */
	}else{ 								/* get corrected anyway, but better safe than sorry */
		if (tmpparams.input_x>716)
			tmpparams.input_x=716;
	}		
	if (tmpparams.input_y>(max_height-1)) 
		tmpparams.input_y=max_height-1;	/* y starts with 0, height with 1 */

	/* now correct the size */
	if (!tmpparams.input_width){
		if (avia_gt_chip(GTX)) 
			tmpparams.input_width=2;
		else 
			tmpparams.input_width=4;
	}
	if (!tmpparams.input_height) tmpparams.input_height=1;

	if ((tmpparams.input_x+tmpparams.input_width)>720)
			tmpparams.input_width = (720-tmpparams.input_x);
	if ((tmpparams.input_y+tmpparams.input_height)>max_height)
			tmpparams.input_height = (max_height-tmpparams.input_y);

	tmpparams.input_width&=~1;	/* we round down here to 16-Bit */

		/* verify scale values, we're restricted to what the hardware can do
			- only integer reduction (1,2,3..16/32), even = even fields, odd = both */

	scale_x = tmpparams.input_width / tmpparams.captured_width;	/* integer */
	scale_y = tmpparams.input_height / tmpparams.captured_height;
	
	if (!scale_x)
		scale_x = 2;
	if (!scale_y)
		scale_y = 2;

	tmpparams.captured_height = tmpparams.input_height / scale_y;	/* this is the possible value now */
	tmpparams.captured_width = tmpparams.input_width / scale_x;
	
	printk (KERN_DEBUG "avia_gt_capture: from: %d x %d -> %d x %d, (requested: %d x %d)\n",
			tmpparams.input_width,tmpparams.input_height,
			tmpparams.captured_width,tmpparams.captured_height,
			params->captured_width,params->captured_height);

	/* NB: the framesize itself is always the same, but depending on the scaling operation it's either
		just even or both fields. This must be reversed correctly by the PIG (see there)
		Due to the different alignment between GTX/eNX if the line width is only 2-byte aligned 
		we get an extra 2 bytes at the end (Stride is 4-byte aligned). */
	
	if (avia_gt_chip(GTX))
		linesize = tmpparams.captured_width*bytesperpix;
	else
		linesize = (((tmpparams.captured_width*bytesperpix)+3)&~3);

	framesize = linesize * tmpparams.captured_height;

	/* final test: if framesize is not sufficient we do not keep the parameters and stay with the old set */
	if (framesize > capture_hw_par.size){
		if (!test) printk (KERN_WARNING "avia_gt_capture: not enough RAM for capturing %dx%d, current configuration not changed\n",tmpparams.captured_width,tmpparams.captured_height);
		retval=-EINVAL;
	} else if (!test){		/* setup hardware specific struct and apply the values */
		current_params = tmpparams;	/* transfer corrected values */
		capture_framesize = framesize;
		if (!(scale_y&1)) 
			capture_hw_par.oddoffset = 0;	/* even scale means we don't capture odd */
		else 
			capture_hw_par.oddoffset = linesize*(((tmpparams.captured_height+1)&~1)>>1);

		capture_hw_par.line_stride = linesize;

		/* TODO: if 2-byte aligned and stored interlaced, stride would be 4-byte aligned, no need for extra 2 bytes */
		if (tmpparams.eNX_extras&ENX_CAPT_STORE_INTERLACED)
			capture_hw_par.line_stride<<=1;
		*params = tmpparams;	/* return the valid values */
		avia_gt_capture_setup_params();
	}
	if (info && (retval != -EINVAL)){
		info->drambufstart=capture_hw_par.addr;
		info->framesize=framesize;
		info->bytesperline=linesize;
		info->oddoffset=capture_hw_par.oddoffset;
	}
	return retval;
}

int avia_gt_capture_test_params(struct avia_gt_capture_params *params){
	return avia_gt_capture_test_apply_params(params, NULL, 1);
}
int avia_gt_capture_apply_params(struct avia_gt_capture_params *params, struct avia_gt_capture_info *info){
	return avia_gt_capture_test_apply_params(params, info, 0);
}
void avia_gt_capture_get_info(struct avia_gt_capture_info *info){
	info->drambufstart = capture_hw_par.addr;
	info->framesize = capture_framesize;
	info->bytesperline=capture_framesize/current_params.captured_height;
	info->oddoffset=capture_hw_par.oddoffset;
}
int avia_gt_capture_get_params(struct avia_gt_capture_params *params){
	if (params) *params = current_params;
	return 0;
}

void avia_gt_capture_reset(int reenable)
{
	avia_gt_reg_set(RSTR0, VIDC, 1);

	if (reenable)
		avia_gt_reg_set(RSTR0, VIDC, 0);
}

int AVIA_GT_INIT avia_gt_capture_init(void)
{
	printk(KERN_INFO "avia_gt_capture: $Id: avia_gt_capture.c,v 1.32.4.5 2007/10/09 01:03:38 carjay Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
		printk(KERN_ERR "avia_gt_capture: Unsupported chip type\n");
		return -EIO;
	}
 
	avia_gt_capture_reset(1);

	if (avia_gt_chip(ENX)) {
		enx_reg_set(VCP, U, 0);		/* Using squashed mode */
		enx_reg_set(VCSTR, B, 0);	/* Hardware double buffering is off */
	}
#if 0
	/* FIXME */
	else if (avia_gt_chip(GTX)) {
		gtx_reg_set(VCS, B, 0);		/* Hardware double buffering */
	}
#endif
	avia_gt_reg_set(VCSZ, F, 1);	/*  Filter */

	avia_gt_reg_set(VLI1, F, 0);	/* only even field... */
	avia_gt_reg_set(VLI1, E, 1);	/* ...triggers IRQ... */
	avia_gt_reg_set(VLI1, LINE, 0);	/* ... when we reach this line */

	avia_gt_capture_setup_params();	/* setup the default params to hardware */

	return 0;
}

void AVIA_GT_EXIT avia_gt_capture_exit(void)
{
	avia_gt_capture_stop();
	avia_gt_capture_reset(0);
}

#if defined(STANDALONE)
module_init(avia_gt_capture_init);
module_exit(avia_gt_capture_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX capture driver");
MODULE_LICENSE("GPL");
#endif

EXPORT_SYMBOL(avia_gt_capture_get_params);
EXPORT_SYMBOL(avia_gt_capture_test_params);
EXPORT_SYMBOL(avia_gt_capture_apply_params);
EXPORT_SYMBOL(avia_gt_capture_get_info);
EXPORT_SYMBOL(avia_gt_capture_copybuffer);
EXPORT_SYMBOL(avia_gt_capture_start);
EXPORT_SYMBOL(avia_gt_capture_stop);
