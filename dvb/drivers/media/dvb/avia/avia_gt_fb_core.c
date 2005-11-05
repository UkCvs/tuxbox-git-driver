/*
 * $Id: avia_gt_fb_core.c,v 1.54.2.3 2005/11/05 16:25:06 carjay Exp $
 *
 * AViA eNX/GTX framebuffer driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * - initial version
 *   Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
 * - kernel 2.6 version
 *   Copyright (C) 2005 Carsten Juttner (carjay@gmx.net)
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <asm/uaccess.h>

#include <linux/dvb/avia/avia_gt_fb.h>
#include "avia_gt.h"
#include "avia_gt_gv.h"

#ifndef FB_ACCEL_CCUBE_AVIA_GTX
#define FB_ACCEL_CCUBE_AVIA_GTX 120
#endif
#ifndef FB_ACCEL_CCUBE_AVIA_ENX
#define FB_ACCEL_CCUBE_AVIA_ENX 121
#endif

static sAviaGtInfo *gt_info;
//static int console_transparent;

struct pixelmode{
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;
	u8 bpp;
	u8 avia_mode;
};

// all possible pixelmodes of the GTX)
static const struct pixelmode gtx_pixmodes[] = {
	{ .avia_mode = AVIA_GT_GV_INPUT_MODE_RGB4, 	// CLUT4 (ARGB1555)
	  	.bpp = 4,
		.red = 	 { .offset = 0, .length=5, .msb_right =0 },
		.green = { .offset = 0, .length=5, .msb_right =0 },
		.blue =  { .offset = 0, .length=5, .msb_right =0 },
		.transp=  { .offset = 0, .length=1, .msb_right =0 }
	},
	{ .avia_mode = AVIA_GT_GV_INPUT_MODE_RGB8,	// CLUT8 (ARGB1555)
		.bpp = 8,
		.red = 	 { .offset = 0, .length=5, .msb_right =0 },
		.green = { .offset = 0, .length=5, .msb_right =0 },
		.blue =  { .offset = 0, .length=5, .msb_right =0 },
		.transp=  { .offset = 0, .length=1, .msb_right =0 }
	},
	{ .avia_mode = AVIA_GT_GV_INPUT_MODE_ARGB1555, 	// ARGB1555
		.bpp = 16,
		.red = 	 { .offset = 10, .length=5, .msb_right =0 },
		.green = { .offset = 5,  .length=5, .msb_right =0 },
		.blue =  { .offset = 0,  .length=5, .msb_right =0 },
		.transp=  { .offset = 15, .length=1, .msb_right =0 }
	}
};

// All (liar!) possible pixelmodes of the eNX
// NB: functionally the ARGB8888 and ARGB1555 CLUT-modes are the same, the modes are just
// 	there for compatibility with existing GTX-software since the palette setting interface 
//	is the same (using 16 Bit r,g,b,a-components)
static const struct pixelmode enx_pixmodes[] = {
	{ .avia_mode = AVIA_GT_GV_INPUT_MODE_RGB4, 	// CLUT4 (ARGB1555)
		.bpp = 4,
		.red = 	 { .offset = 0, .length=5, .msb_right =0 },
		.green = { .offset = 0, .length=5, .msb_right =0 },
		.blue =  { .offset = 0, .length=5, .msb_right =0 },
		.transp=  { .offset = 0, .length=1, .msb_right =0 }
	},
	{ .avia_mode = AVIA_GT_GV_INPUT_MODE_RGB4, 	// CLUT4 (ARGB8888)
		.bpp = 4,
		.red = 	 { .offset = 0, .length=8, .msb_right =0 },
		.green = { .offset = 0, .length=8, .msb_right =0 },
		.blue =  { .offset = 0, .length=8, .msb_right =0 },
		.transp=  { .offset = 0, .length=8, .msb_right =0 }
	},
	{ .avia_mode = AVIA_GT_GV_INPUT_MODE_RGB8,	// CLUT8 (ARGB1555)
		.bpp = 8,
		.red = 	 { .offset = 0, .length=5, .msb_right =0 },
		.green = { .offset = 0, .length=5, .msb_right =0 },
		.blue =  { .offset = 0, .length=5, .msb_right =0 },
		.transp=  { .offset = 0, .length=1, .msb_right =0 }
	},
	{ .avia_mode = AVIA_GT_GV_INPUT_MODE_RGB8,	// CLUT8 (ARGB8888)
		.bpp = 8,
		.red = 	 { .offset = 0, .length=8, .msb_right =0 },
		.green = { .offset = 0, .length=8, .msb_right =0 },
		.blue =  { .offset = 0, .length=8, .msb_right =0 },
		.transp=  { .offset = 0, .length=8, .msb_right =0 }
	},
	{ .avia_mode = AVIA_GT_GV_INPUT_MODE_RGB565, 	// RGB565
		.bpp = 16,
		.red = 	 { .offset = 11, .length=5, .msb_right =0 },
		.green = { .offset = 5,  .length=6, .msb_right =0 },
		.blue =  { .offset = 0,  .length=5, .msb_right =0 },
		.transp=  { .offset = 0,  .length=0, .msb_right =0 }
	},
	{ .avia_mode = AVIA_GT_GV_INPUT_MODE_ARGB1555, 	// ARGB1555
		.bpp = 16,
		.red = 	 { .offset = 10, .length=5, .msb_right =0 },
		.green = { .offset = 5,  .length=5, .msb_right =0 },
		.blue =  { .offset = 0,  .length=5, .msb_right =0 },
		.transp=  { .offset = 15, .length=1, .msb_right =0 }
	},
	{ .avia_mode = AVIA_GT_GV_INPUT_MODE_ARGB,	// 32 f*cking bits, the real McCoy :)
		.bpp = 32,
		.red = 	 { .offset = 16, .length=8, .msb_right =0 },
		.green = { .offset = 8,  .length=8, .msb_right =0 },
		.blue =  { .offset = 0,  .length=8, .msb_right =0 },
		.transp=  { .offset = 24, .length=8, .msb_right =0 }
	}
};


/* default: 720x576, 8 bpp, CLUT8 (PAL-compatible)*/
// NB: this is not applied on its own, it just offers a reasonable (for both GTX/eNX) default for the FB-user */
static const struct fb_var_screeninfo __initdata default_var = {
	.xres = 720,		// size of graphics viewport
	.yres = 576,
	.xres_virtual = 720,	// size of graphics framebuffer, will be changed later on if there is more RAM
	.yres_virtual = 576,
	.xoffset = 0,		// offset from virtual to visible
	.yoffset = 0,
	.bits_per_pixel = 8,
	.grayscale = 0,		// no grayscale mode supported, always colors
	.red = { .offset = 0, .length = 5, .msb_right = 0 },	// CLUT8-entries, compatible to GTX
	.green = { .offset = 0, .length = 5, .msb_right = 0 },
	.blue = { .offset = 0, .length = 5, .msb_right = 0 },
	.transp = { .offset = 0, .length = 1, .msb_right = 0 },
	.nonstd = 0,
	.activate = FB_ACTIVATE_NOW,
	.height = -1,		// size in mm, makes no sense
	.width = -1,
	.accel_flags = 0,
	.pixclock = 74074,		// BT.601: 864 pixels per line@15625 Hz (PAL) -> 13.5 MHz -> 74074E-12 s (pico seconds)
	.left_margin = 126,
	.right_margin = 18,
	.upper_margin = 21,
	.lower_margin = 5,
	.hsync_len = 0,		
	.vsync_len = 0,
	.sync = FB_SYNC_EXT,
	.vmode = FB_VMODE_INTERLACED
};

