/*
 *   fp.c - FP driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
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
 *
 *   $Log: fp.c,v $
 *   Revision 1.37  2001/12/02 10:33:01  TripleDES
 *   added low-band support (never missed ;)
 *
 *   Revision 1.36  2001/12/01 10:51:22  gillem
 *   - add vcr handling
 *   - todo: add event (tmbinc???)
 *
 *   Revision 1.35  2001/12/01 06:52:28  gillem
 *   - malloc.h -> slab.h
 *
 *   Revision 1.34  2001/11/06 15:54:59  tmbinc
 *   added FP_WAKEUP and ioctls. (only for nokia)
 *
 *   Revision 1.33  2001/10/30 23:17:26  derget
 *
 *   FP_IOCTL_POWEROFF f�r sagem eingebaut
 *
 *   Revision 1.32  2001/10/30 13:40:55  derget
 *   sagem restart
 *
 *   Revision 1.31  2001/07/31 03:01:39  Hunz
 *   DiSEqC fix (sagem still untested)
 *
 *   Revision 1.30  2001/07/31 01:40:28  Hunz
 *   experimental sagem-diseqc support
 *
 *   Revision 1.29  2001/05/01 02:00:01  TripleDES
 *   added fp_sagem_set_secpower for LNB-voltage control (H/V)
 *   -not completed
 *
 *   Revision 1.28  2001/04/26 17:28:07  Hunz
 *   breakcode-fix
 *
 *   Revision 1.27  2001/04/26 16:56:58  Hunz
 *   added breakcodes support
 *
 *   Revision 1.26  2001/04/22 20:43:40  tmbinc
 *   fixed RC for new and old fp-code
 *
 *   Revision 1.25  2001/04/09 22:58:22  tmbinc
 *   added philips-support.
 *
 *   Revision 1.24  2001/04/09 22:33:57  TripleDES
 *   some unused commands cleared (sagem testing)
 *
 *   Revision 1.23  2001/04/09 19:49:40  TripleDES
 *   added fp_cam_reset for sagem/philips? support
 *
 *   Revision 1.22  2001/04/06 21:15:20  tmbinc
 *   Finally added new rc-support.
 *
 *   Revision 1.21  2001/04/01 01:54:25  tmbinc
 *   added "poll"-support, blocks on open if already opened
 *
 *   Revision 1.20  2001/03/18 21:28:24  tmbinc
 *   fixed again some bug.
 *
 *   Revision 1.19  2001/03/15 22:20:23  Hunz
 *   nothing important...
 *
 *   Revision 1.18  2001/03/14 14:35:58  Hunz
 *   fixed DiSEqC timing
 *
 *   Revision 1.17  2001/03/12 22:03:40  Hunz
 *   final? SEC-fix (always clrbit the VES)
 *
 *   Revision 1.16  2001/03/12 19:51:37  Hunz
 *   SEC changes
 *
 *   Revision 1.15  2001/03/11 18:28:50  gillem
 *   - add new option (test only)
 *
 *   Revision 1.14  2001/03/08 14:08:50  Hunz
 *   DiSEqC number of params changed to 0-3
 *
 *   Revision 1.13  2001/03/06 18:49:20  Hunz
 *   fix for sat-boxes (fp_set_pol... -> fp_set_sec)
 *
 *   Revision 1.12  2001/03/04 18:48:07  gillem
 *   - fix for sagem box
 *
 *   Revision 1.11  2001/03/03 18:20:39  waldi
 *   complete move to devfs; doesn't compile without devfs
 *
 *   Revision 1.10  2001/03/03 13:03:20  gillem
 *   - fix code
 *
 *   Revision 1.9  2001/02/25 21:11:36  gillem
 *   - fix fpid
 *
 *   Revision 1.8  2001/02/23 18:44:43  gillem
 *   - add ioctl
 *   - add debug option
 *   - some changes ...
 *
 *
 *   $Revision: 1.37 $
 *
 */

