/*
 *   cam.c - CAM driver (dbox-II-project)
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
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <asm/8xx_immap.h>
#include <asm/io.h>

#include <dbox/fp.h>

#define I2C_DRIVERID_CAM	0x6E
#define CAM_INTERRUPT		SIU_IRQ3
#define CAM_CODE_SIZE		0x20000
#define CAM_QUEUE_SIZE		0x800

static int cam_attach_adapter(struct i2c_adapter *adap);
static int cam_detach_client(struct i2c_client *client);

static struct i2c_client *dclient;

static struct i2c_driver cam_i2c_driver = {
	.owner		= THIS_MODULE,
	.name		= "DBox2-CAM",
	.id		= I2C_DRIVERID_CAM,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= &cam_attach_adapter,
	.detach_client	= &cam_detach_client,
	.command	= NULL,
};

static struct i2c_client client_template = {
	.name		= "DBox2-CAM",
	.id		= I2C_DRIVERID_CAM,
	.flags		= 0,
	.addr		= (0x6E >> 1),
	.adapter	= NULL,
	.driver		= &cam_i2c_driver,
};

static DECLARE_MUTEX(cam_busy);
static u8 cam_queue[CAM_QUEUE_SIZE];
static u32 cam_queuewptr, cam_queuerptr;
static wait_queue_head_t cam_wait_queue;

/**
 * I2C functions
 */
static int cam_attach_adapter(struct i2c_adapter *adap)
{
	struct i2c_client *client;
	int ret;

	if (!(client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
		return -ENOMEM;

	memcpy(client, &client_template, sizeof(struct i2c_client));

	client->adapter = adap;

	if ((ret = i2c_attach_client(client))) {
		kfree(client);
		return ret;
	}

	/* ugly */
	dclient = client;

	return 0;
}

static int cam_detach_client(struct i2c_client *client)
{
	int ret;

	if ((ret = i2c_detach_client(client))) {
		printk(KERN_ERR "cam: i2c_detach_client failed\n");
		return ret;
	}

	kfree(client);
	return 0;
}

/**
 * Exported functions
 */
unsigned int cam_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &cam_wait_queue, wait);

	if (cam_queuerptr != cam_queuewptr)
		mask |= (POLLIN | POLLRDNORM);

	return mask;
}

int cam_read_message(char *buf, size_t count)
{
	int cb;

	cb = cam_queuewptr - cam_queuerptr;

	if (cb < 0)
		cb += CAM_QUEUE_SIZE;

	if (count < cb)
		cb = count;

	if ((cam_queuerptr + cb) > CAM_QUEUE_SIZE) {
		memcpy(buf, &cam_queue[cam_queuerptr], CAM_QUEUE_SIZE - cam_queuerptr);
		memcpy(&buf[CAM_QUEUE_SIZE - cam_queuerptr], cam_queue, cb - (CAM_QUEUE_SIZE - cam_queuerptr));
		cam_queuerptr = cb - (CAM_QUEUE_SIZE - cam_queuerptr);
	}
	else {
		memcpy(buf, &cam_queue[cam_queuerptr], cb);
		cam_queuerptr += cb;
	}

	return cb;
}

int cam_reset(void)
{
	return dbox2_fp_reset_cam();
}

int cam_write_message(char *buf, size_t count)
{
	int res;

	/* ugly */
	if (!dclient)
		return -ENODEV;

	if ((res = down_interruptible(&cam_busy)))
		return res;

	res = i2c_master_send(dclient, buf, count);

	cam_queuewptr = cam_queuerptr;  // mich stoerte der Buffer ....		// ?? was soll das? (tmb)
	up(&cam_busy);
	return res;
}

/**
 * IRQ functions
 */
static void cam_task(void *data)
{
	unsigned char buffer[130];
	unsigned char caid[9] = { 0x50, 0x06, 0x23, 0x84, 0x01, 0x02, 0xFF, 0xFF, 0x00 };
	int len, i;

	if (down_interruptible(&cam_busy))
		goto cam_task_enable_irq;

	if (i2c_master_recv(dclient, buffer, 2) != 2)
		goto cam_task_up;

	len = buffer[1] & 0x7f;

	if (i2c_master_recv(dclient, buffer, len + 3) != len + 3)
		goto cam_task_up;

	if ((buffer[1] & 0x7f) != len) {
		len = buffer[1] & 0x7f;	/* length mismatch - try again */
		if (i2c_master_recv(dclient, buffer, len + 3) != len + 3)
			goto cam_task_up;
	}

	len += 3;

	if ((buffer[2] == 0x23) && (buffer[3] <= 7)) {
		memcpy(&caid[6], &buffer[5], 2);
		caid[8] = 0x6e;	/* checksum */
		for (i = 0; i < 8; i++)
			caid[8] ^= caid[i];
		up(&cam_busy);
		cam_write_message(caid, 9);
	}
	else {
		i = cam_queuewptr - cam_queuerptr;

		if (i < 0)
			i += CAM_QUEUE_SIZE;

		i = CAM_QUEUE_SIZE - i;

		if (i < len) {
			cam_queuewptr = cam_queuerptr;
		}
		else {
			i = 0;
			cam_queue[cam_queuewptr++] = 0x6f;
			if (cam_queuewptr == CAM_QUEUE_SIZE)
				cam_queuewptr = 0;
			while (len--) {
				cam_queue[cam_queuewptr++] = buffer[i++];
				if (cam_queuewptr == CAM_QUEUE_SIZE)
					cam_queuewptr = 0;
			}
		}
	}

	wake_up(&cam_wait_queue);
cam_task_up:
	up(&cam_busy);
cam_task_enable_irq:
	enable_irq(CAM_INTERRUPT);
	;
}

