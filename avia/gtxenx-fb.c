/*
 * gtxenx-fb.c: AViA GTX/eNX framebuffer driver (dbox-II-project)
 *
 * Copyright (C) 2000-2001 Felix "tmbinc" Domke <tmbinc@gmx.net>
 *               2002 Bastian Blank <waldi@tuxbox.org>
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
 * $Id: gtxenx-fb.c,v 1.1.2.1 2002/04/01 12:39:42 waldi Exp $
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/console.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>

#include <dbox/gtxenx.h>
#include <dbox/gtxenx-fb.h>

struct gtxenxfb_info {
	struct fb_info_gen gen;

	__u32 offset, videobase, physical_videobase;

	__u8 blendlevel0;
	__u8 blendlevel1;
	__u32 delta_xpos;
	__u32 delta_ypos;
};

struct gtxenxfb_par {
	__u16 xres;
	__u16 yres;
	__u16 bits_per_pixel;
	__u16 stride;
	unsigned lowres : 1;		/* 0: 720 or 640 pixels per line, 1: 360 or 320 pixels per line */
	unsigned interlaced : 1;	/* 0: 240 or 288 lines non-interlaced, 1: 480 or 576 lines interlaced */
	unsigned square : 1;		/* 0: 720 or 360 non-square, 1: 640 or 320 square */
	__u8 blendlevel0;
	__u8 blendlevel1;
	__u32 delta_xpos;
	__u32 delta_ypos;
};

static struct gtxenxfb_info fb_info;
static struct gtxenxfb_par current_par;
static int current_par_valid = 0;
static struct display disp;

static struct fb_var_screeninfo default_var;

static int gtxenxfb_encode_var (struct fb_var_screeninfo *var, const void *par,
			      struct fb_info_gen *info);
static void gtxenxfb_get_par (void *par, struct fb_info_gen *info);

#ifdef FBCON_HAS_CFB4
static u16 fbcon_cfb4_cmap[16];
#endif
#ifdef FBCON_HAS_CFB8
static u16 fbcon_cfb8_cmap[16];
#endif
#ifdef FBCON_HAS_CFB16
static u16 fbcon_cfb16_cmap[16];
#endif

extern unsigned short fbcon_cols, fbcon_rows;
extern unsigned short fbcon_cols_offset, fbcon_rows_offset;

static inline __u32 gtx_encode_blendlevel (__u32 value, unsigned number, unsigned long level);
static inline __u16 enx_encode_blendlevel (__u16 value, unsigned number, unsigned long level);



/* ------------------- chipset specific functions -------------------------- */


/*
 *  This function should detect the current video mode settings and store
 *  it as the default video mode
 */
static void gtxenxfb_detect (void)
{
	struct gtxenxfb_par par;

	gtxenxfb_get_par (&par, NULL);
	gtxenxfb_encode_var (&default_var, &par, NULL);
}

/*
 *  This function should fill in the 'fix' structure based on the values
 *  in the `par' structure.
 */
static int gtxenxfb_encode_fix (struct fb_fix_screeninfo *fix, const void *_par,
			      struct fb_info_gen *info)
{
	struct gtxenxfb_par *par = (struct gtxenxfb_par *) _par;

	strcpy (fix->id, "AViA GTX/eNX");
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;
	        
	if (par->bits_per_pixel != 16)
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
	else
		fix->visual = FB_VISUAL_TRUECOLOR;

	fix->line_length = par->stride;
	fix->smem_start = fb_info.physical_videobase;
	fix->smem_len = 1024 * 1024;
	fix->mmio_start = fb_info.physical_videobase;
	fix->mmio_len = 0x410000;

	fix->xpanstep = 0;
	fix->ypanstep = 0;
	fix->ywrapstep = 0;

	fix->accel = 0;

	return 0;
}

/*
 *  Get the video params out of 'var'. If a value doesn't fit, round it up,
 *  if it's too big, return -EINVAL.
 */
static int gtxenxfb_decode_var (const struct fb_var_screeninfo *var, void *_par,
			      struct fb_info_gen *info)
{
	struct gtxenxfb_par *par = (struct gtxenxfb_par *) _par;

	memset (par, 0, sizeof (struct gtxenxfb_par));