/* ---------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/tqueue.h>
#include <linux/i2c.h>
#include <linux/poll.h>
#include <asm/irq.h>
#include <asm/mpc8xx.h>
#include <asm/8xx_immap.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/signal.h>

#include "dbox/fp.h"
#include "dbox/info.h"

#include <linux/devfs_fs_kernel.h>

#include <ost/sec.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif


static devfs_handle_t devfs_handle[2];
static int sec_bus_status=0;
static struct dbox_info_struct info;
/* ---------------------------------------------------------------------- */

#ifdef MODULE
static int debug=0;
static int useimap=1;
#endif

#define dprintk(fmt,args...) if(debug) printk( fmt,## args)

/* ---------------------------------------------------------------------- */

/*
      exported functions:
      
      int fp_set_tuner_dword(int type, u32 tw);
        T_QAM
        T_QPSK
      int fp_set_sec(int power,int tone);
*/

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

#define FP_INTERRUPT        SIU_IRQ2
#define I2C_FP_DRIVERID     0xF060
#define RCBUFFERSIZE        16
#define FP_GETID            0x1D
#define FP_WAKEUP						0x11

/* ---------------------------------------------------------------------- */

/* Scan 0x60 */
static unsigned short normal_i2c[] = { 0x60>>1,I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { 0x60>>1, 0x60>>1, I2C_CLIENT_END };
I2C_CLIENT_INSMOD;

static int fp_id=0;
static int rc_bcodes=0;

struct fp_data
{
  int fpID;
  int fpVCR;
  struct i2c_client *client;
};

struct fp_data *defdata=0;

static u16 rcbuffer[RCBUFFERSIZE];
static u16 rcbeg, rcend;
static wait_queue_head_t rcwait;
static DECLARE_MUTEX_LOCKED(rc_open);

static void fp_task(void *);

struct tq_struct fp_tasklet=
{
  routine: fp_task,
  data: 0
};

static int fp_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int fp_open (struct inode *inode, struct file *file);
static ssize_t fp_write (struct file *file, const char *buf, size_t count, loff_t *offset);
static ssize_t fp_read (struct file *file, char *buf, size_t count, loff_t *offset);
static int fp_release(struct inode *inode, struct file *file);
static unsigned int fp_poll(struct file *file, poll_table *wait);

static int fp_detach_client(struct i2c_client *client);
static int fp_detect_client(struct i2c_adapter *adapter, int address, unsigned short flags, int kind);
static int fp_attach_adapter(struct i2c_adapter *adapter);
static int fp_getid(struct i2c_client *client);
static void fp_interrupt(int irq, void *dev, struct pt_regs * regs);
static int fp_cmd(struct i2c_client *client, u8 cmd, u8 *res, int size);
static int fp_sendcmd(struct i2c_client *client, u8 b0, u8 b1);
static void fp_check_queues(void);
static int fp_set_wakeup_timer(int minutes);
static int fp_get_wakeup_timer(void);

static void fp_restart(char *cmd);
static void fp_power_off(void);
static void fp_halt(void);

/* ------------------------------------------------------------------------- */

static struct i2c_driver fp_driver=
{
  name:                 "DBox2 Frontprocessor driver",
  id:                   I2C_FP_DRIVERID,
  flags:                I2C_DF_NOTIFY,
  attach_adapter:       &fp_attach_adapter,
  detach_client:        &fp_detach_client,
  command:              0,
  inc_use:              0,
  dec_use:              0
};

static struct file_operations fp_fops = {
        owner:          THIS_MODULE,
        read:           fp_read,
        write:          fp_write,
        ioctl:          fp_ioctl,
        open:           fp_open,
	release:	fp_release,
	poll:		fp_poll,
};

/* ------------------------------------------------------------------------- */

static int fp_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                  unsigned long arg)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
    int val;

	switch (minor)
	{
		case FP_MINOR:
		{
			switch (cmd)
			{
				case FP_IOCTL_GETID:
					return fp_getid(defdata->client);
					break;

				case FP_IOCTL_POWEROFF:	
					if (info.fpREV>=0x80)
						return fp_sendcmd(defdata->client, 0, 3);
					else
						return fp_sendcmd(defdata->client, 0, 0);
					break; 

				case FP_IOCTL_LCD_DIMM:
					if (copy_from_user(&val, (void*)arg, sizeof(val)) )
					{
						return -EFAULT;
					}

					return fp_sendcmd(defdata->client, 0x18, val&0x0f);
					break;

				case FP_IOCTL_LED:
					if (copy_from_user(&val, (void*)arg, sizeof(val)) )
					{
						return -EFAULT;
					}

					return fp_sendcmd(defdata->client, 0x10|(val&1), 0);
					break;
				case FP_IOCTL_GET_WAKEUP_TIMER:
					val=fp_get_wakeup_timer();
					if (val==-1)
						return -EIO;
					if (copy_to_user((void*)arg, &val, sizeof(val)))
						return -EFAULT;
					return 0;
					break;
				case FP_IOCTL_SET_WAKEUP_TIMER:
					if (copy_from_user(&val, (void*)arg, sizeof(val)) )
						return -EFAULT;
					return fp_set_wakeup_timer(val);
				default:
					return -EINVAL;
			}
		}

		case RC_MINOR:
		{
			switch (cmd)
			{
				case RC_IOCTL_BCODES:
					if (arg > 0)
					{
						rc_bcodes=1;
						return fp_sendcmd(defdata->client, 0x26, 0x80);
					}
					else
					{
						rc_bcodes=0;
						return fp_sendcmd(defdata->client, 0x26, 0);
					}
					break;
				default:
					return -EINVAL;
			}
		}

		default:
			return -EINVAL;
	}

	return -EINVAL;
}

