/*
 * $Id: dbox2_cam_napi.h,v 1.1.2.1 2005/01/31 03:04:12 carjay Exp $
 *
 * Copyright (C) 2002, 2003 by Andreas Oberritter <obi@tuxbox.org>
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

#ifdef __KERNEL__
#ifndef DBOX2_CAM_NAPI
#define DBOX2_CAM_NAPI
#ifndef STANDALONE
int cam_napi_init(void);
void cam_napi_exit(void);
#endif
#endif
#endif
