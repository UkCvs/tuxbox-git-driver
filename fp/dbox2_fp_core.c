/*
 *   fp.c - FP driver (dbox-II-project)
 *
 *   Homepage: http://www.tuxbox.org
 *
 *   Copyright (C) 2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
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
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/moduleparam.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#else
#include <linux/i2c_compat.h>
#include <linux/tqueue.h>
#endif

#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/devfs_fs_kernel.h>

#include <asm/8xx_immap.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mpc8xx.h>
#include <asm/signal.h>
#include <asm/uaccess.h>
#include <asm/machdep.h>

#include <dbox/event.h>
#include <dbox/fp.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif
#endif

#include <dbox/dbox2_fp_core.h>
#include <dbox/dbox2_fp_reset.h>
#include <dbox/dbox2_fp_sec.h>
#include <dbox/dbox2_fp_timer.h>
#include <dbox/dbox2_fp_tuner.h>

TUXBOX_INFO(dbox2_mid);
tuxbox_dbox2_mid_t mid;
static u8 fp_revision;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static devfs_handle_t devfs_handle;
#endif

static int useimap = 1;

#ifdef DEBUG
#define dprintk(fmt, args...) printk(fmt, ##args)
#else
#define dprintk(fmt, args...)
#endif

/*
fp:
 03  deep standby
 10 led on
 11 led off
 dez.
 20 reboot
 21 reboot
 42 lcd off / led off ( alloff ;-) )
 ADDR VAL
 18   X0  X = dimm 0=off F=on
 22 off
*/

#define FP_INTERRUPT		SIU_IRQ2
#define I2C_FP_DRIVERID		0xF060
#define FP_GETID		0x1D

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static struct i2c_driver fp_i2c_driver;
#endif
static struct i2c_client fp_client = {
	.name		= "FP",
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12)
	.id			= I2C_FP_DRIVERID,
#endif
	.flags		= 0,
	.addr		= (0x60 >> 1),
	.adapter 	= NULL,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	.driver		= &fp_i2c_driver,
#endif
};
struct fp_data fp_priv_data;
int fp_major;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static void fp_task(void * arg);
struct tq_struct fp_tasklet = {
	.routine = fp_task,
	.data = NULL,
};
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static irqreturn_t fp_interrupt(int irq, void *dev, struct pt_regs *regs);
#else
static void fp_interrupt(int irq, void *dev, struct pt_regs *regs);
#endif

/*****************************************************************************\
 *   Generic Frontprocessor Functions
\*****************************************************************************/

struct i2c_client *fp_get_i2c(void)
{
	return &fp_client;
}

int fp_cmd(struct i2c_client *client, u8 cmd, u8 *res, u32 size)
{
	int ret;
	struct i2c_msg msg [] = { 
		{
			addr: client->addr,
			flags: 0, 
			buf: &cmd,
			len: 1 
		},
		{
			addr: client->addr,
			flags: I2C_M_RD,
			buf: res,
			len: size 
		} 
	};
	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret != 2)
		printk("fp: fp_cmd error (ret == %d)\n", ret);

#ifdef DEBUG
	{
		int i;

		printk("fp: fp_cmd: %02x\n", cmd);
		printk("fp: fp_recv:");

		for (i = 0; i < size; i++)
			printk(" %02x", res[i]);

		printk("\n");
	}
#endif

	return 0;
}


int fp_sendcmd(struct i2c_client *client, u8 b0, u8 b1)
{
	u8 cmd [] = { b0, b1 };

	dprintk("fp: fp_sendcmd: %02x %02x\n", b0, b1);

	if (i2c_master_send(client, cmd, sizeof(cmd)) != sizeof(cmd))
		return -1;

	return 0;
}

static int fp_getid(struct i2c_client *client)
{
	u8 id [] = { 0x00, 0x00, 0x00 };

	if (fp_cmd(client, FP_GETID, id, sizeof(id)))
		return 0;

	return id[0];
}

/*****************************************************************************\
 *   File Operations
\*****************************************************************************/

