/*
 * $Id: avia_gt_v4l2.c,v 1.12.4.3 2005/01/25 01:35:51 carjay Exp $
 *
 * AViA eNX/GTX v4l2 driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
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
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>

#include "avia_gt.h"
#include "avia_gt_pig.h"
#include "avia_gt_capture.h"

#define AVIA_GT_V4L2_DRIVER	"avia"
#define AVIA_GT_V4L2_CARD	"AViA eNX/GTX"
#define AVIA_GT_V4L2_BUS_INFO	"AViA core"
#define AVIA_GT_V4L2_VERSION	KERNEL_VERSION(0,1,14)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static struct video_device *vfd;
#endif

static struct {
	char overlay_params_set;	// do we need to waste this space? user is responsible to trace this...
	char capture_size_params_set;
	char capture_cropping_params_set;
} avia_gt_v4l2_state;

static int avia_gt_v4l2_open(struct inode *inode, struct file *file)
{
	dprintk("avia_gt_v4l2: open\n");
	return 0;
}

static int avia_gt_v4l2_release(struct inode *inode, struct file *file)
{
	dprintk("avia_gt_v4l2: close\n");
	// TODO: cleanup only what we are suppose to clean up
	avia_gt_capture_stop();
	avia_gt_pig_hide(0);
	return 0;
}

static ssize_t avia_gt_v4l2_read(struct file *file, char *data, size_t count, loff_t *ppos)
{
	int stat;
	if (file->f_flags & O_NONBLOCK)
		return -EAGAIN;	// TODO: poll
	avia_gt_capture_start();
	stat = avia_gt_capture_copybuffer (data, (unsigned long)count, 1);
	avia_gt_capture_stop();
	if (stat<0)
		return stat;
	*ppos+=count;
	return count;
}

/*static long	avia_gt_v4l2_write(void *id, const char *buf, unsigned long count, int noblock)
{
	dprintk("avia_gt_v4l2: write\n");
	return -EINVAL;
}*/

/*  The arguments are already copied into kernel memory, so don't use copy_from_user()
		or copy_to_user() on arg.  */
static int avia_gt_v4l2_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *arg)
{
	switch(cmd) {
	case VIDIOC_ENUMINPUT:
	{
		struct v4l2_input *input = (struct v4l2_input *)arg;
		if (input->index != 0)
			return -EINVAL;
			
		strcpy(input->name, "AViA eNX/GTX digital tv picture");
		input->type = V4L2_INPUT_TYPE_TUNER;
		input->audioset = 0;
		input->tuner = 0;
		input->std = V4L2_STD_PAL_BG | V4L2_STD_NTSC_M;
		input->status = 0;
		return 0;
	}
	case VIDIOC_G_INPUT:
		(*((int *)arg)) = 0;	// only one input
		return 0;
				
	case VIDIOC_S_INPUT:
		if ((*((int *)arg)) != 0)
			return -EINVAL;
		else
			return 0;
		break;
		case VIDIOC_OVERLAY:
		if (!avia_gt_v4l2_state.overlay_params_set) return -EINVAL;	// TODO: better apply defaults
		if ((*((int *)arg))) {	// enable PIG, parameters set through S_FMT
			avia_gt_pig_set_stack(0, AVIA_GT_PIG_STACK_ABOVE_GFX);
			return avia_gt_pig_show(0);
		} else {
			return avia_gt_pig_hide(0);
		}
		case VIDIOC_QUERYCAP:
		{struct v4l2_capability *capability = (struct v4l2_capability *)arg;
		strcpy(capability->driver, AVIA_GT_V4L2_DRIVER);
		strcpy(capability->card, AVIA_GT_V4L2_CARD);
		strcpy(capability->bus_info, AVIA_GT_V4L2_BUS_INFO);
		capability->version = AVIA_GT_V4L2_VERSION;
		capability->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_READWRITE | V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT;
		return 0;}
	case VIDIOC_G_OUTPUT:
		(*((int *)arg)) = 0;	// only one output
		return 0;
		break;
	case VIDIOC_S_OUTPUT:
		if ((*((int *)arg)) != 0)
			return -EINVAL;
		else
			return 0;
		break;
		case VIDIOC_G_CROP:
		{struct v4l2_crop *crop = (struct v4l2_crop *)arg;
		struct avia_gt_capture_params capture;
		switch (crop->type){
			case V4L2_BUF_TYPE_VIDEO_OVERLAY:
			case V4L2_BUF_TYPE_VIDEO_CAPTURE:
				avia_gt_capture_get_params(&capture);
				crop->c.left = capture.input_x;
				crop->c.top = capture.input_y;
				crop->c.width = capture.input_width;
				crop->c.height = capture.input_height;
				break;
			default:
				return -EINVAL;
		}
		return 0;}

	case VIDIOC_S_CROP:	// TODO: is this correct?
		{struct v4l2_crop *crop = (struct v4l2_crop *)arg;
		struct avia_gt_capture_params capture;
		switch (crop->type){
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:	// TODO: lock access to overlay/capture
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			avia_gt_capture_get_params(&capture);			

			capture.captured_height = 0;	// TODO: setup params are (for checking reasons) set atomically in the driver
			capture.captured_width = 0;	// -> v4l2 must handle this differently)
									
			capture.input_x = crop->c.left;
			capture.input_y = crop->c.top;
			capture.input_width = crop->c.width;
			capture.input_height = crop->c.height;
			avia_gt_capture_apply_params(&capture,NULL);
			break;
		default:
			return -EINVAL;
		}
		return 0;}

	case VIDIOC_G_FMT:
		// TODO: return "nice and clean" format
		{struct v4l2_format *format = (struct v4l2_format *)arg; 
		struct avia_gt_capture_params capture;
		struct avia_gt_capture_info info;
		struct avia_gt_pig_info pig_info;
		switch (format->type){
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
			avia_gt_pig_get_info(0,&pig_info);
			avia_gt_capture_get_params(&capture);
			format->fmt.win.w.left = pig_info.left;
			format->fmt.win.w.top = pig_info.top;
			format->fmt.win.w.height = capture.captured_height;
			format->fmt.win.w.width = capture.captured_width;
			break;
					
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			avia_gt_capture_get_params(&capture);
			format->fmt.pix.height = capture.captured_height;
			format->fmt.pix.width = capture.captured_width;
			avia_gt_capture_get_info(&info);
			format->fmt.pix.bytesperline = info.bytesperline;
			format->fmt.pix.sizeimage = capture.captured_height * info.bytesperline;
			break;
		default:
				return -EINVAL;
			}
			return 0;}

	case VIDIOC_S_FMT:
		{struct v4l2_format *format = (struct v4l2_format *)arg; 
		struct avia_gt_capture_params capture;
		switch (format->type){
		case V4L2_BUF_TYPE_VIDEO_OVERLAY:
			avia_gt_pig_set_pos(0, format->fmt.win.w.left, format->fmt.win.w.top);
			avia_gt_pig_set_size(0, format->fmt.win.w.width, format->fmt.win.w.height,0);
			avia_gt_v4l2_state.overlay_params_set = 1;
			break;
			
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			avia_gt_capture_get_params(&capture);		// save cropping info if any
			capture.captured_height = format->fmt.pix.height;
			capture.captured_width = format->fmt.pix.width;
			avia_gt_capture_apply_params(&capture,NULL);
			avia_gt_v4l2_state.capture_size_params_set = 1;
			break;
		default:
			return -EINVAL;
		}
			return 0;}

	case VIDIOC_TRY_FMT:	// TODO
		{//struct v4l2_format *format = (struct v4l2_format *)arg;
		//avia_gt_capture_test_params(&capture);
		return -EINVAL;}
		
	default:
		printk (KERN_INFO "avia_gt_v4l2: invalid IOCTL sent\n");
		return -EINVAL;
	}
	
}

