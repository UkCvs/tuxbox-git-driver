/*
 * $Id: avs_core.c,v 1.27.2.4 2005/08/02 20:38:03 carjay Exp $
 * 
 * audio/video switch core driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2001-2002 Gillem gillem@berlios.de
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
#include <linux/version.h>

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/devfs_fs_kernel.h>

#include <dbox/avs_core.h>
#include <dbox/event.h>

#include <tuxbox/info_dbox2.h>

#include "cxa2092.h"
#include "cxa2126.h"
#include "stv6412.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#else
#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif
#endif

TUXBOX_INFO(dbox2_mid);

static int addr;
static int type = CXAAUTO;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static devfs_handle_t devfs_handle;
#endif

/*
 * Addresses to scan
 */
static unsigned short normal_i2c[]		= {I2C_CLIENT_END};
static unsigned short normal_i2c_range[]	= { 0x90>>1,0x94>>1,I2C_CLIENT_END};
static unsigned short probe[2]			= { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short probe_range[2]		= { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore[2]			= { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore_range[2]		= { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short force[2]			= { I2C_CLIENT_END, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	normal_i2c,
	normal_i2c_range,
	probe,
	probe_range,
	ignore,
	ignore_range,
	force
};

static struct i2c_driver avs_i2c_driver;
static struct i2c_client client_template = {
	.name = "AVS",
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12)
	.id = I2C_DRIVERID_AVS,
#endif
	.flags = 0,
	.driver = &avs_i2c_driver
};

static int client_registered;
static int avs_mixerdev;

struct avs_type {
	char *name;
	u8 Vendor;
	u8 Type;
};

static struct avs_type avs_types[] = {
	{"CXA2092", VENDOR_SONY,		CXA2092 },
	{"CXA2126", VENDOR_SONY,		CXA2126 },
	{"STV6412", VENDOR_STMICROELECTRONICS,	STV6412 }
};

struct s_avs {
	int type;	/* chip type */
};

struct s_avs *avs_data;



/*
 * event stuff
 */

#define AVS_EVENT_TIMER 1

static struct timer_list avs_event_timer;

typedef struct avs_event_reg {
	u8 state;
} avs_event_reg;

/*
 * mixer
 */

static const struct {
	unsigned volidx:4;
	unsigned left:4;
	unsigned right:4;
	unsigned stereo:1;
	unsigned recmask:13;
	unsigned avail:1;
} mixtable[SOUND_MIXER_NRDEVICES] = {
  [SOUND_MIXER_VOLUME] = { 0, 0x0, 0x0, 0, 0x0000, 1 },   /* master */
};

static loff_t avs_llseek_mixdev(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}

static int mixer_ioctl(unsigned int cmd, unsigned long arg)
{
	unsigned int i,l;
	int val;

	if (_SIOC_DIR(cmd) == _SIOC_READ)
	{
		printk("mixer: READ\n");
		switch (_IOC_NR(cmd))
		{
			case SOUND_MIXER_RECSRC:
				printk("mixer: 1\n");
				return put_user(0,(int *)arg);
			case SOUND_MIXER_DEVMASK:
				printk("mixer: 2\n");
				return put_user(1,(int *)arg);
			case SOUND_MIXER_RECMASK:
				printk("mixer: 3\n");
				return put_user(0,(int *)arg);
			case SOUND_MIXER_STEREODEVS:
				printk("mixer: 4\n");
				return put_user(0,(int *)arg);
			case SOUND_MIXER_CAPS:
				printk("mixer: 5\n");
				return put_user(0,(int *)arg);
			case SOUND_MIXER_IMIX:
				return -EINVAL;
			default:
				i = _IOC_NR(cmd);

				if (i >= SOUND_MIXER_NRDEVICES || !mixtable[i].avail)
				{
					return -EINVAL;
				}

				switch (type)
				{
					case CXA2092:
						l=cxa2092_get_volume();
						break;
					case CXA2126:
						l=cxa2126_get_volume();
						break;
					case STV6412:
						l=stv6412_get_volume();
						break;
					default:
						return -EINVAL;
				}

				l = (((63-l)*100)/63);
				return put_user(l,(int *)arg);
		}
	}


	if (_SIOC_DIR(cmd) != (_SIOC_READ|_SIOC_WRITE))
	{
		return -EINVAL;
	}

	printk("mixer: write\n");
	switch (_IOC_NR(cmd))
	{
		case SOUND_MIXER_IMIX:
				return -EINVAL;
		case SOUND_MIXER_RECSRC:
			return -EINVAL;
		default:
			i = _IOC_NR(cmd);

			if (i >= SOUND_MIXER_NRDEVICES || !mixtable[i].avail)
			{
				return -EINVAL;
			}

			if(get_user(val, (int *)arg))
				return -EFAULT;

			l = val & 0xff;

			if (l > 100)
				l = 100;

			l = (63-((63*l)/100));

			switch (type)
			{
				case CXA2092:
					return cxa2092_set_volume(&client_template, l );
					break;
				case CXA2126:
					return cxa2126_set_volume(&client_template, l );
					break;
				case STV6412:
					return stv6412_set_volume(&client_template, l );
					break;
				default:
					return -EINVAL;
			}
	}

	return 0;
}

static int avs_ioctl_mixdev(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	return mixer_ioctl(cmd, arg);
}

static struct file_operations avs_mixer_fops = {
	owner:		THIS_MODULE,
	llseek:		avs_llseek_mixdev,
	ioctl:		avs_ioctl_mixdev,
};



/*
 * i2c probe
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static int avs_attach(struct i2c_adapter *adap, int addr, int kind)
#else
static int avs_attach(struct i2c_adapter *adap, int addr, unsigned short flags, int kind)
#endif
{
	struct i2c_client *client;

	dprintk("[AVS]: attach\n");

	if (client_registered > 0) {
		dprintk("[AVS]: attach failed\n");
		return -1;
	}

	client_registered++;

	dprintk("[AVS]: chip found @ 0x%x\n",addr);

	if (!(client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
		dprintk("[AVS]: attach nomem 1\n");
		return -ENOMEM;
	}

	/*
		NB: this code only works for a single client
	*/
	client_template.adapter = adap;
	client_template.addr = addr;
	memcpy(client, &client_template, sizeof(struct i2c_client));
	avs_data = kmalloc(sizeof(struct s_avs), GFP_KERNEL);

	if (!avs_data)
	{
		dprintk("[AVS]: attach nomem 2\n");
		kfree(client);
		return -ENOMEM;
	}

	memset(avs_data, 0, sizeof(struct s_avs));

	if ((type >= 0) && (type < AVS_COUNT)) {
		avs_data->type = type;
		strncpy(client->name, avs_types[avs_data->type].name, sizeof(client->name));
	}
	else {
		avs_data->type = -1;
	}

	dprintk("[AVS]: attach final\n");

	i2c_attach_client(client);

	dprintk("[AVS]: attach final ok\n");

	return 0;
}

static int avs_probe(struct i2c_adapter *adap)
{
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
#endif
	
	dprintk("[AVS]: probe\n");

	if (addr) {
		normal_i2c_range[0] = addr;
		normal_i2c_range[1] = addr;
	}

	ret = i2c_probe(adap, &addr_data, avs_attach);

	dprintk("[AVS]: probe end %d\n",ret);

	return ret;
}

static int avs_detach(struct i2c_client *client)
{
	dprintk("[AVS]: detach\n");

	i2c_detach_client(client);

	if (avs_data)
		kfree(avs_data);
	
	if (client)
		kfree(client);

	return 0;
}



/*
 * devfs fops
 */

static int avs_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	int err = 0;

	dprintk("[AVS]: command\n");

	switch (type) {
	case CXA2092:
		err = cxa2092_command(client, cmd, arg);
		break;
	case CXA2126:
		err = cxa2126_command(client, cmd, arg);
		break;
	case STV6412:
		err = stv6412_command(client, cmd, arg);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int avs_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	dprintk("[AVS]: IOCTL\n");

	if (cmd == AVSIOGTYPE)
		return put_user(type, (int *) arg);

	return avs_command(&client_template, cmd, (void *) arg);
}

static struct file_operations avs_fops = {
	owner:		THIS_MODULE,
	ioctl:		avs_ioctl,
};



/*
 * event
 */

static void avs_event_task(void *data)
{
	struct avs_event_reg *reg;
	struct event_t event;
	int state;

	reg = (struct avs_event_reg *) data;

	if (reg) {

		switch(type) {
		case CXA2092:
			state = cxa2092_get_status(&client_template);
			break;
		case CXA2126:
			state = cxa2126_get_status(&client_template);
			break;
		case STV6412:
			state = stv6412_get_status(&client_template);
			break;
		default:
			state = -1;
			break;
		}

		if (state != -1) {

			if ((state & 0x0f) != (reg->state & 0x0f)) {

				dprintk("[AVS]: state change %02X -> %02X\n", reg->state, state);

				switch (type) {
				case CXA2092:
				case STV6412:
					if ((state & 0x0c) != (reg->state & 0x0c)) {
						dprintk("[AVS]: vcr state change %02X -> %02X\n", (reg->state >> 2) & 3, (state >> 3) & 3);
						event.event = EVENT_SBVCR_CHANGE;
						event_write_message(&event, 1);
					}
					if ((state & 0x03) != (reg->state & 0x03)) {
						dprintk("[AVS]: tv state change %02X -> %02X\n", reg->state & 3, state & 3);
						event.event = EVENT_SBTV_CHANGE;
						event_write_message(&event, 1);
					}
					break;
				case CXA2126:
					if ((state & 0x0c) != (reg->state & 0x0c)) {
						dprintk("[AVS]: tv state change %02X -> %02X\n", (reg->state >> 2) & 3, (state >> 3) & 3);
						event.event = EVENT_SBTV_CHANGE;
						event_write_message(&event, 1);
					}
					if ((state & 0x03) != (reg->state & 0x03)) {
						dprintk("[AVS]: vcr state change %02X -> %02X\n", reg->state & 3, state & 3);
						event.event = EVENT_SBVCR_CHANGE;
						event_write_message(&event, 1);
					}
					break;
				}

				reg->state = state;
			}
		}

		mod_timer(&avs_event_timer, jiffies + HZ / AVS_EVENT_TIMER + 2 * HZ / 100);
	}
	else
	{
		dprintk("[AVS]: event task error\n");
	}
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static DECLARE_WORK(avs_work, avs_event_task, NULL);
#else
static struct tq_struct avs_event_tasklet = {
	routine: avs_event_task,
	data: 0
};
#endif

static void avs_event_func(unsigned long data)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	void *d = (void *)data;
	PREPARE_WORK(&avs_work, avs_event_task, d);
	schedule_work(&avs_work);
#else
	avs_event_tasklet.data = (void *) data;
	schedule_task(&avs_event_tasklet);
#endif
}

static int avs_event_init(void)
{
	struct avs_event_reg *reg;

	dprintk("[AVS]: event init\n");

	reg = (avs_event_reg*) kmalloc(sizeof(struct avs_event_reg), GFP_KERNEL);

	if (!reg)
		return -ENOMEM;
		
	switch (type) {
	case CXA2092:
		reg->state = cxa2092_get_status(&client_template);
		break;
	case CXA2126:
		reg->state = cxa2126_get_status(&client_template);
		break;
	case STV6412:
		reg->state = stv6412_get_status(&client_template);
		break;
	}

	init_timer(&avs_event_timer);

	avs_event_timer.function = avs_event_func;
	avs_event_timer.expires  = jiffies + HZ/AVS_EVENT_TIMER + 2 * HZ / 100;
	avs_event_timer.data     = (unsigned long) reg;

	add_timer(&avs_event_timer);
	return 0;
}

static void avs_event_cleanup(void)
{
	dprintk("[AVS]: event cleanup\n");

	del_timer_sync(&avs_event_timer);
	if (avs_event_timer.data) {
		kfree((char*) avs_event_timer.data);
		avs_event_timer.data = 0;
	}

}



/*
 * i2c
 */

static struct i2c_driver avs_i2c_driver = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)	
	.owner			= THIS_MODULE,
#endif
	.name           = "avs driver",
	.id             = I2C_DRIVERID_AVS,
	.flags          = I2C_DF_NOTIFY,
	.attach_adapter = &avs_probe,
	.detach_client  = &avs_detach,
	.command        = &avs_command
};