	if (var->bits_per_pixel <= 4)
		par->bits_per_pixel = 4;
	else if (var->bits_per_pixel <= 8)
		par->bits_per_pixel = 8;
	else if (var->bits_per_pixel <= 16)
		par->bits_per_pixel = 16;
	else
		return -EINVAL;

	if (var->xres <= 320) {
		par->xres = 320;
		par->lowres = 1;
		par->square = 1;
	}
	else if (var->xres <= 360) {
		par->xres = 360;
		par->lowres = 1;
	}
	else if (var->xres <= 640) {
		par->xres = 640;
		par->square = 1;
	}
	else if (var->xres <= 720)
		par->xres = 720;
	else
		return -EINVAL;

	if (var->yres <= 240 && par->square == 1)
		par->yres = 240;
	else if (var->yres <= 288 && par->square == 0)
		par->yres = 288;
	else if (var->yres <= 480 && par->square == 1) {
		par->yres = 480;
		par->interlaced = 1;
	}
	else if (var->yres <= 576 && par->square == 0) {
		par->yres = 576;
		par->interlaced = 1;
	}
	else
		return -EINVAL;

	switch (par->bits_per_pixel) {
		case  4: par->stride = par->xres / 2; break;
		case  8: par->stride = par->xres;     break;
		case 16: par->stride = par->xres * 2; break;
	}

	if (current_par_valid) {
		par->blendlevel0 = current_par.blendlevel0;
		par->blendlevel1 = current_par.blendlevel1;
		par->delta_xpos = current_par.delta_xpos;
		par->delta_ypos = current_par.delta_ypos;
	}
	else {
		par->blendlevel0 = fb_info.blendlevel0;
		par->blendlevel1 = fb_info.blendlevel1;
		par->delta_xpos = fb_info.delta_xpos;
		par->delta_ypos = fb_info.delta_ypos;
	}

	return 0;
}

/*
 *  Fill the 'var' structure based on the values in 'par'.
 */
static int gtxenxfb_encode_var (struct fb_var_screeninfo *var, const void *_par,
			      struct fb_info_gen *info)
{
	struct gtxenxfb_par *par = (struct gtxenxfb_par *) _par;

	memset (var, 0, sizeof (struct fb_var_screeninfo));

	var->xres = var->xres_virtual = par->xres;
	var->yres = var->yres_virtual = par->yres;

	var->bits_per_pixel = par->bits_per_pixel;
	var->red.offset    = 10;
	var->green.offset  =  5;
	var->blue.offset   =  0;
	var->transp.offset = 15;
	var->red.length = var->green.length = var->blue.length = 5;
	var->transp.length = 1;
	var->red.msb_right = var->green.msb_right = var->blue.msb_right = var->transp.msb_right = 0;

	var->activate = FB_ACTIVATE_NOW;
	var->height = var->width = -1;

	var->pixclock = 20000;
	var->left_margin = 64;	/* don't know what whis is but i think it's correct */
	var->right_margin = 64;
	var->upper_margin = 32;
	var->lower_margin = 32;
	var->hsync_len = 64;
	var->vsync_len = 2;
	if (par->lowres)
		var->vmode = FB_VMODE_DOUBLE;
	if (par->interlaced)
		var->vmode |= FB_VMODE_INTERLACED;

	return 0;
}

/*
 *  Fill the hardware's 'par' structure.
 */
static void gtxenxfb_get_par (void *_par, struct fb_info_gen *info)
{
	struct gtxenxfb_par *par = (struct gtxenxfb_par *) _par;

	if (current_par_valid)
		*par = current_par;
	else {
		memset (par, 0, sizeof (struct gtxenxfb_par));

		par->xres = 720;
		par->yres = 576;
		par->bits_per_pixel = 16;
		par->interlaced = 1;
		par->stride = 720 * 2;
		par->blendlevel0 = fb_info.blendlevel0;
		par->blendlevel1 = fb_info.blendlevel1;
		par->delta_xpos = fb_info.delta_xpos;
		par->delta_ypos = fb_info.delta_ypos;
	}
}

/*
 *  Set the hardware according to 'par'.
 */
