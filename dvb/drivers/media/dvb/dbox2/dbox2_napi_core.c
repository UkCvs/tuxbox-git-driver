/*
 * $Id: dbox2_napi_core.c,v 1.1.2.11 2006/02/20 21:35:53 racker Exp $
 *
 * Dbox2 DVB Adapter driver
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2005 Carsten Juttner <carjay@gmx.net>
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
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>

#include <avia/avia_gt_dmx.h>
#include <dvb-core/dvbdev.h>
#include <dvb-core/dvb_frontend.h>

#include "dbox2_pll.h"
#include "dbox2_fp_napi.h"
#ifndef STANDALONE
#include "dbox2_avia_av_napi.h"
#include "dbox2_avia_gt_napi.h"
#include "dbox2_cam_napi.h"
#endif

#include <dbox/dbox2_fp_sec.h>

#include <frontends/at76c651.h>
#include <frontends/tda80xx.h>
#include <frontends/ves1820.h>
#include <frontends/ves1x93.h>

static struct dbox2_fe {
	struct dvb_adapter *dvb_adap;
	struct i2c_adapter *i2c_adap;
	struct dvb_frontend *dvb_fe;
	struct pll_state pll;
	int (*pll_init)(struct dvb_frontend* fe);	/* pll-functions for current frontend */
	int (*pll_set)(struct pll_state* pll, struct dvb_frontend_parameters* params);
	int (*fe_read_status)(struct dvb_frontend *, fe_status_t *);
	void *fe_config;	/* type is different for each fe */
	unsigned int irq;
} fe_state;

enum {
	DBOX2_NAPI_NOKIA = 1,
	DBOX2_NAPI_PHILIPS = 2,
	DBOX2_NAPI_SAGEM = 3
};
static int manuf_id;

struct dvb_adapter *dbox2_napi_get_adapter(void)
{
	return fe_state.dvb_adap;
}

/* DVB API does not check for NULL, so we have to always implement these */
static int dbox2_napi_pll_init(struct dvb_frontend* fe)
{
	if (fe_state.pll_init)
		return fe_state.pll_init(fe);
	else
		return 0;
}

static int dbox2_napi_pll_set(struct dvb_frontend* fe, struct dvb_frontend_parameters* params)
{
	if (fe_state.pll_set)
		return fe_state.pll_set(&fe_state.pll,params);
	else
		return 0;
}

static int dbox2_napi_status_monitor(struct dvb_frontend *fe, fe_status_t *status)
{
	static fe_status_t last_lock = 0;
	fe_state.fe_read_status(fe, status);
	if ((*status ^ last_lock) & FE_HAS_LOCK){	/* got/lost lock */
		last_lock = *status & FE_HAS_LOCK;
		if (last_lock)
			avia_gt_dmx_enable_framer();
		else
			avia_gt_dmx_disable_framer();
	}
	return 0;
}

/**************************/
/* DVB API frontend probe */
/**************************/

/* Nokia cable boxes store this value in a small serial EEPROM */
static int nokia_C_get_pwm (u8 *pwm_val)
{
	u8 b = 0xff;
	u8 pwm;
	struct i2c_msg msg [] = { { .addr = 0x50, .flags = 0, .buf = &b, .len = 1 },
			 { .addr = 0x50, .flags = I2C_M_RD, .buf = &pwm, .len = 1 } };

	if ((i2c_transfer(fe_state.i2c_adap, msg, 2) != 2) || (pwm == 0xff))
		pwm = 0x48;

	printk(KERN_INFO "dbox2_napi_core: pwm=0x%02x\n", pwm);

	if (pwm_val)
		*pwm_val = pwm;

	return 0;
}

static int dbox2_fe_setup_ves1820(struct dbox2_fe *state, struct ves1820_config *cfg)
{
	u8 pwm;
	nokia_C_get_pwm(&pwm);

	state->dvb_fe = ves1820_attach(cfg,state->i2c_adap,pwm);
	if (!state->dvb_fe)
		return -ENODEV;

	dbox2_fp_napi_get_sec_ops(state->dvb_fe->ops);
	/* 	ves1820 use SPI (through FP) */
	state->pll_set = dbox2_fp_napi_qam_set_freq;
	
	return 0;
}

