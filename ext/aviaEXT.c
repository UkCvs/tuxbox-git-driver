/*
 * Extension device for non-API covered stuff for the Avia
 * (hopefully will disappear at some point)
 *
 * $Id: aviaEXT.c,v 1.4.2.2 2006/12/08 23:19:49 carjay Exp $
 *
 * Copyright (C) 2004,2006 Carsten Juttner <carjay@gmx.net>
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
#include <linux/fs.h>
#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#include <linux/version.h>
#include <asm/uaccess.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
#include <linux/miscdevice.h>
#endif

#include "avia_av.h"
#include <dbox/aviaEXT.h>

#ifdef CONFIG_DEVFS_FS
static devfs_handle_t devfs_h;
#endif

static int handle_gbus_read(struct cmd_gbus *p)
{
	unsigned int regcnt;
	unsigned int *pu;

	/* copy the requested range to user space */
	pu = p->buffer;
	for (regcnt = p->start; regcnt <= p->end; regcnt++) {
		if (put_user(avia_av_gbus_read(regcnt), pu++))
			return -EFAULT;
	}

	return 0;
}

static int handle_gbus_write(struct cmd_gbus *p)
{
	unsigned int regcnt;
	unsigned int *pu;
	
	if (!p->buffer)
		return -EINVAL;

	/* copy the requested range from user space */
	pu = p->buffer;
	for (regcnt = p->start; regcnt <= p->end; regcnt++) {
		unsigned int val;
		if (get_user(val, pu++))
			return -EFAULT;
		avia_av_gbus_write(regcnt, val);
	}

	return 0;
}

static int handle_mem_ioctl(unsigned long arg)
{
	unsigned int res = 0;
	struct cmd_gbus *p = (struct cmd_gbus *)arg;
	struct cmd_gbus b;

	/* peek at header */
	if (copy_from_user(&b, (void *)arg, sizeof(struct cmdheader))) {
		printk("copy header failed\n");
		return -EFAULT;
	}

	if (b.header.length != sizeof(struct cmd_gbus)) {
		return -EINVAL;
	}

	/* looks ok, copy rest */
	if (copy_from_user(&b, (void *)(arg + sizeof(struct cmdheader)), sizeof(struct cmd_gbus) - sizeof(struct cmdheader)))
		return -EFAULT;
	
	switch (p->header.cmd) {

	case AVIA_EXT_MEM_GBUS_READ:
		res = handle_gbus_read(p);
		break;

	case AVIA_EXT_MEM_GBUS_WRITE:
		res = handle_gbus_write(p);
		break;

	}
	
	return res;	
}

static int aviaEXT_ioctl(struct inode *inode, struct file *file, 
						unsigned int cmd, unsigned long arg)
{
	switch (cmd){
	case AVIA_EXT_IEC_SET:	/* true turns on optical audio output, false turns it off */
		if (avia_av_is500())
			return -EOPNOTSUPP;
		if (!arg) {
			avia_av_dram_write(AUDIO_CONFIG, avia_av_dram_read(AUDIO_CONFIG)|0x100);
		} else {
			avia_av_dram_write(AUDIO_CONFIG, avia_av_dram_read(AUDIO_CONFIG)&~0x100);
		}
		avia_av_new_audio_config();
		break;
	case AVIA_EXT_IEC_GET:	/* true if optical output enabled, false if disabled */
		if (avia_av_is500())
			return -EOPNOTSUPP;
		if (put_user((!(avia_av_dram_read(AUDIO_CONFIG)&0x100)),(int *)arg))
			return -EFAULT;
		break;
		
	case AVIA_EXT_AVIA_PLAYBACK_MODE_GET:	/* 0=DualPES, 1=SPTS */
		if (put_user(avia_gt_get_playback_mode(),(int *)arg))
			return -EFAULT;
		break;

	case AVIA_EXT_AVIA_PLAYBACK_MODE_SET:
		avia_gt_set_playback_mode(arg);
		break;

	case AVIA_EXT_MEM_CMD:
		return handle_mem_ioctl(arg);
		break;
	
	default:
		printk (KERN_WARNING "aviaEXT: unknown ioctl %08x\n",cmd);
		break;
	}
	return 0;
}

static struct file_operations aviaEXT_fops = {
		THIS_MODULE,
		.ioctl = aviaEXT_ioctl
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static struct miscdevice ext_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "avia extensions",
	.fops = &aviaEXT_fops
};
#endif

static int __init aviaEXT_init(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	int ret = misc_register(&ext_dev);
	if (ret<0) {
		printk(KERN_ERR "aviaEXT: could not register misc device.\n");
		return -EIO;
	}

#ifdef CONFIG_DEVFS_FS
	devfs_mk_cdev(MKDEV(MISC_MAJOR, ext_dev.minor),
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
		"dbox/aviaEXT");
#endif /* CONFIG_DEVFS_FS */
#else
	if (!(devfs_h = devfs_register(NULL,"dbox/aviaEXT", DEVFS_FL_DEFAULT, 0, 0, 
					S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &aviaEXT_fops, NULL))){
		printk(KERN_ERR "aviaEXT: could not register with devfs.\n");
		return -EIO;
	}
#endif
	return 0;
}

static void __exit aviaEXT_exit(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
#ifdef CONFIG_DEVFS_FS
	devfs_remove("dbox/aviaEXT");
#endif /* CONFIG_DEVFS_FS */
	misc_deregister(&ext_dev);
#else
	devfs_unregister(devfs_h);
#endif
}

module_init(aviaEXT_init);
module_exit(aviaEXT_exit);

MODULE_AUTHOR("Carsten Juttner <carjay@gmx.net>");
MODULE_DESCRIPTION("AViA non-API extensions");
MODULE_LICENSE("GPL");