static void gtxenxfb_set_par (const void *_par, struct fb_info_gen *info)
{
	register __u32 val, temp;
	struct gtxenxfb_par *par = (struct gtxenxfb_par *) _par;

	current_par = *par;
	current_par_valid = 1;

	if (gtxenx_chip == 1) {		/* GTX */
		/* graphics register */

		switch (par->bits_per_pixel) {
			case 4:  val = 1<<30; break;
			case 8:  val = 2<<30; break;
			case 16: val = 3<<30; break;
		}

		if (par->lowres) val |= 1<<29;
		if (!par->interlaced) val |= 1<<26;

		val |= 3<<24;			/* Chroma Filter Taps: Filter */

		val |= gtx_encode_blendlevel (0, 0, par->blendlevel0);
						/* Blend Level 0 */
		val |= gtx_encode_blendlevel (0, 1, par->blendlevel1);
						/* Blend Level 1 */
		val |= par->stride;		/* stride */
		gtx_reg_32 (GMR) = val;		/* Graphics Mode Register */

		gtx_reg_16 (TCR) = 0xfc0f;	/* Transparent Color Register */
		gtx_reg_16 (CCR) = 0x7fff;	/* Cursor Color Register */

		gtx_reg_32 (GVSA) = fb_info.offset;
						/* Graphics Viewport Start Address */

		if (par->lowres) {
			if (par->square)
				temp = 18;
			else
				temp = 16;
		}
		else {
			if (par->square)
				temp = 9;
			else
				temp = 8;
		}

		val = 127 * 8;
		gtx_reg_32 (GVP) = ((val % temp) << 27) | ((val / temp - 3 + par->delta_xpos) << 16) | (42 + par->delta_ypos);
						/* Graphics Viewport Position */
		gtx_reg_32 (GVS) = (par->xres << 16) | par->yres;
						/* Graphics Viewport Size */
		gtx_reg_16 (GFUNC) = 0x10;	/* Graphics Miscellaneous Function Register
						 * Dynamic CLUT */

		/* video register */

		gtx_reg_32 (VBR) = 0;		/* Video Background Register */
		gtx_reg_16 (VCR) = 2 << 10;	/* Video Control Register
						 * HSYNC polarity: encoder high, decoder low */
		gtx_reg_16 (VHT) = 858;		/* Video Horizontal Total */
		gtx_reg_16 (VLT) = 623 | (21 << 11);
	       					/* Video Line Total */
	}
	else if (gtxenx_chip == 2) {	/* eNX */
		/* graphics register */

		switch (par->bits_per_pixel) {
			case 4:  val = 2 << 20; break;
			case 8:  val = 6 << 20; break;
			case 16: val = 3 << 20; break;
			case 32: val = 7 << 20; break;
		}

		if (par->lowres) val |= 1 << 31;
		if (!par->interlaced) val |= 1 << 29;

		val |= 1 << 26;			/* Smootinh Filter: 2-tap */

		val |= par->stride;		/* stride */
		enx_reg_32 (GMR1) = val;	/* Graphics Mode Plane 1 */

		val = enx_encode_blendlevel (0, 0, par->blendlevel0);
						/* Blend Level 0 */
		val |= enx_encode_blendlevel (0, 1, par->blendlevel1);
						/* Blend Level 1 */
		enx_reg_16 (GBLEV1) = val;	/* Graphics Blend Level Plane 1 */

		enx_reg_32 (TCR1) = 0x177007f;	/* Transparent Color Plane 1 */

		enx_reg_32 (GVSA1) = fb_info.offset;
						/* Graphics Viewport Start Address */

		if (par->lowres) {
			if (par->square)
				temp = 18;
			else
				temp = 16;
		}
		else {
			if (par->square)
				temp = 9;
			else
				temp = 8;
		}

		val = ( 132 - 16 ) * 8;
		enx_reg_32 (GVP1) = ((val % temp) << 27) | ((val / temp - 3 + par->delta_xpos) << 16) | (42 + par->delta_ypos);
						/* Graphics Viewport Position Plane 1 */
		enx_reg_32 (GVSZ1) = (par->xres << 16) | par->yres;
						/* Graphics Viewport Size Plane 1 */

		enx_reg_32 (GMR2) = 0;		/* Graphics Mode Plane 2 */
		enx_reg_16 (GBLEV2) = 0;	/* Graphics Blend Level Plane 2 */
		enx_reg_32 (TCR2) = 0x0ff007f;	/* Transparent Color Plane 2 */

		/* video register */

		enx_reg_32 (VBR) = 0;		/* Video Background Register */
		enx_reg_16 (VCR) = (1 << 13) | (1 << 8);
						/* Video Control Register
						 * Chroma Sense, Encoder Sync Tristate */
		enx_reg_16 (VHT) = 851;		/* Video Horizontal Total */
		enx_reg_16 (VLT) = 623 | (21<<11);
	       					/* Video Line Total */
	}
}

