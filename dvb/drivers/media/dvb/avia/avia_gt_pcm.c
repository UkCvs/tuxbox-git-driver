/*
 * $Id: avia_gt_pcm.c,v 1.25 2003/07/24 01:59:22 homar Exp $
 *
 * AViA eNX/GTX pcm driver (dbox-II-project)
 *
 * Homepage: http://dbox2.elxsi.de
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
 * oundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/byteorder/swab.h>

#include "avia_gt.h"
#include "avia_gt_pcm.h"

DECLARE_WAIT_QUEUE_HEAD(pcm_wait);

typedef struct {

	struct list_head list;
	unsigned int offset;
	unsigned int sample_count;
	unsigned char queued;

} sPcmBuffer;

LIST_HEAD(pcm_busy_buffer_list);
LIST_HEAD(pcm_free_buffer_list);

static spinlock_t busy_buffer_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t free_buffer_lock = SPIN_LOCK_UNLOCKED;

static sAviaGtInfo *gt_info = NULL;
static unsigned char swab_samples = 0;
static sPcmBuffer pcm_buffer_array[AVIA_GT_PCM_BUFFER_COUNT];
static unsigned char swab_buffer[AVIA_GT_PCM_BUFFER_SIZE] = { 0 };

// Warning - result is _per_ channel
unsigned int avia_gt_pcm_calc_sample_count(unsigned int buffer_size)
{

	if (avia_gt_chip(ENX)) {

		if (enx_reg_s(PCMC)->W)
			buffer_size /= 2;

		if (enx_reg_s(PCMC)->C)
			buffer_size /= 2;

	} else if (avia_gt_chip(GTX)) {

		if (gtx_reg_s(PCMC)->W)
			buffer_size /= 2;

		if (gtx_reg_s(PCMC)->C)
			buffer_size /= 2;

	}

	return buffer_size;

}

// Warning - if stereo result is for _both_ channels
unsigned int avia_gt_pcm_calc_buffer_size(unsigned int sample_count)
{

	if (avia_gt_chip(ENX)) {

		if (enx_reg_s(PCMC)->W)
			sample_count *= 2;

		if (enx_reg_s(PCMC)->C)
			sample_count *= 2;

	} else if (avia_gt_chip(GTX)) {

		if (gtx_reg_s(PCMC)->W)
			sample_count *= 2;

		if (gtx_reg_s(PCMC)->C)
			sample_count *= 2;

	}

	return sample_count;

}

void avia_gt_pcm_queue_buffer(void)
{

	unsigned long  flags = 0;
	sPcmBuffer *pcm_buffer = NULL;
	struct list_head *ptr = NULL;

	if (avia_gt_chip(ENX)) {

		if (!enx_reg_s(PCMA)->W)
			return;

	} else if (avia_gt_chip(GTX)) {

		if (!gtx_reg_s(PCMA)->W)
			return;

	}

	spin_lock_irqsave(&busy_buffer_lock, flags);

	list_for_each(ptr, &pcm_busy_buffer_list) {

		pcm_buffer = list_entry(ptr, sPcmBuffer, list);

		if (!pcm_buffer->queued) {

			if (avia_gt_chip(ENX)) {

				enx_reg_set(PCMS, NSAMP, pcm_buffer->sample_count);
				enx_reg_set(PCMA, Addr, pcm_buffer->offset >> 1);
				enx_reg_set(PCMA, W, 0);

			} else if (avia_gt_chip(GTX)) {

				gtx_reg_set(PCMA, NSAMP, pcm_buffer->sample_count);
				gtx_reg_set(PCMA, Addr, pcm_buffer->offset >> 1);
				gtx_reg_set(PCMA, W, 0);

			}

			pcm_buffer->queued = 1;

			break;

		}

	}

	spin_unlock_irqrestore(&busy_buffer_lock, flags);

}

static void avia_gt_pcm_irq(unsigned short irq)
{

	unsigned long flags = 0;
	sPcmBuffer *pcm_buffer = NULL;
	//int i = 0;
	//struct list_head *ptr;

	spin_lock_irqsave(&busy_buffer_lock, flags);

/*
	if ((irq == ENX_IRQ_PCM_AD) || (irq == GTX_IRQ_PCM_AD))
		printk("X");

	list_for_each(ptr, &pcm_busy_buffer_list) {

		i++;

	}

	printk("%d ", i);
*/

	if (!list_empty(&pcm_busy_buffer_list)) {

		pcm_buffer = list_entry(pcm_busy_buffer_list.next, sPcmBuffer, list);
		list_del(&pcm_buffer->list);

		pcm_buffer->queued = 0;

		spin_lock_irqsave(&free_buffer_lock, flags);

		list_add_tail(&pcm_buffer->list, &pcm_free_buffer_list);

		spin_unlock_irqrestore(&free_buffer_lock, flags);

	}

	spin_unlock_irqrestore(&busy_buffer_lock, flags);

	avia_gt_pcm_queue_buffer();

	wake_up_interruptible(&pcm_wait);

}

