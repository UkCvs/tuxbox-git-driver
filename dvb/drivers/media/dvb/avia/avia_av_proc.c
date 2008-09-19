/*
 * $Id: avia_av_proc.c,v 1.14.2.5 2008/09/19 22:43:42 seife Exp $
 *
 * AViA 500/600 proc driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2002 Florian Schirmer <jolt@tuxbox.org>
 * Copyright (C) 2003 Andreas Oberritter <obi@tuxbox.org>
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
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/version.h>

#include "avia_av.h"
#include "avia_av_proc.h"

static u32 *dram_copy;

static int avia_av_proc_read_debug(char *buf, char **start, off_t offset, int len, int *eof, void *private)
{
	int nr = 0;
	nr = sprintf(buf, "Debug:\n");
	nr += sprintf(buf + nr, "PROC_STATE: %d\n",avia_av_dram_read(PROC_STATE));
	nr += sprintf(buf + nr, "MRC_ID: 0x%02x\n",avia_av_dram_read(MRC_ID));
	nr += sprintf(buf + nr, "MRC_STATUS: %d\n",avia_av_dram_read(MRC_STATUS));
	nr += sprintf(buf + nr, "INT_STATUS: %d\n",avia_av_dram_read(INT_STATUS));
	nr += sprintf(buf + nr, "BUFF_INT_SRC: %d\n",avia_av_dram_read(BUFF_INT_SRC));
	nr += sprintf(buf + nr, "UND_INT_SRC: %d\n",avia_av_dram_read(UND_INT_SRC));
	nr += sprintf(buf + nr, "ERR_INT_SRC: %d\n",avia_av_dram_read(ERR_INT_SRC));
	nr += sprintf(buf + nr, "VIDEO_EMPTINESS: 0x%x\n",avia_av_dram_read(VIDEO_EMPTINESS));
	nr += sprintf(buf + nr, "AUDIO_EMPTINESS: 0x%x\n",avia_av_dram_read(AUDIO_EMPTINESS));
	nr += sprintf(buf + nr, "N_SYS_ERRORS: %d\n",avia_av_dram_read(N_SYS_ERRORS));
	nr += sprintf(buf + nr, "N_VID_ERRORS: %d\n",avia_av_dram_read(N_VID_ERRORS));
	nr += sprintf(buf + nr, "N_AUD_ERRORS: %d\n",avia_av_dram_read(N_AUD_ERRORS));
	nr += sprintf(buf + nr, "N_VID_DECODED: %d\n",avia_av_dram_read(N_DECODED));
	nr += sprintf(buf + nr, "N_AUD_DECODED: %d\n",avia_av_dram_read(N_AUD_DECODED));
	nr += sprintf(buf + nr, "VSYNC_HEARTBEAT: 0x%06x\n",avia_av_dram_read(VSYNC_HEARTBEAT));
	nr += sprintf(buf + nr, "ML_HEARTBEAT: 0x%06x\n",avia_av_dram_read(ML_HEARTBEAT));
	return nr;
}

static int avia_av_proc_read_bitstream_settings(char *buf, char **start, off_t offset, int len, int *eof, void *private)
{
	int nr = 0;
	int audiotype;
	long mpegheader=0;

	nr = sprintf(buf, "Bitstream Settings:\n");

	nr += sprintf(buf + nr, "H_SIZE:  %d\n", avia_av_dram_read(H_SIZE) & 0xFFFF);
	nr += sprintf(buf + nr, "V_SIZE:  %d\n", avia_av_dram_read(V_SIZE) & 0xFFFF);
	nr += sprintf(buf + nr, "A_RATIO: %d\n", avia_av_dram_read(ASPECT_RATIO) & 0xFFFF);
	nr += sprintf(buf + nr, "F_RATE:  %d\n", avia_av_dram_read(FRAME_RATE) & 0xFFFF);
	nr += sprintf(buf + nr, "B_RATE:  %d\n", avia_av_dram_read(BIT_RATE) & 0xFFFF);
	nr += sprintf(buf + nr, "VB_SIZE: %d\n", avia_av_dram_read(VBV_SIZE) & 0xFFFF);
	audiotype=avia_av_dram_read(AUDIO_TYPE)&0xFFFF;
	nr += sprintf(buf + nr, "A_TYPE:  %d\n",audiotype);
	if (audiotype==3) mpegheader=(((avia_av_dram_read(MPEG_AUDIO_HEADER1) & 0xFFFF))<<16)|((avia_av_dram_read(MPEG_AUDIO_HEADER2))&0xFFFF);
	nr += sprintf(buf + nr, "MPEG_AUDIO_HEADER: 0x%04lx\n",mpegheader);
	nr += sprintf(buf + nr, "MR_PIC_PTS: 0x%04x\n", avia_av_dram_read(MR_PIC_PTS));
	nr += sprintf(buf + nr, "MR_PIC_STC: 0x%04x\n", avia_av_dram_read(MR_PIC_STC));
	nr += sprintf(buf + nr, "MR_AUD_PTS: 0x%04x\n", avia_av_dram_read(MR_AUD_PTS));
	nr += sprintf(buf + nr, "MR_AUD_STC: 0x%04x\n", avia_av_dram_read(MR_AUD_STC));

	return nr;
}

static int avia_av_proc_read_dram(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int n;

	/* copy dram to buffer on first read */
	if (off == 0) {
		if (!dram_copy)
			dram_copy = vmalloc(0x200000);
		if (!dram_copy)
			return -ENOMEM;
		for (n = 0; n < 512 * 1024; n++)
			dram_copy[n] = avia_av_dram_read(n << 2);
	}
	else if (!dram_copy) {
		return -ESPIPE;
	}

	n = 0x200000;

	if (off >= n)
		n = 0;

	if (n > count)
		n = count;
	else
		*eof = 1;

	if (n) {
		memcpy(page, &((u8*)dram_copy)[off], n);
		*start = page;
	}

	if (*eof) {
		vfree(dram_copy);
		dram_copy = NULL;
	}

	return n;
}