static int fp_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int val;

	switch (cmd) {
	case FP_IOCTL_GETID:
		return fp_getid(&fp_client);

	case FP_IOCTL_POWEROFF:
		if (fp_revision >= 0x80)
			return fp_sendcmd(&fp_client, 0x00, 0x03);
		else
			return fp_sendcmd(&fp_client, 0x00, 0x00);

	case FP_IOCTL_REBOOT:
		dbox2_fp_restart("");
		return 0;

	case FP_IOCTL_LCD_DIMM:
		if (copy_from_user(&val, (void*)arg, sizeof(val)) )
			return -EFAULT;

		if (fp_revision >= 0x80)
			return fp_sendcmd(&fp_client, 0x18, val & 0xff);
		else
			return fp_sendcmd(&fp_client, 0x06, val & 0xff);

	case FP_IOCTL_LCD_AUTODIMM:
		/* only works on Sagem and Philips */
		if (copy_from_user(&val, (void*)arg, sizeof(val)) )
			return -EFAULT;

		if (fp_revision >= 0x80)
			return 0;
		else
			return fp_sendcmd(&fp_client, 0x08, val & 0x01);

	case FP_IOCTL_LED:
		if (copy_from_user(&val, (void*)arg, sizeof(val)) )
			return -EFAULT;

		if (fp_revision >= 0x80)
			return fp_sendcmd(&fp_client, 0x00, 0x10 | ((~val) & 1));
		else
			return fp_sendcmd(&fp_client, 0x10 | (val & 1), 0x00);
	
	case FP_IOCTL_GET_WAKEUP_TIMER:
		val = dbox2_fp_timer_get();

		if (val == -1)
			return -EIO;

		if (copy_to_user((void *) arg, &val, sizeof(val)))
			return -EFAULT;

		return 0;

	case FP_IOCTL_SET_WAKEUP_TIMER:
		if (copy_from_user(&val, (void *) arg, sizeof(val)) )
			return -EFAULT;

		return dbox2_fp_timer_set(val);

	case FP_IOCTL_IS_WAKEUP:
	{
		u8 is_wakeup = dbox2_fp_timer_get_boot_trigger();
		if (copy_to_user((void *) arg, &is_wakeup, sizeof(is_wakeup)))
			return -EFAULT;

		return 0;
	}

	case FP_IOCTL_CLEAR_WAKEUP_TIMER:
		return dbox2_fp_timer_clear();
	
	case FP_IOCTL_GET_VCR:{
		int vcrstate = fp_priv_data.fpVCR;
		if (copy_to_user((void *) arg, &vcrstate, sizeof(int)))
			return -EFAULT;
		return 0;
	}

	case FP_IOCTL_GET_REGISTER:
	{
		u32 foo=0;
		int len;
		if (copy_from_user(&val, (void *) arg, sizeof(val)))
	                return -EFAULT;
		len = ((val>>8)&3)+1;
		fp_cmd(&fp_client, val & 0xFF, (u8 *) &foo, len);
		foo = foo >> (4-len)*8;

		if (copy_to_user((void*)arg, &foo, sizeof(foo)))
			return -EFAULT;
		
		return 0;
	}
	
	case FP_IOCTL_SET_REGISTER:
	{
		if (copy_from_user(&val, (void *) arg, sizeof(val)))
	                return -EFAULT;

		fp_sendcmd(&fp_client, val & 0xff, (val >> 8) & 0xff);
		return 0;
	}

	default:
		return -EINVAL;
	}
}

static struct file_operations fp_fops = {
	.owner = THIS_MODULE,
	.ioctl = fp_ioctl,
};

/*****************************************************************************\
 *   I2C Functions
\*****************************************************************************/

static int fp_detach_client(struct i2c_client *client)
{
	int err;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	free_irq(FP_INTERRUPT, &fp_priv_data.fpID);
#endif

	if ((err=i2c_detach_client(client))) {
		dprintk("fp: couldn't detach client driver.\n");
		return err;
	}

	return 0;
}

static void fp_setup_client(void)
{
	ppc_md.restart = dbox2_fp_restart;
	ppc_md.power_off = dbox2_fp_power_off;
	ppc_md.halt = dbox2_fp_power_off;

	dbox2_fp_reset_init();
	dbox2_fp_sec_init();
	dbox2_fp_timer_init();
	dbox2_fp_tuner_init();
}