/*
 * module init/exit
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static struct miscdevice avs_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "avswitch",
	.fops = &avs_fops
};
#endif

int __init avs_core_init(void)
{
	int res;

	if (type == CXAAUTO) {

		switch(tuxbox_dbox2_mid) {
		case TUXBOX_DBOX2_MID_NOKIA:
			type = CXA2092;
			break;
		case TUXBOX_DBOX2_MID_PHILIPS:
			type = STV6412;
			break;
		case TUXBOX_DBOX2_MID_SAGEM:
			type = CXA2126;
			break;
		default:
			return -ENODEV;
		}
	}

	if ((res = i2c_add_driver(&avs_i2c_driver))) {
		dprintk("[AVS]: i2c add driver failed\n");
		return res;
	}

	if (!client_registered){
		printk(KERN_ERR "avs: no client found\n");
		return -EIO;
	}
	
	switch (type) {
	case CXA2092:
		cxa2092_init(&client_template);
		break;
	case CXA2126:
		cxa2126_init(&client_template);
		break;
	case STV6412:
		stv6412_init(&client_template);
		break;
	default:
		printk(KERN_ERR "avs: unknown type %d\n", type);
		i2c_del_driver(&avs_i2c_driver);
		return -EIO;
	}

	if (avs_event_init() != 0) {
		i2c_del_driver(&avs_i2c_driver);
		return -ENOMEM;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	if (misc_register(&avs_dev)<0){
		printk(KERN_ERR "avs: unable to register device\n");
		return -EIO;
	}
	devfs_mk_cdev(MKDEV(MISC_MAJOR,avs_dev.minor),
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
		"dbox/avs0");
#else
	devfs_handle = devfs_register(NULL, "dbox/avs0", DEVFS_FL_DEFAULT, 0, 0,
			S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
			&avs_fops, NULL);

	if (!devfs_handle) {
		i2c_del_driver(&avs_i2c_driver);
		return -EIO;
	}
#endif

	avs_mixerdev = register_sound_mixer(&avs_mixer_fops, -1);

	return 0;
}

void __exit avs_core_exit(void)
{
	avs_event_cleanup();

	unregister_sound_mixer(avs_mixerdev);

	i2c_del_driver(&avs_i2c_driver);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	devfs_remove("dbox/avs0");
	misc_deregister(&avs_dev);
#else
	devfs_unregister(devfs_handle);
#endif
}

module_init(avs_core_init);
module_exit(avs_core_exit);

MODULE_AUTHOR("Gillem <gillem@berlios.de>");
MODULE_DESCRIPTION("dbox2 audio/video switch core driver");
MODULE_LICENSE("GPL");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
module_param(addr,int,0);
module_param(type,int,0);
#else
MODULE_PARM(addr,"i");
MODULE_PARM(type,"i");
#endif