/*
 *  Read a single color register and split it into colors/transparent.
 *  The return values must have a 16 bit magnitude.
 */
static int gtxenxfb_getcolreg (unsigned regno, unsigned *red, unsigned *green,
			     unsigned *blue, unsigned *transp,
			     struct fb_info *info)
{
	register __u32 val;

	if (regno > 255)
		return -1;

	if (gtxenx_chip == 1) {		/* GTX */
		gtx_reg_16 (CLTA) = regno;
		mb();
		val = gtx_reg_16 (CLTD);
		if (val == 0xfc0f) {
			*red = *green = *blue = 0;
			*transp = 0xff;
		}
		else {
			*red =    (val & 0x7C00) <<  1;
			*green =  (val & 0x3E0)  <<  6;
			*blue =   (val & 0x1F)   << 11;
			*transp = (val & 0x8000) ? current_par.blendlevel1*0x2000 : current_par.blendlevel0*0x2000;
		}
	}
	else if (gtxenx_chip == 2) {	/* eNX */
		enx_reg_16 (CLUTA) = regno;
		mb();
		val = enx_reg_32 (CLUTD);
		*transp = (val & 0xFF000000) >> 16;
		*red    = (val & 0x00FF0000) >>  8;
		*green  = (val & 0x0000FF00);
		*blue   = (val & 0x000000FF) <<  8;
	}

	return 0;
}

/*
 *  Set a single color register. The values supplied have a 16 bit
 *  magnitude.
 */
static int gtxenxfb_setcolreg (unsigned regno, unsigned red, unsigned green,
			     unsigned blue, unsigned transp,
			     struct fb_info *info)
{
	if (regno > 255)
		return -1;

	if (gtxenx_chip == 1) {		/* GTX */
		red    >>= 11;
		green  >>= 11;
		blue   >>= 11;
		transp >>= 14;

		gtx_reg_16 (CLTA) = regno;
		mb();
		if (transp >= 2)
			gtx_reg_16 (CLTD) = 0xfc0f;
		else
			gtx_reg_16 (CLTD) = ((transp?1:0) << 15) | (red << 10) | (green << 5) | blue;
	}
	else if (gtxenx_chip == 2) {	/* eNX */
		red    >>= 8;
		green  >>= 8;
		blue   >>= 8;
		transp >>= 8;

		enx_reg_16 (CLUTA) = regno;
		mb();
		enx_reg_32 (CLUTD) = (transp << 24) | (red << 16) | (green << 8) | blue;

		red    >>= 3;
		green  >>= 3;
		blue   >>= 3;
	}

#ifdef FBCON_HAS_CFB16
	if (regno < 16)
		fbcon_cfb16_cmap[regno] = ((!!transp) << 15) | (red << 10) | (green << 5) | blue;
#endif

	return 0;
}

/*
 *  Pan (or wrap, depending on the `vmode' field) the display using the
 *  `xoffset' and `yoffset' fields of the `var' structure.
 */
static int gtxenxfb_pan_display (const struct fb_var_screeninfo *var, struct fb_info_gen *info)
{
	return 0;
}

/*
 *  Blank the screen if blank_mode != 0, else unblank. If blank == NULL
 *  then the caller blanks by setting the CLUT (Color Look Up Table) to all
 *  black. Return 0 if blanking succeeded, != 0 if un-/blanking failed due
 *  to e.g. a video mode which doesn't support it. Implements VESA suspend
 *  and powerdown modes on hardware that supports disabling hsync/vsync:
 *    blank_mode == 2: suspend vsync
 *    blank_mode == 3: suspend hsync
 *    blank_mode == 4: powerdown
 */
static int gtxenxfb_blank (int blank_mode, struct fb_info_gen *info)
{
    return 0;
}