/* holds all info about the current buffer */
static struct {
	struct fb_info info;
	unsigned char *videobase;
	int offset;
} avia_gt_fb_info;

/* all the display specific information */
struct avia_gt_fb_par {
	int xres, yres, virtual_yres;
	// offset from virtual to visible, for panning
	int yoffset;
	// index into pixmode-list
	int pixmode;
};

static struct avia_gt_fb_par current_par;

static inline const struct pixelmode *avia_gt_fb_get_ppixelmodes(void){
	if (avia_gt_chip(GTX))
		return gtx_pixmodes;
	else
		return enx_pixmodes;
}

/* compares mode to the modelist and returns the index if successful, otherwise the last entry that
	fits the bits_per_pixel is returned. If no mode is found, -1 is returned */
static int avia_gt_fb_pixelmode_compare (const struct pixelmode modelist[],
			const struct fb_var_screeninfo *mode){
	int i;
	int retval=-1;
	int modelistsize;
	if (modelist == gtx_pixmodes)
		modelistsize = ARRAY_SIZE(gtx_pixmodes);
	else
		modelistsize = ARRAY_SIZE(enx_pixmodes);
	
	for (i=0;i<modelistsize;i++){ /* default is the last(!) available entry */
		if (modelist[i].bpp == mode->bits_per_pixel) retval = i;
		/* all are checked for future compatibility */
		if (modelist[i].bpp == mode->bits_per_pixel &&	
				modelist[i].red.offset      == mode->red.offset &&
				modelist[i].red.length      == mode->red.length &&
				modelist[i].red.msb_right   == mode->red.msb_right &&
				modelist[i].green.offset    == mode->green.offset &&
				modelist[i].green.length    == mode->green.length &&
				modelist[i].green.msb_right == mode->green.msb_right &&
				modelist[i].blue.offset     == mode->blue.offset &&
				modelist[i].blue.length     == mode->blue.length &&
				modelist[i].blue.msb_right  == mode->blue.msb_right &&
				modelist[i].transp.offset   == mode->transp.offset &&
				modelist[i].transp.length   == mode->transp.length &&
				modelist[i].transp.msb_right== mode->transp.msb_right){
					retval = i;
					break;
				}
	}
	return retval;
}

