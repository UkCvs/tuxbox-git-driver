#ifndef __GTXENX_H
#define __GTXENX_H

#include <dbox/enx.h>
#include <dbox/gtx.h>

extern __u8 gtxenx_chip;
extern __u8 *gtxenx_mem_base, *gtxenx_reg_base;
extern __u8 *gtxenx_physical_mem_base, *gtxenx_physical_reg_base;

extern int gtxenxcore_allocate_irq(int reg, int bit, void (*isr)(int, int));
extern void gtxenxcore_free_irq(int reg, int bit);

#endif
