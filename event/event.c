/*
 *	event.c - global event driver (dbox-II-project)
 *
 *	Homepage: http://dbox2.elxsi.de
 *
 *	Copyright (C) 2001 Gillem (gillem@berlios.de)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *	$Log: event.c,v $
 *	Revision 1.3  2001/12/08 15:20:58  gillem
 *	- remove debug stuff ;-)
 *	
 *	Revision 1.2  2001/12/08 15:13:54  gillem
 *	- add more functions
 *	
 *	Revision 1.1  2001/12/07 14:30:37  gillem
 *	- initial release (not ready today)
 *	
 *
 *	$Revision: 1.3 $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/devfs_fs_kernel.h>

#include <dbox/event.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

#define EVENTBUFFERSIZE		32

struct event_data_t {
	struct event_t event[EVENTBUFFERSIZE];
	u16 event_free;
	u16 event_ptr;
	u16 event_read_ptr;
};

static struct event_data_t event_data;

static DECLARE_WAIT_QUEUE_HEAD(event_wait);

static spinlock_t event_lock;

static devfs_handle_t devfs_handle;

static int event_ioctl (struct inode *inode, struct file *file,
                         unsigned int cmd, unsigned long arg);
static int event_open (struct inode *inode, struct file *file);

static ssize_t event_read (struct file *file, char *buf, size_t count, loff_t *offset);

static struct file_operations event_fops = {
	owner:		THIS_MODULE,
        read:           event_read,
	ioctl:		event_ioctl,
	open:		event_open
};

#define dprintk     if (debug) printk

static int debug = 0;

int event_write_message( struct event_t * event, size_t count )
{
	int retval = -1;

	spin_lock (&event_lock);

	// only one event at a time	
	if ( count == 1 )
	{
		if ( event_data.event_free > 0 )
		{
			event_data.event_free--;
			memcpy( &event_data.event[event_data.event_ptr], event, sizeof(event_t) );
                        event_data.event_ptr++;
			if ( EVENTBUFFERSIZE == event_data.event_ptr )
	                        event_data.event_ptr = 0;
			retval=0;
		}
	}

	spin_unlock (&event_lock);

	wake_up_interruptible(&event_wait);	

	return 0;
}

static int event_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                  unsigned long arg)
{
	return 0;
}

static int event_open (struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t event_read (struct file *file, char *buf, size_t count, loff_t *offset)
{
	DECLARE_WAITQUEUE(wait, current);
	ssize_t retval = 0;
        int err = 0;

	if (count < sizeof(struct event_t))
		return -EINVAL;

	add_wait_queue(&event_wait, &wait);

	current->state = TASK_INTERRUPTIBLE;

	do {
		err = 0;

		spin_lock_irq (&event_lock);

		if ( EVENTBUFFERSIZE != event_data.event_free ) {
			dprintk("found data !\n");
			retval = put_user(event_data.event[event_data.event_read_ptr], (struct event_t *)buf);

			if (!retval) {
				event_data.event_free++;

				event_data.event_read_ptr++;
				if ( EVENTBUFFERSIZE == event_data.event_read_ptr )
					event_data.event_read_ptr = 0;

				retval = sizeof(unsigned long);
			}
			else
				err = 1;
                }
		else
			dprintk("no data !\n");

		spin_unlock_irq (&event_lock);

		if (err || retval)
			break;

		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			break;
		}
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	} while (1);

	current->state = TASK_RUNNING;

	remove_wait_queue(&event_wait, &wait);

	return retval;
}

#ifdef MODULE
int init_module(void)
#else
int event_init(void)
#endif
{
	printk("event: init ...\n");

	event_data.event_ptr = 0;
	event_data.event_read_ptr = 0;
	event_data.event_free = EVENTBUFFERSIZE;

	devfs_handle = devfs_register ( NULL, "dbox/event0", DEVFS_FL_DEFAULT,
		0, 0,
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
		&event_fops, NULL );

	if ( ! devfs_handle )
	{
		return -EIO;
	}

	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
	printk("event: cleanup ...\n");

	devfs_unregister ( devfs_handle );
}
#endif