/* HACK - ves1x93-module does not offer a way to retrieve the identity :S */
static int ves1x93_get_identity (int *id)
{
	int ret;
	u8 b0 [] = { 0x00, 0x1e };
	u8 b1 [] = { 0 };
	struct i2c_msg msg [] = { { .addr = 0x08, .flags = 0, .buf = b0, .len = 2 },
			   { .addr = 0x08, .flags = I2C_M_RD, .buf = b1, .len = 1 } };

	ret = i2c_transfer (fe_state.i2c_adap, msg, 2);
	if (ret != 2){
		printk(KERN_ERR "dbox2_napi: i2c error in ves1x93_getidentity");
		return -EIO;	
	}
	if (id)
		*id = b1[0];
	return 0;
}
/* HACK */

static int dbox2_fe_setup_ves1x93(struct dbox2_fe *state, struct ves1x93_config *cfg)
{
	int id;
	state->dvb_fe = ves1x93_attach(cfg,state->i2c_adap);
	if (!state->dvb_fe)
		return -ENODEV;

	dbox2_fp_napi_get_sec_ops(state->dvb_fe->ops);
	/* 	ves1893 use SPI (through FP)
		ves1993 uses I2C (through MPC) */
	
	if (!ves1x93_get_identity(&id)){
		switch (id){
		case 0xdc: /* VES1893A rev1 */
		case 0xdd: /* VES1893A rev2 */
			state->pll_set = dbox2_fp_napi_qpsk_set_freq;
			break;
		case 0xde: /* VES1993 */
			state->pll_set = dbox2_pll_tsa5059_set_freq;
			state->pll.i2c_addr = 0xc2>>1;
			if (manuf_id == DBOX2_NAPI_NOKIA) {
				state->pll.clk = 16000000;
				state->pll.tsa5059_xc = 2;
			} else if (manuf_id == DBOX2_NAPI_SAGEM) {
				state->pll.clk = 4000000;
				state->pll.tsa5059_xc = 0;
			} else {
				printk(KERN_WARNING "dbox2_napi: no known pll for manufacturer %d\n",manuf_id);
				return -ENODEV;
			} 
			break;
		default:
			printk(KERN_WARNING "dbox2_napi: unknown identity %d for ves1x93\n",id);
			return -ENODEV;
		}
	}
	return 0;
}

/***************/
/* board probe */
/***************/

int dbox2_probe_nokia_S_frontend(struct dbox2_fe *state){
	struct ves1x93_config *cfg = kmalloc(sizeof(struct ves1x93_config),GFP_KERNEL);
	int id;

	if (!cfg)
		return -ENOMEM;
	if (!ves1x93_get_identity(&id)){
		switch (id){
			case 0xdc: /* VES1893A rev1 */
			case 0xdd: /* VES1893A rev2 */
				cfg->xin = 91000000UL;
				cfg->invert_pwm = 1;
				break;
			case 0xde: /* VES1993 */
	cfg->xin = 96000000UL;
	cfg->invert_pwm = 0;
				break;
			default:
				printk(KERN_WARNING "dbox2_napi: default value cfg->xin for ves1x93\n");
				cfg->xin = 91000000UL;
				cfg->invert_pwm = 1;
		} 
	} else {
		printk(KERN_WARNING "dbox2_napi: default value cfg->xin for ves1x93\n");
		cfg->xin = 91000000UL;
		cfg->invert_pwm = 1;
	}
	cfg->demod_address = 0x10>>1;
	cfg->pll_init = dbox2_napi_pll_init;
	cfg->pll_set = dbox2_napi_pll_set;
	if (dbox2_fe_setup_ves1x93(state,cfg)){
		kfree(cfg);
		return -ENODEV;
	}
	state->fe_config = cfg;
	return 0;
}