static int avia_gt_fb_istruecolour(const struct pixelmode *pixm){
	if (pixm->bpp==4||pixm->bpp==8) return 0;	// 4/8 Bit are always LUT
	else return 1;
}


/* returns the fb_fix_screeninfo struct for the current mode */
static int avia_gt_fb_encode_fix(struct fb_fix_screeninfo *fix, const struct avia_gt_fb_par *par)
{
	const struct pixelmode *pixm;
	u8 *phys_start;
	
	memset(fix,0,sizeof(fix));
	strncpy(fix->id, "AViA eNX/GTX FB", sizeof(((struct fb_fix_screeninfo*)0)->id));

	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;

	pixm = avia_gt_fb_get_ppixelmodes();
	if (pixm[par->pixmode].bpp <= 8)
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
	else
		fix->visual = FB_VISUAL_TRUECOLOR;

	fix->line_length = (par->xres*pixm[par->pixmode].bpp)>>3;
	avia_gt_gv_get_info(&phys_start, NULL, NULL);
	fix->smem_start = (unsigned long)phys_start;
	fix->smem_len = (AVIA_GT_MEM_GV_SIZE+PAGE_SIZE)&PAGE_MASK;	/* page boundary for mmio */

	fix->xpanstep = 0;
	fix->ypanstep = 1;
	fix->ywrapstep = 0;

	if (avia_gt_chip(GTX)) {
		fix->accel = FB_ACCEL_CCUBE_AVIA_GTX;
		fix->mmio_start = (unsigned long)GTX_REG_BASE;
		fix->mmio_len = (GTX_REG_SIZE+PAGE_SIZE)&PAGE_MASK;
	}
	else if (avia_gt_chip(ENX)) {
		fix->accel = FB_ACCEL_CCUBE_AVIA_ENX;
		fix->mmio_start = (unsigned long)ENX_REG_BASE;
		fix->mmio_len = (ENX_REG_SIZE+PAGE_SIZE)&PAGE_MASK;
	}
	return 0;
}


