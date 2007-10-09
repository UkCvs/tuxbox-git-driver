/* 
 * tda8044h.c
 *
 * Philips TDA8044H QPSK demodulator driver
 *
 * Copyright (C) 2001 Felix Domke <tmbinc@elitedvb.net>
 * Copyright (C) 2002-2004 Andreas Oberritter <obi@linuxtv.org>
 *
 * Adoption to Kernel 2.6 2006 by Markus Gerber
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
 */
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/threads.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/div64.h>

#include "dvb_frontend.h"
#include "tda8044h.h"

static int debug = 0;
#define dprintk	if (debug) printk

struct tda8044_state {
	struct i2c_adapter* i2c;

	/* configuration settings */
	const struct tda8044_config* config;

	struct dvb_frontend frontend;

	u32 clk;
	int afc_loop;
	struct work_struct worklet;
	fe_code_rate_t code_rate;
	fe_spectral_inversion_t spectral_inversion;
	fe_status_t status;
	u8 id;
	u8 reg02;
};

static u8 tda8044_inittab [] = {
	0x02, 0x00, 0x6f, 0xb5, 0x86, 0x22, 0x00, 0xea,
	0x30, 0x42, 0x98, 0x68, 0x70, 0x42, 0x99, 0x58,
	0x95, 0x10, 0xf5, 0xe7, 0x93, 0x0b, 0x15, 0x68,
	0x9a, 0x90, 0x61, 0x80, 0x00, 0xe0, 0x40, 0x00,
	0x0f, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};


static int tda8044_writereg(struct tda8044_state *state, u8 reg, u8 data)
{
	int ret;
	u8 buf[] = { reg, data };
	struct i2c_msg msg = { .addr = 0x68, .flags = 0, .buf = buf, .len = 2 };

	ret = i2c_transfer(state->i2c, &msg, 1);

		if (ret != 1)
		dprintk("%s: writereg error "
			"(reg == 0x%02x, val == 0x%02x, ret == %i)\n",
			__FUNCTION__, reg, data, ret);

	mdelay(10);
	return (ret != 1) ? -EREMOTEIO : 0;;
}


static u8 tda8044_readreg(struct tda8044_state *state, u8 reg)
{
	int ret;
	u8 b0[] = { reg };
	u8 b1[] = { 0x00 };
	struct i2c_msg msg[] = { { .addr = 0x68, .flags = 0, .buf = b0, .len = 1 },
			  { .addr = 0x68, .flags = I2C_M_RD, .buf = b1, .len = 1 } };

	ret = i2c_transfer(state->i2c, msg, 2);

	if (ret != 2)
		dprintk("%s: readreg error (ret == %i)\n", __FUNCTION__, ret);

	return b1[0];	
}

static __inline__ u32 tda8044_div(u32 a, u32 b)
{
	return (a + (b / 2)) / b;
}

static __inline__ u32 tda8044_gcd(u32 a, u32 b)
{
	u32 r;

	while ((r = a % b)) {
		a = b;
		b = r;
	}

	return b;
}

static int tda8044_init(struct dvb_frontend *fe)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;

	u32 i;

	for (i = 0; i < sizeof(tda8044_inittab); i++)
		if (tda8044_writereg(state, i, tda8044_inittab[i]) < 0)
			return -1;

	mdelay(10);

	tda8044_writereg(state, 0x0F, 0x50);
#if 1
	tda8044_writereg(state, 0x20, 0x8F); /*FIXME*/
	tda8044_writereg(state, 0x20, 0xBF);	/*FIXME*/
	//tda8044_writereg(state, 0x00, 0x04);
	tda8044_writereg(state, 0x00, 0x0C);
#endif
	tda8044_writereg(state, 0x00, 0x08); /* Reset AFC1 loop filter */

	mdelay(10);

	tda8044_inittab[0x00] = 0x04; /* 0x04: request interrupt */
	tda8044_inittab[0x0F] = 0x50;
	tda8044_inittab[0x1F] = 0x6c;

	for (i = 0; i < sizeof(tda8044_inittab); i++)
		tda8044_writereg(state, i, tda8044_inittab[i]);

