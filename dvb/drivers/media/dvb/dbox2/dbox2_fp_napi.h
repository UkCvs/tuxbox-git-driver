/*
 * $Id: dbox2_fp_napi.h,v 1.1.2.2 2005/02/02 02:28:51 carjay Exp $
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
#ifndef DBOX2_FP_NAPI_H
#define DBOX2_FP_NAPI_H
#include <dvb-core/dvb_frontend.h>
#include "dbox2_pll.h"

int dbox2_fp_napi_init(void);
void dbox2_fp_napi_exit(void);
int dbox2_fp_napi_qam_set_freq(struct pll_state *pll, struct dvb_frontend_parameters *p);
int dbox2_fp_napi_qpsk_set_freq(struct pll_state *pll, struct dvb_frontend_parameters *p);
int dbox2_fp_napi_get_sec_ops(struct dvb_frontend_ops *ops);

#endif /* DBOX2_FP_NAPI_H */
#endif /* __KERNEL__ */