int dbox2_probe_nokia_C_frontend(struct dbox2_fe *state){
	struct ves1820_config *cfg = kmalloc(sizeof(struct ves1820_config),GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;
	cfg->demod_address = 0x10>>1;
	cfg->xin = 69600000UL;
	cfg->invert = 0;
	cfg->selagc = 0;
	cfg->pll_init = dbox2_napi_pll_init;
	cfg->pll_set = dbox2_napi_pll_set;
	if (dbox2_fe_setup_ves1820(state,cfg)){
		kfree(cfg);
		return -ENODEV;
	}
	state->fe_config = cfg;
	state->pll_init = NULL; 
	state->pll_set = dbox2_fp_napi_qam_set_freq;
	return 0;
}

int dbox2_probe_philips_S_frontend(struct dbox2_fe *state){
	struct tda80xx_config *cfg = kmalloc(sizeof(struct tda80xx_config),GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;
	cfg->demod_address = 0xd0>>1;
//	cfg->irq = state->irq;	/* TODO: irq handling needs to be fixed */
	cfg->irq = 0;
	cfg->volt13setting = 0x3f;
	cfg->volt18setting = 0xbf;
	cfg->pll_init = dbox2_napi_pll_init;
	cfg->pll_set = dbox2_napi_pll_set;
	if ((state->dvb_fe = tda80xx_attach(cfg,state->i2c_adap))==0){
		kfree(cfg);
		return -ENODEV;
	}
	state->fe_config = cfg;
	state->pll_init = NULL; 
	state->pll_set = dbox2_pll_tsa5059_set_freq;
	state->pll.clk = 4000000;
	state->pll.tsa5059_xc = 0;
	state->pll.i2c_addr = 0xc6>>1;
	return 0;
}

int dbox2_probe_philips_C_frontend(struct dbox2_fe *state){
	return -ENODEV;
}

int dbox2_probe_sagem_S_frontend(struct dbox2_fe *state){
	struct ves1x93_config *cfg = kmalloc(sizeof(struct ves1x93_config),GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;
	cfg->demod_address = 0x10>>1;
	cfg->xin = 92160000UL;
	cfg->invert_pwm = 1;
	cfg->pll_init = dbox2_napi_pll_init;
	cfg->pll_set = dbox2_napi_pll_set;
	if (dbox2_fe_setup_ves1x93(state,cfg)){
		kfree(cfg);
		return -ENODEV;
	}
	state->fe_config = cfg;
	return 0;
}

int dbox2_probe_sagem_C_frontend(struct dbox2_fe *state){
	struct at76c651_config *cfg = kmalloc(sizeof(struct at76c651_config),GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;
	cfg->demod_address = 0x1a>>1;
	cfg->pll_init = dbox2_napi_pll_init;
	cfg->pll_set = dbox2_napi_pll_set;
	if ((state->dvb_fe = at76c651_attach(cfg,state->i2c_adap))==0){
		kfree(cfg);
		return -ENODEV;
	}
	state->fe_config = cfg;
	state->pll_init = NULL; 
	state->pll_set = dbox2_pll_tua6010_set_freq;
	state->pll.i2c_addr = 0xc2>>1;
	return 0;
}

/****************/
/* driver probe */
/****************/

static int dbox2_fe_probe(struct device *dev)
{
    /* find out board manufacturer */
	int ret;
	struct platform_device *pdev = to_platform_device(dev);
	manuf_id = (int)pdev->dev.platform_data;
	fe_state.irq = platform_get_irq(pdev,0);
	
    switch (manuf_id){
    case DBOX2_NAPI_NOKIA:
		if (dbox2_probe_nokia_S_frontend(&fe_state) &&
				dbox2_probe_nokia_C_frontend(&fe_state)){
			printk(KERN_ERR "dbox2_napi: no Nokia frontend found\n");
			return -ENODEV;
		}
	    break;

    case DBOX2_NAPI_PHILIPS:
		if (dbox2_probe_philips_S_frontend(&fe_state) &&
				dbox2_probe_philips_C_frontend(&fe_state)){
			printk(KERN_ERR "dbox2_napi: no Philips frontend found\n");
			return -ENODEV;
		}
	    break;

    case DBOX2_NAPI_SAGEM:
		if (dbox2_probe_sagem_S_frontend(&fe_state) &&
				dbox2_probe_sagem_C_frontend(&fe_state)){
			printk(KERN_ERR "dbox2_napi: no Sagem frontend found\n");
			return -ENODEV;
		}
		break;
    default:
	    printk(KERN_ERR "dbox2_napi: unknown manufacturer id: %d\n",manuf_id);
	    return -ENODEV;	
    }

    /* We want to quicky restart the framer if the signal gets interrupted, so we
	listen in on the status. Not all ucodes report errors so this might
	be necessary */

    fe_state.fe_read_status = fe_state.dvb_fe->ops->read_status;
    fe_state.dvb_fe->ops->read_status = dbox2_napi_status_monitor;

    if ((ret = dvb_register_frontend(fe_state.dvb_adap, fe_state.dvb_fe))<0){
    	printk(KERN_ERR "dbox2_napi: error registering frontend\n");
		if (fe_state.fe_config){
			kfree(fe_state.fe_config);
			fe_state.fe_config = NULL;
		}
		return ret;
    }
    return 0;
}

static int dbox2_fe_remove(struct device *dev)
{
    if (fe_state.dvb_fe)
		dvb_unregister_frontend(fe_state.dvb_fe);
	if (fe_state.fe_config)
		kfree(fe_state.fe_config);
	return 0;
}

static struct device_driver dbox2_fe_driver = {
	.name	= "fe",
	.bus	= &platform_bus_type,
	.probe 	= dbox2_fe_probe,
	.remove	= dbox2_fe_remove
};

static int __init dbox2_napi_init(void)
{
	int res;
	printk(KERN_INFO "$Id: dbox2_napi_core.c,v 1.1.2.11 2006/02/20 21:35:53 racker Exp $\n");

	fe_state.dvb_adap = kmalloc (sizeof(struct dvb_adapter),GFP_KERNEL);
	if (!fe_state.dvb_adap)
		return -ENOMEM;

	if ((res = dvb_register_adapter(fe_state.dvb_adap, "C-Cube AViA GTX/eNX with AViA 500/600",THIS_MODULE))<0){
		printk(KERN_ERR "dbox2_napi: error registering adapter\n");
		goto out_adap;
	}

	fe_state.i2c_adap = i2c_get_adapter(0);
	if (!fe_state.i2c_adap || (fe_state.i2c_adap->class&I2C_CLASS_TV_DIGITAL) != I2C_CLASS_TV_DIGITAL){
	    /* whoa... a modded dbox2 */
	    printk(KERN_ERR "dbox2_napi: no suitable i2c adapter found\n");
		goto out_dvb;
	}
	dbox2_pll_init(fe_state.i2c_adap);
	
	if ((res = driver_register(&dbox2_fe_driver))<0){
		printk(KERN_ERR "dbox2_napi: error registering fe device driver\n");
		goto out_i2c;
	}

#ifndef STANDALONE
	if ((res = avia_av_napi_init())<0){
		printk(KERN_ERR "dbox2_napi: error initialising DVB API decoder driver\n");
		goto out_driver;
	}

	if ((res = cam_napi_init())<0){
		printk(KERN_ERR "dbox2_napi: error initialising DVB API CAM driver\n");
		goto out_gt;
	}

	if ((res = avia_gt_napi_init())<0){
		printk(KERN_ERR "dbox2_napi: error initialising DVB API demux driver\n");
		goto out_av;
	}

#endif
	if ((res = dbox2_fp_napi_init())<0){
		printk(KERN_ERR "dbox2_napi: error initialising DVB API fp driver\n");
		goto out_cam;
	}

	return 0;
	
	/* unwind code */
out_cam:
#ifndef STANDALONE
	cam_napi_exit();
out_gt:
	avia_gt_napi_exit();
out_av:
	avia_av_napi_exit();
out_driver:
#endif
	driver_unregister(&dbox2_fe_driver);
out_i2c:
	i2c_put_adapter(fe_state.i2c_adap);
out_dvb:
	dvb_unregister_adapter(fe_state.dvb_adap);
out_adap:
	kfree(fe_state.dvb_adap);

	return res;
}

static void __exit dbox2_napi_exit(void)
{
#ifndef STANDALONE
	avia_gt_napi_exit();
	cam_napi_exit();
	avia_av_napi_exit();
#endif
	dbox2_fp_napi_exit();
	driver_unregister(&dbox2_fe_driver);
	dvb_unregister_adapter(fe_state.dvb_adap);
	i2c_put_adapter(fe_state.i2c_adap);
	kfree(fe_state.dvb_adap);
}

module_init(dbox2_napi_init);
module_exit(dbox2_napi_exit);

EXPORT_SYMBOL(dbox2_napi_get_adapter);
MODULE_DESCRIPTION("dbox2 DVB adapter driver");
MODULE_AUTHOR("Carsten Juttner <carjay@gmx.net>");
MODULE_LICENSE("GPL");
