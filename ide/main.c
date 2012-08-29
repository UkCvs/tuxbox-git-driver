/*
 * $Id: main.c,v 1.4.2.9 2012/08/29 17:59:57 rhabarber1848 Exp $
 *
 * Copyright (C) 2006 Uli Tessel <utessel@gmx.de>
 * Linux 2.6 port: Copyright (C) 2006 Carsten Juttner <carjay@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; (version 2 of the License)
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
#include <linux/ioport.h>
#include <linux/ide.h>
#include <linux/version.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/irq.h>
#include <asm/8xx_immap.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
#include <asm/cpm1.h>
#else
#include <asm/commproc.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/device.h>
#include <linux/platform_device.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
#include <linux/delay.h>
#endif
#include <syslib/m8xx_wdt.h>

static uint idebase = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static struct platform_device *ide_dev;
#else
static int ideindex = -1;
#endif
static int irq6;

/* address-offsets of features in the CPLD 
  (we can't call them registers...) */
#define CPLD_READ_DATA          0x00000000
#define CPLD_READ_FIFO          0x00000A00
#define CPLD_READ_CTRL          0x00000C00

#define CPLD_WRITE_FIFO         0x00000980
#define CPLD_WRITE_FIFO_HIGH    0x00000900
#define CPLD_WRITE_FIFO_LOW     0x00000880
#define CPLD_WRITE_CTRL_TIMING  0x00000860
#define CPLD_WRITE_CTRL         0x00000840
#define CPLD_WRITE_TIMING       0x00000820
#define CPLD_WRITE_DATA         0x00000800

/* bits in the control word */
#define CPLD_CTRL_WRITING 0x20
#define CPLD_CTRL_ENABLE  0x40
#define CPLD_CTRL_REPEAT  0x80

/* helping macros to access the CPLD */
#define CPLD_OUT(offset, value) ( *(volatile uint*)(idebase+(offset)) = (value))
#define CPLD_IN(offset) ( *(volatile uint*)(idebase+(offset)))
#define CPLD_FIFO_LEVEL() (CPLD_IN( CPLD_READ_CTRL)>>28)

/* hidden(?) function of linux ide part */
extern void ide_probe_module(int);

/* assembler implementation of transfer loops */
extern void dboxide_insw_loop(uint ctrl_address,
			      uint data_address, void *dest, int count);

extern void dboxide_outsw_loop(uint ctrl_address,
			       uint data_address, void *src, int count);

/* enable/disable low level traces */
#undef TRACE_DBOXIDE

/* trace routines */
extern void dboxide_log_trace(unsigned int typ, unsigned int a, unsigned int b);
extern void dboxide_print_trace(void);

#define TRACE_LOG_INB   0x01
#define TRACE_LOG_INW   0x02
#define TRACE_LOG_INSW  0x03
#define TRACE_LOG_OUTB  0x04
#define TRACE_LOG_OUTW  0x05
#define TRACE_LOG_OUTSW 0x06

/* replace fixed printk strings with char * */
const char *modname = "dboxide:";
#define DBOXIDE_PRINTK(txt, a...) printk( "%s "txt, modname, ##a)

/* knwon DBox 2 IDE vendors  */
const char *dboxide_vendors[] = {
	"Unknown",
	"Gurgel",
	"DboxBaer or kpt.ahab/Stingray"
}; 

const char *dboxide_trace_msg[] = {
	NULL,
	"INB  ",
	"INW  ",
	"INSW ",
	"OUTB ",
	"OUTW ",
	"OUTSW",
};

#ifdef TRACE_DBOXIDE
/* debug/performance information */
static int dboxide_max_printk = 0;
/* max Num of performance traces */
#define DBOXIDE_MAX_PRINTK 1000 
#endif

/* some functions are not implemented and I don't expect we ever
   need them. But if one of them is called, we can work on that. */
#define NOT_IMPL(txt, a...) printk( "%s NOT IMPLEMENTED: "txt, modname, ##a )

/* whenever a something didn't work as expected: print everything
   that might be interesting for the developers what has happened */
void dboxide_problem(const char *msg)
{
	printk("%s %s\n", modname, msg);
	printk("CPLD Status is %08x\n", CPLD_IN(CPLD_READ_CTRL));
	dboxide_print_trace();
}

/* compat code for 2.6 kernel */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#define IDE_DELAY() ide_delay_50ms()
#else
#define IDE_DELAY() \
	do { \
		__set_current_state(TASK_UNINTERRUPTIBLE); \
		schedule_timeout(1+HZ/20); \
	} while (0);
#endif

#define WAIT_FOR_FIFO_EMPTY() wait_for_fifo_empty()
#define MAX_WAIT_FOR_FIFO_EMPTY  1000
static void wait_for_fifo_empty(void)
{
	int cnt = MAX_WAIT_FOR_FIFO_EMPTY;
	uint level;

	do {
		cnt--;
		level = CPLD_FIFO_LEVEL();
	} while ((level != 0) && (cnt > 0));

	if (cnt <= 0)
		dboxide_problem("fifo didn't get empty in time");
}

/*---------------------------------------------------------*/
/* These functions are called via function pointer by the  */
/* IDE Subsystem of the Linux Kernel                       */
/*---------------------------------------------------------*/