static int fp_attach_adapter(struct i2c_adapter *adapter)
{
	volatile immap_t *immap = (volatile immap_t *)IMAP_ADDR;
	int err = 0;

	/*
		driver is dbox2-specific and all known species
		only feature one fp
	*/
	if (fp_client.adapter){
		printk(KERN_ERR "fp: fp_attach_adapter can't be called more than once");
		return -EIO;
	}

	fp_client.adapter = adapter;

	/* init vcr value (off) */
	fp_priv_data.fpVCR = 0;

	/*
	 * FP ID
	 * -------------
	 * NOKIA  : 0x5A
	 * SAGEM  : 0x52
	 * PHILIPS: 0x52
	 */
	fp_priv_data.fpID = fp_getid(&fp_client);
	if ((fp_priv_data.fpID != 0x52) && (fp_priv_data.fpID != 0x5a)) {
		printk("fp: unknown fpID %d\n", fp_priv_data.fpID);
		fp_priv_data.fpID = 0x00;
		return -EIO;
	}
	if (useimap) {
		immap->im_ioport.iop_papar &= ~0x0002;
		immap->im_ioport.iop_paodr &= ~0x0002;
		immap->im_ioport.iop_padir |=  0x0002;
		immap->im_ioport.iop_padat &= ~0x0002;
	}
	/* sagem needs this (71) LNB-Voltage 51-V 71-H */
	/*
		fp_sendcmd(&fp_client, 0x04, 0x51);
	*/
	/*
		fp_sendcmd(&fp_client, 0x22, 0xbf);
		fp_cmd(&fp_client, 0x25, buf, 2);
		fp_sendcmd(&fp_client, 0x19, 0x04);
		fp_sendcmd(&fp_client, 0x18, 0xb3);
		fp_cmd(&fp_client, 0x1e, buf, 2);
	*/
	/* disable (non-working) break code */
	dprintk("fp: detect_client 0x26\n");
	fp_sendcmd(&fp_client, 0x26, 0x00);
	/*
		fp_cmd(&fp_client, 0x23, buf, 1);
		fp_cmd(&fp_client, 0x20, buf, 1);
		fp_cmd(&fp_client, 0x01, buf, 2);
	*/

	if ((err = i2c_attach_client(&fp_client)))
		return err;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	if (request_irq(FP_INTERRUPT, fp_interrupt, SA_ONESHOT, "fp", &fp_priv_data.fpID) != 0)
		panic("Could not allocate FP IRQ!");
#endif

	fp_setup_client();
	return 0;
}

static struct i2c_driver fp_i2c_driver = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	.owner			= THIS_MODULE,
#endif
	.name           = "DBox2 Frontprocessor driver",
	.id             = I2C_FP_DRIVERID,
	.flags          = I2C_DF_NOTIFY,
	.attach_adapter = &fp_attach_adapter,
	.detach_client  = &fp_detach_client,
	.command        = NULL
};

/*****************************************************************************\
 *   Interrupt Handler Functions
\*****************************************************************************/

#define QUEUE_COUNT	8

static struct fp_queue {
	u8 busy;
	queue_proc_t queue_proc;
} queue_list[QUEUE_COUNT];

int dbox2_fp_queue_alloc(u8 queue_nr, queue_proc_t queue_proc)
{
	if (queue_nr >= QUEUE_COUNT)
		return -EINVAL;

	if (queue_list[queue_nr].busy)
		return -EBUSY;

	queue_list[queue_nr].busy = 1;
	queue_list[queue_nr].queue_proc = queue_proc;

	return 0;
}

void dbox2_fp_queue_free(u8 queue_nr)
{
	if (queue_nr >= QUEUE_COUNT)
		return;

	queue_list[queue_nr].busy = 0;
}

static void fp_handle_vcr(struct fp_data *dev, int fpVCR)
{
	struct event_t event;

	memset(&event, 0x00, sizeof(event));

	if (dev->fpVCR!=fpVCR) {

		dev->fpVCR = fpVCR;
		event.event = EVENT_VCR_CHANGED;
		event_write_message(&event, 1);

	}
}


#if 0
static void fp_handle_unknown(struct fp_data *dev)
{
	u8 rc;

	fp_cmd(dev->client, 0x24, (u8*)&rc, 1);
	dprintk("fp: mysterious interrupt source 0x40: %x\n", rc);
}
#endif


static void fp_check_queues(void)
{
	u8 status;
	u8 queue_nr;

	dprintk("fp: checking queues.\n");
	fp_cmd(&fp_client, 0x23, &status, 1);

	if (fp_priv_data.fpVCR != status)
		fp_handle_vcr(&fp_priv_data, status);

/*
 * fp status:
 *
 * 0x00 0x01	ir remote control (dbox1, old dbox2)
 * 0x01 0x02	ir keyboard
 * 0x02 0x04	ir mouse
 * 0x03 0x08	ir remote control (new dbox2)
 *
 * 0x04 0x10	frontpanel button
 * 0x05 0x20	scart status
 * 0x06 0x40	lnb alarm
 * 0x07 0x80	timer underrun
 */

	do {

		fp_cmd(&fp_client, 0x20, &status, 1);

		dprintk("status: %02x\n", status);
		
		for (queue_nr = 0; queue_nr < QUEUE_COUNT; queue_nr++) {

			if ((status & (1 << queue_nr)) && (queue_list[queue_nr].busy))
				queue_list[queue_nr].queue_proc(queue_nr);

		}

	} while (status & 0x1F);

}


