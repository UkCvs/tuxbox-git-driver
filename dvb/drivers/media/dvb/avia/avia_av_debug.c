/*
 * $Id: avia_av_debug.c,v 1.1.2.1 2006/12/05 20:54:52 carjay Exp $
 *
 * AViA 500/600 debug (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2006 Carsten Juttner (carjay@gmx.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, 
 * as published by the Free Software Foundation;
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


#include <linux/module.h>

#include "avia_av.h"

#define CASESTRING(t) case(t): return (#t)
char *syncmode2string(unsigned int mode)
{
	switch(mode) {
	CASESTRING(AVIA_AV_SYNC_MODE_NONE);
	CASESTRING(AVIA_AV_SYNC_MODE_AUDIO);
	CASESTRING(AVIA_AV_SYNC_MODE_VIDEO);
	CASESTRING(AVIA_AV_SYNC_MODE_AV);
	default: return "unknown syncmode";
	}
}

char *playstate2string(unsigned int state)
{
	switch(state) {
	CASESTRING(AVIA_AV_PLAY_STATE_PAUSED);
	CASESTRING(AVIA_AV_PLAY_STATE_PLAYING);
	CASESTRING(AVIA_AV_PLAY_STATE_STOPPED);
	default: return "unknown playstate";
	}
}

char *streamtype2string(unsigned int type)
{
	switch(type) {
	CASESTRING(AVIA_AV_STREAM_TYPE_0);
	CASESTRING(AVIA_AV_STREAM_TYPE_PES);
	CASESTRING(AVIA_AV_STREAM_TYPE_ES);
	CASESTRING(AVIA_AV_STREAM_TYPE_SPTS);
	default: return "unknown stream type";
	}
}

char *aviacmd2string(unsigned int command)
{
	switch(command) {
	CASESTRING(Abort);
	CASESTRING(Digest);
	CASESTRING(Fade);
	CASESTRING(Freeze);
	CASESTRING(NewPlayMode);
	CASESTRING(Pause);
	CASESTRING(Reset);
	CASESTRING(Resume);
	CASESTRING(SelectStream);
	CASESTRING(SetFill);
	CASESTRING(CancelStill);
	CASESTRING(DigitalStill);
	CASESTRING(NewChannel);
	CASESTRING(Play);
	CASESTRING(SetWindow);
	CASESTRING(WindowClear);
	CASESTRING(SetStreamType);
	CASESTRING(SwitchOSDBuffer);
	CASESTRING(CancelTimeCode);
	CASESTRING(SetTimeCode1);
	CASESTRING(SetTimeCode);
	CASESTRING(GenerateTone);
	CASESTRING(OSDCopyData);
	CASESTRING(OSDCopyRegion);
	CASESTRING(OSDFillData);
	CASESTRING(OSDFillRegion);
	CASESTRING(OSDXORData);
	CASESTRING(OSDXORegion);
	CASESTRING(PCM_Mix);
	CASESTRING(PCM_MakeWaves);
	default: return "unknown command";
	}
}