/* inb reads one byte from an IDE Register */
static u8 dboxide_inb(unsigned long port)
{
	int val;

	if (CPLD_FIFO_LEVEL() != 0)
		dboxide_problem("inb: fifo not empty?!\n");

	CPLD_OUT(CPLD_WRITE_CTRL, port);
	CPLD_OUT(CPLD_WRITE_CTRL, port | CPLD_CTRL_ENABLE);

	while (CPLD_FIFO_LEVEL() == 0) {
	};

	val = CPLD_IN(CPLD_READ_FIFO);

	val >>= 8;
	val &= 0xFF;
#ifdef TRACE_DBOXIDE
	dboxide_log_trace(TRACE_LOG_INB, port, val);
#endif
	return val;
}

/* inw reads one word from an IDE Register
   As only the data register has 16 bit, and that is read
   with insw, this function might never be called */
static u16 dboxide_inw(unsigned long port)
{
	int val;

	if (CPLD_FIFO_LEVEL() != 0)
		dboxide_problem("inw: fifo not empty?!");

	CPLD_OUT(CPLD_WRITE_CTRL, port);
	CPLD_OUT(CPLD_WRITE_CTRL, port | CPLD_CTRL_ENABLE);

	while (CPLD_FIFO_LEVEL() == 0) {
	};

	val = CPLD_IN(CPLD_READ_FIFO);

	val &= 0xFFFF;
#ifdef TRACE_DBOXIDE
	dboxide_log_trace(TRACE_LOG_INW, port, val);
#endif
	return val;
}

/* insw reads several words from an IDE register.
   Typically from the data register. This is the most important
   function to read data */
static void dboxide_insw(unsigned long port, void *addr, u32 count)
{
	uint *dest = addr;
	register uint a;
	register uint b;
#ifdef TRACE_DBOXIDE
	u32 busywait = 00;
	u32 numwords = count; 
#endif
	/* special for ATAPI: the kernel calls insw with count=1?! */
	if (count<4)
	{
		short * sdest = addr;
#ifdef TRACE_DBOXIDE
		printk("%s short insw %d\n", modname, (int)count);
#endif
		for (;count>0;count--)
		{
			*sdest++ = dboxide_inw( port ); 
		}
		return;
	}


	/* outcommented for better performance, errorchecking is not needed

	if (CPLD_FIFO_LEVEL() != 0)
		dboxide_problem("insw: fifo not empty?!");
	*/
#ifdef TRACE_DBOXIDE
	dboxide_log_trace(TRACE_LOG_INSW, port, count);
#endif

	/* activate reading to fifo with auto repeat */
	CPLD_OUT(CPLD_WRITE_CTRL, port);
	CPLD_OUT(CPLD_WRITE_CTRL, port | CPLD_CTRL_ENABLE | CPLD_CTRL_REPEAT);

	/* todo: replace the code below by an assembler implementation in this
	   routine 

	   jojo: did some tests using a simple busywait counter. It doesn't look
		like we need an assembler optimization, because we'd wait only
		faster. The current C code has already 65-70 busywaits per 
		sector, so assembler wouldn't accelerate much more.

	dboxide_insw_loop(idebase + CPLD_READ_CTRL, idebase + CPLD_READ_FIFO,
			  dest, count);
	*/

	{

		while (count > 16) {
			while (CPLD_FIFO_LEVEL() != 0xF) {
#ifdef TRACE_DBOXIDE
				busywait++;
#endif 
			};
			a = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			b = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			dest[0] = a;
			dest[1] = b;
			while (CPLD_FIFO_LEVEL() != 0xF) {
#ifdef TRACE_DBOXIDE
				busywait++; 
#endif
			};
			a = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			b = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			dest[2] = a;
			dest[3] = b;

			count -= 8;
			dest += 4;
		}

		while (count > 4) {
			while (CPLD_FIFO_LEVEL() != 0xF) {
#ifdef TRACE_DBOXIDE
				busywait++; 
#endif
			};
			a = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			b = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
			dest[0] = a;
			dest[1] = b;

			count -= 4;
			dest += 2;
		}

	}

	if (count != 4)
		printk ("%s oops: insw: something has gone wrong: count is %d\n", modname, count);

	/* wait until fifo is full = 4 Words */
	while (CPLD_FIFO_LEVEL() != 0xF) {
#ifdef TRACE_DBOXIDE
		busywait++; 
#endif
	};

	/* then stop reading from ide */
	CPLD_OUT(CPLD_WRITE_CTRL, port);

	/* and read the final 4 16 bit words */
	a = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
	b = CPLD_IN(CPLD_READ_FIFO);	/* read 2 16 bit words */
	dest[0] = a;
	dest[1] = b;

#ifdef TRACE_DBOXIDE
	if (dboxide_max_printk < DBOXIDE_MAX_PRINTK) {
		dboxide_max_printk++;
		printk("%s: insw: count=0x%lx, busywait=%ld\n", modname, numwords, busywait);
	}
#endif
}

/* insl reads several 32 bit words from an IDE register.
   The IDE Bus has only 16 bit words, but the CPLD always
   generates 32 Bit words from that, so the same routine
   as for 16 bit can be used. */
static void dboxide_insl(unsigned long port, void *addr, u32 count)
{
	dboxide_insw(port, addr, count * 2);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,20)
