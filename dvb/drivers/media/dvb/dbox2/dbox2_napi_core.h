/*
 * $Id: dbox2_napi_core.h,v 1.1.2.1 2005/01/31 03:04:12 carjay Exp $
 *
 * Dbox2 DVB Adapter driver
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2005 Carsten Juttner <carjay@gmx.net>
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
#ifndef DBOX2_NAPI_CORE_H
#define DBOX2_NAPI_CORE_H
#include <dvb-core/dvbdev.h>
struct dvb_adapter *dbox2_napi_get_adapter(void);
#endif
#endif