/*	if (state->config->pll_init) {
	   	tda8044_writereg(state, 0x1c, 0x80);
		state->config->pll_init(fe);
		tda8044_writereg(state, 0x1c, 0x00);
	}
*/
	return 0;

}


static int tda8044_write_buf(struct tda8044_state *state, u8 *buf, u8 len)
{
	int ret;
	struct i2c_msg msg = { .addr = 0x68, .flags = 0, .buf = buf, .len = len };

	ret = i2c_transfer(state->i2c, &msg, 1);

	if (ret != 1) {
		dprintk("%s: i2c xfer error (ret == %i)\n", __FUNCTION__, ret);
		return -EREMOTEIO;
	}

	return 0;
}

static int tda8044_set_parameters(struct tda8044_state *state,
				  fe_spectral_inversion_t inversion,
				  u32 symbol_rate,
				  fe_code_rate_t fec_inner)
{
	u8 buf[16];
	u64 ratio;
	u32 clk = 96000000;
	u32 k = (1 << 21);
	u32 sr = symbol_rate;
	u32 gcd;

	/* register */
	buf[0x00] = 0x01;

	/*
	 * Viterbi decoder:
	 * Differential decoding off
	 * Spectral inversion unknown
	 * QPSK modulation
	 */
	buf[0x01] = 0x00;

	if (inversion == INVERSION_ON)
		buf[0x01] |= 0x60;
	else if (inversion == INVERSION_OFF)
		buf[0x01] |= 0x20;

	/*
	 * CLK ratio:
	 * system clock frequency is 96000000 Hz
	 * formula: 2^21 * freq / symrate
	 */
	gcd = tda8044_gcd(clk, sr);
	clk /= gcd;
	sr /= gcd;

	gcd = tda8044_gcd(k, sr);
	k /= gcd;
	sr /= gcd;

	ratio = (u64)k * (u64)clk;
	do_div(ratio, sr);

	buf[0x02] = ratio >> 16;
	buf[0x03] = ratio >> 8;
	buf[0x04] = ratio;

	/* nyquist filter roll-off factor 35% */
	buf[0x05] = 0x20;

	/* Anti Alias Filter */
	if (symbol_rate < 4500000) {
		printk("%s: unsupported symbol rate: %d\n", __FILE__, symbol_rate);
	}
	else if (symbol_rate <= 6000000)
		buf[0x05] |= 0x07;
	else if (symbol_rate <= 9000000)
		buf[0x05] |= 0x06;
	else if (symbol_rate <= 12000000) 
		buf[0x05] |= 0x05; 
	else if (symbol_rate <= 18000000)
		buf[0x05] |= 0x04;
	else if (symbol_rate <= 24000000)
		buf[0x05] |= 0x03;
	else if (symbol_rate <= 36000000)
		buf[0x05] |= 0x02;
	else if (symbol_rate <= 45000000)
		buf[0x05] |= 0x01;
	else
		printk("%s: unsupported symbol rate: %d\n", __FILE__, symbol_rate);

	/* Sigma Delta converter */
	buf[0x06] = 0x00;

	/* FEC: Possible puncturing rates */
	if (fec_inner == FEC_NONE)
		buf[0x07] = 0x00;
	else if ((fec_inner >= FEC_1_2) && (fec_inner <= FEC_8_9))
		buf[0x07] = (1 << (8 - fec_inner));
	else if (fec_inner == FEC_AUTO)
		buf[0x07] = 0xff;
	else
		return -EINVAL;

	/* carrier lock detector threshold value */
	buf[0x08] = 0x30;
	/* AFC1: proportional part settings */
	buf[0x09] = 0x42;
	/* AFC1: integral part settings */
	buf[0x0a] = 0x98;
	/* PD: Leaky integrator SCPC mode */
	buf[0x0b] = 0x28;
	/* AFC2, AFC1 controls */
	buf[0x0c] = 0x30;
	/* PD: proportional part settings */
	buf[0x0d] = 0x42;
	/* PD: integral part settings */
	buf[0x0e] = 0x99;
	/* AGC */
	buf[0x0f] = 0x50;

	return tda8044_write_buf(state, buf, sizeof(buf));
}