/* avia_gt_fb_decode_var - decode fb_var_screeninfo to hw-specific par
	@var: the var that should be set, in case of a missing pixelformat it is set
			to the last available entry in the pixelformat-table
	@par: if not NULL we set up the hardware-specific parameters
*/
static int avia_gt_fb_decode_var(struct fb_var_screeninfo *var, struct avia_gt_fb_par *par)
{
	u32 bpp;
	unsigned int frameram;
	const struct pixelmode *pmode;
	int pixmode;
	
	switch (var->xres){
	case 720:
	case 640:
	case 360:
	case 320:
		break;
	default:
		printk (KERN_INFO "avia_gt_fb_core: Unsupported xres:%d\n",var->xres);
		return -EINVAL;
	}
	switch (var->yres){
	case 576:
	case 480:
	case 288:
	case 240:
		break;
	default:
		printk (KERN_INFO "avia_gt_fb_core: Unsupported yres:%d\n",var->yres);
		return -EINVAL;
	}


	// check how much space we have for virtual resolution
	bpp = var->bits_per_pixel;
	frameram = ((var->xres*var->yres)*bpp)>>3;	// we can't divide bpp by 8 because of the 4 Bit mode

	// only integer values necessary, we only use this for single,double,triple buffering

	if (avia_gt_fb_info.info.screen_size < frameram){
		printk (KERN_INFO "avia_gt_fb_core: Not enough graphics RAM"
							" for requested resolution %dx%d\n",var->xres,var->yres);
		return -EINVAL;
	}
	if (var->xres_virtual!=var->xres){
		printk (KERN_INFO "avia_gt_fb_core: Virtual xres different from xres not supported\n");
		return -EINVAL;
	}
	if ((((var->xres*var->yres_virtual)*bpp)>>3)>avia_gt_fb_info.info.screen_size){
		printk (KERN_INFO "avia_gt_fb_core: Not enough graphics RAM for requested virtual resolution\n");
		return -EINVAL;
	}

	/*
		find a suitable mode, if bpp does not fit return an error, else
		the returned mode is the last found that was valid
	*/
	pmode = avia_gt_fb_get_ppixelmodes();
	pixmode = avia_gt_fb_pixelmode_compare(pmode,var);
	if (pixmode == -1){
		printk (KERN_INFO "avia_gt_fb_core: Unsupported bits per pixels: %d\n",bpp);
		return -EINVAL;
	}
	if (var->red.length!=pmode[pixmode].red.length||
		var->green.length!=pmode[pixmode].green.length||
		var->blue.length!=pmode[pixmode].blue.length||
		var->transp.length!=pmode[pixmode].transp.length){
			printk(KERN_INFO "avia_gt_fb: requested RGBA (%d %d %d %d)\n"
				"granted RGBA (%d %d %d %d)\n",
				var->red.length,var->green.length,var->blue.length,var->transp.length,
				pmode[pixmode].red.length,pmode[pixmode].green.length,
				pmode[pixmode].blue.length,pmode[pixmode].transp.length);
	}

	if (par){	/* set par if not NULL */
		par->yoffset = 0;
		par->pixmode = pixmode;
		par->xres = var->xres;
		par->yres = var->yres;
 		par->virtual_yres = var->yres_virtual;
	}

	return 0;
}

/***************************************/
/* implementation for the fb_functions */
/***************************************/

/* checks passed in fb_var_screeninfo for validity but does not change any
	hardware state. Is allowed to correct var slightly if possible */
static int avia_gt_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info){
	return avia_gt_fb_decode_var(var, NULL);
}

/* sets up the card using the current parameters */
static int avia_gt_fb_set_par(struct fb_info *info)
{
	const struct pixelmode *pixm;
	int xpos=0,ypos=0;
	struct avia_gt_fb_par *ppar = info->par;
	
	/* decode var to par and correct var if possible */
	avia_gt_fb_decode_var (&info->var, ppar);
	
	/* get modes for current chip */
	pixm = avia_gt_fb_get_ppixelmodes();
	avia_gt_gv_set_input_mode (pixm[ppar->pixmode].avia_mode);
	avia_gt_gv_set_blevel(4, 0);
	
//	if (avia_gt_gv_50Hz()){	// TODO: sth to that effect	// center window on screen
		if ((ppar->yres==480)||(ppar->yres==240)) 
				ypos = (576-480)/2;
		if (((ppar->xres==640)&&(ppar->yres!=480))||	// 640x480 and 320x240 are stretched
			((ppar->xres==320)&&(ppar->yres!=240)))	// and must not be corrected
				xpos = (720-640)/2;
//	} else{	// 60 Hz mode
		//TODO: what happens with graphics in this mode?
//	}
	avia_gt_gv_set_pos(xpos, ypos);
	avia_gt_gv_set_input_size(ppar->xres, ppar->yres);
	avia_gt_gv_set_size(ppar->xres, ppar->yres);

	avia_gt_gv_show();
	return 0;
}

