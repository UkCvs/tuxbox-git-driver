#include <linux/delay.h>
#include <linux/timer.h>

/*
 * $Id: mmc_spi_block_core.c,v 1.1.2.3 2007/10/12 20:59:25 carjay Exp $
 *
 * Block device driver for a MMC/SD card in SPI mode using GPIOs
 * Gendisk routines
 *
 * Linux 2.4 driver copyright Madsuk,Rohde,TaGana
 * Linux 2.6 driver changes added by Carsten Juttner <carjay@gmx.net>
 * This implementation does not use the 2.6 MMC subsystem (yet).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2
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
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/version.h>

#include <linux/platform_device.h>

#include "8xx_mmc.h"
#include "mmc_spi_io.h"

static int major = 121;

static int mmc_media_detect = 0;

#define DEVICE_NAME "mmc_spi"

static int hd_sizes[1<<6];
static int hd_blocksizes[1<<6];
static int hd_hardsectsizes[1<<6];
static int hd_maxsect[1<<6];
static struct hd_struct hd[1<<6];

static struct gendisk *mmc_disk;

static struct platform_device *mmc_dev; /* the one and only instance */

static spinlock_t mmc_spi_lock;

/* NB: There might be several requests in the queue, simply dequeuing only one
	and not checking for more will cause a stall because the block subsystem
	will not call this function again unless the queue is "plugged" which can
	only happen if it runs empty... */
static void mmc_spi_request(struct request_queue *q)
{
	struct request *req;
	int ret;
	
	unsigned int mmc_address;
	unsigned char *buffer_address;
	int nr_sectors;
	int i;
	int rc, success;

	if (blk_queue_plugged(q)) {
		return;
	}

	spin_lock(&mmc_spi_lock);
	for(;;) {
		req = elv_next_request(q);
		if (!req)
			break;
		
		if (!blk_fs_request(req)) {
			printk("not a blk_fs_request\n");
			spin_unlock(&mmc_spi_lock);
			continue;
		}

		mmc_address = req->sector * hd_hardsectsizes[0];
		buffer_address = req->buffer;
		nr_sectors = req->current_nr_sectors;
		success = 1;
		if (rq_data_dir(req) == READ) {
			spin_unlock_irq(q->queue_lock);
			for (i = 0; i < nr_sectors; i++) {
				rc = mmc_spi_read_block(buffer_address, mmc_address);
				if (unlikely(rc < 0)) {
					printk(KERN_ERR "mmi_spi_block: error reading block (%d)\n", rc);
					success = 0;
					break;
				}
				mmc_address += hd_hardsectsizes[0];
				buffer_address += hd_hardsectsizes[0];
			}
			spin_lock_irq(q->queue_lock);
		} else {
			spin_unlock_irq(q->queue_lock);
			for (i = 0; i < nr_sectors; i++) {
				rc = mmc_spi_write_block(mmc_address, buffer_address);
				if (unlikely(rc < 0)) {
					printk(KERN_ERR "mmi_spi_block: error writing block (%d)\n", rc);
					success = 0;
					break;
				}
				mmc_address += hd_hardsectsizes[0];
				buffer_address += hd_hardsectsizes[0];
			}
			spin_lock_irq(q->queue_lock);
		}
		ret = end_that_request_chunk(req, success, nr_sectors * hd_hardsectsizes[0]);
		if (!ret) {
			blkdev_dequeue_request(req);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,15)
			end_that_request_last(req);
#else
			end_that_request_last(req, success);
#endif
		}
	}
	spin_unlock(&mmc_spi_lock);
}


static int mmc_open(struct inode *inode, struct file *filp)
{
	if (!mmc_media_detect)
		return -ENODEV;

	return 0;
}

static int mmc_release(struct inode *inode, struct file *filp)
{
//	(void)filp;
//	fsync_dev(inode->i_rdev);
//	invalidate_buffers(inode->i_rdev);

	return 0;
}


#if 0
static int mmc_revalidate(kdev_t dev)
{
	int target, max_p, start, i;
	if (mmc_media_detect == 0)
		return -ENODEV;

	target = DEVICE_NR(dev);

	max_p = hd_gendisk.max_p;
	start = target << 6;
	for (i = max_p - 1; i >= 0; i--) {
		int minor = start + i;
		invalidate_device(MKDEV(major, minor), 1);
		hd_gendisk.part[minor].start_sect = 0;
		hd_gendisk.part[minor].nr_sects = 0;
	}

	grok_partitions(&hd_gendisk, target, 1 << 6, hd_sizes[0] * 2);

	return 0;
}
#endif

