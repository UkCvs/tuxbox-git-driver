/*
 *   avia_gt_pig.c - pig driver for AViA eNX/GTX (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2001-2002 Florian Schirmer <jolt@tuxbox.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *   $Log: avia_gt_pig.c,v $
 *   Revision 1.9  2002/04/12 18:59:29  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.8  2002/04/12 14:28:13  Jolt
 *   eNX/GTX merge
 *
 *
 *
 *   $Revision: 1.9 $
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
#include <linux/devfs_fs_kernel.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

#include <dbox/avia_gt.h>
#include <dbox/avia_gt_capture.h>
#include <dbox/avia_gt_pig.h>

#define ENX_PIG_COUNT 2

//#define CAPTURE_WIDTH 720
#define CAPTURE_WIDTH 640
#define CAPTURE_HEIGHT 576
#define PIG_WIDTH (160*3)
#define PIG_HEIGHT (72*3)

static devfs_handle_t devfs_handle[ENX_PIG_COUNT];
static unsigned char pig_busy[ENX_PIG_COUNT] = {0, 0};
static unsigned char *pig_buffer[ENX_PIG_COUNT] = {NULL, NULL};
static unsigned short pig_stride[ENX_PIG_COUNT] = {0, 0};
static unsigned int pig_offset[ENX_PIG_COUNT] = {0, 0};

static int avia_gt_pig_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t avia_gt_pig_read(struct file *file, char *buf, size_t count, loff_t *offset)
{
    return 0;
}

static int avia_gt_pig_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int avia_gt_pig_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned char pig_nr = (unsigned char)MINOR(file->f_dentry->d_inode->i_rdev);
    
    if (pig_nr >= ENX_PIG_COUNT)
	return -ENODEV;

    switch(cmd) {
    
	case AVIA_PIG_HIDE:

	    avia_gt_pig_hide(pig_nr);

	break;

	case AVIA_PIG_SET_POS:

	    return avia_gt_pig_set_pos(pig_nr, (unsigned short)((arg >> 16) & 0xFFFF), (unsigned short)(arg & 0xFFFF));

	break;

	case AVIA_PIG_SET_SIZE:

	    return avia_gt_pig_set_size(pig_nr, (unsigned short)((arg >> 16) & 0xFFFF), (unsigned short)(arg & 0xFFFF), 0);

	break;

	case AVIA_PIG_SET_STACK:

	    return avia_gt_pig_set_stack(pig_nr, (unsigned char)(arg & 0xFF));

	break;
	
	case AVIA_PIG_SHOW:

	    avia_gt_pig_show(pig_nr);

	break;

    }

    return 0;
}

static ssize_t avia_gt_pig_write(struct file *file, const char *buf, size_t count, loff_t *offset) 
{
    unsigned char pig_nr = (unsigned char)MINOR(file->f_dentry->d_inode->i_rdev);
    unsigned char *kbuf;
    unsigned char *vbuf;
    unsigned int todo;
    
    if (pig_nr >= ENX_PIG_COUNT)
	return -ENODEV;

    if (!pig_busy[pig_nr])
	return -EIO;

    /*if ((kbuf = kmalloc(count, GFP_KERNEL)) == NULL )
	return -ENOMEM;

    if (copy_from_user(kbuf, buf, count) ) {
    
	kfree(kbuf);
	return -EFAULT;
	
    }
    
    vbuf = pig_buffer[pig_nr];
    
    while (count > 0) {
    
	count -= PIG_WIDTH;
	vbuf += pig_stride[pig_nr];
    
    }
	
    kfree(kbuf);*/
    
    if ((pig_offset[pig_nr] + count) >= (PIG_WIDTH * PIG_HEIGHT)) {
	todo = (pig_offset[pig_nr] + count) - (PIG_WIDTH * PIG_HEIGHT);
	count -= todo;
    } else {
	todo = 0;
    }
    
    kbuf = avia_gt_get_mem_addr();
    kbuf = (unsigned char *)(((unsigned int)kbuf) + ((unsigned int)(pig_buffer[pig_nr])) + (pig_offset[pig_nr]));
    
    if (copy_from_user(kbuf, buf, count)) {
	printk("avia_gt_pig: copy_from_user failed\n");
	return -EFAULT;
    }

    pig_offset[pig_nr] += count;
    
    if (todo) {
        pig_offset[pig_nr] = 0;
	count = todo;
	
        kbuf = avia_gt_get_mem_addr();
	kbuf = (unsigned char *)(((unsigned int)kbuf) + ((unsigned int)(pig_buffer[pig_nr])) + (pig_offset[pig_nr]));
    
	if (copy_from_user(kbuf, buf, count)) {
	    printk("avia_gt_pig: copy_from_user failed\n");
	    return -EFAULT;
	}
	
	pig_offset[pig_nr] += count;
    }
    
    printk("avia_gt_pig: wrote %d bytes\n", count);

    return 0;
}