unsigned int avia_gt_pcm_get_block_size(void)
{

	return avia_gt_pcm_calc_buffer_size(AVIA_GT_PCM_MAX_SAMPLES);

}

void avia_gt_pcm_reset(unsigned char reenable)
{

	if (avia_gt_chip(ENX)) {

		enx_reg_set(RSTR0, PCMA, 1);
		enx_reg_set(RSTR0, PCM, 1);

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_set(RR0, ACLK, 1);
		gtx_reg_set(RR0, PCM, 1);

	}

	if (reenable) {

		if (avia_gt_chip(ENX)) {

			enx_reg_set(RSTR0, PCM, 0);
			enx_reg_set(RSTR0, PCMA, 0);

		} else if (avia_gt_chip(GTX)) {

			gtx_reg_set(RR0, PCM, 0);
			gtx_reg_set(RR0, ACLK, 0);

		}

	}

}

void avia_gt_pcm_set_mpeg_attenuation(unsigned char left, unsigned char right)
{

	if (avia_gt_chip(ENX)) {

		enx_reg_set(PCMN, MPEGAL, left >> 1);
		enx_reg_set(PCMN, MPEGAR, right >> 1);

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_set(PCMN, MPEGAL, left >> 1);
		gtx_reg_set(PCMN, MPEGAR, right >> 1);

	}

}

void avia_gt_pcm_set_pcm_attenuation(unsigned char left, unsigned char right)
{

	if (avia_gt_chip(ENX)) {

		enx_reg_set(PCMN, PCMAL, left >> 1);
		enx_reg_set(PCMN, PCMAR, right >> 1);

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_set(PCMN, PCMAL, left >> 1);
		gtx_reg_set(PCMN, PCMAR, right >> 1);

	}

}

int avia_gt_pcm_set_rate(unsigned short rate)
{

	unsigned char divider_mode = 3;

	switch(rate) {

		case 48000:
		case 44100:

			divider_mode = 3;
		
		break;

		case 24000:
		case 22050:

			divider_mode = 2;
		
		break;

		case 12000:
		case 11025:

			divider_mode = 1;
		
		break;

		default:

			return -EINVAL;
			
		break;

	}

	if (avia_gt_chip(ENX))
		enx_reg_set(PCMC, R, divider_mode);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(PCMC, R, divider_mode);

	return 0;

}

int avia_gt_pcm_set_width(unsigned char width)
{

	if ((width == 8) || (width == 16)) {

		if (avia_gt_chip(ENX))
			enx_reg_set(PCMC, W, (width == 16));
		else if (avia_gt_chip(GTX))
			gtx_reg_set(PCMC, W, (width == 16));

	} else {

		return -EINVAL;

	}

	return 0;

}