/* inl: read a single 32 bit word from IDE. 
   As there are no 32 Bit IDE registers, this function
   is not implemented. */
static u32 dboxide_inl(unsigned long port)
{
	NOT_IMPL("inl %lx\n", port);
	return 0xFFFFFFFF;
}
#endif

/* outb: write a single byte to an IDE register */
static void dboxide_outb(u8 value, unsigned long port)
{
	if (CPLD_FIFO_LEVEL() != 0)
		dboxide_problem("outb: fifo not empty?!");
#ifdef TRACE_DBOXIDE
	dboxide_log_trace(TRACE_LOG_OUTB, port, value);
#endif
	CPLD_OUT(CPLD_WRITE_CTRL, port | CPLD_CTRL_ENABLE | CPLD_CTRL_WRITING);
	CPLD_OUT(CPLD_WRITE_FIFO_LOW, value << 8);

	WAIT_FOR_FIFO_EMPTY();
}

/* outbsync: write a single byte to an IDE register, typically
   an IDE command. */
/* todo: the sync is related to interrupts */
static void dboxide_outbsync(ide_drive_t * drive, u8 value, unsigned long port)
{
	/* todo: use a different cycle-length here?! */
	dboxide_outb(value, port);
}

/* outw: write a single 16-bit word to an IDE register. 
   As only the data register hast 16 bit, and that is written 
   with outsw, this function is not implemented. */
static void dboxide_outw(u16 value, unsigned long port)
{
	NOT_IMPL("outw %lx %x\n", port, value);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,20)
/* outl: write a single 32-bit word to an IDE register. 
   As there are no 32 Bit IDE registers, this function 
   is not implemented. */
static void dboxide_outl(u32 value, unsigned long port)
{
	NOT_IMPL("outl %lx %lx\n", port, port);
}
#endif

/* write several 16 bit words to an IDE register, typically to the
   data register. 
   This is the most important function to write data */
static void dboxide_outsw(unsigned long port, void *addr, u32 count)
{
	uint *src = addr;

//	u32 busywait = 0;
//	u32 numwords = count; 

/*
	if (CPLD_FIFO_LEVEL() != 0)
		dboxide_problem("outsw: fifo not empty?!");
*/
#ifdef TRACE_DBOXIDE
	dboxide_log_trace(TRACE_LOG_OUTSW, port, count);
#endif
	/* activate writing to fifo with auto repeat */
	CPLD_OUT(CPLD_WRITE_CTRL,
		 port | CPLD_CTRL_WRITING | CPLD_CTRL_ENABLE |
		 CPLD_CTRL_REPEAT);

/*
	dboxide_outsw_loop(idebase + CPLD_READ_CTRL, idebase + CPLD_READ_FIFO,
			   src, count);
*/

	{
		register int a;
		register int b;

		while (count > 0) {
			a = *src++;
			b = *src++;
			while (CPLD_FIFO_LEVEL() != 0) {
#ifdef TRACE_DBOXIDE
				busywait++;
#endif
			};
			CPLD_OUT(CPLD_WRITE_FIFO, a);
			CPLD_OUT(CPLD_WRITE_FIFO, b);

			count -= 4;
		}

		if (count == 2) {
			a = *src++; 
			while (CPLD_FIFO_LEVEL()!=0) { 
#ifdef TRACE_DBOXIDE
				busywait++;
#endif
			};
			CPLD_OUT( CPLD_WRITE_FIFO, a );
		}
	}

	WAIT_FOR_FIFO_EMPTY();

	/* and stop writing to IDE */
	CPLD_OUT(CPLD_WRITE_CTRL, port | CPLD_CTRL_WRITING);

#ifdef TRACE_DBOXIDE
	if (dboxide_max_printk < DBOXIDE_MAX_PRINTK) {
		dboxide_max_printk++;
		printk("%s: outsw: count=0x%lx, busywait=%ld\n", modname, numwords, busywait);
	}
#endif
}

/* outsl writes several 32 bit words to an IDE register. 
   The IDE Bus has only 16 bit words, but the CPLD always 
   generates 32 Bit words from that, so the same routine
   as for 16 bit can be used. */
static void dboxide_outsl(unsigned long port, void *addr, u32 count)
{
	dboxide_outsw(port, addr, count * 2);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,25)
static void dboxide_tf_load(ide_drive_t *drive, ide_task_t *task)
{
ide_hwif_t *hwif = drive->hwif;
	struct ide_io_ports *io_ports = &hwif->io_ports;
	struct ide_taskfile *tf = &task->tf;
	u8 HIHI = (task->tf_flags & IDE_TFLAG_LBA48) ? 0xE0 : 0xEF;

	if (task->tf_flags & IDE_TFLAG_FLAGGED)
		HIHI = 0xFF;

	//ide_set_irq(drive, 1);

	if (task->tf_flags & IDE_TFLAG_OUT_DATA)
		dboxide_outw((tf->hob_data << 8) | tf->data, io_ports->data_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_HOB_FEATURE)
		dboxide_outb(tf->hob_feature, io_ports->feature_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_HOB_NSECT)
		dboxide_outb(tf->hob_nsect, io_ports->nsect_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_HOB_LBAL)
		dboxide_outb(tf->hob_lbal, io_ports->lbal_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_HOB_LBAM)
		dboxide_outb(tf->hob_lbam, io_ports->lbam_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_HOB_LBAH)
		dboxide_outb(tf->hob_lbah, io_ports->lbah_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_FEATURE)
		dboxide_outb(tf->feature, io_ports->feature_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_NSECT)
		dboxide_outb(tf->nsect, io_ports->nsect_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_LBAL)
		dboxide_outb(tf->lbal, io_ports->lbal_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_LBAM)
		dboxide_outb(tf->lbam, io_ports->lbam_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_LBAH)
		dboxide_outb(tf->lbah, io_ports->lbah_addr);
	if (task->tf_flags & IDE_TFLAG_OUT_DEVICE)
		dboxide_outb((tf->device & HIHI) | drive->select.all,
		     io_ports->device_addr);
#ifdef TRACE_DBOXIDE
	/* show what happened so far... */
	dboxide_problem("dboxide_tf_load");
#endif
}

static void dboxide_tf_read(ide_drive_t *drive, ide_task_t *task)
{
	ide_hwif_t *hwif = drive->hwif;
	struct ide_io_ports *io_ports = &hwif->io_ports;
	struct ide_taskfile *tf = &task->tf;

	if (task->tf_flags & IDE_TFLAG_IN_DATA) {
		u16 data = dboxide_inw(io_ports->data_addr);

		tf->data = data & 0xff;
		tf->hob_data = (data >> 8) & 0xff;
	}

	/* be sure we're looking at the low order bits */
	dboxide_outb(drive->ctl & ~0x80, io_ports->ctl_addr);

	if (task->tf_flags & IDE_TFLAG_IN_NSECT)
		tf->nsect  = dboxide_inb(io_ports->nsect_addr);
	if (task->tf_flags & IDE_TFLAG_IN_LBAL)
		tf->lbal   = dboxide_inb(io_ports->lbal_addr);
	if (task->tf_flags & IDE_TFLAG_IN_LBAM)
		tf->lbam   = dboxide_inb(io_ports->lbam_addr);
	if (task->tf_flags & IDE_TFLAG_IN_LBAH)
		tf->lbah   = dboxide_inb(io_ports->lbah_addr);
	if (task->tf_flags & IDE_TFLAG_IN_DEVICE)
		tf->device = dboxide_inb(io_ports->device_addr);

	if (task->tf_flags & IDE_TFLAG_LBA48) {
		dboxide_outb(drive->ctl | 0x80, io_ports->ctl_addr);

		if (task->tf_flags & IDE_TFLAG_IN_HOB_FEATURE)
			tf->hob_feature = dboxide_inb(io_ports->feature_addr);
		if (task->tf_flags & IDE_TFLAG_IN_HOB_NSECT)
			tf->hob_nsect   = dboxide_inb(io_ports->nsect_addr);
		if (task->tf_flags & IDE_TFLAG_IN_HOB_LBAL)
			tf->hob_lbal    = dboxide_inb(io_ports->lbal_addr);
		if (task->tf_flags & IDE_TFLAG_IN_HOB_LBAM)
			tf->hob_lbam    = dboxide_inb(io_ports->lbam_addr);
		if (task->tf_flags & IDE_TFLAG_IN_HOB_LBAH)
			tf->hob_lbah    = dboxide_inb(io_ports->lbah_addr);
	}
#ifdef TRACE_DBOXIDE
	/* show what happened so far... */
	dboxide_problem("dboxide_tf_read");
#endif

}


static void dboxide_input_data(ide_drive_t *drive, struct request *rq,
			   void *buf, unsigned int len)
{
	unsigned long data_addr = drive->hwif->io_ports.data_addr;

	len++;

	if (drive->io_32bit) {
		dboxide_insl(data_addr, buf, len / 4);

		if ((len & 3) >= 2)
			dboxide_insw(data_addr, (u8 *)buf + (len & ~3), 1);
	} else
		dboxide_insw(data_addr, buf, len / 2);
#ifdef TRACE_DBOXIDE
	/* show what happened so far... */
	dboxide_problem("dboxide_input_data %8x %8x", data_addr, len);
#endif

}

static void dboxide_output_data(ide_drive_t *drive,  struct request *rq,
			    void *buf, unsigned int len)
{
	unsigned long data_addr = drive->hwif->io_ports.data_addr;

	len++;

	if (drive->io_32bit) {
		dboxide_outsl(data_addr, buf, len / 4);

		if ((len & 3) >= 2)
			dboxide_outsw(data_addr, (u8 *)buf + (len & ~3), 1);
	} else
		dboxide_outsw(data_addr, buf, len / 2);
#ifdef TRACE_DBOXIDE
	/* show what happened so far... */
	dboxide_problem("dboxide_output_data %8x %8x", data_addr, len);
#endif

}


#endif

/*---------------------------------------------------------*/
/* acknowledge the interrupt? */
/*---------------------------------------------------------*/

int dboxide_ack_intr(ide_hwif_t * hwif)
{
	printk("%s ack irq\n", modname);
	return 1;
}

/*---------------------------------------------------------*/
/* some other functions that might be important, but it    */
/* also works without them                                 */
/*---------------------------------------------------------*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
void dboxide_tuneproc(ide_drive_t * drive, u8 pio)
{
	printk("%s tuneproc called: %d\n", modname, pio);
}
#endif

/*---------------------------------------------------------*/
/* end of functions called via function pointer */
/*---------------------------------------------------------*/

static int configure_interrupt(void)
{
	immap_t *immap = (immap_t *) IMAP_ADDR;

	/* configure Port C, Pin 15 so, that it creates interrupts 
	   only on the falling edge. (That it will create interrupts
	   at all is done by the Kernel itself) */

	immap->im_ioport.iop_pcint |= 0x0001;
	/* As this routine is the only one that needs to know about
	   wich interrupt is used in this code, it returns the number
	   so it can be given to the kernel */


	return CPM_IRQ_OFFSET + ( irq6 ? -4 : CPMVEC_PIO_PC15);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static void reset_function_pointer(ide_hwif_t *hwif)
{
	int i;
	printk("%s reset_function_pointer called\n", modname);
	for (i = 0; i < ARRAY_SIZE(hwif->hw.io_ports); i++) {
		uint addr;

		addr = hwif->hw.io_ports[i];
		if ((addr > idebase) && (addr <= idebase + 0x100))
			hwif->hw.io_ports[i] = addr - idebase;
		addr = hwif->io_ports[i];
		if ((addr > idebase) && (addr <= idebase + 0x100))
			hwif->io_ports[i] = addr - idebase;
	}
}
#endif

/* set the function pointer in the kernel structur to our
   functions */
static void set_access_functions(ide_hwif_t * hwif)
{
#ifdef TRACE_DBOXIDE
	printk("%s set_access_functions called\n", modname);
#endif
	hwif->mmio = 2;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	hwif->OUTB = dboxide_outb;
	hwif->OUTBSYNC = dboxide_outbsync;
	hwif->OUTW = dboxide_outw;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,20)
	hwif->OUTL = dboxide_outl;
#endif
	hwif->OUTSW = dboxide_outsw;
	hwif->OUTSL = dboxide_outsl;
	hwif->INB = dboxide_inb;
	hwif->INW = dboxide_inw;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,20)
	hwif->INL = dboxide_inl;
#endif
	hwif->INSW = dboxide_insw;
	hwif->INSL = dboxide_insl;
#else
	hwif->INB         = dboxide_inb;
	hwif->OUTB        = dboxide_outb;
	hwif->OUTBSYNC    = dboxide_outbsync;
	hwif->tf_load     = dboxide_tf_load;
	hwif->tf_read     = dboxide_tf_read;
	hwif->input_data  = dboxide_input_data;
	hwif->output_data = dboxide_output_data;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
	hwif->tuneproc = dboxide_tuneproc;
#endif

#if 0
	hwif->ack_intr = dboxide_ack_intr;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/* now, after setting the function pointer, the port info is not
	   an address anymore, so remove the idebase again */
	reset_function_pointer(hwif);
#endif

}

/* common function to setup the io_ports. "idebase" is necessary
   for the 2.4 kernel since the first time the ports are
   used as addresses which is not what we really want so the
   initial address is different for 2.4 and 2.6. */
static void init_hw_struct( ide_hwif_t *hwif, unsigned long idebase)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
	hw_regs_t *hw;

	hwif->tuneproc = NULL;
#endif
	hwif->chipset = ide_unknown;
	hwif->mate = NULL;
	hwif->channel = 0;

	strncpy (hwif->name, "DBox2 IDE", sizeof(hwif->name));

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
	hw = &hwif->hw;
	memset(hw, 0, sizeof(hw_regs_t));

	hw->io_ports[IDE_DATA_OFFSET] 		= idebase + 0x0010;
	hw->io_ports[IDE_ERROR_OFFSET] 		= idebase + 0x0011;
	hw->io_ports[IDE_NSECTOR_OFFSET] 	= idebase + 0x0012;
	hw->io_ports[IDE_SECTOR_OFFSET] 	= idebase + 0x0013;
	hw->io_ports[IDE_LCYL_OFFSET] 		= idebase + 0x0014;
	hw->io_ports[IDE_HCYL_OFFSET] 		= idebase + 0x0015;
	hw->io_ports[IDE_SELECT_OFFSET] 	= idebase + 0x0016;
	hw->io_ports[IDE_STATUS_OFFSET] 	= idebase + 0x0017;

	hw->io_ports[IDE_CONTROL_OFFSET] 	= idebase + 0x004E;
	hw->io_ports[IDE_IRQ_OFFSET] 		= idebase + 0x004E;

	hwif->irq = hw->irq = configure_interrupt();

	memcpy(hwif->io_ports, hw->io_ports, sizeof(hw->io_ports));
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)

	hwif->io_ports[IDE_DATA_OFFSET] 	= idebase + 0x0010;
	hwif->io_ports[IDE_ERROR_OFFSET] 	= idebase + 0x0011;
	hwif->io_ports[IDE_NSECTOR_OFFSET] 	= idebase + 0x0012;
	hwif->io_ports[IDE_SECTOR_OFFSET] 	= idebase + 0x0013;
	hwif->io_ports[IDE_LCYL_OFFSET] 	= idebase + 0x0014;
	hwif->io_ports[IDE_HCYL_OFFSET] 	= idebase + 0x0015;
	hwif->io_ports[IDE_SELECT_OFFSET] 	= idebase + 0x0016;
	hwif->io_ports[IDE_STATUS_OFFSET] 	= idebase + 0x0017;

	hwif->io_ports[IDE_CONTROL_OFFSET] 	= idebase + 0x004E;
	hwif->io_ports[IDE_IRQ_OFFSET] 	  	= idebase + 0x004E;

	hwif->irq = configure_interrupt();
#else
	hwif->io_ports.data_addr 	= idebase + 0x0010;
	hwif->io_ports.error_addr 	= idebase + 0x0011;
	hwif->io_ports.nsect_addr 	= idebase + 0x0012;
	hwif->io_ports.lbal_addr  	= idebase + 0x0013;
	hwif->io_ports.lbam_addr 	= idebase + 0x0014;
	hwif->io_ports.lbah_addr 	= idebase + 0x0015;
	hwif->io_ports.device_addr 	= idebase + 0x0016;
	hwif->io_ports.status_addr 	= idebase + 0x0017;

	hwif->io_ports.ctl_addr 	= idebase + 0x004E;
	hwif->io_ports.irq_addr	  	= idebase + 0x004E;
	hwif->irq = configure_interrupt();
#endif

#endif
}

/* the CPLD is connected to CS2, which should be inactive.
   if not there might be something using that hardware and
   we don't want to disturb that */
static int activate_cs2(void)
{
	immap_t *immap = (immap_t *) IMAP_ADDR;
	memctl8xx_t *memctl = &immap->im_memctl;
	uint br2 = memctl->memc_br2;

	if (br2 & 0x1) {
		printk("%s cs2 already activated\n", modname);
		return 0;
	}

	if (br2 != 0x02000080) {
		printk("%s cs2: unexpected value for br2: %08x\n", modname, br2);
		return 0;
	}

	br2 |= 0x1;

	printk("%s activating cs2\n", modname );
	memctl->memc_br2 = br2;

	return 1;
}

/* Deactivate CS2 when driver is not loaded */
static int deactivate_cs2(void)
{
	immap_t *immap = (immap_t *) IMAP_ADDR;
	memctl8xx_t *memctl = &immap->im_memctl;
	uint br2 = memctl->memc_br2;

	if (br2 != 0x02000081) {
		printk("%s cs2 configuration unexpected: %08x\n", modname, br2);
		return 0;
	}

	printk("%s deactivating cs2\n", modname);
	br2 &= ~1;
	memctl->memc_br2 = br2;

	return 1;
}

/*read out id-code, this value can only get on first read!*/
static unsigned int read_if_idcode(void) 
{
	static unsigned int idcode;
	static int alreadyread = 0;
	if (!alreadyread) {
		idcode = CPLD_IN(CPLD_READ_FIFO);
		alreadyread = 1;
	}
	return idcode;
}

static int write_if_idcodeback(unsigned int idcode) 
{
	CPLD_OUT(CPLD_WRITE_FIFO, idcode);
}

static int ide_software_reset_drives(void) 
{
        int i, j, tmp;

	for (i = 0; i<10; i++) {
		dboxide_outb(0x0E, 0x4E		/*IDE_CONTROL_REG*/);
		udelay(10);
		dboxide_outb(0x0A, 0x4E		/*IDE_CONTROL_REG*/);
		udelay(10);
		for (j = 0; j < 200; j++) {
			tmp = dboxide_inb(0x17	/*IDE_STATUS_REG*/);
			if (!(tmp & 0x80))
				break;
			IDE_DELAY();
	        }
        	if (tmp & 0x80)
			printk("%s timeout, drive is still busy\n", modname);
		tmp = dboxide_inb(0x11 		/*IDE_ERROR_REG*/);

        	if (tmp == 1) {
			printk("%s sreset succeeded\n", modname);
			return 1;
		}
	}
	printk("%s sreset failed, status: 0x%04x\n", modname, tmp);
	return 0;
}

/* detect_cpld: Check that the CPLD really works */
static int detect_cpld(void)
{
	int i;
	int vendor_idx = 0;	/* index into vendor table */

	uint check, back, idcode;
	uint patterns[2] = { 0xCAFEFEED, 0xBEEFC0DE };

	/* This detection code not only checks that there is a CPLD,
	   but also that it does work more or less as expected.  */

	/*read out identifynumber*/
	idcode = read_if_idcode();

	/* first perform a walking bit test via data register:
	   this checks that there is a data register and
	   that the data bus is correctly connected */

	for (i = 0; i < 31; i++) {
		/* only one bit is 1 */
		check = 1 << i;
		CPLD_OUT(CPLD_WRITE_DATA, check);
		back = CPLD_IN(CPLD_READ_DATA);
		if (check != back) {
			printk
			    ("%s probing DBox2 IDE CPLD: walking bit test failed: %08x != %08x\n", modname, 
			     check, back);
			return 0;
		}

		/* only one bit is 0 */
		check = ~check;
		CPLD_OUT(CPLD_WRITE_DATA, check);
		back = CPLD_IN(CPLD_READ_DATA);
		if (check != back) {
			printk
			    ("%s probing DBox2 IDE CPLD: walking bit test failed: %08x != %08x\n", modname,
			     check, back);
			return 0;
		}
	}

	/* second: check ctrl register.*/
	check = 0x0012001F; /* Activate PIO Mode 4 timing and remove IDE Reset */

	CPLD_OUT(CPLD_WRITE_CTRL_TIMING, check);
	back = CPLD_IN(CPLD_READ_CTRL);
	if ((back & check) != check) {
		printk
		    ("%s probing DBox2 IDE CPLD: ctrl register not valid: %08x != %08x\n", modname,
		     check, back & check);
		return 0;
	}

	/* Now test the fifo:
	   If there is still data inside, read it out to clear it */
	for (i = 3; (i > 0) && ((back & 0xF0000000) != 0); i--) {
		CPLD_IN(CPLD_READ_FIFO);
		back = CPLD_IN(CPLD_READ_CTRL);
	}

	if (i == 0) {
		printk
		    ("%s fifo seems to have data but clearing did not succeed\n", modname);
		return 0;
	}

	/* then write two long words to the fifo */
	CPLD_OUT(CPLD_WRITE_FIFO, patterns[0]);
	CPLD_OUT(CPLD_WRITE_FIFO, patterns[1]);

	/* and read them back */
	back = CPLD_IN(CPLD_READ_FIFO);
	if (back != patterns[0]) {
		printk("%s fifo did not store first test pattern\n", modname);
		return 0;
	}
	back = CPLD_IN(CPLD_READ_FIFO);
	if (back != patterns[1]) {
		printk("%s fifo did not store second test pattern\n", modname);
		return 0;
	}

	/* now the fifo must be empty again */
	back = CPLD_IN(CPLD_READ_CTRL);
	if ((back & 0xF0000000) != 0) {
		printk("%s fifo not empty after test\n", modname);
		return 0;
	}

	/* Clean up: clear bits in fifo */
	check = 0;
	CPLD_OUT(CPLD_WRITE_FIFO, check);
	back = CPLD_IN(CPLD_READ_FIFO);
	if (back != check) {
		printk("%s final fifo clear did not work: %x!=%x\n", modname, 
			back, check);
		return 0;
	}

	/* CPLD is valid!
	   Hopefully the IDE part will also work:
	   A test for that part is not implemented, but the kernel
	   will probe for drives etc, so this will check a lot
	 */

	switch (idcode) {
		case 0:
		case 0x50505050:
			vendor_idx = 1;		/* Gurgel */
			break;
		case 0x556c6954:
			vendor_idx = 2;		/* DboxBaer or kpt.ahab/Stingray */
			break;
	}
	printk("%s IDE-Interface detected, %08x, Vendor: %s\n", modname, 
		idcode, dboxide_vendors[vendor_idx]);


	/* before going releasing IDE Reset, wait some time... */
	for (i = 0; i < 10; i++)
		IDE_DELAY();

	/*Reset Drives via IDE-Device-Control-Register (SRST)*/
	ide_software_reset_drives();

	/* finally set all bits in data register, so nothing
	   useful is read when the CPLD is accessed by the
	   original inb/w/l routines */
	CPLD_OUT(CPLD_WRITE_DATA, 0xFFFFFFFF);

	return 1;
}

/* map_memory: we know the physical address of our chip. 
   But the kernel has to give us a virtual address. */
static void map_memory(void)
{
	unsigned long mem_addr = 0x02000000;
	unsigned long mem_size = 0x00001000;
	/* ioremap also activates the guard bit in the mmu, 
	   so the MPC8xx core does not do speculative reads
	   to these addresses
	 */

	idebase = (uint) ioremap(mem_addr, mem_size);
}

/* unmap_memory: we will not use these virtual addresses anymore */
static void unmap_memory(void)
{
	if (idebase) {
		iounmap((uint *) idebase);
		idebase = 0;
	}
}