static int avia_gt_fb_setcolreg(unsigned regno,
				unsigned red, unsigned green, unsigned blue, unsigned transp,
				struct fb_info *info)
{
//	unsigned short vc_text = 0;
	const struct pixelmode *pixm;
	struct avia_gt_fb_par *ppar;

	pixm = avia_gt_fb_get_ppixelmodes();
	ppar = info->par;

	if (avia_gt_fb_istruecolour(&pixm[ppar->pixmode])){
		printk (KERN_INFO "avia_gt_fb_core: setcolreg called for TRUECOLOR mode\n");
		return 1;	// it's not a palette mode
	}

	if (regno > 255)
		return 1;
/*	if (info->display_fg)
		vc_text = (vt_cons[info->display_fg->vc_num]->vc_mode == KD_TEXT);
*/
	switch (pixm[current_par.pixmode].bpp) {
	case 4:
/*		if (regno == 0 && console_transparent && vc_text)
			avia_gt_gv_set_clut(0, 0xffff, 0, 0, 0);
		else
*/			avia_gt_gv_set_clut(regno, transp, red, green, blue);
		break;
	case 8:
/*		if ((regno == 0) && console_transparent && vc_text)
			avia_gt_gv_set_clut(0, 0xffff, 0, 0, 0);
		else
*/			avia_gt_gv_set_clut(regno, transp, red, green, blue);
		break;
	case 16:
		red >>= 11;
		green >>= 11;
		blue >>= 11;
		transp >>= 15;

/*		if ((regno == 0) && console_transparent && vc_text)
			fbcon_cfb16_cmap[0] = 0xfc0f;
		if (regno < 16)
			fbcon_cfb16_cmap[regno] = (transp << 15) | (red << 10) | (green << 5) | (blue);
*/
		break;
	default:
		return 1;
	}
	return 0;
}

/*
static int avia_gt_fb_blank(int blank_mode, struct fb_info *info)
{
	return 0;
}
*/

static void avia_gt_fb_fillrect(struct fb_info *p, const struct fb_fillrect *region)
{
	/* TODO - generic for now */
	cfb_fillrect(p,region);
}

static void avia_gt_fb_copyarea(struct fb_info *p, const struct fb_copyarea *area)
{
	/* TODO */
	cfb_copyarea(p,area);
}

static void avia_gt_fb_imageblit(struct fb_info *p, const struct fb_image *image)
{
	/* TODO */
	cfb_imageblit(p,image);
}

static int avia_gt_fb_cursor(struct fb_info *p, struct fb_cursor *cursor)
{
	/* TODO */
	soft_cursor(p,cursor);
	return 0;
}

static int avia_gt_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info) {
	struct avia_gt_fb_par *ppar = (struct avia_gt_fb_par *)info->par;
	if ((var->vmode & FB_VMODE_YWRAP)||
		(var->yoffset+var->yres > var->yres_virtual || var->xoffset))
			return -EINVAL;
	ppar->yoffset = var->yoffset;
	avia_gt_gv_set_viewport (0, var->yoffset);
	return 0;
}

static int avia_gt_fb_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg, 
			struct fb_info *info)
{
	fb_copyarea copyarea;
	unsigned int val;
	u8 blev0, blev1;

