/*
 * dvbdev.c
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *		  & Marcus Metzler <marcus@convergence.de>
 *		    for convergence integrated media GmbH
 *	       2001 Bastian Blank <bastianb@gmx.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id: dvbdev.c,v 1.9 2002/05/08 13:21:50 obi Exp $
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/kmod.h>

#include "dvbdev.h"

static struct dvb_device * dvb_device = NULL;

static devfs_handle_t devfs_dir_handle_dvb;
static devfs_handle_t devfs_dir_handle_ost;

static char * dnames[DVB_DEVICES_NUM] =
{
	"video0",
	"audio0",
	"sec0",
	"frontend0",
	"demux0",
	"dvr0",
	"ca0",
	"net0",
	"osd0"
};

static int dvbdev_open (struct inode * inode, struct file * file)
{
	dvbdev_devfsinfo_t * info;
	int err;

	info = (dvbdev_devfsinfo_t *) devfs_get_info(devfs_get_handle_from_inode(inode));

	err = info->device->open(info->device, info->type, inode, file);

	if (err < 0)
	{
		return err;
	}

	return 0;
}

static int dvbdev_release (struct inode * inode, struct file * file)
{
	dvbdev_devfsinfo_t * info;
	int err;

	info = (dvbdev_devfsinfo_t *) devfs_get_info(devfs_get_handle_from_inode(inode));

	err = info->device->close(info->device, info->type, inode, file);

	if (err < 0)
	{
		return err;
	}

	return 0;
}

static ssize_t dvbdev_read (struct file * file, char * buf, size_t count, loff_t * ppos)
{
	dvbdev_devfsinfo_t * info;

	info = (dvbdev_devfsinfo_t *) devfs_get_info(devfs_get_handle_from_inode(file->f_dentry->d_inode));

	return info->device->read(info->device, info->type, file, buf, count, ppos);
}

static ssize_t dvbdev_write (struct file * file, const char * buf, size_t count, loff_t * ppos)
{
	dvbdev_devfsinfo_t * info;

	info = (dvbdev_devfsinfo_t *) devfs_get_info(devfs_get_handle_from_inode(file->f_dentry->d_inode));

	return info->device->write(info->device, info->type, file, buf, count, ppos);
}

static int dvbdev_ioctl (struct inode *inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	dvbdev_devfsinfo_t * info;

	info = (dvbdev_devfsinfo_t *) devfs_get_info(devfs_get_handle_from_inode(inode));

	return info->device->ioctl(info->device, info->type, file, cmd, arg);
}

unsigned int dvbdev_poll (struct file * file, poll_table * wait)
{
	dvbdev_devfsinfo_t * info;

	info = (dvbdev_devfsinfo_t *) devfs_get_info(devfs_get_handle_from_inode(file->f_dentry->d_inode));

	return info->device->poll(info->device, info->type, file, wait);
}

static struct file_operations dvbdev_fops =
{
	open:		dvbdev_open,
	release:	dvbdev_release,
	read:		dvbdev_read,
	write:		dvbdev_write,
	ioctl:		dvbdev_ioctl,
	poll:		dvbdev_poll
};

void dvbdev_devfs_init (void)
{
	devfs_dir_handle_dvb = devfs_mk_dir(NULL, "dvb", NULL);
	devfs_dir_handle_ost = devfs_mk_dir(NULL, "ost", NULL);
}

void dvbdev_devfs_cleanup (void)
{
	devfs_unregister(devfs_dir_handle_ost);
	devfs_unregister(devfs_dir_handle_dvb);
}

void dvbdev_devfs_register_dev (dvb_device_t * dev)
{
	unsigned char i;
	char path[24];

	dev->devfs_handle_dvb_card = devfs_mk_dir(devfs_dir_handle_dvb, "card0", NULL);

	for (i = 0; i < DVB_DEVICES_NUM; i++)
	{
		dev->devfs_info[i].device = dev;
		dev->devfs_info[i].type = i;
		dev->devfs_handle_dvb_device[i] = devfs_register(dev->devfs_handle_dvb_card, dnames[i], DEVFS_FL_DEFAULT, 0, 0,
				S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &dvbdev_fops, &dev->devfs_info[i]);

		sprintf(path, "../dvb/card0/%s", dnames[i]);
		devfs_mk_symlink(devfs_dir_handle_ost, dnames[i], DEVFS_FL_DEFAULT, path, NULL, NULL);
	}
}

void dvbdev_devfs_unregister_dev (dvb_device_t * dev)
{
	unsigned char i;

	for (i = 0; i < DVB_DEVICES_NUM; i++)
	{
		devfs_unregister(dev->devfs_handle_dvb_device[i]);
	}

	devfs_unregister(dev->devfs_handle_dvb_card);
}

int dvb_register_device (dvb_device_t * dev)
{
	if (dvb_device == NULL)
	{
		dvb_device = dev;
		dvbdev_devfs_register_dev(dev);
#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif /* MODULE */
		return 0;
	}

	return -ENFILE;
}

void dvb_unregister_device (dvb_device_t * dev)
{
	if (dvb_device == dev)
	{
		dvbdev_devfs_unregister_dev(dev);
		dvb_device = NULL;
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif /* MODULE */
	}
}

#ifdef MODULE
int __init dvbdev_init_module (void)
{
	dvbdev_devfs_init();
	return 0;
}

void __exit dvbdev_cleanup_module (void)
{
	dvbdev_devfs_cleanup();
}

module_init(dvbdev_init_module);
module_exit(dvbdev_cleanup_module);

EXPORT_SYMBOL(dvb_register_device);
EXPORT_SYMBOL(dvb_unregister_device);

MODULE_AUTHOR("Bastian Blank");
MODULE_DESCRIPTION("Device registrar for DVB drivers");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
#endif /* MODULE */