static int mmc_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
		     unsigned long arg)
{
	if (!inode || !inode->i_rdev)
		return -EINVAL;

	switch (cmd) {
#if 0
	case BLKGETSIZE:
		return put_user(hd[MINOR(inode->i_rdev)].nr_sects,
				(unsigned long *)arg);
	case BLKGETSIZE64:
		return put_user((u64) hd[MINOR(inode->i_rdev)].
				nr_sects, (u64 *) arg);
	case BLKRRPART:
		if (!capable(CAP_SYS_ADMIN))
			return -EACCES;

		return mmc_revalidate(inode->i_rdev);
#endif
	case HDIO_GETGEO:
		{
			struct block_device *bdev = inode->i_bdev;
			struct hd_geometry *loc, g;
			loc = (struct hd_geometry *)arg;
			if (!loc)
				return -EINVAL;
			memset(loc, 0, sizeof(struct hd_geometry));
			g.heads = 4;
			g.sectors = 16;
			g.cylinders = get_capacity(bdev->bd_disk) / (4*16);
			g.start = get_start_sect(bdev);
			return copy_to_user(loc, &g, sizeof(g)) ? -EFAULT : 0;
		}
	default:
		return -ENOTTY;
	}
}

static int mmc_card_init(void)
{
	unsigned char r = 0;
	short i, j;
	unsigned long flags;
	
	local_irq_save(flags);

	mmc_spi_cs_high();
	for (i = 0; i < 1000; i++)
		mmc_spi_io(0xff);

	mmc_spi_cs_low();

	mmc_spi_io(0x40);
	for (i = 0; i < 4; i++)
		mmc_spi_io(0x00);
	mmc_spi_io(0x95);
	for (i = 0; i < 8; i++) {
		r = mmc_spi_io(0xff);
		if (r == 0x01)
			break;
	}
	mmc_spi_cs_high();
	mmc_spi_io(0xff);
	if (r != 0x01) {
		local_irq_restore(flags);
		return (1);
	}

	for (j = 0; j < 30000; j++) {
		mmc_spi_cs_low();

		mmc_spi_io(0x41);
		for (i = 0; i < 4; i++)
			mmc_spi_io(0x00);
		mmc_spi_io(0xff);
		for (i = 0; i < 8; i++) {
			r = mmc_spi_io(0xff);
			if (r == 0x00)
				break;
		}
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		if (r == 0x00) {
			local_irq_restore(flags);
			return (0);
		}
	}
	local_irq_restore(flags);
	return (2);
}

static int mmc_card_config(void)
{
	unsigned char r = 0;
	short i;
	unsigned char csd[32];
	unsigned int c_size;
	unsigned int c_size_mult;
	unsigned int mult;
	unsigned int read_bl_len;
	unsigned int blocknr = 0;
	unsigned int block_len = 0;
	unsigned int size = 0;

	mmc_spi_cs_low();
	for (i = 0; i < 4; i++)
		mmc_spi_io(0xff);
	mmc_spi_io(0x49);
	for (i = 0; i < 4; i++)
		mmc_spi_io(0x00);
	mmc_spi_io(0xff);
	for (i = 0; i < 8; i++) {
		r = mmc_spi_io(0xff);
		if (r == 0x00)
			break;
	}
	if (r != 0x00) {
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		return (1);
	}
	for (i = 0; i < 8; i++) {
		r = mmc_spi_io(0xff);
		if (r == 0xfe)
			break;
	}
	if (r != 0xfe) {
		mmc_spi_cs_high();
		mmc_spi_io(0xff);
		return (2);
	}
	for (i = 0; i < 16; i++) {
		r = mmc_spi_io(0xff);
		csd[i] = r;
	}
	for (i = 0; i < 2; i++) {
		r = mmc_spi_io(0xff);
	}
	mmc_spi_cs_high();
	mmc_spi_io(0xff);
	if (r == 0x00)
		return (3);

	c_size = csd[8] + csd[7] * 256 + (csd[6] & 0x03) * 256 * 256;
	c_size >>= 6;
	c_size_mult = csd[10] + (csd[9] & 0x03) * 256;
	c_size_mult >>= 7;
	read_bl_len = csd[5] & 0x0f;
	mult = 1;
	mult <<= c_size_mult + 2;
	blocknr = (c_size + 1) * mult;
	block_len = 1;
	block_len <<= read_bl_len;
	size = block_len * blocknr;	/* Bytes  */
	size >>= 10;				/* MiBytes */

	for (i = 0; i < (1 << 6); i++) {
		hd_blocksizes[i] = 1024;
		hd_hardsectsizes[i] = block_len;
		hd_maxsect[i] = 256;
	}
	hd_sizes[0] = size;
	hd[0].nr_sects = blocknr;

	printk(KERN_INFO "Size = %d, hardsectsize = %d, sectors = %d\n", size, block_len, blocknr);

	return 0;
}

static struct block_device_operations mmc_spi_bdops = {
	.open = mmc_open,
	.release = mmc_release,
	.ioctl = mmc_ioctl,
	.owner = THIS_MODULE,
#if 0
	.check_media_change = mmc_check_media_change,
	.revalidate = mmc_revalidate
#endif
};