static DECLARE_WORK(cam_work, cam_task, NULL);

static irqreturn_t cam_interrupt(int irq, void *dev, struct pt_regs *regs)
{
printk("cam_irq\n");
	schedule_work(&cam_work);
	disable_irq(CAM_INTERRUPT);
	return IRQ_HANDLED;
}

/**
 * Firmware functions
 */
static int cam_write_firmware(struct resource *res, const struct firmware *fw)
{
	volatile immap_t *immap = (volatile immap_t *)IMAP_ADDR;
	volatile cpm8xx_t *cp = &immap->im_cpm;
	unsigned char *code_base;
	size_t mem_size;
	int i;

	mem_size = res->end - res->start + 1;

	if (fw->size > mem_size) {
		printk(KERN_ERR "cam: firmware file is too large\n");
		return -EINVAL;
	}

	code_base = ioremap(res->start, mem_size);
	if (!code_base) {
		printk(KERN_ERR "cam: ioremap failed\n");
		return -EIO;
	}

	/* 0xabc */
	cp->cp_pbpar &= ~0x0000000f;	// GPIO (not cpm-io)

	/* 0xac0 */
	cp->cp_pbodr &= ~0x0000000f;	// driven output (not tristate)

	/* 0xab8 */
	cp->cp_pbdir |= 0x0000000f;	// output (not input)

	/* 0xac4 */
	(void)cp->cp_pbdat;
	cp->cp_pbdat = 0x00000000;
	cp->cp_pbdat = 0x000000a5;
	cp->cp_pbdat |= 0x000000f;
	cp->cp_pbdat &= ~0x00000002;
	cp->cp_pbdat |= 0x00000002;

	for (i = 0; i <= 8; i++) {
		cp->cp_pbdat &= ~0x00000008;
		cp->cp_pbdat |= 0x00000008;
	}

	cam_reset();

	cp->cp_pbdat &= ~0x00000001;

	if (fw) {
		memcpy(code_base, fw->data, fw->size);
		if (mem_size - fw->size != 0)
			memset(&code_base[fw->size], 0x5a, mem_size - fw->size);
	}
	else {
		memset(code_base, 0x5a, mem_size);
	}

	wmb();
	cp->cp_pbdat |= 0x00000001;
	wmb();

	cp->cp_pbdat &= ~0x00000002;
	cp->cp_pbdat |= 0x00000002;

	cam_reset();

	iounmap(code_base);

	return 0;
}

static int cam_drv_probe(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;
	const struct firmware *fw;
	int ret;
	int irq;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);

	if ((!res) || (!irq)) {
		ret = -ENODEV;
		goto out;
	}

	init_waitqueue_head(&cam_wait_queue);

	ret = request_firmware(&fw, "cam-alpha.bin", dev);
	if (ret) {
		if (ret == -ENOENT) {
			printk(KERN_ERR "cam: could not load firmware, "
					"file not found: cam-alpha.bin\n");
		}
		goto out;
	}

	if (fw->size > 0x20000) {
		printk(KERN_ERR "cam: invalid firmware size %d, must be <= 128KB\n", fw->size);
		goto release;
	}

	ret = cam_write_firmware(res, fw);
	if (ret)
		goto release;

	ret = i2c_add_driver(&cam_i2c_driver);
	if (ret) {
		printk(KERN_ERR "cam: i2c driver registration failed\n");
		goto release;
	}

	if (request_irq(irq, cam_interrupt, SA_INTERRUPT, "cam", dev))
		panic("cam: could not allocate irq");

release:
	release_firmware(fw);
out:
	return ret;
}

static int cam_drv_remove(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int irq;

	irq = platform_get_irq(pdev, 0);

	free_irq(irq, dev);
	i2c_del_driver(&cam_i2c_driver);

	return 0;
}

static struct device_driver cam_driver = {
	.name		= "cam",
	.bus		= &platform_bus_type,
	.probe		= cam_drv_probe,
	.remove		= cam_drv_remove,
};

static int __init cam_init(void)
{
	printk(KERN_INFO "$Id: cam.c,v 1.30.2.1 2005/01/15 21:18:24 carjay Exp $\n");

	return driver_register(&cam_driver);
}

static void __exit cam_exit(void)
{
	driver_unregister(&cam_driver);
}

module_init(cam_init);
module_exit(cam_exit);

EXPORT_SYMBOL(cam_poll);
EXPORT_SYMBOL(cam_read_message);
EXPORT_SYMBOL(cam_reset);
EXPORT_SYMBOL(cam_write_message);

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("DBox2 CAM Driver");
MODULE_LICENSE("GPL");
