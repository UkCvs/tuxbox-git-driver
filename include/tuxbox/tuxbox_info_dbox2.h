/*
 * tuxbox_info_dbox2.h - TuxBox hardware info - dbox2
 *
 * Copyright (C) 2003 Florian Schirmer <jolt@tuxbox.org>
 *                    Bastian Blank <waldi@tuxbox.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id: tuxbox_info_dbox2.h,v 1.1 2003/03/04 21:18:09 waldi Exp $
 */

#ifndef TUXBOX_INFO_DBOX2_H
#define TUXBOX_INFO_DBOX2_H

typedef enum tuxbox_dbox2_av
{
	TUXBOX_DBOX2_AV_GTX			= 1,
	TUXBOX_DBOX2_AV_ENX			= 2,
}
tuxbox_dbox2_av_t;

typedef enum tuxbox_dbox2_demod
{
	TUXBOX_DBOX2_DEMOD_VES1893		= 1,
	TUXBOX_DBOX2_DEMOD_VES1993		= 2,
	TUXBOX_DBOX2_DEMOD_VES1820		= 3,
	TUXBOX_DBOX2_DEMOD_TDA8044H		= 4,
	TUXBOX_DBOX2_DEMOD_AT76C651		= 5,
}
tuxbox_dbox2_demod_t;

typedef enum tuxbox_dbox2_mid
{
	TUXBOX_DBOX2_MID_NOKIA			= 1,
	TUXBOX_DBOX2_MID_PHILIPS		= 2,
	TUXBOX_DBOX2_MID_SAGEM			= 3,
}
tuxbox_dbox2_mid_t;

#endif