/*
 *  Fill in a pointer with the virtual address of the mapped frame buffer.
 *  Fill in a pointer to appropriate low level text console operations (and
 *  optionally a pointer to help data) for the video mode `par' of your
 *  video hardware. These can be generic software routines, or hardware
 *  accelerated routines specifically tailored for your hardware.
 *  If you don't have any appropriate operations, you must fill in a
 *  pointer to dummy operations, and there will be no text output.
 */
static void gtxenxfb_set_disp (const void *_par, struct display *disp,
			     struct fb_info_gen *info)
{
	struct gtxenxfb_par *par = (struct gtxenxfb_par *) _par;

	switch (par->bits_per_pixel)
	{
#ifdef FBCON_HAS_CFB4                                                                                       
		case 4:
			disp->dispsw = &fbcon_cfb4;
			disp->dispsw_data = &fbcon_cfb4_cmap;
			break;
#endif
#ifdef FBCON_HAS_CFB8
		case 8:
			disp->dispsw = &fbcon_cfb8;
			disp->dispsw_data = &fbcon_cfb8_cmap;
			break;
#endif
#ifdef FBCON_HAS_CFB16
		case 16:
			disp->dispsw = &fbcon_cfb16;
			disp->dispsw_data = &fbcon_cfb16_cmap;
			break;
#endif
		default:
			disp->dispsw = &fbcon_dummy;
	}

	disp->screen_base = (char *) fb_info.videobase;
	disp->scrollmode = SCROLL_YREDRAW;
}


static int gtxenxfb_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
			   unsigned long arg, int con, struct fb_info *info)
{
	unsigned number;

	printk ("--> %s\n", __FUNCTION__);
	switch (cmd) {
		case GTXENXFB_GET_BLEV0:
		case GTXENXFB_GET_BLEV1:
			return 0;
		case GTXENXFB_SET_BLEV0:
		case GTXENXFB_SET_BLEV1:
			if (arg > 8)
				break;
			if (cmd == GTXENXFB_SET_BLEV0) {
				number = 0;
				current_par.blendlevel0 = arg;
			}
			else if (cmd == GTXENXFB_SET_BLEV1) {
				number = 1;
				current_par.blendlevel1 = arg;
			}

			if (gtxenx_chip == 1)
				gtx_reg_32 (GMR) = gtx_encode_blendlevel (gtx_reg_32 (GMR), number, arg);
			else if (gtxenx_chip == 2)
				enx_reg_16 (GBLEV1) = enx_encode_blendlevel (enx_reg_16 (GBLEV1), number, arg);

			return 0;
		case GTXENXFB_CONSOLE_GIVEUP:
			printk ("console giveup\n");
			give_up_console (&fb_con);
			return 0;
		case GTXENXFB_CONSOLE_TAKEOVER:
			printk ("console takeover\n");
			take_over_console (&fb_con, 0, 6, 1);
			return 0;
	}

	return -EINVAL;
}



/* ------------ chip specific functions ----------- */


static inline __u32 gtx_encode_blendlevel (__u32 value, unsigned number, unsigned long level)
{
	if (number == 0)
		return (value &~ (15<<16)) | (level<<16);
	else if (number == 1)
		return (value &~ (15<<20)) | (level<<20);
	return value;
}

static inline __u16 enx_encode_blendlevel (__u16 value, unsigned number, unsigned long level)
{
	if (number == 0)
		return (value &~  0xff    ) | (level<<4);
	else if (number == 1)
		return (value &~ (0xff<<8)) | (level<<12);
	return value;
}



/* ------------ Interfaces to hardware functions ------------ */


struct fbgen_hwswitch gtxenxfb_switch = {
	gtxenxfb_detect,
	gtxenxfb_encode_fix,
       	gtxenxfb_decode_var,
       	gtxenxfb_encode_var,
       	gtxenxfb_get_par,
	gtxenxfb_set_par,
       	gtxenxfb_getcolreg,
       	gtxenxfb_setcolreg,
       	gtxenxfb_pan_display,
       	gtxenxfb_blank,
	gtxenxfb_set_disp
};

static struct fb_ops gtxenxfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	fbgen_get_fix,
	fb_get_var:	fbgen_get_var,
	fb_set_var:	fbgen_set_var,
	fb_get_cmap:	fbgen_get_cmap,
	fb_set_cmap:	fbgen_set_cmap,
	fb_pan_display:	fbgen_pan_display,
	fb_ioctl:	gtxenxfb_ioctl,
};



