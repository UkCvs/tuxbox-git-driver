/*
 * gtxenx-core.c: AViA GTX/eNX core driver (dbox-II-project)
 *
 * Copyright (C) 2000-2001 Felix "tmbinc" Domke <tmbinc@gmx.net>
 *               2000-2001 Florian "Jolt" Schirmer <jolt@tuxbox.org>
 *               2002 Bastian Blank <waldi@tuxbox.org>
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
 * $Id: gtxenx-core.c,v 1.1.2.1 2002/04/01 12:39:42 waldi Exp $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <asm/irq.h>
#include <asm/io.h>

#include <dbox/gtxenx.h>
#include <dbox/info.h>

/* ---------------------------------------------------------------------- */

__u8 gtxenx_chip;
__u8 *gtxenx_mem_base, *gtxenx_reg_base;
__u8 *gtxenx_physical_mem_base, *gtxenx_physical_reg_base;
__u32 gtxenx_mem_size, gtxenx_reg_size;

static void gtxenxcore_gtx_interrupt (int irq, void *dev, struct pt_regs * regs);
static void gtxenxcore_enx_interrupt (int irq, void *dev, struct pt_regs * regs);

/* ---------------------------------------------------------------------- */

int __init gtxenxcore_init (void)
{
	struct dbox_info_struct *info;
	__u16 val;

	dbox_get_info_ptr(&info);
	if (info->gtxID != -1) {
		gtxenx_chip = 1;
		printk (KERN_INFO "gtxenx-core: loading AViA GTX/eNX core driver, GTX mode\n");
	}
	else if (info->enxID != -1) {
		gtxenx_chip = 2;
		printk (KERN_INFO "gtxenx-core: loading AViA GTX/eNX core driver, eNX mode\n");
	}
	else {
		printk (KERN_ERR "gtxenx-core: unknown chip!\n");
		return -EINVAL;
	}

	if (gtxenx_chip == 1) {		/* GTX */
		gtxenx_physical_mem_base = (__u8 *) GTX_PHYSBASE;
		gtxenx_physical_reg_base = (__u8 *) GTX_PHYSBASE + 0x400000;
		gtxenx_mem_size = 0x200000;

		gtxenx_mem_base = (__u8 *) ioremap ((__u32) gtxenx_physical_mem_base, 0x403000);

		if (!gtxenx_mem_base) {
			printk (KERN_ERR "gtxenx-core: failed to remap gtx memory.\n");
			return -ENOMEM;
		}

		gtxenx_reg_base = gtxenx_mem_base + 0x400000;

					/* set complete reset */
		gtx_reg_16 (RR0) = 0xffff;
		gtx_reg_16 (RR1) = 0x00ff;

		udelay (500);

		gtx_reg_16 (RR0) &= ~(	/* Reset Register 0 */
			(1 << 10));	/* DRAM Memory Controller */

		memset (gtxenx_mem_base, 0, gtxenx_mem_size);

		val = gtx_reg_16 (CR0);
		printk (KERN_INFO "gtxenx-core: gtxID: %.2x\n", (val & 0xf000) >> 12);

		val &= ~(3 << 6);	/* Delay DTACK */
		val |= 1 << 6;
		val &= ~(1 << 5);	/* DAC Output Disable */
		val |= 1 << 3;		/* 16 Mbit DRAM Select */
		val &= ~(1 << 2);	/* Refresh Disable */

		gtx_reg_16 (CR0) = val;

		gtx_reg_16 (IPR0) = -1;	/* assign to HIRQ1 */
		gtx_reg_16 (IPR1) = -1;
		gtx_reg_16 (IPR2) = -1;
		gtx_reg_16 (IPR3) = -1;

		gtx_reg_16 (IMR0) = 0;	/* mask all interrupts */
		gtx_reg_16 (IMR1) = 0;
		gtx_reg_16 (IMR2) = 0;
		gtx_reg_16 (IMR3) = 0;

					/* clear all interrupts */
		gtx_reg_16 (ISR0) = 0xffff;
		gtx_reg_16 (ISR1) = 0xffff;
		gtx_reg_16 (ISR2) = 0xffff;
		gtx_reg_16 (ISR3) = 0xffff;

		gtx_reg_16 (RR0) &= ~(
			(1 << 4));	/* Interrupt Logic */

		if (request_8xxirq (SIU_IRQ1, gtxenxcore_gtx_interrupt, 0, "gtx", 0) != 0) {
			printk (KERN_ERR "gtxenx-core: Could not allocate GTX IRQ!");
			return -ENODEV;
		}

//		gtx_reg_32 (PCMA) = 1;	/* buffer disable */
//		gtx_reg_32 (PCMN) = 0x80808080;
					/* set volume for pcm and mpeg */
//		gtx_reg_16 (PCMC) = (3 << 14) |  (1 << 13) | (1 << 12) | (1 << 11) | 2 << 2 | 2;
//		gtx_reg_16 (PCMC) &= ~(0 << 6);

		/* enable teletext */
//		gtx_reg_16 (TTCR) |= (1 << 9);

	}
	else if (gtxenx_chip == 2) {	/* eNX */
		gtxenx_physical_mem_base = (__u8 *) ENX_MEM_BASE;
		gtxenx_physical_reg_base = (__u8 *) ENX_REG_BASE;
		gtxenx_mem_size = ENX_MEM_SIZE;

		gtxenx_mem_base = (__u8 *) ioremap ((__u32) gtxenx_physical_mem_base, gtxenx_mem_size);

		if (!gtxenx_mem_base) {
			printk (KERN_ERR "gtxenx-core: failed to remap enx memory.\n");
			return -ENOMEM;
		}

		gtxenx_reg_base = (__u8 *) ioremap ((__u32) gtxenx_physical_reg_base, ENX_REG_SIZE);

		if (!gtxenx_reg_base) {
			iounmap (gtxenx_mem_base);
			printk (KERN_ERR "gtxenx-core: failed to remap enx registers.\n");
			return -ENOMEM;
		}

		enx_reg_32 (RSTR0) = 0xfffcffff;
					/* set complete reset */
		enx_reg_32 (SCSC) = 0x00000000;
					/* Set sd-ram start address */
		udelay (500);

					/* Reset Register 0 */
		enx_reg_32 (RSTR0) &= ~(
			(1 << 12));	/* SDRAM Controller */

		udelay (500);

					/* Write memory configuration */
		enx_reg_32 (MC) = 0x00001011;
					/* XXX: don't know */
		//enx_reg_32n (0x88) |= 0x3E << 4;

		enx_reg_w(CFGR0) &= ~(
			(1 << 3) |	/* Teletext Clip Mode */
			(1 << 1) |	/* Audio Clip Mode */
			(1 << 0));	/* Video Clip Mode */

		udelay (500);

		memset (gtxenx_mem_base, 0, gtxenx_mem_size);

		printk (KERN_INFO "gtxenx-core: enxID: %.2x\n", (enx_reg_32 (CRR) & 0xff000000) >> 24);

		enx_reg_32 (EHIDR)=0;	/* External Host Interrupt */

					/* Interrupt Priority Register 4-5: assign to HIRQ1 */
		enx_reg_32 (IPR4) = 0x55555555;
		enx_reg_32 (IPR5) = 0x55555555;

   					/* Interrupt Status Register 0-5: clear all interrupts */
		enx_reg_16 (ISR0) = 0xfffe;
		enx_reg_16 (ISR1) = 0xfffe;
		enx_reg_16 (ISR2) = 0xfffe;
		enx_reg_16 (ISR3) = 0xfffe;
		enx_reg_16 (ISR4) = 0xfffe;
		enx_reg_16 (ISR5) = 0xfffe;

					/* Interrupt Mask Register 0-5: mask all interrupts */
		enx_reg_16 (IMR0) = 0xfffe;
		enx_reg_16 (IMR1) = 0xfffe;
		enx_reg_16 (IMR2) = 0xfffe;
		enx_reg_16 (IMR3) = 0xfffe;
		enx_reg_16 (IMR4) = 0xfffe;
		enx_reg_16 (IMR5) = 0xfffe;

		if (request_8xxirq (SIU_IRQ1, gtxenxcore_enx_interrupt, 0, "enx", 0) != 0) {
			printk (KERN_ERR "gtxenx-core: Could not allocate eNX IRQ!");
			return -ENODEV;
		}

//		enx_reg_32 (PCMN) = 0x80808080;
//		enx_reg_16 (PCMC) = (3 << 14) |  (1 << 13) | (1 << 12) | (1 << 11) | 2 << 2 | 2;
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

void gtxenxcore_cleanup (void)
{
	if (gtxenx_chip == 1) {		/* GTX */
		gtx_reg_16 (RR0) |= (1 << 4);

		gtx_reg_16 (IMR0) = 0;
		gtx_reg_16 (IMR1) = 0;
		gtx_reg_16 (IMR2) = 0;
		gtx_reg_16 (IMR3) = 0;

					/* clear all interrupts */
		gtx_reg_16 (ISR0) = 0xffff;
		gtx_reg_16 (ISR1) = 0xffff;
		gtx_reg_16 (ISR2) = 0xffff;
		gtx_reg_16 (ISR3) = 0xffff;

		free_irq (SIU_IRQ1, 0);

		gtx_reg_16 (CR0) = 0x0030;
		gtx_reg_16 (CR1) = 0x0000;

		gtx_reg_16 (RR0) = 0xFBFF;
		gtx_reg_16 (RR1) = 0x00FF;

		iounmap (gtxenx_mem_base);
	}
	else if (gtxenx_chip == 2) {	/* eNX */
		enx_reg_32 (IDR) = 1;

					/* mask all interrupts */
		enx_reg_16 (IMR0) = 0xfffe;
		enx_reg_16 (IMR1) = 0xfffe;
		enx_reg_16 (IMR2) = 0xfffe;
		enx_reg_16 (IMR3) = 0xfffe;
		enx_reg_16 (IMR4) = 0xfffe;
		enx_reg_16 (IMR5) = 0xfffe;

					/* clear all interrupts */
		enx_reg_16 (ISR0) = 0xfffe;
		enx_reg_16 (ISR1) = 0xfffe;
		enx_reg_16 (ISR2) = 0xfffe;
		enx_reg_16 (ISR3) = 0xfffe;
		enx_reg_16 (ISR4) = 0xfffe;
		enx_reg_16 (ISR5) = 0xfffe;

		enx_reg_32 (IPR4) = 0;	/* unassign */
		enx_reg_32 (IPR5) = 0;

		free_irq (SIU_IRQ1, 0);

		iounmap (gtxenx_mem_base);
		iounmap (gtxenx_reg_base);
	}
}

/* ---------------------------------------------------------------------- */

static void (*gtx_irq[64]) (int reg, int bit);
static int gtx_isr[4] = { GTX_REG_ISR0, GTX_REG_ISR1, GTX_REG_ISR2, GTX_REG_ISR3 };
static int gtx_imr[4] = { GTX_REG_IMR0, GTX_REG_IMR1, GTX_REG_IMR2, GTX_REG_IMR3 };

static void (*enx_irq[128])(int reg, int bit);
static int enx_isr[6] = { ENX_REG_ISR0, ENX_REG_ISR1, ENX_REG_ISR2, ENX_REG_ISR3, ENX_REG_ISR4, ENX_REG_ISR5, };
static int enx_imr[6] = { ENX_REG_IMR0, ENX_REG_IMR1, ENX_REG_IMR2, ENX_REG_IMR3, ENX_REG_IMR4, ENX_REG_IMR5, };

/* ---------------------------------------------------------------------- */

static void gtxenxcore_gtx_interrupt (int irq, void *dev, struct pt_regs * regs)
{
	int i, j;

	for (i = 0; i < 3; i++) {
		int rn = gtx_reg_16n (gtx_isr[i]);

		for (j = 0; j < 16; j++) {
			if (rn & (1 << j)) {
				int nr = i * 16 + j;

				if (gtx_irq[nr])
					gtx_irq[nr] (i, j);
				else {
					if (gtx_reg_16n (gtx_imr[i]) & (1 << j)) {
						printk (KERN_DEBUG "gtxenx-core: "
							 "masking unhandled gtx interrupt %d/%d\n",
							 i, j);
						gtx_reg_16n (gtx_imr[i]) &= ~(1 << j);
					}
				}
				gtx_reg_16n (gtx_isr[i]) |= 1 << j;
			}
		}
	}
}

/* ---------------------------------------------------------------------- */

static void gtxenxcore_enx_interrupt (int irq, void *dev, struct pt_regs * regs)
{
	int i, j;

	for (i = 0; i < 5; i++) {
		int rn = enx_reg_16n (enx_isr[i]);

		for (j = 0; j < 16; j++) {
			if (rn & (1 << j)) {
				int nr = i * 16 + j;

				if (enx_irq[nr])
					enx_irq[nr] (i, j);
				else {
					if (enx_reg_16n (enx_imr[i]) & (1 << j)) {
						printk (KERN_DEBUG "gtxenx-core: "
							 "masking unhandled gtx interrupt %d/%d\n",
							 i, j);
						enx_reg_16n (enx_imr[i]) &= ~(1 << j);
					}
				}
				enx_reg_16n (enx_isr[i]) |= 1 << j;
			}
		}
	}
}

/* ---------------------------------------------------------------------- */

int gtxenxcore_allocate_irq (int reg, int bit, void (*isr) (int, int))
{
	int nr = reg * 16 + bit;

	if (gtxenx_chip == 1) {		/* GTX */
		if (gtx_irq[nr]) {
			panic (KERN_ERR "gtxenx-core: FATAL: gtx irq already used.\n");
			return -ENODEV;
		}

		gtx_irq[nr] = isr;
		gtx_reg_16n (gtx_imr[reg]) |= 1 << bit;
	}
	else if (gtxenx_chip == 2) {		/* eNX */
		if (gtx_irq[nr]) {
			panic (KERN_ERR "gtxenx-core: FATAL: enx irq already used.\n");
			return -ENODEV;
		}

		enx_irq[nr] = isr;
		enx_reg_16n (enx_imr[reg]) |= 1 << bit;
	}
	else
		return -EINVAL;

	return 0;
}

/* ---------------------------------------------------------------------- */

void gtxenxcore_free_irq (int reg, int bit)
{
	if (gtxenx_chip == 1) {		/* GTX */
		gtx_reg_16n (gtx_imr[reg]) &= ~(1 << bit);
		gtx_irq[reg * 16 + bit] = 0;
	}
	else if (gtxenx_chip == 2) {		/* eNX */
		enx_reg_16n (enx_imr[reg]) &= ~(1 << bit);
		enx_irq[reg * 16 + bit] = 0;
	}
}

/* ---------------------------------------------------------------------- */

#ifdef MODULE
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>, Florian Schirmer <jolt@tuxbox.org>, Bastian Blank <waldi@tuxbox.org>");
MODULE_DESCRIPTION("AViA GTX/eNX core driver");

int init_module (void)
{
	return gtxenxcore_init ();
}

void cleanup_module (void)
{
	return gtxenxcore_cleanup ();
}
#endif

EXPORT_SYMBOL_GPL(gtxenx_chip);
EXPORT_SYMBOL_GPL(gtxenx_mem_base);
EXPORT_SYMBOL_GPL(gtxenx_reg_base);
EXPORT_SYMBOL_GPL(gtxenx_physical_mem_base);
EXPORT_SYMBOL_GPL(gtxenx_physical_reg_base);
EXPORT_SYMBOL_GPL(gtxenx_mem_size);
EXPORT_SYMBOL_GPL(gtxenx_reg_size);

EXPORT_SYMBOL_GPL(gtxenxcore_allocate_irq);
EXPORT_SYMBOL_GPL(gtxenxcore_free_irq);

