/*
 * $Id: avia_gt_ir.c,v 1.30.4.6 2008/09/19 22:43:42 seife Exp $
 * 
 * AViA eNX/GTX ir driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
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

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/version.h>

#include "avia_gt.h"
#include "avia_gt_ir.h"

DECLARE_WAIT_QUEUE_HEAD(rx_wait);
DECLARE_WAIT_QUEUE_HEAD(tx_wait);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
DECLARE_MUTEX(ir_sem);
#else
DEFINE_MUTEX(ir_sem);
#endif

static struct ir_client clientlist[2]; /* max. 2 clients (RX,TX) */

static sAviaGtInfo *gt_info;
static u32 duty_cycle = 33;
static u16 first_period_low;
static u16 first_period_high;
static u32 frequency = 38000;
#define RX_MAX 50000
static sAviaGtIrPulse *rx_buffer;
static u32 rx_buffer_read_position;
static u32 rx_buffer_write_position;
static u8 rx_unit_busy;
static sAviaGtTxIrPulse *tx_buffer;
static u8 tx_buffer_pulse_count;
static u8 tx_unit_busy;

/* measured tick length in microseconds */
#define IR_TICK_LENGTH ((1125000/10) / 8)
/* half sysclk in (1/(MHz/1000)) */
#define HALFSYSCLOCK 20

#define TICK_COUNT_TO_USEC(tick_count) ((tick_count) * IR_TICK_LENGTH / 1000)

static struct timeval last_timestamp;

static void avia_gt_ir_tx_irq(unsigned short irq)
{
	dprintk("avia_gt_ir: tx irq\n");

	tx_unit_busy = 0;

	wake_up_interruptible(&tx_wait);

	if ((clientlist[0].flags & AVIA_GT_IR_TX)&&clientlist[0].tx_task)
		tasklet_schedule(clientlist[0].tx_task);
	else if ((clientlist[1].flags & AVIA_GT_IR_TX)&&clientlist[1].tx_task)
		tasklet_schedule(clientlist[1].tx_task);

}

static void avia_gt_ir_rx_irq(unsigned short irq)
{
	struct timeval timestamp;
	
	do_gettimeofday(&timestamp);

	rx_buffer[rx_buffer_write_position].high = (enx_reg_s(RPH)->RTCH * IR_TICK_LENGTH) / 1000;
	rx_buffer[rx_buffer_write_position].low = ((timestamp.tv_sec - last_timestamp.tv_sec) * 1000 * 1000) + (timestamp.tv_usec - last_timestamp.tv_usec) - rx_buffer[rx_buffer_write_position].high;

	last_timestamp = timestamp;

	if (rx_buffer_write_position < (RX_MAX - 1))
		rx_buffer_write_position++;
	else
		rx_buffer_write_position = 0;

	rx_unit_busy = 0;

	wake_up_interruptible(&rx_wait);

	if ((clientlist[0].flags & AVIA_GT_IR_RX)&&clientlist[0].rx_task)
		tasklet_schedule(clientlist[0].rx_task);
	else if ((clientlist[1].flags & AVIA_GT_IR_RX)&&clientlist[1].rx_task)
		tasklet_schedule(clientlist[1].rx_task);

}

void avia_gt_ir_enable_rx_dma(unsigned char enable, unsigned char offset)
{
	avia_gt_reg_set(IRRO, Offset, 0);
	avia_gt_reg_set(IRRE, Offset, offset >> 1);
	avia_gt_reg_set(IRRE, E, enable);
}

void avia_gt_ir_enable_tx_dma(unsigned char enable, unsigned char length)
{
	avia_gt_reg_set(IRTO, Offset, 0);
	avia_gt_reg_set(IRTE, Offset, length - 1);
	avia_gt_reg_set(IRTE, C, 0);
	avia_gt_reg_set(IRTE, E, enable);
}

u32 avia_gt_ir_get_rx_buffer_read_position(void)
{
	return rx_buffer_read_position;
}

u32 avia_gt_ir_get_rx_buffer_write_position(void)
{
	return rx_buffer_write_position;
}


int avia_gt_ir_queue_pulse(u32 period_high, u32 period_low, u8 block)
{
	WAIT_IR_UNIT_READY(tx);

	if (tx_buffer_pulse_count >= AVIA_GT_IR_MAX_PULSE_COUNT)
		return -EBUSY;

	if (tx_buffer_pulse_count == 0) {
		first_period_high = period_high;
		first_period_low = period_low;
	}
	else {
		tx_buffer[tx_buffer_pulse_count - 1].MSPR = USEC_TO_CWP(period_high + period_low) - 1;

		if (period_low != 0)
			tx_buffer[tx_buffer_pulse_count - 1].MSPL = USEC_TO_CWP(period_low) - 1;
		else
			tx_buffer[tx_buffer_pulse_count - 1].MSPL = 0;
			// Mhhh doesnt work :(
			//tx_buffer[tx_buffer_pulse_count - 1].MSPL = USEC_TO_CWP(period_high) - 1;
	}

	tx_buffer_pulse_count++;

	return 0;
}

