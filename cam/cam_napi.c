/*
 * $Id: cam_napi.c,v 1.4 2002/12/03 20:58:57 obi Exp $
 *
 * Copyright (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
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
#include <linux/init.h>

#include <avia/avia_napi.h>
#include <dvb-core/dvbdev.h>
#include <linux/dvb/ca.h>

#include <dbox/cam.h>


static struct dvb_device *ca_dev = NULL;


static struct ca_slot_info slot_info_0 = {
	.num = 0,
	.type = 0,
	.flags = 0,
};


static struct ca_slot_info slot_info_1 = {
	.num = 1,
	.type = 0,
	.flags = 0,
};


static struct ca_descr_info descr_info = {
	.num = 0,
	.type = 0,
};


static struct ca_caps caps = {
	.slot_num = 2,
	.slot_type = 0,
	.descr_num = 0,
	.descr_type = 0,
};


static int cam_napi_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *parg)
{

	int ret = 0;

	if (((file->f_flags & O_ACCMODE) == O_RDONLY) &&
		((cmd != CA_GET_CAP) ||
		(cmd != CA_GET_SLOT_INFO) ||
		(cmd != CA_GET_DESCR_INFO) ||
		(cmd != CA_GET_MSG)))
			return -EPERM;

	switch (cmd) {

		case CA_RESET:
			ret = cam_reset();
			break;

		case CA_GET_CAP:
			memcpy(parg, &caps, sizeof(struct ca_caps));
			break;

		case CA_GET_SLOT_INFO:
			switch (((struct ca_slot_info *)parg)->num) {
			case 0:
				memcpy(parg, &slot_info_0, sizeof(struct ca_slot_info));
				break;
			case 1:
				memcpy(parg, &slot_info_1, sizeof(struct ca_slot_info));
				break;
			default:
				ret = -EINVAL;
				break;
			}
			break;

		case CA_GET_DESCR_INFO:
			memcpy(parg, &descr_info, sizeof(struct ca_descr_info));
			break;

		case CA_GET_MSG:
		{
			struct ca_msg *msg = parg;
			msg->index = 0;
			msg->type = 0;
			msg->length = cam_read_message(msg->msg, msg->length);
			break;
		}

		case CA_SEND_MSG:
		{
			struct ca_msg *msg = parg;
			ret = cam_write_message(msg->msg, msg->length);
			break;
		}

		case CA_SET_DESCR:
			ret = -EOPNOTSUPP;
			break;

		case CA_SET_PID:
			ret = -EOPNOTSUPP;
			break;

		default:
			ret = -EINVAL;
			break;
	}

	return ret;

}


static struct file_operations cam_napi_fops = {

	.owner = THIS_MODULE,
	.ioctl = dvb_generic_ioctl,
	.open = dvb_generic_open,
	.release = dvb_generic_release,

};


static struct dvb_device cam_napi_dev = {

	.priv = 0,
	.users = 1,
	.writers = 1,
	.fops = &cam_napi_fops,
	.kernel_ioctl = cam_napi_ioctl,

};

int cam_napi_init(void)
{

	int result;

	printk("$Id: cam_napi.c,v 1.4 2002/12/03 20:58:57 obi Exp $\n");
	
	if ((result = dvb_register_device(avia_napi_get_adapter(), &ca_dev, &cam_napi_dev, NULL, DVB_DEVICE_CA)) < 0)
		printk("cam_napi: cam_napi_register failed (errno = %d)\n", result);

	return result;

}


void cam_napi_exit(void)
{

	dvb_unregister_device(ca_dev);

}

module_init(cam_napi_init);
module_exit(cam_napi_exit);

#ifdef MODULE
MODULE_DESCRIPTION("dbox2 cam dvb api driver");
MODULE_AUTHOR("Andreas Oberritter <obi@saftware.de>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif
