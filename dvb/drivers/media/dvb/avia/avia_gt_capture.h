/*
 *   avia_gt_capture.h - capture driver for AViA (dbox-II-project)
 *
 *   Homepage: http://www.tuxbox.org
 *
 *   Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
 *   Copyright (C) 2003 Carsten Juttner (carjay@gmx.net)
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

#ifndef AVIA_GT_CAPTURE_H
#define AVIA_GT_CAPTURE_H

// software params (for world)
#define ENX_CAPT_UNSQUASHED		1
#define ENX_CAPT_STORE_INTERLACED	2

// (input/feedback)
struct avia_gt_capture_params {
	unsigned short input_x;		// left top starting position of capture rect
	unsigned short input_y;
	unsigned short input_height;	// heigth/width of capture rect
	unsigned short input_width;
	unsigned short captured_height;	// how to scale the capture rect (only 1:1 or downscale possible)
	unsigned short captured_width;
	unsigned char eNX_extras;	// eNX: ENX_CAPT_UNSQUASHED: (UYVY)
					//	ENX_CAPT_STORE_INTERLACED : store even/odd data interlaced (not like GTX)
};
// (output, dependent on internal settings)
struct avia_gt_capture_info {
	unsigned long drambufstart;	// start of capture buffer within DRAM
	unsigned long framesize;	// amount of space for one complete frame
	unsigned short bytesperline;
	unsigned long oddoffset;	// offset odd from even or 0 if no odd data is present
};

extern int avia_gt_capture_apply_params(struct avia_gt_capture_params *params, struct avia_gt_capture_info *info);
extern int avia_gt_capture_get_params(struct avia_gt_capture_params *params);
extern void avia_gt_capture_get_info(struct avia_gt_capture_info *info);
extern int avia_gt_capture_copybuffer (unsigned char *buffer, unsigned long count, char userspace);
extern int avia_gt_capture_start(void);
extern void avia_gt_capture_stop(void);

int avia_gt_capture_init(void);
void avia_gt_capture_exit(void);

#endif