wait_queue_head_t *avia_gt_ir_get_receive_wq(void)
{
	return &rx_wait;
}

int avia_gt_ir_receive_pulse(u32 *period_low, u32 *period_high, u8 block)
{
	if (rx_buffer_write_position == rx_buffer_read_position) {
		rx_unit_busy = 1;
		WAIT_IR_UNIT_READY(rx);
	}

	if (period_low)
		*period_low = rx_buffer[rx_buffer_read_position].low;

	if (period_high)
		*period_high = rx_buffer[rx_buffer_read_position].high;

	if (rx_buffer_read_position < RX_MAX)
		rx_buffer_read_position++;
	else
		rx_buffer_read_position = 0;

	return 0;
}

void avia_gt_ir_reset(int reenable)
{
	avia_gt_reg_set(RSTR0, IR, 1);

	if (reenable)
		avia_gt_reg_set(RSTR0, IR, 0);
}

int avia_gt_ir_send_buffer(u8 block)
{
	WAIT_IR_UNIT_READY(tx);

	if (tx_buffer_pulse_count == 0)
		return 0;
	
	if (tx_buffer_pulse_count >= 2)
		avia_gt_ir_enable_tx_dma(1, tx_buffer_pulse_count);

	avia_gt_ir_send_pulse(first_period_high, first_period_low, block);
	
	tx_buffer_pulse_count = 0;
	
	return 0;
}

int avia_gt_ir_send_pulse(u32 period_high, u32 period_low, u8 block)
{
	WAIT_IR_UNIT_READY(tx);

	tx_unit_busy = 1;
					
	if (avia_gt_chip(ENX)) {
		// Verify this	
		if (period_low != 0)
			enx_reg_16(MSPL) = USEC_TO_CWP(period_low) - 1;
		else
			enx_reg_16(MSPL) = USEC_TO_CWP(period_high) - 1;
			
		enx_reg_16(MSPR) = (1 << 10) | (USEC_TO_CWP(period_high + period_low) - 1);
	}
	else if (avia_gt_chip(GTX)) {
		// Verify this	
		if (period_low != 0)
			gtx_reg_16(MSPL) = USEC_TO_CWP(period_low) - 1;
		else
			gtx_reg_16(MSPL) = USEC_TO_CWP(period_high) - 1;
			
		gtx_reg_16(MSPR) = (1 << 10) | (USEC_TO_CWP(period_high + period_low) - 1);
	}

	return 0;
}

void avia_gt_ir_set_duty_cycle(u32 new_duty_cycle)
{
	duty_cycle = new_duty_cycle;

	avia_gt_reg_set(CWPH, WavePulseHigh, ((gt_info->ir_clk * duty_cycle) / (frequency * 100)) - 1);
}

void avia_gt_ir_set_frequency(u32 new_frequency)
{
	frequency = new_frequency;

	avia_gt_reg_set(CWP, CarrierWavePeriod, (gt_info->ir_clk / frequency) - 1);

	avia_gt_ir_set_duty_cycle(duty_cycle);
}

void avia_gt_ir_set_filter(u8 enable, u8 low, u8 high)
{
	avia_gt_reg_set(RFR, Filt_H, high);
	avia_gt_reg_set(RFR, Filt_L, low);
	avia_gt_reg_set(RTC, S, enable);
}

void avia_gt_ir_set_polarity(u8 polarity)
{
	avia_gt_reg_set(RFR, P, polarity);
}

// Given in microseconds
void avia_gt_ir_set_tick_period(u32 tick_period)
{
	avia_gt_reg_set(RTP, TickPeriod, (tick_period / HALFSYSCLOCK) - 1);
}

void avia_gt_ir_set_queue(unsigned int addr)
{
	avia_gt_reg_set(IRQA, Addr, addr >> 9);

	//rx_buffer = (sAviaGtRxIrPulse *)(gt_info->mem_addr + addr);
	tx_buffer = (sAviaGtTxIrPulse *)(&gt_info->mem_addr[addr + 256]);
}

/* f.ex. the samsung driver wants to roll its own, so the IRQs are only 
		requested when a client attaches */