static int setup_cpld(void)
{
	if (activate_cs2() == 0)
		return -ENODEV;

	map_memory();

	if (idebase == 0) {
		printk(KERN_ERR "%s address space of DBox2 IDE CPLD not mapped to kernel address space\n", modname);
		return -ENOMEM;
	}

	printk(KERN_INFO "%s address space of DBox2 IDE CPLD is at: 0x%08x\n", modname, idebase);

	if (detect_cpld() == 0) {
		printk(KERN_ERR "%s not a valid DBox2 IDE CPLD detected\n", modname);
		unmap_memory();
		return -ENODEV;
	}
	
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static void dboxide_register(void)
{
	ide_hwif_t *hwif = NULL;
	ide_hwif_t tmp_hwif;	/* we only use the hw_ports stucture */

	/*
	   I think this is a hack, but it works...

	   the io_port values are not really used as addresses by
	   this driver and, of course, they will not really work if
	   used like that.
	   "Not really" because the kernel might use the default
	   inb/etc. routines if the detect module is already loaded.
	   To avoid problems with that I use addresses where I know
	   what happens, because these addresses access the CPLD:

	   Writing to these addresses does nothing, and reading will
	   return the data register, which is (should be) at this
	   time FFFFFFFF  (see cpld_detect).

	   The result is that the detection of a disk will fail when
	   it is done with the original kernel functions.

	   A clean order is to load the ide-detect module after
	   loading this module!
	 */
	 
	/* set up with "fake" ioport addresses (that do not fault) */
	init_hw_struct(&tmp_hwif, idebase);

	/* we register the ports and get back a pointer to
	   the hwif that we are supposed to use */
	ideindex = ide_register_hw(&tmp_hwif.hw, &hwif);

	if (hwif != NULL) {
		if (ideindex == -1) {
			/* registering failed? This is not wrong because for the
			   kernel there is no drive on this controller because
			   wrong routines were used to check that.
			   Or the kernel didn't check at all.
			   But to unregister this driver, we will need this
			   index. */

			ideindex = hwif - ide_hwifs;
		}

		/* now change the IO Access functions and use the
		   real values for the IDE Ports */
		set_access_functions(hwif);

		SELECT_DRIVE(&hwif->drives[0]);

		/* finally: probe again: this time with my routines,
		   so this time the detection will not fail (if there
		   is a drive connected) */
		ide_probe_module(1);

	} else {
		printk("%s no hwif was given\n", modname);
	}
}

/* dboxide_scan: this is called by the IDE part of the kernel via
   a function pointer when the kernel thinks it is time to check
   this ide controller. So here the real activation of the CPLD
   is done (using the functions above) */
static void dboxide_scan(void)
{
	/*  
	   todo: If u-boot uses the IDE CPLD, then it should remove the CS2 activation 
	   or it should tell us somehow, so we can skip the detection...
	 */

	/* check BR2 register that CS2 is enabled.  if so, then something has activated it. 
	   Maybe there is RAM or a different hardware?  Don't try to use the CPLD then.  */
	int ret;
	
	ret = setup_cpld();
	if (ret)
		return;
	
	dboxide_register();
}

/* dbox_ide_init is called when the module is loaded */
static int __init dboxide_init(void)
{
	/* register driver will call the scan function above, maybe immediately 
	   when we are a module, or later when it thinks it is time to do so */
	printk(KERN_INFO
	       "%s $Id: main.c,v 1.4.2.9 2012/08/29 17:59:57 rhabarber1848 Exp $\n", modname);

	ide_register_driver(dboxide_scan);

	return 0;
}

/* dbox_ide_exit is called when the module is unloaded */
static void __exit dboxide_exit(void)
{
	if (idebase != 0) {
		CPLD_OUT(CPLD_WRITE_CTRL_TIMING, 0x00FF0007);
		write_if_idcodeback(read_if_idcode());
	}

	idebase = 0;

	if (ideindex != -1)
		ide_unregister(ideindex);

	unmap_memory();
	deactivate_cs2();

	printk("dboxide: driver unloaded\n");
}
#else
static int dbox2_ide_probe(struct platform_device *pdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
	u8 idx[4] = { 0xff, 0xff, 0xff, 0xff };
#endif
	ide_hwif_t *hwif = &ide_hwifs[pdev->id];

	init_hw_struct(hwif, 0);
	set_access_functions(hwif);

	hwif->drives[pdev->id].hwif = hwif;
	SELECT_DRIVE(&hwif->drives[pdev->id]);

	/* tell kernel to the interface
		(the default is not to probe) */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	hwif->noprobe = 0;
#else
	hwif->drives[0].noprobe=0;
	hwif->drives[1].noprobe=0;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
	probe_hwif_init(hwif);
#else
	idx[0] = hwif->index;
	ide_device_add(idx, NULL);
#endif

	platform_set_drvdata(pdev, hwif);
	
	return 0;
}

static int dbox2_ide_remove(struct platform_device *pdev)
{
	ide_hwif_t *hwif = platform_get_drvdata(pdev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
	ide_unregister(hwif - ide_hwifs);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	ide_unregister(hwif - ide_hwifs, 0, 0);
#else	
	ide_unregister(hwif);
#endif
#endif
	
	return 0;
}

static struct platform_driver dbox2_ide_driver = {
	.driver = {
		.name = "dbox2_ide"
	},
	.probe 	= dbox2_ide_probe,
	.remove = dbox2_ide_remove
};

/* dbox_ide_init is called when the module is loaded */
static int __init dboxide_init(void) {
	int ret;

	printk(KERN_INFO
	       "%s $Id: main.c,v 1.4.2.9 2012/08/29 17:59:57 rhabarber1848 Exp $\n", modname);

	ret = setup_cpld();
	if (ret < 0)
		return ret;

	/* register the driver */
	ret = platform_driver_register(&dbox2_ide_driver);
	if (ret < 0)
		return ret;
	
	/* "insert" the corresponding device to trigger the probe */
	ide_dev = platform_device_register_simple("dbox2_ide", 0, NULL, 0);
	if (IS_ERR(ide_dev)) {
		platform_driver_unregister(&dbox2_ide_driver);
		return PTR_ERR(ide_dev);
	}
	
	return 0;
}

/* dbox_ide_exit is called when the module is unloaded */
static void __exit dboxide_exit(void)
{
	if (idebase != 0) {
		CPLD_OUT(CPLD_WRITE_CTRL_TIMING, 0x00FF0007);
		write_if_idcodeback(read_if_idcode());
	}
	if (ide_dev)
		platform_device_unregister(ide_dev);

	platform_driver_unregister(&dbox2_ide_driver);

	unmap_memory();
	deactivate_cs2();

	printk("%s driver unloaded\n", modname);
}
#endif

module_init(dboxide_init);
module_exit(dboxide_exit);

module_param(irq6,int,0);
MODULE_AUTHOR("Uli Tessel <utessel@gmx.de>");
MODULE_DESCRIPTION("DBox2 IDE CPLD Interface driver");
MODULE_LICENSE("GPL");