/* ------------------------------------------------------------------------- */

static ssize_t fp_write (struct file *file, const char *buf, size_t count, loff_t *offset)
{
	return 0;
}                             

/* ------------------------------------------------------------------------- */

static ssize_t fp_read (struct file *file, char *buf, size_t count, loff_t *offset)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev), read;

	switch (minor)
	{
		case FP_MINOR:
			return -EINVAL;

		case RC_MINOR:
		{
			int i;
			DECLARE_WAITQUEUE(wait, current);
			read=0;

			for(;;)
			{
				if (rcbeg==rcend)
				{
					if (file->f_flags & O_NONBLOCK)
					{
						return read;
					}

					add_wait_queue(&rcwait, &wait);
					set_current_state(TASK_INTERRUPTIBLE);
					schedule();
					current->state = TASK_RUNNING;
					remove_wait_queue(&rcwait, &wait);

					if (signal_pending(current))
					{
						return -ERESTARTSYS;
					}

					continue;
				}

				break;
			}

			count&=~1;

			for (i=0; i<count; i+=2)
			{
				if (rcbeg==rcend)
				{
					break;
				}

				*((u16*)(buf+i))=rcbuffer[rcbeg++];
				read+=2;

				if (rcbeg>=RCBUFFERSIZE)
				{
					rcbeg=0;
				}
			}

			return read;
		}

		default:
			return -EINVAL;
	}

	return -EINVAL;
}

/* ------------------------------------------------------------------------- */

static int fp_open (struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);

	switch (minor)
	{
		case FP_MINOR:
			return 0;

		case RC_MINOR:
			if (file->f_flags & O_NONBLOCK)
			{
				if (down_trylock(&rc_open))
					return -EAGAIN;
			}	else
			{
				if (down_interruptible(&rc_open))
					return -ERESTARTSYS;
			}
			return 0;

		default:
			return -ENODEV;
	}

	return -EINVAL;
}

/* ------------------------------------------------------------------------- */

