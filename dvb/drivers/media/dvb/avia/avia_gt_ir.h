/*
 *   avia_gt_ir.h - ir driver for AViA (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
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
 */

#ifndef AVIA_GT_IR_H
#define AVIA_GT_IR_H

extern void avia_gt_ir_set_duty_cycle(u32 new_duty_cycle);
extern void avia_gt_ir_set_frequency(u32 new_frequency);
extern void avia_gt_ir_send_pulse(unsigned short period_high, unsigned short period_low);
extern int avia_gt_ir_init(void);
extern void avia_gt_ir_exit(void);
	    
#endif
	    