static struct file_operations avia_gt_pig_fops = {
	owner:  	THIS_MODULE,
	read:   	avia_gt_pig_read,
	ioctl:  	avia_gt_pig_ioctl,
	open:   	avia_gt_pig_open,
	release:	avia_gt_pig_release,
	write:		avia_gt_pig_write
};

int avia_gt_pig_hide(unsigned char pig_nr)
{
    if (pig_nr >= ENX_PIG_COUNT)
	return -ENODEV;

    if (pig_busy[pig_nr]) {

	enx_reg_w(VPSA1) |= 1;

	avia_gt_capture_stop();
    
	pig_busy[pig_nr] = 0;
	
    }

    return 0;
}

int avia_gt_pig_set_pos(unsigned char pig_nr, unsigned short x, unsigned short y)
{
    if (pig_nr >= ENX_PIG_COUNT)
	return -ENODEV;

    enx_reg_s(VPP1)->HPOS = 63 + ( x / 2);
    enx_reg_s(VPP1)->VPOS = 21 + y;
    
    return 0;
}

int avia_gt_pig_set_stack(unsigned char pig_nr, unsigned char stack_order)
{
    if (pig_nr >= ENX_PIG_COUNT)
	return -ENODEV;
	
    enx_reg_h(VPSO1) = stack_order;							
    
    return 0;
}

int avia_gt_pig_set_size(unsigned char pig_nr, unsigned short width, unsigned short height, unsigned char stretch)
{
    int result;

    if (pig_nr >= ENX_PIG_COUNT)
	return -ENODEV;
	
    if (pig_busy[pig_nr])
	return -EBUSY;
	
    result = avia_gt_capture_set_output(width, height);
    
    if (result < 0)
	return result;

    enx_reg_s(VPSZ1)->WIDTH = width / 2;
    enx_reg_s(VPSZ1)->S = stretch;
    enx_reg_s(VPSZ1)->HEIGHT = height / 2;
    
    printk("avia_gt_pig: WIDTH=0x%X, S=0x%X, HEIGHT=0x%X\n", enx_reg_s(VPSZ1)->WIDTH, enx_reg_s(VPSZ1)->S, enx_reg_s(VPSZ1)->HEIGHT);
    
    return 0;
}

int avia_gt_pig_show(unsigned char pig_nr)
{
    unsigned short odd_offset;

    if (pig_nr >= ENX_PIG_COUNT)
	return -ENODEV;
	
    if (pig_busy[pig_nr])
	return -EBUSY;

    avia_gt_capture_set_input(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT);
    avia_gt_capture_start(&pig_buffer[pig_nr], &pig_stride[pig_nr], &odd_offset);

    printk("avia_gt_pig: buffer=0x%X, stride=0x%X\n", (unsigned int)pig_buffer[pig_nr], pig_stride[pig_nr]);

    enx_reg_h(VPSTR1) = 0;				
    enx_reg_h(VPSTR1) |= (((((unsigned int)(pig_stride[pig_nr])) / 4) & 0x7FF) << 2);
    enx_reg_h(VPSTR1) |= 1;				// Enable hardware double buffering
    
    enx_reg_s(VPSZ1)->P = 0;
    
    enx_reg_w(VPSA1) = 0;
    enx_reg_w(VPSA1) |= ((unsigned int)pig_buffer[pig_nr] & 0xFFFFFC);			// Set buffer address (for non d-buffer mode)
    
//    enx_reg_s(VPOFFS1)->OFFSET = odd_offset >> 2;
    enx_reg_s(VPOFFS1)->OFFSET = 0;

    enx_reg_s(VPP1)->U = 0;
    enx_reg_s(VPP1)->F = 0;
        
    enx_reg_w(VPSA1) |= 1;
    
    pig_busy[pig_nr] = 1;
    
    return 0;
}

int __init avia_gt_pig_init(void)
{

    unsigned char pig_nr = 0;

    printk("$Id: avia_gt_pig.c,v 1.9 2002/04/12 18:59:29 Jolt Exp $\n");

    devfs_handle[0] = devfs_register(NULL, "dbox/pig0", DEVFS_FL_DEFAULT, 0, 0, S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &avia_gt_pig_fops, NULL);

    if (!devfs_handle[0])
	return -EIO;
    
    enx_reg_w(RSTR0) &= ~(1 << 7);							// Take video pig out of reset

    avia_gt_pig_set_pos(pig_nr, 150, 50);
    avia_gt_pig_set_size(pig_nr, PIG_WIDTH, PIG_HEIGHT, 0);
    avia_gt_pig_set_stack(pig_nr, 1);
    
    //avia_gt_pig_show(pig_nr);

    return 0;
    
}

void __exit avia_gt_pig_exit(void)
{

    devfs_unregister(devfs_handle[0]);

    avia_gt_pig_hide(0);
    
    enx_reg_w(RSTR0) |= (1 << 7);
    
}

#if defined(MODULE) && defined(STANDALONE)
module_init(avia_gt_pig_init);
module_exit(avia_gt_pig_exit);
#endif