static int avia_gt_v4l2_ioctl_prepare(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	return video_usercopy(inode, file, cmd, arg, avia_gt_v4l2_ioctl);
}

static int avia_gt_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{
	dprintk("avia_gt_v4l2: mmap\n");
	return 0;
}

static unsigned int avia_gt_v4l2_poll(struct file *file, struct poll_table_struct *wait)
{
	dprintk("avia_gt_v4l2: poll\n");
	return 0;
}

static struct file_operations device_fops = {
	.owner = THIS_MODULE,
	.open = avia_gt_v4l2_open,
	.release = avia_gt_v4l2_release,
	.read = avia_gt_v4l2_read,
	.poll = avia_gt_v4l2_poll,
	.mmap = avia_gt_v4l2_mmap,
	.ioctl = avia_gt_v4l2_ioctl_prepare,
	.llseek = no_llseek,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static struct video_device device_info_template __initdata = {
#else
static struct video_device device_info = {
#endif
	
//	.owner =
	.name = AVIA_GT_V4L2_CARD,
//	.type = 
//	.type2 =
//	.hardware = 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	.release = video_device_release,
#endif
	.minor = -1,
	.fops = &device_fops,
	.priv = NULL,
	
};

static int __init avia_gt_v4l2_init(void)
{
	printk("avia_gt_v4l2: $Id: avia_gt_v4l2.c,v 1.12.4.3 2005/01/25 01:35:51 carjay Exp $\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	vfd = video_device_alloc();
	if (!vfd){
		printk(KERN_ERR "avia_gt_v4l2: unable to allocate video device structure");
		return -ENOMEM;
	}
	memcpy (vfd,&device_info_template,sizeof(struct video_device));
	avia_gt_v4l2_state.overlay_params_set = 0;
	return video_register_device(vfd, VFL_TYPE_GRABBER, -1);
#else
	return video_register_device(&device_info, VFL_TYPE_GRABBER, -1);
#endif
}

static void __exit avia_gt_v4l2_exit(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	video_unregister_device(vfd);
#else
	video_unregister_device(&device_info);
#endif
}

module_init(avia_gt_v4l2_init);
module_exit(avia_gt_v4l2_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX V4L2 driver");
MODULE_LICENSE("GPL");