int avia_av_proc_init(void)
{
	struct proc_dir_entry *proc_bus_avia;
	struct proc_dir_entry *proc_bus_avia_dram;
	struct proc_dir_entry *proc_bus_avia_debug;

	printk("avia_av_proc: $Id: avia_av_proc.c,v 1.14.2.5 2008/09/19 22:43:42 seife Exp $\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	if (!proc_bus) {
		printk("avia_av_proc: /proc/bus does not exist");
		return -ENOENT;
	}

	proc_bus_avia = create_proc_read_entry("bitstream", 0, proc_bus, &avia_av_proc_read_bitstream_settings, NULL);
#else
	proc_bus_avia = create_proc_read_entry("bus/bitstream", 0, NULL, &avia_av_proc_read_bitstream_settings, NULL);
#endif

	if (!proc_bus_avia) {
		printk("avia_av_proc: could not create /proc/bus/bitstream");
		return -ENOENT;
	}
	proc_bus_avia->owner = THIS_MODULE;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	proc_bus_avia_dram = create_proc_read_entry("avia_dram", 0, proc_bus, &avia_av_proc_read_dram, NULL);
#else
	proc_bus_avia_dram = create_proc_read_entry("bus/avia_dram", 0, NULL, &avia_av_proc_read_dram, NULL);
#endif
	if (!proc_bus_avia_dram) {
		printk("avia_av_proc: could not create /proc/bus/avia_dram");
		return -ENOENT;
	}
	proc_bus_avia_dram->owner = THIS_MODULE;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	proc_bus_avia_debug = create_proc_read_entry("avia_debug", 0, proc_bus, &avia_av_proc_read_debug, NULL);
#else
	proc_bus_avia_debug = create_proc_read_entry("bus/avia_debug", 0, NULL, &avia_av_proc_read_debug, NULL);
#endif
	if (!proc_bus_avia_debug) {
		printk("avia_av_proc: could not create /proc/bus/avia_debug");
		return -ENOENT;
	}
	proc_bus_avia_debug->owner = THIS_MODULE;

	return 0;
}

void avia_av_proc_exit(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	remove_proc_entry("avia_dram", proc_bus);
	remove_proc_entry("bitstream", proc_bus);
	remove_proc_entry("avia_debug", proc_bus);
#else
	remove_proc_entry("bus/avia_dram", NULL);
	remove_proc_entry("bus/bitstream", NULL);
	remove_proc_entry("bus/avia_debug", NULL);
#endif
}

#if defined(STANDALONE)
module_init(avia_av_proc_init);
module_exit(avia_av_proc_exit);
EXPORT_SYMBOL(avia_av_proc_init);
EXPORT_SYMBOL(avia_av_proc_exit);
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>, Andreas Oberritter <obi@tuxbox.org>");
MODULE_DESCRIPTION("AViA 500/600 proc interface");
MODULE_LICENSE("GPL");
#endif