int avia_gt_ir_register(struct ir_client *irc){
	int clientnr;
	if (!irc)
		return -EINVAL;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	if (down_interruptible(&ir_sem))
#else
	if (mutex_lock_interruptible(&ir_sem))
#endif
		return -ERESTARTSYS;

	if ((irc->flags & (clientlist[0].flags | clientlist[1].flags))||
			(clientlist[0].flags!=0 && clientlist[1].flags!=0)){
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
		up(&ir_sem);
#else
		mutex_unlock(&ir_sem);
#endif
		return -EUSERS;
	}

	if (irc->flags & AVIA_GT_IR_RX) {
		rx_buffer = (sAviaGtIrPulse*) vmalloc (RX_MAX * sizeof (sAviaGtIrPulse));
		if (!rx_buffer){
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
			up(&ir_sem);
#else
			mutex_unlock(&ir_sem);
#endif
			return -ENOMEM;
		}
		memset (rx_buffer, 0, RX_MAX * sizeof(sAviaGtIrPulse));
		if (avia_gt_alloc_irq(gt_info->irq_ir, avia_gt_ir_rx_irq)) {
			printk(KERN_ERR "avia_gt_ir: unable to get rx interrupt\n");
			kfree(rx_buffer);
			rx_buffer = NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
			up(&ir_sem);
#else
			mutex_unlock(&ir_sem);
#endif
			return -EIO;
		}
	}
	
	if (irc->flags & AVIA_GT_IR_TX) {
		if (avia_gt_alloc_irq(gt_info->irq_it, avia_gt_ir_tx_irq)) {
			printk(KERN_ERR "avia_gt_ir: unable to get tx interrupt\n");
			if (irc->flags & AVIA_GT_IR_RX)
				avia_gt_free_irq(gt_info->irq_ir);
			if (rx_buffer){
				vfree (rx_buffer);
				rx_buffer = NULL;
			}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
			up(&ir_sem);
#else
			mutex_unlock(&ir_sem);
#endif
			return -EIO;
		}
	}

	avia_gt_ir_reset(1);

	avia_gt_ir_set_tick_period(IR_TICK_LENGTH);
	avia_gt_ir_set_filter(0, 3, 5);
	avia_gt_ir_set_polarity(0);
	avia_gt_ir_set_queue(AVIA_GT_MEM_IR_OFFS);
	avia_gt_ir_set_frequency(frequency);

	if (!clientlist[0].flags)
		clientnr = 0;
	else
		clientnr = 1;

	clientlist[clientnr] = *irc;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	up(&ir_sem);
#else
	mutex_unlock(&ir_sem);
#endif
	return clientnr;
}

int avia_gt_ir_unregister(int ir_handle){
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	if (down_interruptible(&ir_sem))
#else
	if (mutex_lock_interruptible(&ir_sem))
#endif
		return -ERESTARTSYS;

	if ( (ir_handle>1) || (!clientlist[ir_handle].flags)){
		printk ("avia_gt_ir: invalid client handle");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
		up(&ir_sem);
#else
		mutex_unlock(&ir_sem);
#endif
		return -EIO;
	}

	if (clientlist[ir_handle].flags & AVIA_GT_IR_TX)
		avia_gt_free_irq(gt_info->irq_it);
	if (clientlist[ir_handle].flags & AVIA_GT_IR_RX){
		avia_gt_free_irq(gt_info->irq_ir);
		vfree(rx_buffer);
		rx_buffer = NULL;
	}
	avia_gt_ir_reset(0);

	clientlist[ir_handle].flags=0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	up(&ir_sem);
#else
	mutex_unlock(&ir_sem);
#endif
	return 0;
}

int avia_gt_ir_init(void)
{
	printk(KERN_INFO "avia_gt_ir: $Id: avia_gt_ir.c,v 1.30.4.6 2008/09/19 22:43:42 seife Exp $\n");

	do_gettimeofday(&last_timestamp);
	
	gt_info = avia_gt_get_info();

	if (!avia_gt_supported_chipset(gt_info))
		return -ENODEV;
	return 0;
}

void avia_gt_ir_exit(void)
{
}

#if defined(STANDALONE)
module_init(avia_gt_ir_init);
module_exit(avia_gt_ir_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX infrared rx/tx driver");
MODULE_LICENSE("GPL");
#endif

EXPORT_SYMBOL(avia_gt_ir_get_rx_buffer_read_position);
EXPORT_SYMBOL(avia_gt_ir_get_rx_buffer_write_position);
EXPORT_SYMBOL(avia_gt_ir_queue_pulse);
EXPORT_SYMBOL(avia_gt_ir_get_receive_wq);
EXPORT_SYMBOL(avia_gt_ir_receive_pulse);
EXPORT_SYMBOL(avia_gt_ir_send_buffer);
EXPORT_SYMBOL(avia_gt_ir_send_pulse);
EXPORT_SYMBOL(avia_gt_ir_set_duty_cycle);
EXPORT_SYMBOL(avia_gt_ir_set_frequency);
EXPORT_SYMBOL(avia_gt_ir_register);
EXPORT_SYMBOL(avia_gt_ir_unregister);