	switch (cmd) {
	case AVIA_GT_GV_COPYAREA:
		if (copy_from_user(&copyarea, (void *)arg, sizeof(copyarea)))
			return -EFAULT;
			
		avia_gt_gv_copyarea(copyarea.sx, copyarea.sy, copyarea.width, copyarea.height, copyarea.dx, copyarea.dy);
		break;

	case AVIA_GT_GV_SET_BLEV:
		avia_gt_gv_set_blevel((arg >> 8) & 0xFF, arg & 0xFF);
		break;

	case AVIA_GT_GV_GET_BLEV:
		avia_gt_gv_get_blevel(&blev0, &blev1);
		val = (blev0 << 8) | blev1;
		if (copy_to_user((void *) arg, &val, sizeof(val)))
			return -EFAULT;
		break;

	case AVIA_GT_GV_SET_POS:
		avia_gt_gv_set_pos((arg >> 16) & 0xFFFF, arg & 0xFFFF);
		break;

	case AVIA_GT_GV_HIDE:
		avia_gt_gv_hide();
		break;

	case AVIA_GT_GV_SHOW:
		avia_gt_gv_show();
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static struct fb_ops avia_gt_fb_ops = {
	.owner			= THIS_MODULE,
	.fb_check_var	= avia_gt_fb_check_var,
	.fb_set_par		= avia_gt_fb_set_par,
	.fb_setcolreg	= avia_gt_fb_setcolreg,
	/*.fb_blank		= avia_gt_fb_blank,*/
	.fb_pan_display	= avia_gt_fb_pan_display,
	.fb_fillrect	= avia_gt_fb_fillrect,
	.fb_copyarea	= avia_gt_fb_copyarea,
	.fb_imageblit	= avia_gt_fb_imageblit,
	.fb_cursor		= avia_gt_fb_cursor,
	.fb_ioctl		= avia_gt_fb_ioctl,
};

/* taking options makes only sense if it's part of the kernel */
#ifndef MODULE
static int __init avia_gt_fb_setup(char *options)
{
	/* TODO: think of some snazzy options... */
}
#endif

int __init avia_gt_fb_init(void)
{
#ifndef MODULE
	char *option = NULL;
#endif
	u8 *fb_virmem;
	u32 fb_size;
	
	printk(KERN_INFO "avia_gt_fb: $Id: avia_gt_fb_core.c,v 1.54.2.3 2005/11/05 16:25:06 carjay Exp $\n");

	gt_info = avia_gt_get_info();

	if (!avia_gt_supported_chipset(gt_info))
		return -ENODEV;

	avia_gt_gv_get_info(NULL, &fb_virmem, &fb_size);
	avia_gt_fb_info.info.screen_base = (char __iomem *)fb_virmem;
	avia_gt_fb_info.info.screen_size = (unsigned long)fb_size;
	
#ifndef MODULE
	if (fb_get_options("avia_gt_fb", &option))
		return -ENODEV;
	fb_setup(option);
#endif
	
	avia_gt_fb_info.info.node = 0;	/* /dev/fb0 */
	avia_gt_fb_info.info.flags = FBINFO_DEFAULT;
	avia_gt_fb_info.info.fbops = &avia_gt_fb_ops;
	avia_gt_fb_info.info.var = default_var;
	avia_gt_fb_decode_var(&avia_gt_fb_info.info.var,&current_par);
	avia_gt_fb_info.info.par = &current_par;

	avia_gt_fb_encode_fix(&avia_gt_fb_info.info.fix, avia_gt_fb_info.info.par);

	/* mandatory, FIXME: is this the maximum or just what the current mode demands */
	/* 256 entries RGB + A (transparency) */
	fb_alloc_cmap(&avia_gt_fb_info.info.cmap, 256, 1);

	if (register_framebuffer(&avia_gt_fb_info.info) < 0)
		return -EINVAL;

	printk(KERN_INFO "avia_gt_fb: fb%d: %s frame buffer device\n",
		avia_gt_fb_info.info.node, avia_gt_fb_info.info.fix.id);

	avia_gt_gv_show();

	return 0;
}

void __exit avia_gt_fb_exit(void)
{
	unregister_framebuffer(&avia_gt_fb_info.info);
}

MODULE_AUTHOR("Carsten Juttner <carjay@gmx.net>,Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("AViA eNX/GTX framebuffer driver");
MODULE_LICENSE("GPL");
//module_param(console_transparent, int, 0);
//MODULE_PARM_DESC(console_transparent, "1: black in text mode is transparent");

module_init(avia_gt_fb_init);
module_exit(avia_gt_fb_exit);