static int tda8044_set_clk(struct tda8044_state *state)
{
	u8 buf[3];

	/* register */
	buf[0x00] = 0x17;
	/* CLK proportional part */
	buf[0x01] = 0x68;
	/* CLK integral part */
	buf[0x02] = 0x9a;

	return tda8044_write_buf(state, buf, sizeof(buf));
}

# if 0
static int tda8044_set_scpc_freq_offset(struct dvb_i2c_bus *i2c)
{
	return tda8044_writereg(i2c, 0x22, 0xf9);
}
# endif

static int tda8044_close_loop(struct tda8044_state *state)
{
	u8 buf[3];

	/* register */
	buf[0x00] = 0x0b;
	/* PD: Loop closed, LD: lock detect enable, SCPC: Sweep mode - AFC1 loop closed */
	buf[0x01] = 0x68;
	/* AFC1: Loop closed, CAR Feedback: 8192 */
	buf[0x02] = 0x70;

	return tda8044_write_buf(state, buf, sizeof(buf));
}


static int tda8044_set_voltage(struct dvb_frontend* fe, fe_sec_voltage_t voltage)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;

	switch (voltage) {
	case SEC_VOLTAGE_13:
		return tda8044_writereg(state, 0x20, 0x3f);
	case SEC_VOLTAGE_18:
		return tda8044_writereg(state, 0x20, 0xbf);
	case SEC_VOLTAGE_OFF:
		return tda8044_writereg(state, 0x20, 0);
	default:
		return -EINVAL;
	}
}


static int tda8044_set_tone(struct dvb_frontend* fe, fe_sec_tone_mode_t tone)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;
	switch (tone) {
	case SEC_TONE_OFF:
		return tda8044_writereg(state, 0x29, 0x00);
	case SEC_TONE_ON:
		return tda8044_writereg(state, 0x29, 0x80);
	default:
		return -EINVAL;
	}
}


static int tda8044_send_diseqc_msg(struct dvb_frontend* fe, struct dvb_diseqc_master_cmd *cmd)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;
	u8 buf[cmd->msg_len + 1];

	/* register */
	buf[0] = 0x23;
	/* diseqc command */
	memcpy(buf + 1, cmd->msg, cmd->msg_len);

	tda8044_write_buf(state, buf, cmd->msg_len + 1);
	tda8044_writereg(state, 0x29, 0x0c + (cmd->msg_len - 3));

	return 0;
}


static int tda8044_send_diseqc_burst(struct dvb_frontend* fe, fe_sec_mini_cmd_t cmd)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;

	switch (cmd) {
	case SEC_MINI_A:
		return tda8044_writereg(state, 0x29, 0x14);
	case SEC_MINI_B:
		return tda8044_writereg(state, 0x29, 0x1c);
	default:
		return -EINVAL;
	}
}


static int tda8044_sleep(struct dvb_frontend* fe)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;
	
	tda8044_writereg(state, 0x20, 0x00);	/* enable loop through */
	tda8044_writereg(state, 0x00, 0x02);	/* enter standby */
	return 0;
}