static int detect_card(void)
{
	int rc;

	rc = mmc_card_init();
	if (rc != 0) {
		// Give it an extra shot
		rc = mmc_card_init();
		if (rc != 0) {
			printk(KERN_ERR "mmc_spi_block: error in mmc_card_init (%d)\n", rc);
			return -ENODEV;
		}
	}

	rc = mmc_card_config();
	if (rc != 0) {
		printk(KERN_ERR "mmc_spi_block: error in mmc_card_config (%d)\n", rc);
		return -ENODEV;
	}

	return 0;
}

/* Fills in the gendisk structure from the received card
   data.  */
static int gendisk_init(struct device *dev, struct gendisk *gd)
{
	if (!gd)
		return -EINVAL;

	gd->major = major;
	gd->first_minor = 0; /* only one device supported */
	gd->fops = &mmc_spi_bdops;
	gd->driverfs_dev = dev;

	gd->queue = blk_init_queue(mmc_spi_request,NULL);

	if (!gd->queue)
		return -ENOMEM;

	sprintf(gd->disk_name, "mmcblk");

	blk_queue_hardsect_size(gd->queue, hd_hardsectsizes[0]);

	set_capacity(gd, hd_sizes[0]<<1);

	return 0;
}

static int gendisk_fini(struct gendisk *gd)
{
	BUG_ON(!gd);

	if (gd->queue)
		blk_cleanup_queue(gd->queue);

	del_gendisk(gd);
}

/* platform driver device instance routines */
static int mmc_spi_probe(struct platform_device *pdev)
{
	int rc;
	printk("$Id: mmc_spi_block_core.c,v 1.1.2.3 2007/10/12 20:59:25 carjay Exp $\n");

	rc = mmc_spi_hardware_init();
	if (rc != 0) {
		printk(KERN_ERR "mmc_spi_block: error in mmc_spi_hardware_init (%d)\n", rc);
		rc = -ENODEV;
		return rc;
	}
	
	rc = detect_card();
	if (rc < 0)
		return rc;

	mmc_media_detect = 1;

	rc = register_blkdev(major, DEVICE_NAME);
	if (rc < 0)
		return rc;

	if (!major)
		major = rc;

	/* allow 8 partitions per device */
	BUG_ON(mmc_disk!=NULL);
	mmc_disk = alloc_disk(1 << 3);
	if (!mmc_disk) {
		rc = -ENOMEM;
		goto out;
	}

	rc = gendisk_init(&pdev->dev, mmc_disk);
	if (rc < 0)
		goto out;

	add_disk(mmc_disk);
	
	/*init_timer(&mmc_timer);
	   mmc_timer.expires = jiffies + HZ;
	   mmc_timer.function = (void *)mmc_check_media;
	   add_timer(&mmc_timer); */
	return 0;

out:
	if (mmc_disk)
		put_disk(mmc_disk);
		
	unregister_blkdev(major, DEVICE_NAME);
	return rc;
}

static int mmc_spi_remove(struct platform_device *dev)
{
//	int i;

	/* del_timer(&mmc_timer); */

//	for (i = 0; i < (1 << 6); i++)
//		fsync_dev(MKDEV(major, i));

//	devfs_register_partitions(&hd_gendisk, 0<<6, 1);
	if (mmc_disk) {
		gendisk_fini(mmc_disk);
		put_disk(mmc_disk);
	}

	unregister_blkdev(major, DEVICE_NAME);
	return 0;
}

struct platform_driver mmc_spi_driver = {
	.driver {
		.name = "mmc_spi"
	},
	.probe = mmc_spi_probe,
	.remove = mmc_spi_remove
};


/* module init/exit */
static int __init mmc_block_spi_init(void)
{
	int ret;
	spin_lock_init(&mmc_spi_lock);
	
	ret = platform_driver_register(&mmc_spi_driver);
	if (ret < 0)
		return ret;
	
	/* we just support one device */
	mmc_dev = platform_device_register_simple("mmc_spi", -1, NULL, 0);
	if (IS_ERR(mmc_dev))
		return PTR_ERR(mmc_dev);
	
	return 0;
}

static void __exit mmc_block_spi_exit(void)
{
	platform_driver_unregister(&mmc_spi_driver);
	if (mmc_dev)
		platform_device_unregister(mmc_dev);
}

module_init(mmc_block_spi_init);
module_exit(mmc_block_spi_exit);

MODULE_AUTHOR("Madsuk,Rohde,TaGana,Carsten Juttner <carjay@gmx.net>");
MODULE_DESCRIPTION("Driver MMC/SD-Cards (1Bit,SPI)");
MODULE_LICENSE("GPL");

module_param(major, int, 0444);
MODULE_PARM_DESC(major, "specify the major device number for the MMC/SD SPI driver");