int avia_gt_pcm_set_channels(unsigned char channels)
{

	if ((channels == 1) || (channels == 2)) {

		if (avia_gt_chip(ENX))
			enx_reg_set(PCMC, C, (channels == 2));
		else if (avia_gt_chip(GTX))
			gtx_reg_set(PCMC, C, (channels == 2));

	} else {

		return -EINVAL;

	}

	return 0;

}

int avia_gt_pcm_set_signed(unsigned char signed_samples)
{

	if ((signed_samples == 0) || (signed_samples == 1)) {

		if (avia_gt_chip(ENX))
			enx_reg_set(PCMC, S, (signed_samples == 1));
		else if (avia_gt_chip(GTX))
			gtx_reg_set(PCMC, S, (signed_samples == 1));

	} else {

		return -EINVAL;

	}

	return 0;

}

int avia_gt_pcm_set_endian(unsigned char be)
{

	if ((be == 0) || (be == 1))
		swab_samples = (be == 0);
	else
		return -EINVAL;

	return 0;

}

int avia_gt_pcm_play_buffer(void *buffer, unsigned int buffer_size, unsigned char block) {

	unsigned char bps_16 = 0;
	unsigned long flags = 0;
	sPcmBuffer *pcm_buffer = NULL;
	unsigned int sample_nr = 0;
	unsigned short *swab_dest = NULL;
	unsigned short *swab_src = NULL;
	unsigned int sample_count = 0;
	unsigned char stereo = 0;

	sample_count = avia_gt_pcm_calc_sample_count(buffer_size);

	if (sample_count > AVIA_GT_PCM_MAX_SAMPLES)
		sample_count = AVIA_GT_PCM_MAX_SAMPLES;

	if (avia_gt_chip(ENX)) {

		bps_16 = enx_reg_s(PCMC)->W;
		stereo = enx_reg_s(PCMC)->C;

	} else if (avia_gt_chip(GTX)) {

		bps_16 = gtx_reg_s(PCMC)->W;
		stereo = gtx_reg_s(PCMC)->C;

	}

	// If 8-bit mono then sample count has to be even
	if ((!bps_16) && (!stereo))
		sample_count &= ~1;

	while (list_empty(&pcm_free_buffer_list)) {

		if (block) {

			if (wait_event_interruptible(pcm_wait, !list_empty(&pcm_free_buffer_list)))
				return -ERESTARTSYS;

		} else {

			return -EWOULDBLOCK;

		}

	}

	spin_lock_irqsave(&free_buffer_lock, flags);

	pcm_buffer = list_entry(pcm_free_buffer_list.next, sPcmBuffer, list);
	list_del(&pcm_buffer->list);

	spin_unlock_irqrestore(&free_buffer_lock, flags);

	if ((bps_16) && (swab_samples)) {

		copy_from_user(swab_buffer, buffer, avia_gt_pcm_calc_buffer_size(sample_count));

		swab_dest = (unsigned short *)(gt_info->mem_addr + pcm_buffer->offset);
		swab_src = (unsigned short *)swab_buffer;

		for (sample_nr = 0; sample_nr < avia_gt_pcm_calc_buffer_size(sample_count) / 2; sample_nr++)
			swab_dest[sample_nr] = swab16(swab_src[sample_nr]);

	} else {

		copy_from_user(gt_info->mem_addr + pcm_buffer->offset, buffer, avia_gt_pcm_calc_buffer_size(sample_count));

	}

	pcm_buffer->sample_count = sample_count;

	spin_lock_irqsave(&busy_buffer_lock, flags);

	list_add_tail(&pcm_buffer->list, &pcm_busy_buffer_list);

	spin_unlock_irqrestore(&busy_buffer_lock, flags);

	avia_gt_pcm_queue_buffer();

	return avia_gt_pcm_calc_buffer_size(sample_count);

}

unsigned int avia_gt_pcm_poll(struct file *file, struct poll_table_struct *wait)
{
	poll_wait(file, &pcm_wait, wait);
	if (!list_empty(&pcm_free_buffer_list))
		return POLLOUT|POLLWRNORM;
	return 0;
}