#if 0
static void tda8044_reset(struct dvb_frontend* fe)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;
	u8 reg0 = tda8044_readreg(state, 0x00);

	tda8044_writereg(state, 0x00, reg0 | 0x35);
	tda8044_writereg(state, 0x00, reg0);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void tda8044_tasklet(struct work_struct *work)
{
	struct tda8044_state* state = container_of(work, struct tda8044_state, worklet);
#else
static void tda8044_tasklet(void *priv)
{
	struct tda8044_state* state = priv;
#endif
	u8 val;

	static const fe_spectral_inversion_t inv_tab[] = {
		INVERSION_OFF, INVERSION_ON
	};

	static const fe_code_rate_t fec_tab[] = {
		FEC_8_9, FEC_1_2, FEC_2_3, FEC_3_4,
		FEC_4_5, FEC_5_6, FEC_6_7, FEC_7_8,
	};

	val = tda8044_readreg(state, 0x02);

	if (val == state->reg02) {
		if (state->config->irq) {
			tda8044_writereg(state, 0x00, 0x04);
			enable_irq(state->config->irq);
		}
		return;
	}
	state->status = 0;

	if (val & 0x01) /* demodulator lock */
		state->status |= FE_HAS_SIGNAL;
	if (val & 0x02) /* clock recovery lock */
		state->status |= FE_HAS_CARRIER;
	if (val & 0x04) /* viterbi lock */
		state->status |= FE_HAS_VITERBI;
	if (val & 0x08) /* deinterleaver lock (packet sync) */
		state->status |= FE_HAS_SYNC;
	if (val & 0x10) /* derandomizer lock (frame sync) */
		state->status |= FE_HAS_LOCK;

	state->reg02 = val;

	if (state->status & FE_HAS_LOCK) {
		val = tda8044_readreg(state, 0x0e);
		state->spectral_inversion = inv_tab[(val >> 7) & 0x01];
		state->code_rate = fec_tab[val & 0x07];
	}
	else {
		state->spectral_inversion = INVERSION_AUTO;
		state->code_rate = FEC_AUTO;
	}
	if (state->config->irq) {
		tda8044_writereg(state, 0x00, 0x04);
		enable_irq(state->config->irq);
	}
	       
}

static int tda8044_set_frontend(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
   	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;

	tda8044_writereg(state, 0x1c, 0x80);
	state->config->pll_set(fe, p);
	tda8044_writereg(state, 0x1c, 0x00);

	tda8044_set_parameters(state, p->inversion, p->u.qpsk.symbol_rate, p->u.qpsk.fec_inner);
	tda8044_set_clk(state);
//	tda8044_set_scpc_freq_offset(state);
	tda8044_close_loop(state);  
	return 0;
}

static int tda8044_get_frontend(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;

	if (!state->config->irq)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
		tda8044_tasklet(&state->worklet);
#else
		tda8044_tasklet((struct tda8044_state*) fe->demodulator_priv);
#endif

	p->inversion = state->spectral_inversion;
	p->u.qpsk.fec_inner = state->code_rate;

	return 0;
}

static int tda8044_read_status(struct dvb_frontend* fe, fe_status_t* status)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;

	if (!state->config->irq)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
		tda8044_tasklet(&state->worklet);
#else
		tda8044_tasklet((struct tda8044_state*) fe->demodulator_priv);
#endif

	*status = state->status;

	return 0;
}

static int tda8044_read_ber(struct dvb_frontend* fe, u32* ber)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;

	*ber = ((tda8044_readreg(state, 0x0b) & 0x1f) << 16) |
				(tda8044_readreg(state, 0x0c) << 8) |
				tda8044_readreg(state, 0x0d);

	return 0;
}

static int tda8044_read_signal_strength(struct dvb_frontend* fe, u16* strength)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;

	u8 gain = ~tda8044_readreg(state, 0x01);
	*strength = (gain << 8) | gain;

	return 0;
}

static int tda8044_read_snr(struct dvb_frontend* fe, u16* snr)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;

	u8 quality = tda8044_readreg(state, 0x08);
	*snr = (quality << 8) | quality;

	return 0;
}