static int fp_release(struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);

	switch (minor)
	{
	case FP_MINOR:
		return 0;
	case RC_MINOR:
		if (rc_bcodes != 0)
			fp_sendcmd(defdata->client, 0x26, 0);
		up(&rc_open);
		return 0;
	}
	return -EINVAL;
}

/* ------------------------------------------------------------------------- */

static unsigned int fp_poll(struct file *file, poll_table *wait)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
	switch (minor)
	{
	case FP_MINOR:
		return -EINVAL;
	case RC_MINOR:
		poll_wait(file, &rcwait, wait);
		if (rcbeg!=rcend)
			return POLLIN|POLLRDNORM;
		return 0;
	}
	return -EINVAL;
}

/* ------------------------------------------------------------------------- */

static int fp_detach_client(struct i2c_client *client)
{
	int err;

	free_irq(FP_INTERRUPT, client->data);

	if ((err=i2c_detach_client(client)))
	{
		dprintk("fp.o: couldn't detach client driver.\n");
		return err;
	}

	kfree(client);
	return 0;
}

/* ------------------------------------------------------------------------- */

static int fp_detect_client(struct i2c_adapter *adapter, int address, unsigned short flags, int kind)
{
	int err = 0;
	struct i2c_client *new_client;
	struct fp_data *data;
	const char *client_name="DBox2 Frontprocessor client";

	if (!(new_client=kmalloc(sizeof(struct i2c_client)+sizeof(struct fp_data), GFP_KERNEL)))
	{
		return -ENOMEM;
	}

	new_client->data=new_client+1;
	defdata=data=(struct fp_data*)(new_client->data);
	/* init vcr value (off) */
	defdata->fpVCR=0;
	rcbeg=0;
	rcend=0;
	new_client->addr=address;
	data->client=new_client;
	new_client->data=data;
	new_client->adapter=adapter;
	new_client->driver=&fp_driver;
	new_client->flags=0;
  
	if (kind<0)
	{
		int fpid;
//		u8 buf[2];
		immap_t *immap=(immap_t*)IMAP_ADDR;

		/* FP ID
		 * NOKIA: 0x5A
		 * SAGEM: 0x52 ???
		 * PHILIPS: 0x52
		 */

		fpid=fp_getid(new_client);

		if ( (fpid!=0x52) && (fpid!=0x5a) )
		{
			dprintk("fp.o: bogus fpID %d\n", fpid);
			kfree(new_client);
			return -1;
		}
    
		if(useimap)
		{
			immap->im_ioport.iop_papar&=~2;
			immap->im_ioport.iop_paodr&=~2;
			immap->im_ioport.iop_padir|=2;
			immap->im_ioport.iop_padat&=~2;
		}

//    fp_sendcmd(new_client, 0x04, 0x51); //sagem needs this (71) LNB-Voltage 51-V 71-H
/*    fp_sendcmd(new_client, 0x22, 0xbf);
    fp_cmd(new_client, 0x25, buf, 2);
    fp_sendcmd(new_client, 0x19, 0x04);
    fp_sendcmd(new_client, 0x18, 0xb3);
    fp_cmd(new_client, 0x1e, buf, 2); 
*/
		fp_sendcmd(new_client, 0x26, 0x00);		// disable (non-working) break code
    
	/*	fp_cmd(new_client, 0x23, buf, 1);
		fp_cmd(new_client, 0x20, buf, 1);
		fp_cmd(new_client, 0x01, buf, 2);*/
	}

	strcpy(new_client->name, client_name);
	new_client->id=fp_id++;

	if ((err=i2c_attach_client(new_client)))
	{
		kfree(new_client);
		return err;
	}

	if (request_8xxirq(FP_INTERRUPT, fp_interrupt, SA_ONESHOT, "fp", data) != 0)
	{
		panic("Could not allocate FP IRQ!");
	}

	up(&rc_open);
	return 0;
}

/* ------------------------------------------------------------------------- */

static int fp_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, &fp_detect_client);
}

/* ------------------------------------------------------------------------- */

