/*
 *   cxa2126.h - audio/video switch driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Gillem htoa@gmx.net
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
 *
 *   $Log: cxa2126.h,v $
 *   Revision 1.1  2001/01/23 00:16:36  gillem
 *   Initial revision
 *
 *
 *   $Revision: 1.1 $
 *
 */

/* READ MODE */

#define AVS_FVCR (3)
#define AVS_POR (1<<4)
#define AVS_ZCS (1<<5)

/* VS1 - TV Output */
#define AVS_TVOUT_DE1  0
#define AVS_TVOUT_DE2  1
#define AVS_TVOUT_VCR  2
#define AVS_TVOUT_RES1 3
#define AVS_TVOUT_DE3  4
#define AVS_TVOUT_DE4  5 /* TV */
#define AVS_TVOUT_RES2 6
#define AVS_TVOUT_VM1  7

/* VS2 - VCR Output */
#define AVS_VCROUT_DE1  0
#define AVS_VCROUT_DE2  1
#define AVS_VCROUT_VCR  2
#define AVS_VCROUT_RES1 3
#define AVS_VCROUT_DE3  4
#define AVS_VCROUT_DE4  5 /* TV */
#define AVS_VCROUT_RES2 6
#define AVS_VCROUT_VM3  7

/* FBLK */
#define AVS_FBLKOUT_NULL 0
#define AVS_FBLKOUT_5V	 1
#define AVS_FBLKOUT_IN1  2
#define AVS_FBLKOUT_IN2  3
#define AVS_FBLKOUT_RES1 4
#define AVS_FBLKOUT_0V_1 5
#define AVS_FBLKOUT_0V_2 6
#define AVS_FBLKOUT_0V_3 7

/* FNC */
#define AVS_FNCOUT_INTTV   0
#define AVS_FNCOUT_EXT169  1
#define AVS_FNCOUT_EXT43   2
#define AVS_FNCOUT_EXT43_1 3

/* AUDIO Output */
#define AVS_AUDOUT_V1   0
#define AVS_AUDOUT_V2   1
#define AVS_AUDOUT_RES1 2
#define AVS_AUDOUT_V3   3
#define AVS_AUDOUT_M1   4
#define AVS_AUDOUT_M2   5
#define AVS_AUDOUT_M3   6
#define AVS_AUDOUT_M4   7

/* Volume (course) */
#define AVS_VOLOUT_C00 0
#define AVS_VOLOUT_C08 1
#define AVS_VOLOUT_C16 2
#define AVS_VOLOUT_C24 3
#define AVS_VOLOUT_C32 4
#define AVS_VOLOUT_C40 5
#define AVS_VOLOUT_C48 6
#define AVS_VOLOUT_C56 7

/* Volume (fine) */
#define AVS_VOLOUT_F0 0
#define AVS_VOLOUT_F1 1
#define AVS_VOLOUT_F2 2
#define AVS_VOLOUT_F3 3
#define AVS_VOLOUT_F4 4
#define AVS_VOLOUT_F5 5
#define AVS_VOLOUT_F6 6
#define AVS_VOLOUT_F7 7

/* Mute */
#define AVS_MUTE_IM		0
#define AVS_MUTE_ZC		1
#define AVS_UNMUTE_IM	2
#define AVS_UNMUTE_ZC	3

/* IOCTL */
#define AVSIOSET   0x1000
#define AVSIOGET   0x2000

#define AVSIOSVSW1 (1|AVSIOSET)
#define AVSIOSVSW2 (2|AVSIOSET)
#define AVSIOSVSW3 (3|AVSIOSET)
#define AVSIOSASW1 (4|AVSIOSET)
#define AVSIOSASW2 (5|AVSIOSET)
#define AVSIOSASW3 (6|AVSIOSET)
#define AVSIOSVOL  (7|AVSIOSET)
#define AVSIOSMUTE (8|AVSIOSET)
#define AVSIOSFBLK (9|AVSIOSET)
#define AVSIOSFNC  (10|AVSIOSET)
#define AVSIOSZCD  (12|AVSIOSET)

#define AVSIOSLOG1 (13|AVSIOSET)

#define AVSIOGVSW1 (17|AVSIOGET)
#define AVSIOGVSW2 (18|AVSIOGET)
#define AVSIOGVSW3 (19|AVSIOGET)
#define AVSIOGASW1 (20|AVSIOGET)
#define AVSIOGASW2 (21|AVSIOGET)
#define AVSIOGASW3 (22|AVSIOGET)
#define AVSIOGVOL  (23|AVSIOGET)
#define AVSIOGMUTE (24|AVSIOGET)
#define AVSIOGFBLK (25|AVSIOGET)
#define AVSIOGFNC  (26|AVSIOGET)
#define AVSIOGZCD  (28|AVSIOGET)
#define AVSIOGLOG1 (29|AVSIOGET)

#define AVSIOGSTATUS (33|AVSIOGET)