static void fp_task(void *arg)
{
	volatile immap_t *immap = (volatile immap_t *)IMAP_ADDR;

	fp_check_queues();

	if (useimap)
		immap->im_ioport.iop_padat &= ~2;
	
	enable_irq(FP_INTERRUPT);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static DECLARE_WORK(fp_work, fp_task, NULL);
static irqreturn_t fp_interrupt(int irq, void *vdev, struct pt_regs *regs)
#else
static void fp_interrupt(int irq, void *vdev, struct pt_regs *regs)
#endif
{
	volatile immap_t *immap = (volatile immap_t *)IMAP_ADDR;

	if (useimap)
		immap->im_ioport.iop_padat |= 2;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	disable_irq(FP_INTERRUPT);	/* fp seems to be level-triggered, */
	schedule_work(&fp_work);	/*     at least on Nokia */
	return IRQ_HANDLED;
#else	
	schedule_task(&fp_tasklet);
#endif
}



/*****************************************************************************\
 *   Module Initialization / Module Cleanup
\*****************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static int fp_drv_probe(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int ret,irq;
	irq = platform_get_irq(pdev,0);

	if (!irq){
		printk(KERN_ERR "fp: no platform resources found.\n");
		return -ENODEV;
	}
	
	if ((ret = i2c_add_driver(&fp_i2c_driver))) {
		printk(KERN_ERR "fp: I2C driver registration failed.\n");
	}
	
	if (request_irq(irq, fp_interrupt, SA_INTERRUPT, "fp", dev))
		panic ("fp: could not allocate irq");

	return ret;	
}

static int fp_drv_remove(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int irq;

	irq = platform_get_irq(pdev,0);

	free_irq(irq, dev);
	i2c_del_driver(&fp_i2c_driver);
	
	return 0;
}

static struct device_driver fp_dev_driver = {
	.name		= "fp",
	.bus		= &platform_bus_type,
	.probe		= fp_drv_probe,
	.remove		= fp_drv_remove,
};

static struct miscdevice fp_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "frontprocessor",
	.fops = &fp_fops
};
#endif

static int __init fp_init(void)
{
	mid = tuxbox_dbox2_mid;

	switch (mid) {
	case TUXBOX_DBOX2_MID_NOKIA:
		fp_revision = 0x81;
		break;

	case TUXBOX_DBOX2_MID_PHILIPS:
		fp_revision = 0x30;
		break;

	case TUXBOX_DBOX2_MID_SAGEM:
		fp_revision = 0x23;
		break;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	driver_register(&fp_dev_driver);
	if (misc_register(&fp_dev)<0){
		printk("fp: unable to register device\n");
		return -EIO;
	}
	devfs_mk_cdev(MKDEV(MISC_MAJOR,fp_dev.minor),
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
		"dbox/fp0");
#else
	devfs_handle = devfs_register(NULL, "dbox/fp0", DEVFS_FL_DEFAULT, 0, FP_MINOR,
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
		&fp_fops, NULL);

	if (!devfs_handle) {
		i2c_del_driver(&fp_i2c_driver);
		return -EIO;
	}
	{
	int res;
	if ((res = i2c_add_driver(&fp_driver))){
		printk(KERN_ERR "fp: i2c_add_driver failed.\n");
		return res;
	}
	if (!fp_data.fpID)
		printk(KERN_ERR "fp: no i2c client found\n");
		return -EBUSY;
	}
#endif
	return 0;
}

static void __exit fp_exit(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	devfs_remove("dbox/fp0");
	driver_unregister(&fp_dev_driver);
	misc_deregister(&fp_dev);
#else
	devfs_unregister(devfs_handle);
#endif
	
	if (ppc_md.restart == dbox2_fp_restart)
		ppc_md.restart = NULL;

	if (ppc_md.power_off == dbox2_fp_power_off)
		ppc_md.power_off = NULL;

	if (ppc_md.halt == dbox2_fp_power_off)
		ppc_md.halt = NULL;
}

module_init(fp_init);
module_exit(fp_exit);

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("DBox2 Frontprocessor");
MODULE_LICENSE("GPL");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
module_param(useimap,int,0);
#else
MODULE_PARM(useimap,"i");
#endif
EXPORT_SYMBOL(dbox2_fp_queue_alloc);
EXPORT_SYMBOL(dbox2_fp_queue_free);
EXPORT_SYMBOL(fp_cmd);
EXPORT_SYMBOL(fp_sendcmd);
EXPORT_SYMBOL(fp_get_i2c);