void avia_gt_pcm_stop(void)
{
/*
	if (avia_gt_chip(ENX))
		enx_reg_set(PCMC, T, 1);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(PCMC, T, 1);
*/
	return;
}

int avia_gt_pcm_init(void)
{

	unsigned char buf_nr = 0;
	unsigned short irq_ad = 0;
	unsigned short irq_pf = 0;

	printk("avia_gt_pcm: $Id: avia_gt_pcm.c,v 1.25 2003/07/24 01:59:22 homar Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {

		printk("avia_gt_pcm: Unsupported chip type\n");

		return -EIO;

	}

	if (avia_gt_chip(ENX)) {

		irq_ad = ENX_IRQ_PCM_AD;
		irq_pf = ENX_IRQ_PCM_PF;

	} else if (avia_gt_chip(GTX)) {

		irq_ad = GTX_IRQ_PCM_AD;
		irq_pf = GTX_IRQ_PCM_PF;

	}

	if (avia_gt_alloc_irq(irq_ad, avia_gt_pcm_irq)) {

		printk("avia_gt_pcm: unable to get pcm-ad interrupt\n");

		return -EIO;

	}

	if (avia_gt_alloc_irq(irq_pf, avia_gt_pcm_irq)) {

		printk("avia_gt_pcm: unable to get pcm-pf interrupt\n");

		avia_gt_free_irq(irq_ad);

		return -EIO;

	}

	avia_gt_pcm_reset(1);

	for (buf_nr = 0; buf_nr < AVIA_GT_PCM_BUFFER_COUNT; buf_nr++) {

		pcm_buffer_array[buf_nr].offset = AVIA_GT_MEM_PCM_OFFS + (AVIA_GT_PCM_BUFFER_SIZE * buf_nr);
		pcm_buffer_array[buf_nr].queued = 0;

		list_add_tail(&pcm_buffer_array[buf_nr].list, &pcm_free_buffer_list);

	}

	// Use external clock from AViA 500/600
	if (avia_gt_chip(ENX))
		enx_reg_set(PCMC, I, 0);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(PCMC, I, 0);

	// Pass through mpeg samples
	avia_gt_pcm_set_mpeg_attenuation(0x80, 0x80);

	// Set a default mode
	avia_gt_pcm_set_rate(44100);
	avia_gt_pcm_set_width(16);
	avia_gt_pcm_set_channels(2);
	avia_gt_pcm_set_signed(1);
	avia_gt_pcm_set_endian(1);

	return 0;

}

void avia_gt_pcm_exit(void)
{

	if (avia_gt_chip(ENX)) {

		avia_gt_free_irq(ENX_IRQ_PCM_AD);
		avia_gt_free_irq(ENX_IRQ_PCM_PF);

	} else if (avia_gt_chip(GTX)) {

		avia_gt_free_irq(GTX_IRQ_PCM_AD);
		avia_gt_free_irq(GTX_IRQ_PCM_PF);

	}

	avia_gt_pcm_reset(0);

}

#if defined(STANDALONE)
module_init(avia_gt_pcm_init);
module_exit(avia_gt_pcm_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX PCM driver");
MODULE_LICENSE("GPL");
#endif

EXPORT_SYMBOL(avia_gt_pcm_play_buffer);
EXPORT_SYMBOL(avia_gt_pcm_stop);
EXPORT_SYMBOL(avia_gt_pcm_set_signed);
EXPORT_SYMBOL(avia_gt_pcm_set_endian);
EXPORT_SYMBOL(avia_gt_pcm_set_rate);
EXPORT_SYMBOL(avia_gt_pcm_set_width);
EXPORT_SYMBOL(avia_gt_pcm_set_channels);
EXPORT_SYMBOL(avia_gt_pcm_set_mpeg_attenuation);
EXPORT_SYMBOL(avia_gt_pcm_set_pcm_attenuation);
EXPORT_SYMBOL(avia_gt_pcm_get_block_size);
EXPORT_SYMBOL(avia_gt_pcm_poll);