static int fp_cmd(struct i2c_client *client, u8 cmd, u8 *res, int size)
{
	struct i2c_msg msg[2];
	int i;

	msg[0].flags=0;
	msg[1].flags=I2C_M_RD;
	msg[0].addr=msg[1].addr=client->addr;

	msg[0].buf=&cmd;
	msg[0].len=1;
  
	msg[1].buf=res;
	msg[1].len=size;
  
	i2c_transfer(client->adapter, msg, 2);
  
	dprintk("fp.o: fp_cmd: %02x\n", cmd);
	dprintk("fp.o: fp_recv:");

	if(debug)
	{
		for (i=0; i<size; i++)
			dprintk(" %02x", res[i]);
		dprintk("\n");
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

static int fp_sendcmd(struct i2c_client *client, u8 b0, u8 b1)
{
	u8 cmd[2]={b0, b1};

	dprintk("fp.o: fp_sendcmd: %02x %02x\n", b0, b1);

	if (i2c_master_send(client, cmd, 2)!=2)
		return -1;

	return 0;
}

/* ------------------------------------------------------------------------- */

static int fp_getid(struct i2c_client *client)
{
	u8 id[3]={0, 0, 0};

	if (fp_cmd(client, FP_GETID, id, 3))
		return 0;

	return id[0];
}

/* ------------------------------------------------------------------------- */

static void fp_add_event(int code)
{
	if (atomic_read(&rc_open.count)>=1)
		return;

	rcbuffer[rcend]=code;
	rcend++;
  
	if (rcend>=RCBUFFERSIZE)
	{
		rcend=0;
	}

	if (rcbeg==rcend)
	{
		printk("fp.o: RC overflow.\n");
	} else
	{
		wake_up(&rcwait);
	}
}

/* ------------------------------------------------------------------------- */

static void fp_handle_rc(struct fp_data *dev)
{
	u16 rc;

	fp_cmd(dev->client, 0x1, (u8*)&rc, 2);
	fp_add_event(rc);
}

/* ------------------------------------------------------------------------- */

static void fp_handle_new_rc(struct fp_data *dev)
{
	u16 rc;

	fp_cmd(dev->client, 0x26, (u8*)&rc, 2);
	fp_add_event(rc);
}

/* ------------------------------------------------------------------------- */

static void fp_handle_button(struct fp_data *dev)
{
	u8 rc;

	fp_cmd(dev->client, 0x25, (u8*)&rc, 1);
	fp_add_event(rc|0xFF00);
}

/* ------------------------------------------------------------------------- */

static void fp_handle_vcr(struct fp_data *dev, int fpVCR)
{
	if (dev->fpVCR!=fpVCR)
	{
		dev->fpVCR = fpVCR;

		if (dev->fpVCR)
		{
			dprintk("fp.o: vcr turned on\n");
		}
		else
		{
			dprintk("fp.o: vcr turned off\n");
		}

		/* todo: event erweitern !? evtl 0xFE<00/01> ??? */
	}
}

/* ------------------------------------------------------------------------- */

static void fp_handle_unknown(struct fp_data *dev)
{
	u8 rc;

	fp_cmd(dev->client, 0x24, (u8*)&rc, 1);
	dprintk("fp.o: misterious interrupt source 0x40: %x\n", rc);
}

/* ------------------------------------------------------------------------- */

static void fp_interrupt(int irq, void *vdev, struct pt_regs * regs)
{
	immap_t *immap=(immap_t*)IMAP_ADDR;

	if(useimap)
		immap->im_ioport.iop_padat|=2;
	schedule_task(&fp_tasklet);
	return;
}

/* ------------------------------------------------------------------------- */

static int fp_init(void)
{
	int res;

	dbox_get_info(&info);
	init_waitqueue_head(&rcwait);

	if ((res=i2c_add_driver(&fp_driver)))
	{
		dprintk("fp.o: Driver registration failed, module not inserted.\n");
		return res;
	}

	if (!defdata)
	{
		i2c_del_driver(&fp_driver);
		dprintk("fp.o: Couldn't find FP.\n");
		return -EBUSY;
	}

//	if (register_chrdev(FP_MAJOR, "fp", &fp_fops))
//	{
//		i2c_del_driver(&fp_driver);
//		dprintk("fp.o: unable to get major %d\n", FP_MAJOR);
//		return -EIO;
//	}

  devfs_handle[FP_MINOR] =
    devfs_register ( NULL, "dbox/fp0", DEVFS_FL_DEFAULT, 0, FP_MINOR,
                     S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                     &fp_fops, NULL );

  if ( ! devfs_handle[FP_MINOR] )
  {
    i2c_del_driver ( &fp_driver );
    return -EIO;
  }

  devfs_handle[RC_MINOR] =
    devfs_register ( NULL, "dbox/rc0", DEVFS_FL_DEFAULT, 0, RC_MINOR,
                     S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                     &fp_fops, NULL );

  if ( ! devfs_handle[RC_MINOR] )
  {
    devfs_unregister ( devfs_handle[FP_MINOR] );
    i2c_del_driver ( &fp_driver );
    return -EIO;
  }

	ppc_md.restart=fp_restart;
	ppc_md.power_off=fp_power_off;
	ppc_md.halt=fp_halt;

	return 0;
}

/* ------------------------------------------------------------------------- */

static int fp_close(void)
{
	int res;

	if ((res=i2c_del_driver(&fp_driver)))
	{
		dprintk("fp.o: Driver unregistration failed, module not removed.\n");
		return res;
	}

//	if ((res=unregister_chrdev(FP_MAJOR, "fp")))
//	{
//		dprintk("fp.o: unable to release major %d\n", FP_MAJOR);
//		return res;
//	}

	devfs_unregister ( devfs_handle[FP_MINOR] );
	devfs_unregister ( devfs_handle[RC_MINOR] );
  
	if (ppc_md.restart==fp_restart)
	{
		ppc_md.restart=0;
	}
    
	if (ppc_md.power_off==fp_power_off)
	{
		ppc_md.power_off=0;
	}
  
	if (ppc_md.halt==fp_halt)
	{
		ppc_md.halt=0;
	}
        
	return 0;
}

/* ------------------------------------------------------------------------- */
int fp_cam_reset()    //needed for sagem / philips?
{
	char msg[2]={0x05, 0xEF};
	
	dprintk("fp: CAM-RESET\n");

	if (i2c_master_send(defdata->client, msg, 2)!=2)
	{
		return -1;
	}

	msg[1]=0xFF;

	if (i2c_master_send(defdata->client, msg, 2)!=2)
	{
		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

int fp_do_reset(int type)
{
	char msg[2]={0x22, type};

	if (i2c_master_send(defdata->client, msg, 2)!=2)
	{
		return -1;
	}

	/* TODO: make better */
	udelay(100*1000);

	msg[1]=0xBF;

	if (i2c_master_send(defdata->client, msg, 2)!=2)
	{
		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

static void fp_check_queues(void)
{
	u8 status;
	int iwork=0;
 
 	dprintk("fp.o: checking queues.\n"); 
	fp_cmd(defdata->client, 0x23, &status, 1);

	/* detect vcr */
	if(defdata->fpVCR!=(status&1))
	{
		fp_handle_vcr(defdata,status&1);
	}

	if (status&(~1))
	{
		dprintk("fp.o: Oops, status is %x (dachte immer der w�re 0/1...)\n", status);
	}

	iwork=0;

	do
	{
		fp_cmd(defdata->client, 0x20, &status, 1);
  
		/* remote control */
		if (status&9)
		{
			if (info.fpREV>=0x80)
				fp_handle_rc(defdata);
			else
				fp_handle_new_rc(defdata);
		}
  
		/* front button */
		if (status&0x10)
		{
			fp_handle_button(defdata);
		}

		/* ??? */
		if (status&0x40)
		{
			fp_handle_unknown(defdata);
		}

		if (iwork++ > 100)
		{
			dprintk("fp.o: Too much work at interrupt.\n");
			break;
		}

	} while (status & (0x59));            // only the ones we can handle

	if (status)
		dprintk("fp.o: unhandled interrupt source %x\n", status);

	return;
}

/* ------------------------------------------------------------------------- */

static void fp_task(void *arg)
{
	immap_t *immap=(immap_t*)IMAP_ADDR;

	fp_check_queues();

	if(useimap)
		immap->im_ioport.iop_padat&=~2;

	enable_irq(FP_INTERRUPT);
}

/* ------------------------------------------------------------------------- */

int fp_set_tuner_dword(int type, u32 tw)
{
	char msg[7]={0, 7, 0xC0};	/* default qam */
    int len=0;

	switch (type)
	{
		case T_QAM:
		{
			*((u32*)(msg+3))=tw;

			len = 7;

			dprintk("fp.o: fp_set_tuner_dword: QAM %08x\n", tw);

			break;
		}

		case T_QPSK:
		{
			*((u32*)(msg+2))=tw;
			msg[1] = 5;
			len = 6;

			dprintk("fp.o: fp_set_tuner_dword: QPSK %08x\n", tw);

			break;
  		}

		default:
			break;
	}

	if(len)
	{
		if (i2c_master_send(defdata->client, msg, len)!=len)
		{
			return -1;
		}
	}

	return -1;
}

/* ------------------------------------------------------------------------- */

int fp_sec_status(void) {
  // < 0 means error: -1 for bus overload, -2 for busy
  return sec_bus_status;
}

int fp_send_diseqc(int style, u8 *cmd, unsigned int len)
{
	unsigned char msg[SEC_MAX_DISEQC_PARAMS+2+3]={0, 0};
	unsigned char status_cmd;
	unsigned char sagem_send[1]={0x22};
	int c,sleep_perbyte,sleeptime;

	if (sec_bus_status == -1)
	  return -1;
	
        switch(style) {
	case 1: // NOKIA
		msg[1]=0x1B;
		sleeptime=2300;
		sleep_perbyte=300;
		status_cmd=0x2D;
	break;
	case 2: // SAGEM / PHILLIPS?
		msg[1]=0x25; //28

	/* this values are measured/calculated for nokia
	   dunno wether sagem needs longer or not */
		sleeptime=2300;
		sleep_perbyte=300;

		status_cmd=0x22;
	break;
	default:
		return -1;
	}

	memcpy(msg+2,cmd,len);

	dprintk("DiSEqC sent:");
	for(c=0;c<len;c++) {
	  dprintk(" %02X",msg[2+c]);
	}
	dprintk("\n");
	
	if(style==2 && len>1)
	{
		i2c_master_send(defdata->client, msg, 2+len);
		udelay(1000*100); 																 // <- ;)
		return 0;
	}	
	
	if(style==2) return 0;
	
	sec_bus_status=-2;
	i2c_master_send(defdata->client, msg, 2+len);
	
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout((sleeptime+(len * sleep_perbyte))/HZ);
	
	for (c=1;c<=5;c++) {
	  fp_cmd(defdata->client, status_cmd, msg, 1);
	  if ( !msg[0] )
	    break;
	  current->state = TASK_INTERRUPTIBLE;
	  schedule_timeout(sleep_perbyte/HZ);
	}

	if (c==5) {
	  dprintk("fp.o: DiSEqC TIMEOUT (could have worked anyway)\n");
	}
	else {
	  dprintk("fp.o: DiSEqC sent after %d poll(s)\n", c);
	}

	sec_bus_status=0;

	if (c==5)
	  return -1;
	else
	  return 0;
}

/* ------------------------------------------------------------------------- */
int fp_sagem_set_SECpower(int power,int tone)
{
   char msg[2]={0x4,0x71};
   
   if (power > 0) { 
     if (power == 1)      // 13V
       msg[1]=0x50;
     else if (power == 2) // 14V
       msg[1]=0x50;
     else if (power == 3) // 18V
       msg[1]=0x60;
   }
   
	 if(tone) msg[1]|=0x1;
	 
	 
   dprintk("fp.o: fp_set_SECpower: %02X\n", msg[1]);
   sec_bus_status=-1;
   if (i2c_master_send(defdata->client, msg, 2)!=2)
     {
       return -1;
     }
   sec_bus_status=0;

   return 0;
}
/* ------------------------------------------------------------------------- */

int fp_set_sec(int power,int tone)
{
  char msg[2]={0x21, 0};

  if ((sec_bus_status == -1) && (power > 0))
    printk("restoring power to SEC bus\n");

  if (power > 0) { // bus power off/on
    msg[1]|=0x40;
    if (power == 1) // 13V
      msg[1]|=0x30;
    else if (power == 2) // 14V
      msg[1]|=0x20;
    else if (power == 3) // 18V
      msg[1]|=0x10;
    // otherwise 19V
    if(tone >0)
      msg[1]|=0x01;
  }
  else if (power == -2)
    msg[1]|=0x50; // activate loop-through // CHECK WHETHER THAT's THE RIGHT BIT !!
  
  dprintk("fp.o: fp_set_sec: %02X\n", msg[1]);
  sec_bus_status=-1;
  if (i2c_master_send(defdata->client, msg, 2)!=2)
    {
      return -1;
    }
  sec_bus_status=0;
  return 0;
}


static int fp_set_wakeup_timer(int minutes)
{
	if (info.fpREV<0x80)
	{
		dprintk("fp.o: fp_set_wakeup_timer on sagem/philips nyi\n");
		return -1;
	} else
	{
		u8 cmd[3]={0x11, minutes&0xFF, minutes>>8};

		if (i2c_master_send(defdata->client, cmd, 3)!=3)
			return -1;

		return 0;
	}
}

static int fp_get_wakeup_timer()
{
	if (info.fpREV<0x80)
	{
		dprintk("fp.o: fp_set_wakeup_timer on sagem/philips nyi\n");
		return -1;
	} else
	{
		u8 id[2]={0, 0};

		if (fp_cmd(defdata->client, FP_WAKEUP, id, 2))
			return -1;

		return id[0]+id[1]*256;
	}
}

/* ------------------------------------------------------------------------- */

EXPORT_SYMBOL(fp_set_tuner_dword);
EXPORT_SYMBOL(fp_set_sec);
EXPORT_SYMBOL(fp_do_reset);
EXPORT_SYMBOL(fp_cam_reset);
EXPORT_SYMBOL(fp_send_diseqc);
EXPORT_SYMBOL(fp_sec_status);
EXPORT_SYMBOL(fp_sagem_set_SECpower);

/* ------------------------------------------------------------------------- */

static void fp_restart(char *cmd)
{
	if (info.fpREV>=0x80)
		fp_sendcmd(defdata->client, 0, 20); // nokia 	
	else
		fp_sendcmd(defdata->client, 0, 9);  // sagem/philips
	for (;;);
}

/* ------------------------------------------------------------------------- */

static void fp_power_off(void)
{
	if (info.fpREV>=0x80)
		fp_sendcmd(defdata->client, 0, 3);
	else
		fp_sendcmd(defdata->client, 0, 0);
	for (;;);
}

/* ------------------------------------------------------------------------- */

static void fp_halt(void)
{
	fp_power_off();
}
                  
/* ------------------------------------------------------------------------- */

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("DBox2 Frontprocessor");

MODULE_PARM(debug,"i");
MODULE_PARM(useimap,"i");

int init_module(void)
{
	return fp_init();
}

/* ------------------------------------------------------------------------- */

void cleanup_module(void)
{
	fp_close();
}
#endif

/* ------------------------------------------------------------------------- */