/* ------------ Hardware Independent Functions ------------ */


/*
 *  Initialization
 */

int __init gtxenxfb_init (void)
{
	if (gtxenx_chip == 1) {		/* GTX */
		fb_info.offset = (__u32) 1024*1024;
		printk (KERN_INFO "gtxenx-fb: loading AViA GTX/eNX framebuffer driver, GTX mode\n");
	}
	else if (gtxenx_chip == 2) {	/* eNX */
		fb_info.offset = (__u32) ENX_FB_OFFSET;
		printk (KERN_INFO "gtxenx-fb: loading AViA GTX/eNX framebuffer driver, eNX mode\n");
	}
	else
		return -EINVAL;

	fb_info.videobase = (__u32) gtxenx_mem_base + fb_info.offset;
	fb_info.physical_videobase = (__u32) gtxenx_physical_mem_base + fb_info.offset;

	fb_info.gen.fbhw = &gtxenxfb_switch;
	fb_info.gen.fbhw->detect();
	strcpy(fb_info.gen.info.modename, "AViA GTX/eNX");
	fb_info.gen.info.changevar = NULL;
	fb_info.gen.info.node = -1;
	fb_info.gen.info.fbops = &gtxenxfb_ops;
	fb_info.gen.info.disp = &disp;
	fb_info.gen.info.switch_con = &fbgen_switch;
	fb_info.gen.info.updatevar = &fbgen_update_var;
	fb_info.gen.info.blank = &fbgen_blank;
	fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
	fb_info.gen.parsize = sizeof (struct gtxenxfb_par);
	strcpy(fb_info.gen.info.fontname, "SUN8x16");

	if (gtxenx_chip == 1) {		/* GTX */
		gtx_reg_16 (RR0) &= ~(	/* Reset Register 0 */
			(1 << 13) |	/* Video Control */
			(1 << 0));	/* Graphics/Video */
	}
	else if (gtxenx_chip == 2) {	/* eNX */
					/* Reset Register 0 */
		enx_reg_32 (RSTR0) &= ~(
			(1 << 11) |	/* Graphics */
			(1 << 9) |	/* Video Module */
			(1 << 7));	/* Picture in Graphics 1 */
	}

	/* set standards */
	fb_info.blendlevel0 = 0;
	fb_info.blendlevel1 = 4;
	if (gtxenx_chip == 1)
		fb_info.delta_xpos = -52; /* -60 if if compiling with console patch */
	else if (gtxenx_chip == 2)
		fb_info.delta_xpos = 0;

	/* inofficial things, need patched kernel */
	/*fbcon_cols = 80;
       	fbcon_rows = 30;
	fbcon_cols_offset = 5;
       	fbcon_rows_offset = 2;*/

	/* This should give a reasonable default video mode */
	fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
	fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
	fbgen_set_disp(-1, &fb_info.gen);
	fbgen_install_cmap(0, &fb_info.gen);

	if (register_framebuffer(&fb_info.gen.info) < 0)
		return -EINVAL;

	printk(KERN_INFO "fb%d: %s frame buffer device\n", GET_FB_IDX(fb_info.gen.info.node),
		fb_info.gen.info.modename);

	/* uncomment this if your driver cannot be unloaded */
	/* MOD_INC_USE_COUNT; */
	return 0;
}


/*
 *  Cleanup
 */

void gtxenxfb_cleanup (void)
{
	unregister_framebuffer(&fb_info.gen.info);

	if (gtxenx_chip == 2) {		/* eNX */
		enx_reg_32 (RSTR0) |= (
			(1 << 7));	/* Picture in Graphics 1 */
	}
}


/*
 *  Setup
 */

int __init gtxenxfb_setup (char *options)
{
	/* Parse user speficied options (`video=xxxfb:') */
	return 0;
}


/* ------------------------------------------------------------------------- */


#ifdef MODULE
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>, Bastian Blank <waldi@tuxbox.org>");
MODULE_DESCRIPTION("AViA GTX/eNX framebuffer driver");

int init_module (void)
{
	return gtxenxfb_init ();
}

void cleanup_module (void)
{
	gtxenxfb_cleanup ();
}
#endif /* MODULE */