static int tda8044_read_ucblocks(struct dvb_frontend* fe, u32* ucblocks)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;

	*ucblocks = tda8044_readreg(state, 0x0f);
	if (*ucblocks == 0xff)
		*ucblocks = 0xffffffff;

	return 0;
}

static void tda8044_release(struct dvb_frontend* fe)
{
	struct tda8044_state* state = (struct tda8044_state*) fe->demodulator_priv;
	if (state->config->irq)
		free_irq(state->config->irq, &state->worklet);
	kfree(state);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
static irqreturn_t tda8044_irq(int irq, void *priv)
#else
static irqreturn_t tda8044_irq(int irq, void *priv, struct pt_regs *pt)
#endif
{
	disable_irq(irq);
	schedule_work(priv);
	return IRQ_HANDLED;
}                                      

static struct dvb_frontend_ops tda8044_ops;

struct dvb_frontend *tda8044_attach(const struct tda8044_config* config, struct i2c_adapter* i2c)
{
	struct tda8044_state *state = NULL;
	int ret;

	state = (struct tda8044_state*) kmalloc(sizeof(struct tda8044_state), GFP_KERNEL);
	if (state == NULL) goto error;
	
	/* setup the state */
	state->config = config;
	state->i2c = i2c;
	memcpy(&state->frontend.ops, &tda8044_ops, sizeof(struct dvb_frontend_ops));
	state->spectral_inversion = INVERSION_AUTO;
	state->code_rate = FEC_AUTO;
	state->status = 0;
	state->afc_loop = 0;
	state->reg02 = 0xff;
	state->clk = 96000000;

	if (tda8044_writereg(state, 0x89, 0x00) < 0) goto error;
	if (tda8044_readreg(state, 0x00) != 0x04) goto error;
	printk("tda8044: Detected tda8044\n");

	if (state->config->irq) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
		INIT_WORK(&state->worklet, tda8044_tasklet);
#else
		INIT_WORK(&state->worklet, tda8044_tasklet, state);
#endif
		if ((ret = request_irq(state->config->irq, tda8044_irq, SA_ONESHOT, "tda8044", &state->worklet)) < 0) {
			printk(KERN_ERR "%s: request_irq failed (%d)\n", __FUNCTION__, ret);
			goto error;
		}
	}
	/* create dvb_frontend */
	state->frontend.demodulator_priv = state;
	return &state->frontend;

error:
	if (state) kfree(state);
		return NULL;

}

static struct dvb_frontend_ops tda8044_ops = {

	.info = {
		.name = "Philips TDA8044 DVB-S",
		.type = FE_QPSK,
		.frequency_min = 500000,
		.frequency_max = 2700000,
		.frequency_stepsize = 125,
		.symbol_rate_min = 4500000,
		.symbol_rate_max = 45000000,
		.caps =	FE_CAN_INVERSION_AUTO |
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_4_5 | FE_CAN_FEC_5_6 | FE_CAN_FEC_6_7 |
			FE_CAN_FEC_7_8 | FE_CAN_FEC_8_9 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK |
			FE_CAN_MUTE_TS
	},

	.release = tda8044_release,

	.init = tda8044_init,
	.sleep = tda8044_sleep,

	.set_frontend = tda8044_set_frontend,
	.get_frontend = tda8044_get_frontend,

	.read_status = tda8044_read_status,
	.read_ber = tda8044_read_ber,
	.read_signal_strength = tda8044_read_signal_strength,
	.read_snr = tda8044_read_snr,
	.read_ucblocks = tda8044_read_ucblocks,

	.diseqc_send_master_cmd = tda8044_send_diseqc_msg,
     	.diseqc_send_burst = tda8044_send_diseqc_burst,
     	.set_tone = tda8044_set_tone,
     	.set_voltage = tda8044_set_voltage,
};

module_param(debug, int, 0644);

MODULE_DESCRIPTION("Philips TDA8044 DVB-S Demodulator driver");
MODULE_AUTHOR("Felix Domke");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(tda8044_attach);
