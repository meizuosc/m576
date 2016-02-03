/* linux/drivers/decon_display/decon-fb.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Haowei Li <haowei.li@samsung.com>
 *
 * Samsung DECON(Display Enhancement Controller) Framebuffer driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/smc.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <plat/cpu.h>
#ifdef CONFIG_CPU_FREQ
#include <mach/cpufreq.h>
#endif

#include "regs-decon.h"

#ifdef CONFIG_ION_EXYNOS
#include <linux/dma-buf.h>
#include <linux/exynos_ion.h>
#include <linux/ion.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include "../../../staging/android/sw_sync.h"
#include <linux/exynos_iovmm.h>
#endif

#include <video/mipi_display.h>

#include "decon_display_driver.h"
#include "decon_mipi_dsi.h"
#include "decon_fb.h"
#include "decon_dt.h"
#include "decon_pm.h"
#include "decon_debug.h"
#include "decon_reg.h"

#ifdef CONFIG_FB_WINDOW_UPDATE
#include "dsim_reg.h"
#include "mic_reg.h"
#endif
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include <linux/trustedui.h>
#endif

#if defined(CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ) || defined(CONFIG_ARM_EXYNOS5433_BUS_DEVFREQ)
#define CONFIG_DECON_DEVFREQ
#include <mach/devfreq.h>
#include <mach/bts.h>
static int prev_overlap_cnt = 0;
static int prev_gsc_local_cnt = 0;
#endif

/* This driver will export a number of framebuffer interfaces depending
 * on the configuration passed in via the platform data. Each fb instance
 * maps to a hardware window. Currently there is no support for runtime
 * setting of the alpha-blending functions that each window has, so only
 * window 0 is actually useful.
 *
 * Window 0 is treated specially, it is used for the basis of the LCD
 * output timings and as the control for the output power-down state.
*/

/* note, the previous use of <mach/regs-fb.h> to get platform specific data
 * has been replaced by using the platform device name to pick the correct
 * configuration data for the system.
*/

#ifdef CONFIG_FB_S3C_DEBUG_REGWRITE
#undef writel
#define writel(v, r) do { \
	printk(KERN_DEBUG "%s: %08x => %p\n", __func__, (unsigned int)v, r); \
	__raw_writel(v, r); \
} while (0)
#endif /* FB_S3C_DEBUG_REGWRITE */

#define VSYNC_TIMEOUT_MSEC 50

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#undef readl
#define readlns(c)        ({ u32 __v = 0; if(!(TRUSTEDUI_MODE_VIDEO_SECURED & trustedui_get_current_mode())) {__v = readl_relaxed(c);}else{} __iormb(); __v; } )
#define readl(c) readlns(c)

#undef writel
#define writelns(v,c)        ({ __iowmb(); if(!(TRUSTEDUI_MODE_VIDEO_SECURED & trustedui_get_current_mode())) {writel_relaxed(v,c);}else{  } })
#define writel(v,c) writelns((v),(c))
#endif

#define MAX_BW_PER_WINDOW	(2560 * 1600 * 4 * 60)
#define FHD_MAX_BW_PER_WINDOW	(1080 * 1920 * 4 * 60)

struct s3c_fb;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int decon_hibernation_power_on(struct display_driver *dispdrv);
int decon_hibernation_power_off(struct display_driver *dispdrv);
#endif

#define UNDERRUN_FILTER_INTERVAL_MS	100
#define UNDERRUN_FILTER_INIT		0
#define UNDERRUN_FILTER_IDLE		1
static int underrun_filter_status;
static struct delayed_work underrun_filter_work;

extern struct mipi_dsim_device *dsim_for_decon;
extern int s5p_mipi_dsi_disable(struct mipi_dsim_device *dsim);
extern int s5p_mipi_dsi_enable(struct mipi_dsim_device *dsim);
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
extern int s5p_mipi_dsi_disable_for_tui(struct mipi_dsim_device *dsim);
extern int s5p_mipi_dsi_enable_for_tui(struct mipi_dsim_device *dsim);
#endif
#ifdef CONFIG_DECON_MIC
extern struct decon_mic *mic_for_decon;
extern int decon_mic_enable(struct decon_mic *mic);
extern int decon_mic_disable(struct decon_mic *mic);
#endif
#ifdef CONFIG_ION_EXYNOS
extern struct ion_device *ion_exynos;
#endif

#ifdef CONFIG_OF
static const struct of_device_id exynos5_decon[] = {
	{ .compatible = "samsung,exynos5-decon" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos5_decon);
#endif

void debug_function(struct display_driver *dispdrv, const char *buf);
static int s3c_fb_wait_for_vsync(struct s3c_fb *sfb, u32 timeout);
static int gsc_subdev_wait_stop(struct s3c_fb *sfb, int i);


static bool s3c_fb_validate_x_alignment(struct s3c_fb *sfb, int x, u32 w,
		u32 bits_per_pixel)
{
	uint8_t pixel_alignment = 32 / bits_per_pixel;

	if (x % pixel_alignment) {
		dev_err(sfb->dev, "left X coordinate not properly aligned to %u-pixel boundary (bpp = %u, x = %u)\n",
				pixel_alignment, bits_per_pixel, x);
		return 0;
	}
	if ((x + w) % pixel_alignment) {
		dev_err(sfb->dev, "right X coordinate not properly aligned to %u-pixel boundary (bpp = %u, x = %u, w = %u)\n",
				pixel_alignment, bits_per_pixel, x, w);
		return 0;
	}

	return 1;
}

/**
 * s3c_fb_validate_win_bpp - validate the bits-per-pixel for this mode.
 * @win: The device window.
 * @bpp: The bit depth.
 */
static bool s3c_fb_validate_win_bpp(struct s3c_fb_win *win, unsigned int bpp)
{
	return win->variant.valid_bpp & VALID_BPP(bpp);
}

/**
 * s3c_fb_check_var() - framebuffer layer request to verify a given mode.
 * @var: The screen information to verify.
 * @info: The framebuffer device.
 *
 * Framebuffer layer call to verify the given information and allow us to
 * update various information depending on the hardware capabilities.
 */
static int s3c_fb_check_var(struct fb_var_screeninfo *var,
			    struct fb_info *info)
{
	struct s3c_fb_win *win = info->par;
	struct s3c_fb *sfb = win->parent;
	int x, y;
	unsigned long long hz;

	dev_dbg(sfb->dev, "checking parameters\n");

	var->xres_virtual = max(var->xres_virtual, var->xres);
	var->yres_virtual = max(var->yres_virtual, var->yres);

	if (!s3c_fb_validate_win_bpp(win, var->bits_per_pixel)) {
		dev_dbg(sfb->dev, "win %d: unsupported bpp %d\n",
			win->index, var->bits_per_pixel);
		return -EINVAL;
	}

	if (!s3c_fb_validate_x_alignment(sfb, 0, var->xres,
			var->bits_per_pixel))
		return -EINVAL;

	/* always ensure these are zero, for drop through cases below */
	var->transp.offset = 0;
	var->transp.length = 0;

	switch (var->bits_per_pixel) {
	case 1:
	case 2:
	case 4:
	case 8:
		if (sfb->variant.palette[win->index] != 0) {
			/* non palletised, A:1,R:2,G:3,B:2 mode */
			var->red.offset		= 4;
			var->green.offset	= 2;
			var->blue.offset	= 0;
			var->red.length		= 5;
			var->green.length	= 3;
			var->blue.length	= 2;
			var->transp.offset	= 7;
			var->transp.length	= 1;
		} else {
			var->red.offset	= 0;
			var->red.length	= var->bits_per_pixel;
			var->green	= var->red;
			var->blue	= var->red;
		}
		break;

	case 19:
		/* 666 with one bit alpha/transparency */
		var->transp.offset	= 18;
		var->transp.length	= 1;
	case 18:
		var->bits_per_pixel	= 32;

		/* 666 format */
		var->red.offset		= 12;
		var->green.offset	= 6;
		var->blue.offset	= 0;
		var->red.length		= 6;
		var->green.length	= 6;
		var->blue.length	= 6;
		break;

	case 16:
		/* 16 bpp, 565 format */
		var->red.offset		= 11;
		var->green.offset	= 5;
		var->blue.offset	= 0;
		var->red.length		= 5;
		var->green.length	= 6;
		var->blue.length	= 5;
		break;

	case 32:
	case 28:
	case 25:
		var->transp.length	= var->bits_per_pixel - 24;
		var->transp.offset	= 24;
		/* drop through */
	case 24:
		/* our 24bpp is unpacked, so 32bpp */
		var->bits_per_pixel	= 32;
		var->red.offset		= 16;
		var->red.length		= 8;
		var->green.offset	= 8;
		var->green.length	= 8;
		var->blue.offset	= 0;
		var->blue.length	= 8;
		break;

	default:
		dev_err(sfb->dev, "invalid bpp\n");
	}

#ifdef CONFIG_FB_I80_COMMAND_MODE
	x = var->xres;
	y = var->yres;
#else
	x = var->xres + var->left_margin + var->right_margin + var->hsync_len;
	y = var->yres + var->upper_margin + var->lower_margin + var->vsync_len;
#endif
	hz = 1000000000000ULL;		/* 1e12 picoseconds per second */

	hz += (x * y) / 2;
	do_div(hz, x * y);		/* divide by x * y with rounding */

	hz += var->pixclock / 2;
	do_div(hz, var->pixclock);	/* divide by pixclock with rounding */

	win->fps = hz;

	dev_dbg(sfb->dev, "%s: verified parameters\n", __func__);
	return 0;
}

u32 fb_visual(u32 bits_per_pixel, unsigned short palette_sz)
{
	switch (bits_per_pixel) {
	case 32:
	case 24:
	case 16:
	case 12:
		return FB_VISUAL_TRUECOLOR;
	case 8:
		if (palette_sz >= 256)
			return FB_VISUAL_PSEUDOCOLOR;
		else
			return FB_VISUAL_TRUECOLOR;
	case 1:
		return FB_VISUAL_MONO01;
	default:
		return FB_VISUAL_PSEUDOCOLOR;
	}
}

static inline u32 fb_linelength(u32 xres_virtual, u32 bits_per_pixel)
{
	return (xres_virtual * bits_per_pixel) / 8;
}

static u16 fb_panstep(u32 res, u32 res_virtual)
{
	return res_virtual > res ? 1 : 0;
}

static u32 vidw_buf_size(u32 xres, u32 line_length, u32 bits_per_pixel)
{
	u32 pagewidth = (xres * bits_per_pixel) >> 3;
	return VIDW_BUF_SIZE_OFFSET(line_length - pagewidth) |
	       VIDW_BUF_SIZE_PAGEWIDTH(pagewidth);
}

static u32 vidosd_a(int x, int y)
{
	return VIDOSDxA_TOPLEFT_X(x) |
			VIDOSDxA_TOPLEFT_Y(y);
}

static u32 vidosd_b(int x, int y, u32 xres, u32 yres)
{
	return VIDOSDxB_BOTRIGHT_X(x + xres - 1) |
		VIDOSDxB_BOTRIGHT_Y(y + yres - 1);
}

static u32 vidosd_c(u8 r0, u8 g0, u8 b0)
{
	return VIDOSDxC_ALPHA0_R_F(r0) |
		VIDOSDxC_ALPHA0_G_F(g0) |
		VIDOSDxC_ALPHA0_B_F(b0);
}

static u32 vidosd_d(u8 r1, u8 g1, u8 b1)
{
	return VIDOSDxD_ALPHA1_R_F(r1) |
		VIDOSDxD_ALPHA1_G_F(g1) |
		VIDOSDxD_ALPHA1_B_F(b1);
}

static u32 wincon(u32 bits_per_pixel, u32 transp_length, u32 red_length)
{
	u32 data = 0;

	switch (bits_per_pixel) {
	case 8:
		data |= WINCONx_BPPMODE_8BPP_1232;
		data |= WINCONx_BYTSWP;
		break;
	case 16:
		if (transp_length == 1)
			data |= WINCONx_BPPMODE_16BPP_A1555
				| WINCONx_BLD_PIX;
		else if (transp_length == 4)
			data |= WINCONx_BPPMODE_16BPP_A4444
				| WINCONx_BLD_PIX;
		else
			data |= WINCONx_BPPMODE_16BPP_565;
		data |= WINCONx_HAWSWP;
		break;
	case 24:
	case 32:
		if (red_length == 6) {
			if (transp_length != 0)
				data |= WINCONx_BPPMODE_19BPP_A1666;
			else
				data |= WINCONx_BPPMODE_18BPP_666;
		} else if (transp_length == 1)
			data |= WINCONx_BPPMODE_25BPP_A1888
				| WINCONx_BLD_PIX;
		else if ((transp_length == 4) ||
			(transp_length == 8))
			data |= WINCONx_BPPMODE_32BPP_A8888
				| WINCONx_BLD_PIX;
		else
			data |= WINCONx_BPPMODE_24BPP_888;

		data |= WINCONx_WSWP;
		break;
	}

	if (transp_length != 1)
		data |= WINCONx_ALPHA_SEL;

	return data;
}

static inline u32 blendeq(enum s3c_fb_blending blending, u8 transp_length,
		int plane_alpha)
{
	u8 a, b;
	int is_plane_alpha = (plane_alpha < 255 && plane_alpha > 0) ? 1 : 0;

	if (transp_length == 1 && blending == S3C_FB_BLENDING_PREMULT)
		blending = S3C_FB_BLENDING_COVERAGE;

	switch (blending) {
	case S3C_FB_BLENDING_NONE:
		a = BLENDEQ_COEF_ONE;
		b = BLENDEQ_COEF_ZERO;
		break;

	case S3C_FB_BLENDING_PREMULT:
		if (!is_plane_alpha) {
			a = BLENDEQ_COEF_ONE;
			b = BLENDEQ_COEF_ONE_MINUS_ALPHA_A;
		} else {
			a = BLENDEQ_COEF_ALPHA0;
			b = BLENDEQ_COEF_ONE_MINUS_ALPHA_A;
		}
		break;

	case S3C_FB_BLENDING_COVERAGE:
		a = BLENDEQ_COEF_ALPHA_A;
		b = BLENDEQ_COEF_ONE_MINUS_ALPHA_A;
		break;

	default:
		return 0;
	}

	return BLENDEQ_A_FUNC(a) |
			BLENDEQ_B_FUNC(b) |
			BLENDEQ_P_FUNC(BLENDEQ_COEF_ZERO) |
			BLENDEQ_Q_FUNC(BLENDEQ_COEF_ZERO);
}

#if defined(CONFIG_DECON_DEVFREQ) || defined(CONFIG_FB_WINDOW_UPDATE)
static bool s3c_fb_intersect(struct s3c_fb_rect *r1, struct s3c_fb_rect *r2)
{
	return !(r1->left > r2->right || r1->right < r2->left ||
		r1->top > r2->bottom || r1->bottom < r2->top);
}
#endif

#ifdef CONFIG_DECON_DEVFREQ
static int s3c_fb_intersection(struct s3c_fb_rect *r1,
				struct s3c_fb_rect *r2, struct s3c_fb_rect *r3)
{
	r3->top = max(r1->top, r2->top);
	r3->bottom = min(r1->bottom, r2->bottom);
	r3->left = max(r1->left, r2->left);
	r3->right = min(r1->right, r2->right);
	return 0;
}

static int s3c_fb_get_overlap_cnt(struct s3c_fb *sfb, struct s3c_fb_win_config *win_config)
{
	struct s3c_fb_rect overlaps2[10];
	struct s3c_fb_rect overlaps3[6];
	struct s3c_fb_rect overlaps4[3];
	struct s3c_fb_rect r1, r2;
	struct s3c_fb_win_config *win_cfg1, *win_cfg2;
	int overlaps2_cnt = 0;
	int overlaps3_cnt = 0;
	int overlaps4_cnt = 0;
	int i, j;
	int overlap_max_cnt = 1;
	/* For optimizing power consumuption */
	unsigned long update_area = win_config[0].w * win_config[0].h;

	for (i = 1; i < sfb->variant.nr_windows; i++) {
		win_cfg1 = &win_config[i];
		if (win_cfg1->state != S3C_FB_WIN_STATE_BUFFER)
			continue;
		r1.left = win_cfg1->x;
		r1.top = win_cfg1->y;
		r1.right = r1.left + win_cfg1->w - 1;
		r1.bottom = r1.top + win_cfg1->h - 1;
		/* accumulation update area */
		update_area += (win_cfg1->w * win_cfg1->h);
		for (j = 0; j < overlaps4_cnt; j++) {
			/* 5 window overlaps */
			if (s3c_fb_intersect(&r1, &overlaps4[j])) {
				overlap_max_cnt = 5;
				break;
			}
		}
		for (j = 0; (j < overlaps3_cnt) && (overlaps4_cnt < 3); j++) {
			/* 4 window overlaps */
			if (s3c_fb_intersect(&r1, &overlaps3[j])) {
				s3c_fb_intersection(&r1, &overlaps3[j], &overlaps4[overlaps4_cnt]);
				overlaps4_cnt++;
			}
		}
		for (j = 0; (j < overlaps2_cnt) && (overlaps3_cnt < 6); j++) {
			/* 3 window overlaps */
			if (s3c_fb_intersect(&r1, &overlaps2[j])) {
				s3c_fb_intersection(&r1, &overlaps2[j], &overlaps3[overlaps3_cnt]);
				overlaps3_cnt++;
			}
		}
		for (j = 0; (j < i) && (overlaps2_cnt < 10); j++) {
			win_cfg2 = &win_config[j];
			if (win_cfg2->state != S3C_FB_WIN_STATE_BUFFER)
				continue;
			r2.left = win_cfg2->x;
			r2.top = win_cfg2->y;
			r2.right = r2.left + win_cfg2->w - 1;
			r2.bottom = r2.top + win_cfg2->h - 1;
			/* 2 window overlaps */
			if (s3c_fb_intersect(&r1, &r2)) {
				s3c_fb_intersection(&r1, &r2, &overlaps2[overlaps2_cnt]);
				overlaps2_cnt++;
			}
		}
	}

	if (overlaps4_cnt > 0)
		overlap_max_cnt = max(overlap_max_cnt, 4);
	else if (overlaps3_cnt > 0)
		overlap_max_cnt = max(overlap_max_cnt, 3);
	else if (overlaps2_cnt > 0)
		overlap_max_cnt = max(overlap_max_cnt, 2);

#define OPTIMIZED_BOOST_BTS_THRESHOLD	8601600
	/* For optimizing power consumuption */
	if (overlap_max_cnt==3 && update_area>OPTIMIZED_BOOST_BTS_THRESHOLD &&
		sfb->lcd_info->xres*sfb->lcd_info->yres==2560*1600)
		overlap_max_cnt++;
#undef OPTIMIZED_BOOST_BTS_THRESHOLD

	return overlap_max_cnt;
}
#endif

static unsigned int s3c_fb_calc_bandwidth(u32 w, u32 h, u32 bits_per_pixel, int fps)
{
	unsigned int bw = w * h;

	bw *= DIV_ROUND_UP(bits_per_pixel, 8);
	bw *= fps;

	return bw;
}

/**
 * s3c_fb_set_par() - framebuffer request to set new framebuffer state.
 * @info: The framebuffer to change.
 *
 * Framebuffer layer request to set a new mode for the specified framebuffer
 */
static int s3c_fb_set_par(struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;
	struct s3c_fb_win *win = info->par;
	struct s3c_fb *sfb = win->parent;
	struct decon_regs_data win_regs;
	int win_no = win->index;
	struct display_driver *dispdrv;

	dispdrv = get_display_driver();

	dev_dbg(sfb->dev, "setting framebuffer parameters\n");

	disp_pm_runtime_get_sync(dispdrv);

	if (sfb->power_state == POWER_DOWN)
		return 0;

	decon_reg_shadow_protect_win(win->index, 1);

	info->fix.visual = fb_visual(var->bits_per_pixel,
			win->variant.palette_sz);

	info->fix.line_length = fb_linelength(var->xres_virtual,
			var->bits_per_pixel);

	info->fix.xpanstep = fb_panstep(var->xres, var->xres_virtual);
	info->fix.ypanstep = fb_panstep(var->yres, var->yres_virtual);

	if (decon_reg_is_win_enabled(win_no))
		win_regs.wincon = WINCONx_ENWIN;
	else
		win_regs.wincon = 0;

	win_regs.wincon |= wincon(var->bits_per_pixel, var->transp.length,
			var->red.length);
	win_regs.blendeq = 0x0;
	win_regs.winmap = 0x0;
	win_regs.vidosd_a = vidosd_a(0, 0);
	win_regs.vidosd_b = vidosd_b(0, 0, var->xres, var->yres);
	win_regs.vidosd_c = vidosd_c(0x0, 0x0, 0x0);
	win_regs.vidosd_d = vidosd_d(0xff, 0xff, 0xff);
	win_regs.vidw_buf_start = info->fix.smem_start;
	win_regs.vidw_buf_end = info->fix.smem_start + info->fix.line_length *
			var->yres;
	win_regs.vidw_buf_size = vidw_buf_size(var->xres, info->fix.line_length,
			var->bits_per_pixel);
	decon_reg_set_regs_data(win_no, &win_regs);

	decon_reg_shadow_protect_win(win->index, 0);

	decon_reg_update_standalone();

	disp_pm_runtime_put_sync(dispdrv);

	return 0;
}

static inline unsigned int chan_to_field(unsigned int chan,
					 struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

/**
 * s3c_fb_setcolreg() - framebuffer layer request to change palette.
 * @regno: The palette index to change.
 * @red: The red field for the palette data.
 * @green: The green field for the palette data.
 * @blue: The blue field for the palette data.
 * @trans: The transparency (alpha) field for the palette data.
 * @info: The framebuffer being changed.
 */
static int s3c_fb_setcolreg(unsigned regno,
			    unsigned red, unsigned green, unsigned blue,
			    unsigned transp, struct fb_info *info)
{
	struct s3c_fb_win *win = info->par;
	struct s3c_fb *sfb = win->parent;
	unsigned int val;
	struct display_driver *dispdrv;

	dev_dbg(sfb->dev, "%s: win %d: %d => rgb=%d/%d/%d\n",
		__func__, win->index, regno, red, green, blue);

	dispdrv = get_display_driver();

	disp_pm_runtime_get_sync(dispdrv);

	if (sfb->power_state == POWER_DOWN)
		return 0;

	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/* true-colour, use pseudo-palette */

		if (regno < 16) {
			u32 *pal = info->pseudo_palette;

			val  = chan_to_field(red,   &info->var.red);
			val |= chan_to_field(green, &info->var.green);
			val |= chan_to_field(blue,  &info->var.blue);

			pal[regno] = val;
		}
		break;
	default:
		disp_pm_runtime_put_sync(dispdrv);
		return 1;	/* unknown type */
	}

	disp_pm_runtime_put_sync(dispdrv);
	return 0;
}

static void s3c_fb_activate_window_dma(struct s3c_fb *sfb, unsigned int index)
{
	decon_reg_direct_on_off(1);
	decon_reg_update_standalone();
}

static int s3c_fb_enable(struct s3c_fb *sfb);
static int s3c_fb_disable(struct s3c_fb *sfb);
static int s3c_fb_enable_local_path(struct s3c_fb *sfb, int i,
		struct s3c_reg_data *regs, bool enable);

void s3c_fb_disable_otf(struct s3c_fb *sfb)
{
	u32 data;
	int timecnt = 2000;
	int ret = 0;
	u32 trig_mask_state;

	if (sfb->power_state == POWER_DOWN)
		return;

	if (test_bit(S3C_FB_LOCAL, &sfb->windows[0]->state)) {
		trig_mask_state = decon_read(TRIGCON) & TRIGCON_HWTRIGMASK_I80_RGB;
		decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_DISABLE);

		do {
			data = readl(sfb->regs + VIDCON1) >> VIDCON1_LINECNT_SHIFT;
			timecnt--;
			udelay(100);
		} while (data && timecnt);

		if (timecnt == 0) {
			dev_info(sfb->dev, "%s: Failed to wait LINECNT(%d)\n",
				__func__, data);
		}
		decon_reg_shadow_protect_win(sfb->windows[0]->index, 1);
		set_bit(S3C_FB_READY_TO_LOCAL, &sfb->windows[0]->state);
		ret = s3c_fb_enable_local_path(sfb, 0, NULL, false);
		if (ret)
			dev_err(sfb->dev, "%s: fail stop localpath\n", __func__);

		clear_bit(S3C_FB_LOCAL, &sfb->windows[0]->state);
		set_bit(S3C_FB_DMA, &sfb->windows[0]->state);
		decon_reg_shadow_protect_win(sfb->windows[0]->index, 0);

		decon_reg_clear_win(0);
		decon_reg_update_standalone();
		decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_ENABLE);

		ret = gsc_subdev_wait_stop(sfb, 0);
		if (ret)
			pr_err("failed set wait stop\n");

		if (!trig_mask_state)
			decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_DISABLE);

		dev_info(sfb->dev, "%s", __func__);
	}
}

/**
 * s3c_fb_blank() - blank or unblank the given window
 * @blank_mode: The blank state from FB_BLANK_*
 * @info: The framebuffer to blank.
 *
 * Framebuffer layer request to change the power state.
 */
static int s3c_fb_blank(int blank_mode, struct fb_info *info)
{
	struct s3c_fb_win *win = info->par;
	struct s3c_fb *sfb = win->parent;
	int ret = 0;
	struct display_driver *dispdrv;

	dev_info(sfb->dev, "+%s: %d\n", __func__, blank_mode);
	save_decon_operation_time(OPS_CALL_FB_BLANK_IN);

	dispdrv = get_display_driver();
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_gate_lock(dispdrv, true);
#endif
	disp_pm_runtime_get_sync(dispdrv);


	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
		if ((TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode())) {
			trustedui_blank_inc();
			if ((TRUSTEDUI_MODE_VIDEO_SECURED & trustedui_get_current_mode()))
				s5p_mipi_dsi_disable_for_tui(dsim_for_decon);
		}
#endif
#ifdef CONFIG_PM_RUNTIME
	if (sfb->power_state == POWER_DOWN) {
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
		disp_pm_gate_lock(dispdrv, false);
#endif
		dev_info(sfb->dev, "%s: improper power state.\n", __func__);
		return 0;
	}
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
		/* Normal Display Mode */
		if (!(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode())) {
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
		disp_set_pm_status(DISP_STATUS_PM2);
		if (sfb->power_state == POWER_HIBER_DOWN)
			disp_pm_add_refcount(dispdrv);
#endif
		ret = s3c_fb_disable(sfb);
		s5p_mipi_dsi_disable(dsim_for_decon);
#ifdef CONFIG_DECON_MIC
		decon_mic_disable(mic_for_decon);
#endif
#if defined(CONFIG_DECON_DEVFREQ)
		exynos5_update_media_layers(TYPE_DECON, 0);
		exynos5_update_media_layers(TYPE_GSCL_LOCAL, 0);
		prev_overlap_cnt = 0;
		prev_gsc_local_cnt = 0;
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
		} else {
			/* TUI : Secure Display Mode */
			/* Check TUI State and increase count */
			if (!(TRUSTEDUI_MODE_VIDEO_SECURED& trustedui_get_current_mode())) {
				/* disable normal framebuffer */
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
				disp_pm_gate_lock(dispdrv, true);
#endif
				ret = s3c_fb_disable(sfb);
			}
		}
#endif
		break;

	case FB_BLANK_UNBLANK:
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
               /* Normal Display Mode */
	       if (!(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode())) {
#endif
#if defined(CONFIG_DECON_DEVFREQ)
		exynos5_update_media_layers(TYPE_DECON, 1);
		exynos5_update_media_layers(TYPE_GSCL_LOCAL, 0);
		prev_overlap_cnt = 1;
		prev_gsc_local_cnt = 0;
#endif
#ifdef CONFIG_DECON_MIC
		decon_mic_enable(mic_for_decon);
#endif
		s5p_mipi_dsi_enable(dsim_for_decon);
		ret = s3c_fb_enable(sfb);
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
		} else {
			/* TUI: Secure Display Mode */
			if (!(TRUSTEDUI_MODE_VIDEO_SECURED & trustedui_get_current_mode())) {
				s5p_mipi_dsi_disable_for_tui(dsim_for_decon);
				s5p_mipi_dsi_enable_for_tui(dsim_for_decon);
				ret = s3c_fb_enable(sfb);
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
				disp_pm_gate_lock(dispdrv, false);
#endif
			}
			/* Check TUI State and decrease count */
			trustedui_blank_dec();
		}
#endif
		break;

	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	default:
		ret = -EINVAL;
	}

	disp_pm_runtime_put_sync(dispdrv);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_gate_lock(dispdrv, false);
	if (blank_mode == FB_BLANK_POWERDOWN || blank_mode == FB_BLANK_NORMAL)
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
		if (!(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()))
#endif
			init_display_pm_status(dispdrv);
#endif

	save_decon_operation_time(OPS_CALL_FB_BLANK_OUT);
	dev_info(sfb->dev, "-%s: %d, %d\n", __func__, blank_mode, ret);

	return ret;
}

/**
 * s3c_fb_pan_display() - Pan the display.
 *
 * Note that the offsets can be written to the device at any time, as their
 * values are latched at each vsync automatically. This also means that only
 * the last call to this function will have any effect on next vsync, but
 * there is no need to sleep waiting for it to prevent tearing.
 *
 * @var: The screen information to verify.
 * @info: The framebuffer device.
 */
static int s3c_fb_pan_display(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct s3c_fb_win *win	= info->par;
	struct s3c_fb *sfb	= win->parent;
	void __iomem *buf	= sfb->regs + win->index * 8;
	unsigned int start_boff, end_boff;
	struct display_driver *dispdrv = get_display_driver();

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING
	/* Try to scheduled for DISPLAY power_on */
	if (sfb->power_state != POWER_DOWN)
		disp_pm_add_refcount(dispdrv);
#endif

	/* support LPM (off charging mode) display based on FBIOPAN_DISPLAY */
	s3c_fb_set_par(info);

	disp_pm_runtime_get_sync(dispdrv);

	/* Offset in bytes to the start of the displayed area */
	start_boff = var->yoffset * info->fix.line_length;
	/* X offset depends on the current bpp */
	if (info->var.bits_per_pixel >= 8) {
		start_boff += var->xoffset * (info->var.bits_per_pixel >> 3);
	} else {
		switch (info->var.bits_per_pixel) {
		case 4:
			start_boff += var->xoffset >> 1;
			break;
		case 2:
			start_boff += var->xoffset >> 2;
			break;
		case 1:
			start_boff += var->xoffset >> 3;
			break;
		default:
			dev_err(sfb->dev, "invalid bpp\n");
			disp_pm_runtime_put_sync(dispdrv);
			return -EINVAL;
		}
	}
	/* Offset in bytes to the end of the displayed area */
	end_boff = start_boff + info->var.yres * info->fix.line_length;

	/* Temporarily turn off per-vsync update from shadow registers until
	 * both start and end addresses are updated to prevent corruption */
	decon_reg_shadow_protect_win(win->index, 1);

	s3c_fb_activate_window_dma(sfb, win->index);
	decon_reg_activate_window(win->index);

	writel(info->fix.smem_start + start_boff, buf + sfb->variant.buf_start);
	writel(info->fix.smem_start + end_boff, buf + sfb->variant.buf_end);

	decon_reg_shadow_protect_win(win->index, 0);

#ifdef CONFIG_FB_I80_COMMAND_MODE
	decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_ENABLE);
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	s5p_mipi_dsi_trigger_unmask();
#endif

	s3c_fb_wait_for_vsync(sfb, VSYNC_TIMEOUT_MSEC);

	decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_DISABLE);
#endif

	disp_pm_runtime_put_sync(dispdrv);
	return 0;
}

void s3c_fb_activate_vsync(struct s3c_fb *sfb)
{
	int prev_refcount;
	struct display_driver *dispdrv;
	struct decon_psr_info psr;

	dispdrv = get_display_driver();
	disp_pm_runtime_get_sync(dispdrv);

	mutex_lock(&sfb->vsync_info.irq_lock);
	prev_refcount = sfb->vsync_info.irq_refcount++;
	if (!prev_refcount) {
		psr.psr_mode = sfb->psr_mode;
		psr.trig_mode = sfb->trig_mode;
		decon_reg_set_int(&psr, 1);
	}
	mutex_unlock(&sfb->vsync_info.irq_lock);

	disp_pm_runtime_put_sync(dispdrv);
}

void s3c_fb_deactivate_vsync(struct s3c_fb *sfb)
{
	int new_refcount;
	struct display_driver *dispdrv;
	struct decon_psr_info psr;

	dispdrv = get_display_driver();
	disp_pm_runtime_get_sync(dispdrv);

	mutex_lock(&sfb->vsync_info.irq_lock);
	new_refcount = --sfb->vsync_info.irq_refcount;
	WARN_ON(new_refcount < 0);
	if (!new_refcount) {
		psr.psr_mode = sfb->psr_mode;
		psr.trig_mode = sfb->trig_mode;
		decon_reg_set_int(&psr, 0);
	}
	mutex_unlock(&sfb->vsync_info.irq_lock);

	disp_pm_runtime_put_sync(dispdrv);
}

EXPORT_SYMBOL(s3c_fb_activate_vsync);
EXPORT_SYMBOL(s3c_fb_deactivate_vsync);

/**
 * sfb_fb_log_fifo_underflow_locked - log a FIFO underflow event.  Caller must
 * hold the driver's spin lock while calling.
 *
 * @sfb: main hardware state
 * @timestamp: timestamp of the FIFO underflow event
 */
void s3c_fb_log_fifo_underflow_locked(struct s3c_fb *sfb, ktime_t timestamp)
{
#ifdef CONFIG_DEBUG_FS
	unsigned int idx = sfb->debug_data.num_timestamps %
			S3C_FB_DEBUG_FIFO_TIMESTAMPS;
	sfb->debug_data.fifo_timestamps[idx] = timestamp;
	sfb->debug_data.num_timestamps++;
	if (sfb->debug_data.num_timestamps > S3C_FB_DEBUG_FIFO_TIMESTAMPS) {
		sfb->debug_data.first_timestamp++;
		sfb->debug_data.first_timestamp %= S3C_FB_DEBUG_FIFO_TIMESTAMPS;
	}

	memcpy(sfb->debug_data.regs_at_underflow, sfb->regs,
			sizeof(sfb->debug_data.regs_at_underflow));
#endif
}
#ifdef CONFIG_FB_I80_COMMAND_MODE
static irqreturn_t decon_fb_isr_for_eint(int irq, void *dev_id)
{
	struct s3c_fb *sfb = dev_id;
	ktime_t timestamp = ktime_get();

	spin_lock(&sfb->slock);
	if (sfb->trig_mode == DECON_SW_TRIG) {
		decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_ENABLE);
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		s5p_mipi_dsi_trigger_unmask();
#endif
	}

	sfb->vsync_info.timestamp = timestamp;
	wake_up_interruptible_all(&sfb->vsync_info.wait);

	spin_unlock(&sfb->slock);

	save_decon_operation_time(OPS_RUN_FB_TEIRQ);
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	/* triggering power event for PM */
	if (sfb->output_on && sfb->power_state == POWER_ON)
		disp_pm_te_triggered(get_display_driver());
#endif

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	s5p_mipi_dsi_te_triggered();
#endif
	return IRQ_HANDLED;
}
#endif

static void underrun_filter_handler(struct work_struct *ws)
{
	pr_err("DECON Underrun...\n");
	msleep(UNDERRUN_FILTER_INTERVAL_MS);
	underrun_filter_status = UNDERRUN_FILTER_IDLE;
}

static void decon_oneshot_underrun_log(void)
{
	if (underrun_filter_status++ > UNDERRUN_FILTER_IDLE)
		return;
	queue_delayed_work(system_freezable_wq, &underrun_filter_work, 0);
}

static irqreturn_t s3c_fb_irq(int irq, void *dev_id)
{
	struct s3c_fb *sfb = dev_id;
	void __iomem  *regs = sfb->regs;
	u32 irq_sts_reg;
	ktime_t timestamp;

	timestamp = ktime_get();

	spin_lock(&sfb->slock);

	irq_sts_reg = readl(regs + VIDINTCON1);
	if (irq_sts_reg & VIDINTCON1_INT_FRAME) {
		/* VSYNC interrupt, accept it */
		writel(VIDINTCON1_INT_FRAME, regs + VIDINTCON1);
		sfb->vsync_info.timestamp = timestamp;
		wake_up_interruptible_all(&sfb->vsync_info.wait);
	}
	if (irq_sts_reg & VIDINTCON1_INT_FIFO) {
		writel(VIDINTCON1_INT_FIFO, regs + VIDINTCON1);
		decon_oneshot_underrun_log();
		s3c_fb_log_fifo_underflow_locked(sfb, timestamp);
	}
	if (irq_sts_reg & VIDINTCON1_INT_I80) {
		writel(VIDINTCON1_INT_I80, regs + VIDINTCON1);
	}
	spin_unlock(&sfb->slock);
	return IRQ_HANDLED;
}

/**
 * s3c_fb_wait_for_vsync() - sleep until next VSYNC interrupt or timeout
 * @sfb: main hardware state
 * @timeout: timeout in msecs, or 0 to wait indefinitely.
 */
static int s3c_fb_wait_for_vsync(struct s3c_fb *sfb, u32 timeout)
{
	ktime_t timestamp;
	int ret;
	struct display_driver *dispdrv;

	dispdrv = get_display_driver();

	disp_pm_runtime_get_sync(dispdrv);

	timestamp = sfb->vsync_info.timestamp;
#ifndef CONFIG_FB_I80_SW_TRIGGER
	s3c_fb_activate_vsync(sfb);
#endif
	if (timeout) {
		ret = wait_event_interruptible_timeout(sfb->vsync_info.wait,
				!ktime_equal(timestamp,
						sfb->vsync_info.timestamp),
				msecs_to_jiffies(timeout));
	} else {
		ret = wait_event_interruptible(sfb->vsync_info.wait,
				!ktime_equal(timestamp,
						sfb->vsync_info.timestamp));
	}
#ifndef CONFIG_FB_I80_SW_TRIGGER
	s3c_fb_deactivate_vsync(sfb);
#endif

	disp_pm_runtime_put_sync(dispdrv);

	if (timeout && ret == 0) {
		dev_err(sfb->dev, "wait for vsync timeout");
		return -ETIMEDOUT;
	}

	return 0;
}

int s3c_fb_set_window_position(struct fb_info *info,
				struct s3c_fb_user_window user_window)
{
	struct s3c_fb_win *win = info->par;
	struct s3c_fb *sfb = win->parent;
	struct fb_var_screeninfo *var = &info->var;
	int win_no = win->index;
	void __iomem *regs = sfb->regs;
	u32 data;
	struct display_driver *dispdrv;

	dispdrv = get_display_driver();

	disp_pm_runtime_get_sync(dispdrv);
	decon_reg_shadow_protect_win(win->index, 1);

	if (!s3c_fb_validate_x_alignment(sfb, user_window.x, var->xres,
			var->bits_per_pixel))
		return -EINVAL;

	/* write 'OSD' registers to control position of framebuffer */
	data = vidosd_a(user_window.x, user_window.y);
	writel(data, regs + VIDOSD_A(win_no));

	data = vidosd_b(user_window.x, user_window.y, var->xres, var->yres);
	writel(data, regs + VIDOSD_B(win_no));

	decon_reg_shadow_protect_win(win->index, 0);
	disp_pm_runtime_put_sync(dispdrv);
	return 0;
}

int s3c_fb_set_plane_alpha_blending(struct fb_info *info,
				struct s3c_fb_user_plane_alpha user_alpha)
{
	struct s3c_fb_win *win = info->par;
	struct s3c_fb *sfb = win->parent;
	int win_no = win->index;
	void __iomem *regs = sfb->regs;
	u32 data;

	u32 alpha = 0;
	struct display_driver *dispdrv;

	dispdrv = get_display_driver();

	alpha = ((user_alpha.red & 0xff) << 8) |
			((user_alpha.green & 0xff) << 4) |
			((user_alpha.blue & 0xff) << 0);

	disp_pm_runtime_get_sync(dispdrv);
	decon_reg_shadow_protect_win(win->index, 1);

	data = readl(regs + sfb->variant.wincon + (win_no * 4));
	data &= ~(WINCONx_BLD_PIX | WINCONx_ALPHA_SEL);
	data |= WINCONx_BLD_PLANE;
	writel(data, regs + sfb->variant.wincon + (win_no * 4));

	if (user_alpha.channel == 0)
		writel(alpha, regs + VIDOSD_C(win_no));
	else {
		data |= WINCONx_ALPHA_SEL;
		writel(alpha, regs + VIDOSD_D(win_no));
	}

	decon_reg_shadow_protect_win(win->index, 0);
	disp_pm_runtime_put_sync(dispdrv);

	return 0;
}

int s3c_fb_set_chroma_key(struct fb_info *info,
			struct s3c_fb_user_chroma user_chroma)
{
	struct s3c_fb_win *win = info->par;
	struct s3c_fb *sfb = win->parent;
	int win_no = win->index;
	void __iomem *regs = sfb->regs;
	void __iomem *keycon = regs + sfb->variant.keycon;

	u32 data = 0;

	u32 chroma_value;
	struct display_driver *dispdrv;

	dispdrv = get_display_driver();

	chroma_value = (((user_chroma.red & 0xff) << 16) |
			((user_chroma.green & 0xff) << 8) |
			((user_chroma.blue & 0xff) << 0));

	disp_pm_runtime_get_sync(dispdrv);
	decon_reg_shadow_protect_win(win->index, 1);

	if (user_chroma.enabled)
		data |= WxKEYCON0_KEYEN_F;

	keycon += (win_no-1) * 8;
	writel(data, keycon + WKEYCON0);

	data = (chroma_value & 0xffffff);
	writel(data, keycon + WKEYCON1);

	decon_reg_shadow_protect_win(win->index, 0);
	disp_pm_runtime_put_sync(dispdrv);

	return 0;
}

int s3c_fb_set_vsync_int(struct fb_info *info,
		bool active)
{
	struct s3c_fb_win *win = info->par;
	struct s3c_fb *sfb = win->parent;
	bool prev_active = sfb->vsync_info.active;

	sfb->vsync_info.active = active;
	smp_wmb();

	if (active && !prev_active)
		s3c_fb_activate_vsync(sfb);
	else if (!active && prev_active)
		s3c_fb_deactivate_vsync(sfb);

	return 0;
}

#ifdef CONFIG_ION_EXYNOS
static unsigned int s3c_fb_map_ion_handle(struct s3c_fb *sfb,
		struct s3c_dma_buf_data *dma, struct ion_handle *ion_handle,
		struct dma_buf *buf, int win_no)
{
	dma->fence = NULL;

	dma->dma_buf = buf;

	dma->attachment = dma_buf_attach(dma->dma_buf, sfb->dev);
	if (IS_ERR_OR_NULL(dma->attachment)) {
		dev_err(sfb->dev, "dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma->attachment));
		goto err_buf_map_attach;
	}

#if !defined(CONFIG_FB_EXYNOS_FIMD_SYSMMU_DISABLE)
	dma->sg_table = dma_buf_map_attachment(dma->attachment,
			DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dma->sg_table)) {
		dev_err(sfb->dev, "dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma->sg_table));
		goto err_buf_map_attachment;
	}

	dma->dma_addr = ion_iovmm_map(dma->attachment, 0,
			dma->dma_buf->size, DMA_TO_DEVICE, win_no);
	if (!dma->dma_addr || IS_ERR_VALUE(dma->dma_addr)) {
		dev_err(sfb->dev, "iovmm_map() failed: %pa\n", &dma->dma_addr);
		goto err_iovmm_map;
	}
#else
	if (ion_phys(sfb->fb_ion_client, ion_handle, &dma->dma_addr, &dma->dma_buf->size)) {
		dev_err(sfb->dev, "%s: Failed to get phys. addr of buf\n", __func__);
		goto err_buf_map_attachment;
	}
#endif
	exynos_ion_sync_dmabuf_for_device(sfb->dev, dma->dma_buf,
						dma->dma_buf->size,
						DMA_TO_DEVICE);
	dma->ion_handle = ion_handle;
	return dma->dma_buf->size;

err_iovmm_map:
	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);
err_buf_map_attachment:
	dma_buf_detach(dma->dma_buf, dma->attachment);
err_buf_map_attach:
	return 0;
}

static void s3c_fb_free_dma_buf(struct s3c_fb *sfb,
		struct s3c_dma_buf_data *dma)
{
	if (!dma->dma_addr)
		return;

	if (dma->fence)
		sync_fence_put(dma->fence);

#if !defined(CONFIG_FB_EXYNOS_FIMD_SYSMMU_DISABLE)
	ion_iovmm_unmap(dma->attachment, dma->dma_addr);

	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);
#endif
	dma_buf_detach(dma->dma_buf, dma->attachment);
	dma_buf_put(dma->dma_buf);
	ion_free(sfb->fb_ion_client, dma->ion_handle);
	memset(dma, 0, sizeof(struct s3c_dma_buf_data));
}

static u32 s3c_fb_red_length(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 5;

	case S3C_FB_PIXEL_FORMAT_RGB_565:
		return 5;

	default:
		pr_warn("s3c-fb: unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 s3c_fb_red_offset(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 0;

	case S3C_FB_PIXEL_FORMAT_RGB_565:
		return 11;

	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 16;

	default:
		pr_warn("s3c-fb: unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 s3c_fb_green_length(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 5;

	case S3C_FB_PIXEL_FORMAT_RGB_565:
		return 6;

	default:
		pr_warn("s3c-fb: unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 s3c_fb_green_offset(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
	case S3C_FB_PIXEL_FORMAT_RGB_565:
		return 5;
	default:
		pr_warn("s3c-fb: unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 s3c_fb_blue_length(int format)
{
	return s3c_fb_red_length(format);
}

static u32 s3c_fb_blue_offset(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
		return 16;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 10;

	case S3C_FB_PIXEL_FORMAT_RGB_565:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 0;

	default:
		pr_warn("s3c-fb: unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 s3c_fb_transp_length(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
		return 8;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 1;

	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_RGB_565:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 0;

	default:
		pr_warn("s3c-fb: unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 s3c_fb_transp_offset(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
		return 24;

	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return 15;

	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
		return s3c_fb_blue_offset(format);

	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return s3c_fb_red_offset(format);

	case S3C_FB_PIXEL_FORMAT_RGB_565:
		return 0;

	default:
		pr_warn("s3c-fb: unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 s3c_fb_padding(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
	case S3C_FB_PIXEL_FORMAT_RGB_565:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
		return 0;

	default:
		pr_warn("s3c-fb: unrecognized pixel format %u\n", format);
		return 0;
	}

}

static u32 s3c_fb_rgborder(int format)
{
	switch (format) {
	case S3C_FB_PIXEL_FORMAT_RGBX_8888:
	case S3C_FB_PIXEL_FORMAT_RGBA_8888:
	case S3C_FB_PIXEL_FORMAT_RGBA_5551:
		return WINCONx_BPPORDER_C_F_RGB;

	case S3C_FB_PIXEL_FORMAT_RGB_565:
	case S3C_FB_PIXEL_FORMAT_BGRA_8888:
	case S3C_FB_PIXEL_FORMAT_BGRX_8888:
		return WINCONx_BPPORDER_C_F_BGR;

	default:
		pr_warn("s3c-fb: unrecognized pixel format %u\n", format);
		return 0;
	}
}

static int s3c_fb_set_win_buffer(struct s3c_fb *sfb, struct s3c_fb_win *win,
		struct s3c_fb_win_config *win_config, struct s3c_reg_data *regs)
{
	struct ion_handle *handle;
	struct fb_var_screeninfo prev_var = win->fbinfo->var;
	struct dma_buf *buf;
	struct s3c_dma_buf_data dma_buf_data;
	unsigned short win_no = win->index;
	int ret;
	size_t buf_size, window_size;
	u8 alpha0, alpha1;

	if (win_config->format >= S3C_FB_PIXEL_FORMAT_MAX) {
		dev_err(sfb->dev, "unknown pixel format %u\n",
				win_config->format);
		return -EINVAL;
	}

	if (win_config->blending >= S3C_FB_BLENDING_MAX) {
		dev_err(sfb->dev, "unknown blending %u\n",
				win_config->blending);
		return -EINVAL;
	}

	if (win_no == 0 && win_config->blending != S3C_FB_BLENDING_NONE) {
		dev_err(sfb->dev, "blending not allowed on window 0\n");
		return -EINVAL;
	}

	if (win_config->w == 0 || win_config->h == 0) {
		dev_err(sfb->dev, "window is size 0 (w = %u, h = %u)\n",
				win_config->w, win_config->h);
		return -EINVAL;
	}

	win->fbinfo->var.red.length = s3c_fb_red_length(win_config->format);
	win->fbinfo->var.red.offset = s3c_fb_red_offset(win_config->format);
	win->fbinfo->var.green.length = s3c_fb_green_length(win_config->format);
	win->fbinfo->var.green.offset = s3c_fb_green_offset(win_config->format);
	win->fbinfo->var.blue.length = s3c_fb_blue_length(win_config->format);
	win->fbinfo->var.blue.offset = s3c_fb_blue_offset(win_config->format);
	win->fbinfo->var.transp.length =
			s3c_fb_transp_length(win_config->format);
	win->fbinfo->var.transp.offset =
			s3c_fb_transp_offset(win_config->format);
	win->fbinfo->var.bits_per_pixel = win->fbinfo->var.red.length +
			win->fbinfo->var.green.length +
			win->fbinfo->var.blue.length +
			win->fbinfo->var.transp.length +
			s3c_fb_padding(win_config->format);

	/*
	regs->win_rgborder[win_no] = s3c_fb_rgborder(win_config->format);
	*/
	if (win_config->w * win->fbinfo->var.bits_per_pixel / 8 < 128) {
		dev_err(sfb->dev, "window must be at least 128 bytes wide (width = %u, bpp = %u)\n",
				win_config->w,
				win->fbinfo->var.bits_per_pixel);
		ret = -EINVAL;
		goto err_invalid;
	}

	if (win_config->stride <
			win_config->w * win->fbinfo->var.bits_per_pixel / 8) {
		dev_err(sfb->dev, "stride shorter than buffer width (stride = %u, width = %u, bpp = %u)\n",
				win_config->stride, win_config->w,
				win->fbinfo->var.bits_per_pixel);
		ret = -EINVAL;
		goto err_invalid;
	}

	if (!s3c_fb_validate_x_alignment(sfb, win_config->x, win_config->w,
			win->fbinfo->var.bits_per_pixel)) {
		ret = -EINVAL;
		goto err_invalid;
	}

	handle = ion_import_dma_buf(sfb->fb_ion_client, win_config->fd);
	if (IS_ERR(handle)) {
		dev_err(sfb->dev, "failed to import fd\n");
		ret = PTR_ERR(handle);
		goto err_invalid;
	}

	buf = dma_buf_get(win_config->fd);
	if (IS_ERR_OR_NULL(buf)) {
		dev_err(sfb->dev, "dma_buf_get() failed: %ld\n",
				PTR_ERR(buf));
		ret = PTR_ERR(buf);
		goto err_buf_get;
	}

	buf_size = s3c_fb_map_ion_handle(sfb, &dma_buf_data, handle,
			buf, win_no);
	if (!buf_size) {
		ret = -ENOMEM;
		goto err_map;
	}

	handle = NULL;
	buf = NULL;

	if (win_config->fence_fd >= 0) {
		dma_buf_data.fence = sync_fence_fdget(win_config->fence_fd);
		if (!dma_buf_data.fence) {
			dev_err(sfb->dev, "failed to import fence fd\n");
			ret = -EINVAL;
			goto err_offset;
		}
	}

	window_size = win_config->stride * (win_config->h);
	if (win_config->offset + window_size - (win_config->offset%win_config->stride) > buf_size) {
		dev_err(sfb->dev, "window goes past end of buffer (width = %u, height = %u, stride = %u, offset = %u, buf_size = %zu)\n",
				win_config->w, win_config->h,
				win_config->stride, win_config->offset,
				buf_size);
		ret = -EINVAL;
		goto err_offset;
	}

	win->fbinfo->fix.smem_start = dma_buf_data.dma_addr
			+ win_config->offset;
	win->fbinfo->fix.smem_len = window_size;
	win->fbinfo->var.xres = win_config->w;
	win->fbinfo->var.xres_virtual = win_config->stride * 8 /
			win->fbinfo->var.bits_per_pixel;
	win->fbinfo->var.yres = win->fbinfo->var.yres_virtual = win_config->h;
	win->fbinfo->var.xoffset = win_config->offset % win_config->stride;
	win->fbinfo->var.yoffset = win_config->offset / win_config->stride;

	win->fbinfo->fix.visual = fb_visual(win->fbinfo->var.bits_per_pixel,
			win->variant.palette_sz);
	win->fbinfo->fix.line_length = win_config->stride;
	win->fbinfo->fix.xpanstep = fb_panstep(win_config->w,
			win->fbinfo->var.xres_virtual);
	win->fbinfo->fix.ypanstep = fb_panstep(win_config->h, win_config->h);

	regs->dma_buf_data[win_no] = dma_buf_data;
	regs->vidw_buf_start[win_no] = win->fbinfo->fix.smem_start;
	regs->vidw_buf_end[win_no] = regs->vidw_buf_start[win_no] +
			window_size;
	regs->vidw_buf_size[win_no] = vidw_buf_size(win_config->w,
			win->fbinfo->fix.line_length,
			win->fbinfo->var.bits_per_pixel);

	regs->vidosd_a[win_no] = vidosd_a(win_config->x, win_config->y);
	regs->vidosd_b[win_no] = vidosd_b(win_config->x, win_config->y,
			win_config->w, win_config->h);

	regs->protection[win_no] = win_config->protection;

	if ((win_config->plane_alpha > 0) && (win_config->plane_alpha < 0xFF)) {
		alpha0 = win_config->plane_alpha;
		alpha1 = 0;
	} else if (win->fbinfo->var.transp.length == 1 &&
			win_config->blending == S3C_FB_BLENDING_NONE) {
		alpha0 = 0xff;
		alpha1 = 0xff;
	} else {
		alpha0 = 0;
		alpha1 = 0xff;
	}

	if (win->variant.has_osd_alpha) {
		regs->vidosd_c[win_no] = vidosd_c(alpha0, alpha0, alpha0);
		regs->vidosd_d[win_no] = vidosd_d(alpha1, alpha1, alpha1);
	}

/*	if (win->variant.osd_size_off) {
		u32 size = win_config->w * win_config->h;
		if (win->variant.has_osd_alpha)
			regs->vidosd_d[win_no] = size;
		else
			regs->vidosd_c[win_no] = size;
	}
*/

	regs->wincon[win_no] = wincon(win->fbinfo->var.bits_per_pixel,
			win->fbinfo->var.transp.length,
			win->fbinfo->var.red.length);
	regs->wincon[win_no] |= s3c_fb_rgborder(win_config->format);

	if (win_no) {
		if ((win_config->plane_alpha > 0) && (win_config->plane_alpha < 0xFF)) {
			if (win->fbinfo->var.transp.length) {
				if (win_config->blending != S3C_FB_BLENDING_NONE)
					regs->wincon[win_no] |= WINCONx_ALPHA_MUL;
			} else {
				regs->wincon[win_no] &= (~WINCONx_ALPHA_SEL);
				if (win_config->blending == S3C_FB_BLENDING_PREMULT)
					win_config->blending = S3C_FB_BLENDING_COVERAGE;
			}
		}
		regs->blendeq[win_no - 1] = blendeq(win_config->blending,
				win->fbinfo->var.transp.length, win_config->plane_alpha);
	}

	return 0;

err_offset:
	s3c_fb_free_dma_buf(sfb, &dma_buf_data);
err_map:
	if (buf)
		dma_buf_put(buf);
err_buf_get:
	if (handle)
		ion_free(sfb->fb_ion_client, handle);
err_invalid:
	win->fbinfo->var = prev_var;
	return ret;
}

#ifdef CONFIG_FB_WINDOW_UPDATE
static int s3c_format_bpp(enum s3c_fb_pixel_format fmt)
{
	int bpp;
	bpp = ((s3c_fb_red_length(fmt) + s3c_fb_green_length(fmt) +
		s3c_fb_blue_length(fmt) + s3c_fb_transp_length(fmt)) >> 3);
	return ((bpp + 1) & (~1));
}

static void s3c_set_win_update_config(struct s3c_fb *sfb,
			struct s3c_fb_win_config *win_config,
			struct s3c_reg_data *regs)
{
	int i;
	struct s3c_fb_win_config *update_config = &win_config[S3C_WIN_UPDATE_IDX];
	struct s3c_fb_win_config temp_config;
	struct s3c_fb_rect r1, r2;
	int offset = 0;

	update_config->w = ((update_config->w + 7) >> 3) << 3;
	if (update_config->x + update_config->w > sfb->lcd_info->xres)
		update_config->x = sfb->lcd_info->xres - update_config->w;

	if ((update_config->state == S3C_FB_WIN_STATE_UPDATE) &&
		((update_config->x != sfb->update_win.x) ||
		(update_config->y != sfb->update_win.y) ||
		(update_config->w != sfb->update_win.w) ||
		(update_config->h != sfb->update_win.h))) {
		sfb->update_win.x = update_config->x;
		sfb->update_win.y = update_config->y;
		sfb->update_win.w = update_config->w;
		sfb->update_win.h = update_config->h;
		sfb->need_update = true;
		regs->need_update = true;
		regs->x[S3C_WIN_UPDATE_IDX] = update_config->x;
		regs->y[S3C_WIN_UPDATE_IDX] = update_config->y;
		regs->w[S3C_WIN_UPDATE_IDX] = update_config->w;
		regs->h[S3C_WIN_UPDATE_IDX] = update_config->h;
#ifdef WINDOW_UPDATE_DEBUG
		pr_info("[WIN_UPDATE]need_update_1: [%d %d %d %d]\n",
			 update_config->x, update_config->y, update_config->w, update_config->h);
#endif
	} else if (sfb->need_update &&
		(update_config->state != S3C_FB_WIN_STATE_UPDATE)) {
		sfb->update_win.x = 0;
		sfb->update_win.y = 0;
		sfb->update_win.w = sfb->lcd_info->xres;
		sfb->update_win.h = sfb->lcd_info->yres;
		sfb->need_update = false;
		regs->need_update = true;
		regs->w[S3C_WIN_UPDATE_IDX] = sfb->lcd_info->xres;
		regs->h[S3C_WIN_UPDATE_IDX] = sfb->lcd_info->yres;
#ifdef WINDOW_UPDATE_DEBUG
		pr_info("[WIN_UPDATE]update2org: [%d %d %d %d]\n",
			 sfb->update_win.x, sfb->update_win.y, sfb->update_win.w, sfb->update_win.h);
#endif
	}

	if (update_config->state != S3C_FB_WIN_STATE_UPDATE)
		return;

	r1.left = update_config->x;
	r1.top = update_config->y;
	r1.right = r1.left + update_config->w - 1;
	r1.bottom = r1.top + update_config->h - 1;

	for (i = 0; i < sfb->variant.nr_windows; i++) {
		struct s3c_fb_win_config *config = &win_config[i];
		if (config->state == S3C_FB_WIN_STATE_DISABLED)
			continue;
		r2.left = config->x;
		r2.top = config->y;
		r2.right = r2.left + config->w - 1;
		r2.bottom = r2.top + config->h - 1;
		if (!s3c_fb_intersect(&r1, &r2)) {
			config->state = S3C_FB_WIN_STATE_DISABLED;
			continue;
		}
		memcpy(&temp_config, config, sizeof(struct s3c_fb_win_config));
		if (update_config->x > config->x)
			config->w = min(update_config->w,
				config->x + config->w - update_config->x);
		else if(update_config->x + update_config->w < config->x + config->w)
			config->w = min(config->w,
				update_config->w + update_config->x - config->x);

		if (update_config->y > config->y)
			config->h = min(update_config->h,
				config->y + config->h - update_config->y);
		else if(update_config->y + update_config->h < config->y + config->h)
			config->h = min(config->h,
				update_config->h + update_config->y - config->y);

		config->x = max(config->x - update_config->x, 0);
		config->y = max(config->y - update_config->y, 0);
		offset = 0;
		if (update_config->y > temp_config.y)
			offset += (update_config->y -  temp_config.y) * config->stride;
		if (update_config->x > temp_config.x)
			offset += (update_config->x - temp_config.x) *\
					 s3c_format_bpp(config->format);
		config->offset += offset;
#ifdef WINDOW_UPDATE_DEBUG
		if (regs->need_update == true)
			pr_info("[WIN_UPDATE]win_idx %d: [%d %d %d %d %d] -> [%d %d %d %d %d] \n", i,
				temp_config.x, temp_config.y, temp_config.w, temp_config.h, temp_config.offset,
				config->x, config->y, config->w, config->h, config->offset);
#endif
	}

	return;
}
#endif

static int s3c_fb_set_win_config(struct s3c_fb *sfb,
		struct s3c_fb_win_config_data *win_data)
{
	struct s3c_fb_win_config *win_config = win_data->config;
#ifdef CONFIG_FB_WINDOW_UPDATE
	struct s3c_fb_win_config_data win_data_org;
#endif
	int ret = 0;
	unsigned short i;
	struct s3c_reg_data *regs;
	struct sync_fence *fence;
	struct sync_pt *pt;
	int fd;
	unsigned int bw = 0;

	fd = get_unused_fd();
	if (fd < 0)
		return fd;

	mutex_lock(&sfb->output_lock);

	if (!sfb->output_on && sfb->power_state == POWER_DOWN) {
		sfb->timeline_max++;
		pt = sw_sync_pt_create(sfb->timeline, sfb->timeline_max);
		fence = sync_fence_create("display", pt);
		sync_fence_install(fence, fd);
		win_data->fence = fd;

		sw_sync_timeline_inc(sfb->timeline, 1);
		goto err;
	}

	regs = kzalloc(sizeof(struct s3c_reg_data), GFP_KERNEL);
	if (!regs) {
		dev_err(sfb->dev, "could not allocate s3c_reg_data\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < sfb->variant.nr_windows; i++) {
		regs->otf_state[i] = win_config[i].state;
		if (WIN_CONFIG_DMA(i)) {
			sfb->windows[i]->prev_fix =
				sfb->windows[i]->fbinfo->fix;
			sfb->windows[i]->prev_var =
				sfb->windows[i]->fbinfo->var;
		}
	}

#ifdef CONFIG_FB_WINDOW_UPDATE
	memcpy(&win_data_org, win_data, sizeof(struct s3c_fb_win_config_data));
	s3c_set_win_update_config(sfb, win_config, regs);
#endif

	for (i = 0; i < sfb->variant.nr_windows && !ret; i++) {
		struct s3c_fb_win_config *config = &win_config[i];
		struct s3c_fb_win *win = sfb->windows[i];

		bool enabled = 0;
		u32 color_map = WINxMAP_MAP | WINxMAP_MAP_COLOUR(0);

		if (WIN_CONFIG_DMA(i)) {
			switch (config->state) {
			case S3C_FB_WIN_STATE_DISABLED:
				break;
			case S3C_FB_WIN_STATE_COLOR:
				enabled = 1;
				color_map |= WINxMAP_MAP_COLOUR(config->color);
				regs->vidosd_a[i] = vidosd_a(config->x, config->y);
				regs->vidosd_b[i] = vidosd_b(config->x, config->y,
						config->w, config->h);
				break;
			case S3C_FB_WIN_STATE_BUFFER:
				ret = s3c_fb_set_win_buffer(sfb, win, config, regs);
				if (!ret) {
					enabled = 1;
					color_map = 0;
				}
				break;
			default:
				dev_warn(sfb->dev, "unrecognized window state %u",
						config->state);
				ret = -EINVAL;
				break;
			}
		} else {
			unsigned short win_no = win->index;
			regs->wincon[win_no] =
				wincon(win->fbinfo->var.bits_per_pixel,
			win->fbinfo->var.transp.length,
			win->fbinfo->var.red.length);
			regs->wincon[win_no] |=
				s3c_fb_rgborder(win_config->format);
			regs->x[i] = config->x;
			regs->y[i] = config->y;
			regs->w[i] = config->w;
			regs->h[i] = config->h;
			enabled = 1;
			color_map = 0;
		}
		if (enabled)
			regs->wincon[i] |= WINCONx_ENWIN;
		else
			regs->wincon[i] &= ~WINCONx_ENWIN;

		/*
		 * Because BURSTLEN field does not have shadow register,
		 * this bit field should be retain always.
		 */
		regs->wincon[i] |= WINCONx_BURSTLEN_8WORD;

		regs->winmap[i] = color_map;

		if (enabled && config->state == S3C_FB_WIN_STATE_BUFFER) {
			bw += s3c_fb_calc_bandwidth(config->w, config->h,
					win->fbinfo->var.bits_per_pixel,
					win->fps);
		}
	}

	regs->bandwidth = bw;

	dev_dbg(sfb->dev, "Total BW = %d Mbits, Max BW per window = %d Mbits\n",
			bw / (1024 * 1024), MAX_BW_PER_WINDOW / (1024 * 1024));

#ifdef CONFIG_DECON_DEVFREQ
	regs->win_overlap_cnt = s3c_fb_get_overlap_cnt(sfb, win_config);
#endif

	if (ret) {
		for (i = 0; i < sfb->variant.nr_windows; i++) {
			if (WIN_CONFIG_DMA(i)) {
				sfb->windows[i]->fbinfo->fix =
					sfb->windows[i]->prev_fix;
				sfb->windows[i]->fbinfo->var =
					sfb->windows[i]->prev_var;
				s3c_fb_free_dma_buf(sfb,
					&regs->dma_buf_data[i]);
			}
		}
		put_unused_fd(fd);
		kfree(regs);
	} else {
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
		disp_pm_gate_lock(get_display_driver(), true);
#endif
		mutex_lock(&sfb->update_regs_list_lock);
		sfb->timeline_max++;
		pt = sw_sync_pt_create(sfb->timeline, sfb->timeline_max);
		fence = sync_fence_create("display", pt);
		sync_fence_install(fence, fd);
		win_data->fence = fd;

		list_add_tail(&regs->list, &sfb->update_regs_list);
		mutex_unlock(&sfb->update_regs_list_lock);
		queue_kthread_work(&sfb->update_regs_worker, &sfb->update_regs_work);
	}

#ifdef CONFIG_FB_WINDOW_UPDATE
	fd = win_data->fence;
	memcpy(win_data, &win_data_org, sizeof(struct s3c_fb_win_config_data));
	win_data->fence = fd;
#endif
err:
	mutex_unlock(&sfb->output_lock);
	return ret;
}

static int s3c_fb_enable_local_path(struct s3c_fb *sfb, int i,
		struct s3c_reg_data *regs, bool enable)
{
#ifdef CONFIG_FB_EXYNOS_FIMD_MC
	struct v4l2_subdev_crop crop;
	int ret = 0;
	int gsc_id = sfb->md->gsc_sd[i]->grp_id;
	struct display_driver *dispdrv = get_display_driver();

	if (enable) {
		crop.rect.left = regs->x[i];
		crop.rect.top = regs->y[i];
		crop.rect.width = regs->w[i];
		crop.rect.height = regs->h[i];
		crop.pad = GSC_OUT_PAD_SOURCE;
		crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;

		if (atomic_read(&sfb->dsd_clk_ref_cnt) == 0) {
			GET_DISPCTL_OPS(dispdrv).enable_display_dsd_clocks(sfb->dev);
			atomic_inc(&sfb->dsd_clk_ref_cnt);
		}
		ret = v4l2_subdev_call(sfb->md->gsc_sd[gsc_id],
				pad, set_crop, NULL, &crop);
		if (ret) {
			dev_err(sfb->dev, "Gsc set_crop failed\n");
			goto err;
		}
	}
	ret = v4l2_subdev_call(sfb->md->gsc_sd[gsc_id], video,
				s_stream, enable);
	if (ret) {
		dev_err(sfb->dev, "GSC s_stream(%d) failed\n", enable);
		goto err;
	}
	if (enable) {
		crop.rect.left = regs->x[i];
		crop.rect.top = regs->y[i];
		crop.rect.width = regs->w[i];
		crop.rect.height = regs->h[i];
		crop.pad = FIMD_PAD_SINK_FROM_GSCALER_SRC;
		crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		ret = v4l2_subdev_call(&sfb->windows[i]->sd,
				pad, set_crop, NULL, &crop);
		if (ret) {
			dev_err(sfb->dev, "Decon set_crop failed\n");
			goto err;
		}
	}
	ret = v4l2_subdev_call(&sfb->windows[i]->sd, video,
				s_stream, enable);
	if (ret) {
		dev_err(sfb->dev, "Decon s_stream(%d) failed\n", enable);
		goto err;
	}
err:
	return ret;
#else
	return 0;
#endif
}

static void s3c_fb_separate_regs_param(struct decon_regs_data *win_regs,
		struct s3c_reg_data *regs, int idx)
{
	win_regs->wincon = regs->wincon[idx];
	win_regs->winmap = regs->winmap[idx];
	win_regs->vidosd_a = regs->vidosd_a[idx];
	win_regs->vidosd_b = regs->vidosd_b[idx];
	win_regs->vidosd_c = regs->vidosd_c[idx];
	win_regs->vidosd_d = regs->vidosd_d[idx];
	win_regs->vidw_buf_start = regs->vidw_buf_start[idx];
	win_regs->vidw_buf_end = regs->vidw_buf_end[idx];
	win_regs->vidw_buf_size = regs->vidw_buf_size[idx];

	if (idx)
		win_regs->blendeq = regs->blendeq[idx - 1];
}

static int s3c_fb_change_frame(struct s3c_fb *sfb,
		struct s3c_reg_data *regs, int i)
{
#ifdef CONFIG_FB_EXYNOS_FIMD_MC
	struct v4l2_subdev_crop crop;
	int ret = 0;
	u32 data = 0;
	int gsc_id = sfb->md->gsc_sd[i]->grp_id;

	crop.rect.left = regs->x[i];
	crop.rect.top = regs->y[i];
	crop.rect.width = regs->w[i];
	crop.rect.height = regs->h[i];
	crop.pad = GSC_OUT_PAD_SOURCE;
	crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;

	ret = v4l2_subdev_call(sfb->md->gsc_sd[gsc_id],
			pad, set_crop, NULL, &crop);
	if (ret) {
		dev_err(sfb->dev, "Gscaler set_crop failed\n");
		goto err;
	}

	crop.pad = FIMD_PAD_SINK_FROM_GSCALER_SRC;
	crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(&sfb->windows[i]->sd,
			pad, set_crop, NULL, &crop);
	if (ret) {
		dev_err(sfb->dev, "Decon set_crop failed\n");
		goto err;
	}
	data = readl(sfb->regs + WINCON(i));
	data &= ~(0x07 << 20); /* Masking */
	data |=  (0x01 << 20); /* On GSC#0 and enable */
	writel(data, sfb->regs + WINCON(i));

	dev_dbg(sfb->dev, "(%d : %d,%d,%d,%d)\n", i, regs->x[i], regs->y[i],
			regs->w[i], regs->h[i]);
err:
	return ret;
#else
	return 0;
#endif
}

static int gsc_subdev_set_sfr_update(struct s3c_fb *sfb, int i)
{
	int ret = 0;
#ifdef CONFIG_FB_EXYNOS_FIMD_MC
	int gsc_id = sfb->md->gsc_sd[i]->grp_id;

	ret = v4l2_subdev_call(sfb->md->gsc_sd[gsc_id], core, ioctl,
			GSC_SFR_UPDATE, NULL);
	if (ret)
		dev_err(sfb->dev, "GSC_SFR_UPDATE failed\n");
#endif

	return ret;
}

static int gsc_subdev_wait_stop(struct s3c_fb *sfb, int i)
{
	int ret = 0;
	struct display_driver *dispdrv;
#ifdef CONFIG_FB_EXYNOS_FIMD_MC
	int gsc_id = sfb->md->gsc_sd[i]->grp_id;

	dispdrv = get_display_driver();

	ret = v4l2_subdev_call(sfb->md->gsc_sd[gsc_id], core, ioctl,
			GSC_WAIT_STOP, NULL);
	if (ret)
		pr_err("GSC_WAIT_STOP failed\n");
#endif

	if (atomic_read(&sfb->dsd_clk_ref_cnt) == 1) {
		GET_DISPCTL_OPS(dispdrv).disable_display_dsd_clocks(sfb->dev);
		atomic_dec(&sfb->dsd_clk_ref_cnt);
	}
	return ret;
}

static void s3c_fb_to_psr_info(struct s3c_fb *sfb, struct decon_psr_info *psr)
{
	psr->psr_mode = sfb->psr_mode;
	psr->trig_mode = sfb->trig_mode;
}

static void s3c_fb_vidout_start(struct decon_psr_info *psr)
{
	decon_reg_start(psr);
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	s5p_mipi_dsi_trigger_unmask();
#endif
}

#ifdef CONFIG_FB_WINDOW_UPDATE
static void decon_reg_set_win_update_config(struct s3c_fb *sfb, struct s3c_reg_data *regs)
{
	int dsim_mpkt;
	struct decon_lcd lcd_info;
	struct decon_lcd *lcd_info_org = decon_get_lcd_info();

	if (regs->need_update) {
		/* TODO: Do we need to wait until the MIPI frame done */
		decon_reg_wait_linecnt_is_zero_timeout(35 * 1000);

		dsim_mpkt = dsim_reg_get_pkt_go_status();
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		if (dsim_mpkt)
			dsim_reg_set_pkt_go_enable(false);
#endif

		/* Partial Command */
		s5p_mipi_dsi_partial_area_command(dsim_for_decon,
			regs->x[S3C_WIN_UPDATE_IDX], regs->y[S3C_WIN_UPDATE_IDX],
			regs->w[S3C_WIN_UPDATE_IDX], regs->h[S3C_WIN_UPDATE_IDX]);

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		if (dsim_mpkt)
			dsim_reg_set_pkt_go_enable(true);
#endif
#ifdef CONFIG_DECON_MIC
		dsim_reg_set_win_update_conf(regs->w[S3C_WIN_UPDATE_IDX], regs->h[S3C_WIN_UPDATE_IDX], true);
		mic_reg_set_win_update_conf(regs->w[S3C_WIN_UPDATE_IDX], regs->h[S3C_WIN_UPDATE_IDX]);
#else
		dsim_reg_set_win_update_conf(regs->w[S3C_WIN_UPDATE_IDX], regs->h[S3C_WIN_UPDATE_IDX], false);
#endif
		lcd_info.xres = regs->w[S3C_WIN_UPDATE_IDX];
		lcd_info.yres = regs->h[S3C_WIN_UPDATE_IDX];
		lcd_info.vsa = lcd_info_org->vsa;
		lcd_info.vbp = lcd_info_org->vbp;
		if (regs->h[S3C_WIN_UPDATE_IDX] + lcd_info_org->vfp < (lcd_info_org->yres / 4))
			lcd_info.vfp = (lcd_info_org->yres / 4) - regs->h[S3C_WIN_UPDATE_IDX];
		else
			lcd_info.vfp = lcd_info_org->vfp;

		if (regs->w[S3C_WIN_UPDATE_IDX] + lcd_info_org->hfp < (lcd_info_org->xres / 4))
			lcd_info.hfp = (lcd_info_org->xres / 4) - regs->w[S3C_WIN_UPDATE_IDX];
		else
			lcd_info.hfp = lcd_info_org->hfp;

		lcd_info.hbp = lcd_info_org->hbp;
		lcd_info.hsa = lcd_info_org->hsa;
		if (soc_is_exynos5430()) {
			/* 5430 supports only 8-bit porch */
			if (lcd_info.hfp > 0xff) {
				lcd_info.hbp += lcd_info.hfp - 0xff;
				lcd_info.hfp = 0xff;
				if (lcd_info.hbp > 0xff) {
					lcd_info.hsa += lcd_info.hbp - 0xff;
					lcd_info.hbp = 0xff;
					if (lcd_info.hsa > 0xff)
						lcd_info.hsa = 0xff;
				}
			}
			if (lcd_info.vfp > 0xff) {
				lcd_info.vbp += lcd_info.vfp - 0xff;
				lcd_info.vfp = 0xff;
				if (lcd_info.vbp > 0xff) {
					lcd_info.vsa += lcd_info.vbp - 0xff;
					lcd_info.vbp = 0xff;
					if (lcd_info.vsa > 0xff)
						lcd_info.vsa = 0xff;
				}
			}
		}
		decon_reg_set_porch(&lcd_info);
#ifdef WINDOW_UPDATE_DEBUG
		pr_info("[WIN_UPDATE]%s : vfp %d vbp %d vsa %d hfp %d hbp %d hsa %d w %d h %d\n", __func__,
					lcd_info.vfp, lcd_info.vbp, lcd_info.vsa,
					lcd_info.hfp, lcd_info.hbp, lcd_info.hsa,
					regs->w[S3C_WIN_UPDATE_IDX], regs->h[S3C_WIN_UPDATE_IDX]);
#endif
	}

}
#endif
static void __s3c_fb_update_regs(struct s3c_fb *sfb, struct s3c_reg_data *regs)
{
	unsigned short i;
	int ret = 0;
	struct decon_regs_data win_regs;
	struct decon_psr_info psr;
	int errwin[S3C_FB_MAX_WIN];

	if (sfb->trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_DISABLE);

	for (i = 0; i < sfb->variant.nr_windows; i++)
		decon_reg_shadow_protect_win(sfb->windows[i]->index, 1);

#ifdef CONFIG_FB_WINDOW_UPDATE
	decon_reg_set_win_update_config(sfb, regs);
#endif
	for (i = 0; i < S3C_FB_MAX_WIN; i++)
		errwin[i] = 0;

	for (i = 0; i < sfb->variant.nr_windows; i++) {
		if (WIN_CONFIG_DMA(i)) {
			s3c_fb_separate_regs_param(&win_regs, regs, i);
			decon_reg_set_regs_data(i, &win_regs);
			sfb->windows[i]->dma_buf_data = regs->dma_buf_data[i];
			set_bit(S3C_FB_READY_TO_LOCAL,
					&sfb->windows[i]->state);
			if (test_bit(S3C_FB_LOCAL,
					&sfb->windows[i]->state)) {
				ret = s3c_fb_enable_local_path(sfb, i,
						regs, false);
				if (ret)
					dev_err(sfb->dev, "fail stop localpath\n");
				set_bit(S3C_FB_DMA,
					&sfb->windows[i]->state);
			}
		} else {
			s3c_fb_separate_regs_param(&win_regs, regs, i);
			decon_reg_set_regs_data(i, &win_regs);
			sfb->windows[i]->dma_buf_data = regs->dma_buf_data[i];
			if (test_bit(S3C_FB_READY_TO_LOCAL,
					&sfb->windows[i]->state)) {
				ret = s3c_fb_enable_local_path(sfb, i,
						regs, true);
				if (ret) {
					dev_err(sfb->dev, "fail start localpath: (%d, %d, %d, %d)\n",
						regs->x[i], regs->y[i], regs->w[i], regs->h[i]);
					decon_reg_clear_win(i);
					errwin[i] = 1;
					continue;
				}
				clear_bit(S3C_FB_READY_TO_LOCAL,
					&sfb->windows[i]->state);
				set_bit(S3C_FB_LOCAL,
					&sfb->windows[i]->state);
				clear_bit(S3C_FB_DMA,
					&sfb->windows[i]->state);
			} else {
				ret = s3c_fb_change_frame(sfb, regs, i);
				if (ret) {
					dev_err(sfb->dev, "failed change frame: (%d, %d, %d, %d)\n",
						regs->x[i], regs->y[i], regs->w[i], regs->h[i]);
					decon_reg_clear_win(i);
					errwin[i] = 1;
				}
			}
		}
	}
	for (i = 0; i < sfb->variant.nr_windows; i++)
		decon_reg_shadow_protect_win(sfb->windows[i]->index, 0);

	if (readl(sfb->regs + DECON_UPDATE_SHADOW) & (1<<0)) {
		int irq_ref = 0;
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
		irq_ref = atomic_read(&sfb->vsync_info.eint_refcount);
#endif
		dev_err(sfb->dev, "Error:0x%x, irq_ref: %d\n",
				readl(sfb->regs + DECON_UPDATE_SHADOW),
				irq_ref);
	}

	for (i = 0; i < sfb->variant.nr_windows; i++) {
		if (!WIN_CONFIG_DMA(i) && !errwin[i]) {
			ret = gsc_subdev_set_sfr_update(sfb, i);
			if (ret)
				dev_err(sfb->dev, "failed set sfr update\n");
		}
	}

	s3c_fb_to_psr_info(sfb, &psr);
	s3c_fb_vidout_start(&psr);

	for (i = 0; i < sfb->variant.nr_windows; i++) {
		if (WIN_CONFIG_DMA(i) &&
			test_bit(S3C_FB_LOCAL, &sfb->windows[i]->state)) {
			ret = gsc_subdev_wait_stop(sfb, i);
			if (ret)
				pr_err("failed set wait stop\n");
			clear_bit(S3C_FB_LOCAL,
				&sfb->windows[i]->state);
		}
	}
}

static void decon_set_protected_content(struct s3c_fb *sfb, bool enable)
{
	int  ret;

	if (sfb->protected_content == enable)
		return;

	ret = exynos_smc(SMC_PROTECTION_SET, 0, DEV_DECON, enable);
	if (ret)
		dev_warn(sfb->dev, "decon protection Enable failed. ret(%d)\n", ret);
	else
		dev_dbg(sfb->dev, "DRM %s\n", enable ? "enabled" : "disabled");

	sfb->protected_content = enable;
}

static void s3c_fd_fence_wait(struct s3c_fb *sfb, struct sync_fence *fence)
{
	int err = sync_fence_wait(fence, 900);
	if (err >= 0)
		return;

	if (err < 0)
		dev_warn(sfb->dev, "error waiting on acquire fence: %d\n", err);
}

static void s3c_fb_update_regs(struct s3c_fb *sfb, struct s3c_reg_data *regs)
{
	struct s3c_dma_buf_data old_dma_bufs[S3C_FB_MAX_WIN];
	bool wait_for_vsync;
	int count = 10;
	int local_cnt = 0;
	int i;
	int protection = 0;
	struct display_driver *dispdrv;

	dispdrv = get_display_driver();

	memset(&old_dma_bufs, 0, sizeof(old_dma_bufs));
	save_decon_operation_time(OPS_RUN_UPDATE_REG_QUEUE_ENTER);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_add_refcount(dispdrv);
#endif
	disp_pm_runtime_get_sync(dispdrv);

	for (i = 0; i < sfb->variant.nr_windows; i++) {
		old_dma_bufs[i] = sfb->windows[i]->dma_buf_data;
		if (WIN_CONFIG_DMA(i)) {
			protection += regs->protection[i];
			if (regs->dma_buf_data[i].fence)
				s3c_fd_fence_wait(sfb,
					regs->dma_buf_data[i].fence);
		} else {
			local_cnt++;
		}
	}
	decon_set_protected_content(sfb, !!protection);

#if defined(CONFIG_DECON_DEVFREQ)
	if (prev_overlap_cnt < regs->win_overlap_cnt)
		exynos5_update_media_layers(TYPE_DECON, regs->win_overlap_cnt);
	if (prev_gsc_local_cnt < local_cnt)
		exynos5_update_media_layers(TYPE_GSCL_LOCAL, local_cnt);
#endif

	do {
		__s3c_fb_update_regs(sfb, regs);
		s3c_fb_wait_for_vsync(sfb, VSYNC_TIMEOUT_MSEC);
		wait_for_vsync = false;

		for (i = 0; i < sfb->variant.nr_windows; i++) {
			if (WIN_CONFIG_DMA(i)) {
				u32 new_start = regs->vidw_buf_start[i];
				u32 shadow_start = readl(sfb->regs +
						SHD_VIDW_BUF_START(i));
				if (unlikely(new_start != shadow_start)) {
					wait_for_vsync = true;
					break;
				}
			}
		}
	} while (wait_for_vsync && count--);
	if (wait_for_vsync) {
		pr_err("%s: failed to update registers\n", __func__);
		for (i = 0; i < sfb->variant.nr_windows; i++)
			if (WIN_CONFIG_DMA(i))
				pr_err("window %d new value %08x register value %08x\n",
				i, regs->vidw_buf_start[i],
				readl(sfb->regs + SHD_VIDW_BUF_START(i)));
	}

	count = 9000;
	decon_reg_wait_for_update_timeout(count * 100);

	if (sfb->trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_DISABLE);

	for (i = 0; i < sfb->variant.nr_windows; i++)
		s3c_fb_free_dma_buf(sfb, &old_dma_bufs[i]);

	disp_pm_runtime_put_sync(dispdrv);
	sw_sync_timeline_inc(sfb->timeline, 1);

#if defined(CONFIG_DECON_DEVFREQ)
	if (prev_gsc_local_cnt > local_cnt)
		exynos5_update_media_layers(TYPE_GSCL_LOCAL, local_cnt);

	if (prev_overlap_cnt > regs->win_overlap_cnt)
		exynos5_update_media_layers(TYPE_DECON, regs->win_overlap_cnt);
	prev_overlap_cnt = regs->win_overlap_cnt;
	prev_gsc_local_cnt = local_cnt;
#endif
	save_decon_operation_time(OPS_RUN_UPDATE_REG_QUEUE_EXIT);
}

static void s3c_fb_update_regs_handler(struct kthread_work *work)
{
	struct s3c_fb *sfb =
			container_of(work, struct s3c_fb, update_regs_work);
	struct s3c_reg_data *data, *next;
	struct list_head saved_list;

	mutex_lock(&sfb->update_regs_list_lock);
	saved_list = sfb->update_regs_list;
	list_replace_init(&sfb->update_regs_list, &saved_list);
	mutex_unlock(&sfb->update_regs_list_lock);

	list_for_each_entry_safe(data, next, &saved_list, list) {
		s3c_fb_update_regs(sfb, data);
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
		disp_pm_gate_lock(get_display_driver(), false);
#endif
		list_del(&data->list);
		kfree(data);
	}
}

static int s3c_fb_get_user_ion_handle(struct s3c_fb *sfb,
				struct s3c_fb_win *win,
				struct s3c_fb_user_ion_client *user_ion_client)
{
	/* Create fd for ion_buffer */
	user_ion_client->fd = ion_share_dma_buf_fd(sfb->fb_ion_client,
					win->dma_buf_data.ion_handle);
	if (user_ion_client->fd < 0) {
		pr_err("ion_share_fd failed\n");
		return user_ion_client->fd;
	}
	return 0;
}
#endif

static int s3c_fb_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	struct s3c_fb_win *win = info->par;
	struct s3c_fb *sfb = win->parent;
	struct fb_var_screeninfo *var = &info->var;
	int ret;
	u32 crtc;
	int offset;

	union {
		struct s3c_fb_user_window user_window;
		struct s3c_fb_user_plane_alpha user_alpha;
		struct s3c_fb_user_chroma user_chroma;
		struct s3c_fb_user_ion_client user_ion_client;
		struct s3c_fb_win_config_data win_data;
		u32 vsync;
	} p;

	if ((sfb->power_state == POWER_DOWN) &&
		(cmd != S3CFB_WIN_CONFIG))
		return 0;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING
	/* Try to scheduled for DISPLAY power_on */
	if (disp_pm_sched_power_on(get_display_driver(), cmd) < 0)
		return 0;
#endif

	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		if (get_user(crtc, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		if (crtc == 0)
			ret = s3c_fb_wait_for_vsync(sfb, VSYNC_TIMEOUT_MSEC);
		else
			ret = -ENODEV;

		break;

	case S3CFB_WIN_POSITION:
		if (copy_from_user(&p.user_window,
				(struct s3c_fb_user_window __user *)arg,
				sizeof(p.user_window))) {
			ret = -EFAULT;
			break;
		}

		if (p.user_window.x < 0)
			p.user_window.x = 0;
		if (p.user_window.y < 0)
			p.user_window.y = 0;

		ret = s3c_fb_set_window_position(info, p.user_window);
		break;

	case S3CFB_WIN_SET_PLANE_ALPHA:
		if (copy_from_user(&p.user_alpha,
				(struct s3c_fb_user_plane_alpha __user *)arg,
				sizeof(p.user_alpha))) {
			ret = -EFAULT;
			break;
		}

		ret = s3c_fb_set_plane_alpha_blending(info, p.user_alpha);
		break;

	case S3CFB_WIN_SET_CHROMA:
		if (copy_from_user(&p.user_chroma,
				   (struct s3c_fb_user_chroma __user *)arg,
				   sizeof(p.user_chroma))) {
			ret = -EFAULT;
			break;
		}

		ret = s3c_fb_set_chroma_key(info, p.user_chroma);
		break;

	case S3CFB_SET_VSYNC_INT:
		if (get_user(p.vsync, (int __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		ret = s3c_fb_set_vsync_int(info, p.vsync);
		break;

#ifdef CONFIG_ION_EXYNOS
	case S3CFB_WIN_CONFIG:
		if (copy_from_user(&p.win_data,
				   (struct s3c_fb_win_config_data __user *)arg,
				   sizeof(p.win_data))) {
			ret = -EFAULT;
			break;
		}

		save_decon_operation_time(OPS_GET_WIN_CONFIG_IOCTL);
		ret = s3c_fb_set_win_config(sfb, &p.win_data);
		if (ret)
			break;

		if (copy_to_user((struct s3c_fb_win_config_data __user *)arg,
				 &p.win_data,
				 sizeof(p.win_data))) {
			ret = -EFAULT;
			break;
		}

		break;

	case S3CFB_GET_ION_USER_HANDLE:
		if (copy_from_user(&p.user_ion_client,
				(struct s3c_fb_user_ion_client __user *)arg,
				sizeof(p.user_ion_client))) {
			ret = -EFAULT;
			break;
		}

		if (s3c_fb_get_user_ion_handle(sfb, win, &p.user_ion_client)) {
			ret = -EFAULT;
			break;
		}

		offset = var->xres_virtual * var->yoffset + var->xoffset;
		offset *= var->bits_per_pixel / 8;
		p.user_ion_client.offset = offset;

		dev_dbg(sfb->dev, "Buffer offset: 0x%x\n",
			p.user_ion_client.offset);

		if (copy_to_user((struct s3c_fb_user_ion_client __user *)arg,
				&p.user_ion_client,
				sizeof(p.user_ion_client))) {
			ret = -EFAULT;
			break;
		}
		ret = 0;
		break;
#endif

	default:
		ret = -ENOTTY;
	}

	return ret;
}

static int s3c_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
#ifdef CONFIG_ION_EXYNOS
	struct s3c_fb_win *win = info->par;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	return dma_buf_mmap(win->dma_buf_data.dma_buf, vma, 0);
#else
	return 0;
#endif
}

static int s3c_fb_release(struct fb_info *info, int user)
{
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_sched_power_on(get_display_driver(), S3CFB_PLATFORM_RESET);
#endif
	return 0;
}

static struct fb_ops s3c_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= s3c_fb_check_var,
	.fb_set_par	= s3c_fb_set_par,
	.fb_blank	= s3c_fb_blank,
	.fb_setcolreg	= s3c_fb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_pan_display	= s3c_fb_pan_display,
	.fb_ioctl	= s3c_fb_ioctl,
	.fb_mmap	= s3c_fb_mmap,
	.fb_release     = s3c_fb_release,
};

static void decon_fb_missing_pixclock(struct decon_fb_videomode *win_mode,
		enum s3c_fb_psr_mode mode)
{
	u64 pixclk = 1000000000000ULL;
	u32 div;
	u32 width, height;

	if (mode == S3C_FB_MIPI_COMMAND_MODE) {
		width = win_mode->videomode.xres;
		height = win_mode->videomode.yres;
	} else {
		width  = win_mode->videomode.left_margin + win_mode->videomode.hsync_len +
			win_mode->videomode.right_margin + win_mode->videomode.xres;
		height = win_mode->videomode.upper_margin + win_mode->videomode.vsync_len +
			win_mode->videomode.lower_margin + win_mode->videomode.yres;
	}

	div = width * height * (win_mode->videomode.refresh ? : 60);

	do_div(pixclk, div);
	win_mode->videomode.pixclock = pixclk;
}

/**
 * s3c_fb_alloc_memory() - allocate display memory for framebuffer window
 * @sfb: The base resources for the hardware.
 * @win: The window to initialise memory for.
 *
 * Allocate memory for the given framebuffer.
 */
static int s3c_fb_alloc_memory(struct s3c_fb *sfb,
					 struct s3c_fb_win *win)
{
	struct s3c_fb_pd_win *windata = win->windata;
	unsigned int real_size, virt_size, size;
	struct fb_info *fbi = win->fbinfo;
	struct ion_handle *handle;
	dma_addr_t map_dma;
	struct dma_buf *buf;
	void *vaddr;
	unsigned int ret;

	dev_dbg(sfb->dev, "allocating memory for display\n");

	real_size = windata->win_mode.videomode.xres * windata->win_mode.videomode.yres;
	virt_size = windata->virtual_x * windata->virtual_y;

	dev_dbg(sfb->dev, "real_size=%u (%u.%u), virt_size=%u (%u.%u)\n",
		real_size, windata->win_mode.videomode.xres, windata->win_mode.videomode.yres,
		virt_size, windata->virtual_x, windata->virtual_y);

	size = (real_size > virt_size) ? real_size : virt_size;
	size *= (windata->max_bpp > 16) ? 32 : windata->max_bpp;
	size /= 8;

	fbi->fix.smem_len = size;
	size = PAGE_ALIGN(size);

	dev_dbg(sfb->dev, "want %u bytes for window[%d]\n", size, win->index);

#if defined(CONFIG_ION_EXYNOS)
#if defined(CONFIG_FB_EXYNOS_FIMD_SYSMMU_DISABLE)
	map_dma = NULL;
#else
	handle = ion_alloc(sfb->fb_ion_client, (size_t)size, 0,
					EXYNOS_ION_HEAP_SYSTEM_MASK, 0);
	if (IS_ERR(handle)) {
		dev_err(sfb->dev, "failed to ion_alloc\n");
		return -ENOMEM;
	}

	buf = ion_share_dma_buf(sfb->fb_ion_client, handle);
	if (IS_ERR_OR_NULL(buf)) {
		dev_err(sfb->dev, "ion_share_dma_buf() failed\n");
		goto err_share_dma_buf;
	}

	vaddr = ion_map_kernel(sfb->fb_ion_client, handle);

	fbi->screen_base = vaddr;

	ret = s3c_fb_map_ion_handle(sfb, &win->dma_buf_data, handle, buf, win->index);
	if (!ret)
		goto err_map;
	map_dma = win->dma_buf_data.dma_addr;

#endif
#else
	fbi->screen_base = dma_alloc_writecombine(sfb->dev, size,
						  &map_dma, GFP_KERNEL);
	if (!fbi->screen_base)
		return -ENOMEM;

	dev_dbg(sfb->dev, "mapped %x to %p\n",
		(unsigned int)map_dma, fbi->screen_base);

	memset(fbi->screen_base, 0x0, size);
#endif
	fbi->fix.smem_start = map_dma;

	return 0;

#ifdef CONFIG_ION_EXYNOS
err_map:
	dma_buf_put(buf);
err_share_dma_buf:
	ion_free(sfb->fb_ion_client, handle);
	return -ENOMEM;
#endif
}

/**
 * s3c_fb_free_memory() - free the display memory for the given window
 * @sfb: The base resources for the hardware.
 * @win: The window to free the display memory for.
 *
 * Free the display memory allocated by s3c_fb_alloc_memory().
 */
static void s3c_fb_free_memory(struct s3c_fb *sfb, struct s3c_fb_win *win)
{
#if defined(CONFIG_ION_EXYNOS)
	s3c_fb_free_dma_buf(sfb, &win->dma_buf_data);
#else
	struct fb_info *fbi = win->fbinfo;

	if (fbi->screen_base)
		dma_free_writecombine(sfb->dev, PAGE_ALIGN(fbi->fix.smem_len),
			      fbi->screen_base, fbi->fix.smem_start);
#endif

}

/**
 * s3c_fb_release_win() - release resources for a framebuffer window.
 * @win: The window to cleanup the resources for.
 *
 * Release the resources that where claimed for the hardware window,
 * such as the framebuffer instance and any memory claimed for it.
 */
static void s3c_fb_release_win(struct s3c_fb *sfb, struct s3c_fb_win *win)
{
	if (win->fbinfo) {
		if (win->fbinfo->cmap.len)
			fb_dealloc_cmap(&win->fbinfo->cmap);
		s3c_fb_free_memory(sfb, win);
		framebuffer_release(win->fbinfo);
	}
}

/**
 * s3c_fb_probe_win() - register an hardware window
 * @sfb: The base resources for the hardware
 * @variant: The variant information for this window.
 * @res: Pointer to where to place the resultant window.
 *
 * Allocate and do the basic initialisation for one of the hardware's graphics
 * windows.
 */
static int s3c_fb_probe_win(struct s3c_fb *sfb, unsigned int win_no,
				      struct s3c_fb_win_variant *variant,
				      struct s3c_fb_win **res)
{
	struct fb_var_screeninfo *var;
	struct decon_fb_videomode *initmode;
	struct s3c_fb_pd_win *windata;
	struct s3c_fb_win *win;
	struct fb_info *fbinfo;
	int palette_size;
	int ret;

	dev_dbg(sfb->dev, "probing window %d, variant %p\n", win_no, variant);

	init_waitqueue_head(&sfb->vsync_info.wait);

	palette_size = variant->palette_sz * 4;

	fbinfo = framebuffer_alloc(sizeof(struct s3c_fb_win) +
				   palette_size * sizeof(u32), sfb->dev);
	if (!fbinfo) {
		dev_err(sfb->dev, "failed to allocate framebuffer\n");
		return -ENOENT;
	}

	windata = sfb->pdata->win[win_no];
	initmode = &windata->win_mode;

	WARN_ON(windata->max_bpp == 0);
	WARN_ON(windata->win_mode.videomode.xres == 0);
	WARN_ON(windata->win_mode.videomode.yres == 0);

	win = fbinfo->par;
	*res = win;
	var = &fbinfo->var;
	win->variant = *variant;
	win->fbinfo = fbinfo;
	win->parent = sfb;
	win->windata = windata;
	win->index = win_no;
	win->palette_buffer = (u32 *)(win + 1);
	set_bit(S3C_FB_DMA, &win->state);
#ifdef CONFIG_ION_EXYNOS
	memset(&win->dma_buf_data, 0, sizeof(win->dma_buf_data));
#endif
	/* alloc only for the default window */
	if (win->index == 0) {
		ret = s3c_fb_alloc_memory(sfb, win);
		if (ret) {
			dev_err(sfb->dev, "failed to allocate display memory\n");
			return ret;
		}
	}

	/* setup the r/b/g positions for the window's palette */
	if (win->variant.palette_16bpp) {
		/* Set RGB 5:6:5 as default */
		win->palette.r.offset = 11;
		win->palette.r.length = 5;
		win->palette.g.offset = 5;
		win->palette.g.length = 6;
		win->palette.b.offset = 0;
		win->palette.b.length = 5;

	} else {
		/* Set 8bpp or 8bpp and 1bit alpha */
		win->palette.r.offset = 16;
		win->palette.r.length = 8;
		win->palette.g.offset = 8;
		win->palette.g.length = 8;
		win->palette.b.offset = 0;
		win->palette.b.length = 8;
	}

	fb_videomode_to_var(&fbinfo->var, &initmode->videomode);

	fbinfo->fix.type	= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.accel	= FB_ACCEL_NONE;
	fbinfo->var.activate	= FB_ACTIVATE_NOW;
	fbinfo->var.vmode	= FB_VMODE_NONINTERLACED;
	fbinfo->var.bits_per_pixel = windata->default_bpp;
	fbinfo->var.width	= windata->width;
	fbinfo->var.height	= windata->height;
	fbinfo->fbops		= &s3c_fb_ops;
	fbinfo->flags		= FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette  = &win->pseudo_palette;

	/* prepare to actually start the framebuffer */

	ret = s3c_fb_check_var(&fbinfo->var, fbinfo);
	if (ret < 0) {
		dev_err(sfb->dev, "check_var failed on initial video params\n");
		return ret;
	}

	/* create initial colour map */
	ret = fb_alloc_cmap(&fbinfo->cmap, win->variant.palette_sz, 1);
	if (ret == 0)
		fb_set_cmap(&fbinfo->cmap, fbinfo);
	else
		dev_err(sfb->dev, "failed to allocate fb cmap\n");

	return 0;
}

/* --------------------For Local path from Gscaler ------------------------*/
#ifdef CONFIG_FB_EXYNOS_FIMD_MC
static inline struct s3c_fb_win *v4l2_subdev_to_s3c_fb_win(struct v4l2_subdev *sd)
{
	/* member instance, name of the parent structure, name of memeber in the parent structure */
	return container_of(sd, struct s3c_fb_win, sd);
}

static int s3c_fb_sd_pad_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *format)
{
	struct s3c_fb_win *win = v4l2_subdev_to_s3c_fb_win(sd);

	/* (width, height) : (xres, yres) */
	win->fbinfo->var.xres = format->format.width;
	win->fbinfo->var.yres = format->format.height;
	return 0;
}

static int s3c_fb_sd_pad_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *format)
{
	struct s3c_fb_win *win = v4l2_subdev_to_s3c_fb_win(sd);
	struct s3c_fb *sfb = win->parent;

	/* (width, height) : (xres, yres) */
	format->format.width = win->fbinfo->var.xres;
	format->format.height = win->fbinfo->var.yres;

	/* FIMD only accept the YUV data via local bus from GSCALER */
	format->format.code = V4L2_MBUS_FMT_YUV8_1X24;

	dev_dbg(sfb->dev, "Get sd pad format (width, height) : (%d, %d)\n",
			format->format.width, format->format.height);
	return 0;
}

static int s3c_fb_sd_set_pad_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_crop *crop)
{
	struct s3c_fb_win *win = v4l2_subdev_to_s3c_fb_win(sd);
	struct s3c_fb *sfb = win->parent;
	int win_no = win->index;
	void __iomem *regs = sfb->regs;
	u32 data;

	if (!s3c_fb_validate_x_alignment(sfb, crop->rect.left,
			crop->rect.width, OTF_OUT_BPP))
		return -EINVAL;

	/* write 'OSD' registers to control position of framebuffer */
	data = vidosd_a(crop->rect.left, crop->rect.top);
	writel(data, regs + VIDOSD_A(win_no));

	data = vidosd_b(crop->rect.left, crop->rect.top,
			crop->rect.width, crop->rect.height);
	writel(data, regs + VIDOSD_B(win_no));

	dev_dbg(sfb->dev, "pad crop (x, y, w, h) : (%d, %d, %d, %d)\n",
			crop->rect.left, crop->rect.top,
			crop->rect.width, crop->rect.height);
	return 0;
}

static int s3c_fb_sd_pad_get_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_crop *crop)
{
	struct s3c_fb_win *win = v4l2_subdev_to_s3c_fb_win(sd);
	struct s3c_fb *sfb = win->parent;

	/* (width, height) : (xres, yres) */
	crop->rect.width = win->fbinfo->var.xres;
	crop->rect.height = win->fbinfo->var.yres;
	dev_dbg(sfb->dev, "Get sd pad crop (width, height) : (%d, %d)\n",
			crop->rect.width, crop->rect.height);

	/* (left, top) : (xoffset, yoffset) */
	crop->rect.left = win->fbinfo->var.xoffset;
	crop->rect.top = win->fbinfo->var.yoffset;
	dev_dbg(sfb->dev, "Get sd pad crop (left, top) : (%d, %d)\n",
			crop->rect.left, crop->rect.top);

	return 0;
}

static int s3c_fb_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
	u32 data = 0;
	struct s3c_fb_win *win = v4l2_subdev_to_s3c_fb_win(sd);
	struct s3c_fb *sfb = win->parent;
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	struct display_driver *dispdrv = get_display_driver();
#endif

	if (enable) {
		data = readl(sfb->regs + WINCON(win->index));
		data &= ~(0x07 << 20); /* Masking */
		data |=  (0x01 << 20); /* On GSC#0 and enable */
		writel(data, sfb->regs + WINCON(win->index));
		if (soc_is_exynos5430() || soc_is_exynos5433()) {
			data = readl(sfb->regs + DECON_UPDATE_SCHEME);
			data |= (0x1 << 31);
			writel(data, sfb->regs + DECON_UPDATE_SCHEME);
		}
		set_bit(S3C_FB_S_STREAM, &sfb->windows[win->index]->state);
	} else {
		data = readl(sfb->regs + WINCON(win->index));
		data &= ~(0x07 << 20); /* Masking == diable local path */
		writel(data, sfb->regs + WINCON(win->index));
		clear_bit(S3C_FB_S_STREAM, &sfb->windows[win->index]->state);
	}
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_gate_lock(dispdrv, enable);
#endif
	dev_dbg(sfb->dev, "window via local path started/stopped : %d\n",
		enable);
	return 0;
}

static long s3c_fb_sd_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct s3c_fb_win *win = v4l2_subdev_to_s3c_fb_win(sd);
	struct s3c_fb *sfb = win->parent;
	struct display_driver *dispdrv;
	struct media_link *link;
	struct media_entity *source;
	struct media_entity *sink;
	int ret = 0;

	dispdrv = get_display_driver();

	switch (cmd) {
	case S3CFB_FLUSH_WORKQUEUE:
		source = &sfb->md->gsc_sd[0]->entity;
		sink = &sd->entity;
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
		disp_pm_gate_lock(dispdrv, true);
#endif
		disp_pm_runtime_get_sync(dispdrv);
		mutex_lock(&sfb->output_lock);
		flush_kthread_worker(&sfb->update_regs_worker);
		mutex_unlock(&sfb->output_lock);
		if (sfb->trig_mode == DECON_HW_TRIG) {
			decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_ENABLE);
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
			s5p_mipi_dsi_trigger_unmask();
#endif
		}

		mutex_unlock(&sfb->output_lock);
		s3c_fb_disable_otf(sfb);
		link = media_entity_find_link(&source->pads[1],
					&sink->pads[0]);
		ret = media_entity_setup_link(link, 0);
		if (ret < 0)
			dev_err(sfb->dev, "fail to link disable\n");

		disp_pm_runtime_put_sync(dispdrv);
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
		disp_pm_gate_lock(dispdrv, false);
#endif
		break;

	case S3CFB_DUMP_REGISTER:
		dispdrv = get_display_driver();
		decon_dump_registers(dispdrv);
		break;
	default:
		dev_err(sfb->dev, "unsupported s3c_fb_sd_ioctl\n");
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_subdev_pad_ops s3c_fb_sd_pad_ops = {
	.set_fmt = s3c_fb_sd_pad_set_fmt,
	.get_fmt = s3c_fb_sd_pad_get_fmt,
	.get_crop = s3c_fb_sd_pad_get_crop,
	.set_crop = s3c_fb_sd_set_pad_crop,
};

static const struct v4l2_subdev_core_ops s3c_fb_sd_core_ops = {
	.ioctl = s3c_fb_sd_ioctl,
};

static const struct v4l2_subdev_video_ops s3c_fb_sd_video_ops = {
	.s_stream = s3c_fb_sd_s_stream,
};

static const struct v4l2_subdev_ops s3c_fb_sd_ops = {
	.pad = &s3c_fb_sd_pad_ops,
	.video = &s3c_fb_sd_video_ops,
	.core = &s3c_fb_sd_core_ops,
};

static int s3c_fb_me_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	int i;
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct s3c_fb_win *win = v4l2_subdev_to_s3c_fb_win(sd);
	struct s3c_fb *sfb = win->parent;

	if (flags & MEDIA_LNK_FL_ENABLED) {
		win->use = 1;
	} else {
		win->use = 0;

		for (i = 0; i < entity->num_links; ++i)
			if (entity->links[i].flags & MEDIA_LNK_FL_ENABLED)
				win->use = 1;
	}

	dev_dbg(sfb->dev, "MC link set up between Window[%d] and Gscaler: \
			flag - %d\n", win->index, flags);
	return 0;
}

/* media entity operations */
static const struct media_entity_operations s3c_fb_me_ops = {
	.link_setup = s3c_fb_me_link_setup,
};

/*---- In probing function (local path) ------*/
static int s3c_fb_register_mc_entity(struct s3c_fb_win *win, struct exynos_md *md)
{
	int ret;
	struct s3c_fb *sfb = win->parent;
	struct v4l2_subdev *sd = &win->sd;
	struct media_pad *pads = win->pads;
	struct media_entity *me = &sd->entity;

	/* Init a window of fimd as a sub-device */
	v4l2_subdev_init(sd, &s3c_fb_sd_ops);
	sd->owner = THIS_MODULE;
	snprintf(sd->name, V4L2_SUBDEV_NAME_SIZE,
			"s3c-fb-window%d", win->index);

	/* fimd sub-devices can be opened in user space */
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	/* FIMD takes a role of SINK between FIMD and Gscaler */
	pads[FIMD_PAD_SINK_FROM_GSCALER_SRC].flags = MEDIA_PAD_FL_SINK;
	me->ops = &s3c_fb_me_ops;

	/* Init a sub-device as an entity */
	ret = media_entity_init(me, FIMD_PADS_NUM, pads, 0);
	if (ret) {
		dev_err(sfb->dev, "failed to initialize media entity in FIMD\n");
		return ret;
	}

	ret = v4l2_device_register_subdev(&md->v4l2_dev, sd);
	if (ret) {
		dev_err(sfb->dev, "failed to register FIMD Window subdev\n");
		return ret;
	}

	dev_dbg(sfb->dev, "FIMD Winow[%d] MC entity init & subdev registered: %s\n",
			win->index, sd->name);

	return 0;
}

static int s3c_fb_register_mc_components(struct s3c_fb_win *win)
{
	int ret;
	struct exynos_md *md;
	struct s3c_fb *sfb = win->parent;

	if (sfb->md == NULL) {
		md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);

		if (!md) {
			dev_err(sfb->dev, "failed to get output media device\n");
			return -ENODEV;
		}
		sfb->md = md;
	}

	ret = s3c_fb_register_mc_entity(win, sfb->md);
	if (ret)
		return ret;

	return 0;
}

static int s3c_fb_register_mc_subdev_nodes(struct s3c_fb *sfb)
{
	int ret;

	/* This function is for exposing sub-devices nodes to user space
	 * in case of marking with V4L2_SUBDEV_FL_HAS_DEVNODE flag.
	 *
	 * And it depends on probe sequence
	 * because v4l2_dev ptr is shared all of output devices below
	 *
	 * probe sequence of output devices
	 * output media device -> gscaler -> window in fimd
	 */
	ret = v4l2_device_register_subdev_nodes(&sfb->md->v4l2_dev);
	if (ret) {
		dev_err(sfb->dev, "failed to make nodes for subdev\n");
		return ret;
	}

	dev_dbg(sfb->dev, "Register V4L2 subdev nodes for FIMD\n");

	return 0;
}

static int s3c_fb_create_mc_links(struct s3c_fb_win *win)
{
	int ret;
	int flags;
	char err[80];
	struct exynos_md *md;
	struct s3c_fb *sfb = win->parent;
	int i;

	if (win->use)
		flags = MEDIA_LNK_FL_ENABLED;
	else
		flags = 0;

	/* link creation between pads: Gscaler[1] -> Window[0] */
	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);

	for (i = 0; i < MAX_GSC_SUBDEV; i++) {
		ret = media_entity_create_link(&md->gsc_sd[i]->entity,
				GSC_OUT_PAD_SOURCE, &win->sd.entity,
				FIMD_PAD_SINK_FROM_GSCALER_SRC, 0);
		if (ret) {
			snprintf(err, sizeof(err), "%s --> %s",
				md->gsc_sd[i]->entity.name,
				win->sd.entity.name);
				goto mc_link_create_fail;
		}

	}

	dev_dbg(sfb->dev, "A link between Gscaler and window[%d] is created \
		successfully\n", win->index);

	return 0;

mc_link_create_fail:
	dev_err(sfb->dev, "failed to create a link between Gscaler and \
		window[%d]: %s\n", win->index, err);
	return ret;
}

static void s3c_fb_unregister_mc_entities(struct s3c_fb_win *win)
{
	v4l2_device_unregister_subdev(&win->sd);
}
#endif

/* --------------------For Writeback to Scaler ------------------------*/
#ifdef CONFIG_FB_EXYNOS_FIMD_MC_WB
static inline struct s3c_fb *v4l2_subdev_to_s3c_fb(struct v4l2_subdev *sd)
{
	/* member instance, name of the parent structure, name of memeber
	   in the parent structure */
	return container_of(sd, struct s3c_fb, sd_wb);
}
static int s3c_fb_sd_wb_s_stream(struct v4l2_subdev *sd_wb, int enable)
{
	struct s3c_fb *sfb = v4l2_subdev_to_s3c_fb(sd_wb);
	u32 ret, data;
	u32 vidoutcon = readl(sfb->regs + VIDOUTCON0);

	if (enable)
		vidoutcon |= VIDOUTCON0_WB_F;
	else
		vidoutcon &= ~VIDOUTCON0_WB_F;

	ret = s3c_fb_wait_for_vsync(sfb, VSYNC_TIMEOUT_MSEC);
	if (ret) {
		dev_err(sfb->dev, "wait timeout(writeback) : %s\n", __func__);
		return ret;
	}

	writel(vidoutcon, sfb->regs + VIDOUTCON0);

	dev_dbg(sfb->dev, "Get the writeback started/stopped : %d\n", enable);
	return 0;
}

static int s3c_fb_sd_wb_pad_get_fmt(struct v4l2_subdev *sd_wb, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *format)
{
	struct s3c_fb *sfb = v4l2_subdev_to_s3c_fb(sd_wb);
	int default_win = sfb->pdata->default_win;

	/* (width, height) : (xres, yres) */
	format->format.width = sfb->windows[default_win]->fbinfo->var.xres;
	format->format.height = sfb->windows[default_win]->fbinfo->var.yres;

	/* FIMD writes the video data back to GSCALER */
	format->format.code = V4L2_MBUS_FMT_XRGB8888_4X8_LE;

	dev_dbg(sfb->dev, "Get sd wb pad format (width, height) : (%d, %d)\n",
			format->format.width, format->format.height);
	return 0;
}

static const struct v4l2_subdev_pad_ops s3c_fb_sd_wb_pad_ops = {
	.get_fmt = s3c_fb_sd_wb_pad_get_fmt,
};

static const struct v4l2_subdev_video_ops s3c_fb_sd_wb_video_ops = {
	.s_stream = s3c_fb_sd_wb_s_stream,
};

static const struct v4l2_subdev_ops s3c_fb_sd_wb_ops = {
	.video = &s3c_fb_sd_wb_video_ops,
	.pad = &s3c_fb_sd_wb_pad_ops,
};

static int s3c_fb_me_wb_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	int i;
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct s3c_fb *sfb = v4l2_subdev_to_s3c_fb(sd);

	if (flags & MEDIA_LNK_FL_ENABLED) {
		sfb->use_wb = 1;
		if (local->index == FIMD_WB_PAD_SRC_TO_GSCALER_SINK)
			sfb->local_wb = 1;
	} else {
		if (local->index == FIMD_WB_PAD_SRC_TO_GSCALER_SINK)
			sfb->local_wb = 0;
		sfb->use_wb = 0;

		for (i = 0; i < entity->num_links; ++i)
			if (entity->links[i].flags & MEDIA_LNK_FL_ENABLED)
				sfb->use_wb = 1;
	}

	dev_dbg(sfb->dev, "MC WB link set up between FIMD and Gscaler: \
			flag - %d\n", flags);
	return 0;
}

/* media entity operations */
static const struct media_entity_operations s3c_fb_me_wb_ops = {
	.link_setup = s3c_fb_me_wb_link_setup,
};

/* --- In probing function (writeback) ---*/
static int s3c_fb_register_mc_wb_entity(struct s3c_fb *sfb, struct exynos_md *md_wb)
{
	int ret;

	struct v4l2_subdev *sd_wb = &sfb->sd_wb;
	struct media_pad *pads_wb = &sfb->pads_wb;
	struct media_entity *me_wb = &sd_wb->entity;

	/* Init a window of fimd as a sub-device */
	v4l2_subdev_init(sd_wb, &s3c_fb_sd_wb_ops);
	sd_wb->owner = THIS_MODULE;
#ifdef CONFIG_ARCH_EXYNOS4
		snprintf(sd_wb->name, sizeof(sd_wb->name), "s5p-fimd0");
#else
		snprintf(sd_wb->name, sizeof(sd_wb->name), "s5p-fimd1");
#endif

	/* fimd sub-devices can be opened in user space */
	sd_wb->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	/* FIMD takes a role of sources between FIMD and Gscaler */
	pads_wb->flags = MEDIA_PAD_FL_SOURCE;
	me_wb->ops = &s3c_fb_me_wb_ops;

	/* Init a sub-device as an entity */
	ret = media_entity_init(me_wb, FIMD_WB_PADS_NUM, pads_wb, 0);
	if (ret) {
		dev_err(sfb->dev, "failed to initialize media entity in FIMD WB\n");
		return ret;
	}

	ret = v4l2_device_register_subdev(&md_wb->v4l2_dev, sd_wb);
	if (ret) {
		dev_err(sfb->dev, "failed to register FIMD WB subdev\n");
		return ret;
	}

	dev_dbg(sfb->dev, "FIMD1 WB MC entity init & subdev registered: %s\n", sd_wb->name);

	return 0;
}

static int fimd_get_media_info(struct device *dev, void *p)
{
	struct exynos_md **mdev = p;
	struct platform_device *pdev = to_platform_device(dev);

	mdev[pdev->id] = dev_get_drvdata(dev);

	if (!mdev[pdev->id])
		return -ENODEV;

	return 0;
}

static int s3c_fb_register_mc_wb_components(struct s3c_fb *sfb)
{
	int ret;
	struct exynos_md *mdev[2] = {NULL, NULL};
	struct device_driver *driver;

	driver = driver_find(MDEV_MODULE_NAME, &platform_bus_type);
	if (!driver)
		dev_err(sfb->dev, "MC driver not found in s3c_fb\n");

	ret = driver_for_each_device(driver, NULL, &mdev[0],
		fimd_get_media_info);

	sfb->md_wb = mdev[MDEV_CAPTURE];

	/* Local paths have been set up only between FIMD1 and Gscaler0~3 */
	ret = s3c_fb_register_mc_wb_entity(sfb, sfb->md_wb);
	if (ret)
		return ret;

	return 0;
}

static int s3c_fb_create_mc_wb_links(struct s3c_fb *sfb)
{
	int ret, i;
	int flags;
	char err[80];

	if (sfb->use_wb)
		flags = MEDIA_LNK_FL_ENABLED;
	else
		flags = 0;

	/* FIMD1 --> Gscaler 0, Gscaler 1, Gscaler 2, or Gscaler 3 */
	for (i = 0; i < MAX_GSC_SUBDEV; ++i) {
		if (sfb->md_wb->gsc_cap_sd[i] != NULL) {
			ret = media_entity_create_link(&sfb->sd_wb.entity, /* source */
				FIMD_WB_PAD_SRC_TO_GSCALER_SINK,
				&sfb->md_wb->gsc_cap_sd[i]->entity, /* sink */
				GSC_CAP_PAD_SINK, 0);
			if (ret) {
				sprintf(err, "%s --> %s",
					sfb->md_wb->gsc_cap_sd[i]->entity.name,
					sfb->sd_wb.entity.name);
					goto mc_wb_link_create_fail;
			}
		}

		dev_dbg(sfb->dev, "A link between FIMD1 and Gscaler[%d] is created \
			successfully\n", i);
	}

	return 0;

mc_wb_link_create_fail:
	dev_err(sfb->dev, "failed to create a link  FIMD1 and Gscaler[%d] \
		%s\n", i, err);
	return ret;
}

static int s3c_fb_register_mc_subdev_wb_nodes(struct s3c_fb *sfb)
{
	int ret;

	/* This function is for exposing sub-devices nodes to user space
	 * in case of marking with V4L2_SUBDEV_FL_HAS_DEVNODE flag.
	 *
	 * And it depends on probe sequence
	 * because v4l2_dev ptr is shared all of output devices below
	 *
	 * probe sequence of output devices
	 * output media device -> gscaler -> window in fimd
	 */
	ret = v4l2_device_register_subdev_nodes(&sfb->md_wb->v4l2_dev);
	if (ret) {
		dev_err(sfb->dev, "failed to make nodes for subdev\n");
		return ret;
	}

	dev_dbg(sfb->dev, "Register V4L2 subdev nodes for FIMD\n");

	return 0;
}

static void s3c_fb_unregister_mc_wb_entities(struct s3c_fb *sfb)
{
	v4l2_device_unregister_subdev(&sfb->sd_wb);
}
#endif

static int s3c_fb_wait_for_vsync_thread(void *data)
{
	struct s3c_fb *sfb = data;

	while (!kthread_should_stop()) {
		ktime_t timestamp = sfb->vsync_info.timestamp;
		int ret = wait_event_interruptible(sfb->vsync_info.wait,
			!ktime_equal(timestamp, sfb->vsync_info.timestamp) &&
			sfb->vsync_info.active);

		if (!ret) {
			sysfs_notify(&sfb->dev->kobj, NULL, "vsync");
		}
	}

	return 0;
}

/*------------------------------------------------------------------ */

static ssize_t s3c_fb_vsync_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s3c_fb *sfb = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%llu\n",
			ktime_to_ns(sfb->vsync_info.timestamp));
}

static DEVICE_ATTR(vsync, S_IRUGO, s3c_fb_vsync_show, NULL);

static ssize_t s3c_fb_psr_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s3c_fb *sfb = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%d\n", sfb->psr_mode);
}

static DEVICE_ATTR(psr_info, S_IRUGO, s3c_fb_psr_info, NULL);

#ifdef CONFIG_DEBUG_FS

static int s3c_fb_debugfs_show(struct seq_file *f, void *offset)
{
	unsigned long flags;
	struct s3c_fb *sfb = f->private;
	struct s3c_fb_debug *debug_data = kzalloc(sizeof(struct s3c_fb_debug),
			GFP_KERNEL);

	if (!debug_data) {
		seq_printf(f, "kmalloc() failed; can't generate file\n");
		return -ENOMEM;
	}

	spin_lock_irqsave(&sfb->slock, flags);
	memcpy(debug_data, &sfb->debug_data, sizeof(sfb->debug_data));
	spin_unlock_irqrestore(&sfb->slock, flags);

	seq_printf(f, "%u FIFO underflows\n", debug_data->num_timestamps);
	if (debug_data->num_timestamps) {
		unsigned int i;
		unsigned int temp = S3C_FB_DEBUG_FIFO_TIMESTAMPS;
		unsigned int n = min(debug_data->num_timestamps, temp);

		seq_printf(f, "Last %u FIFO underflow timestamps:\n", n);
		for (i = 0; i < n; i++) {
			unsigned int idx = (debug_data->first_timestamp + i) %
					S3C_FB_DEBUG_FIFO_TIMESTAMPS;
			ktime_t timestamp = debug_data->fifo_timestamps[idx];
			seq_printf(f, "\t%lld\n", ktime_to_ns(timestamp));
		}

		seq_printf(f, "Registers at time of last underflow:\n");
		for (i = 0; i < S3C_FB_DEBUG_REGS_SIZE; i += 32) {
			unsigned char buf[128];
			hex_dump_to_buffer(debug_data->regs_at_underflow + i,
					32, 32, 4, buf,
					sizeof(buf), false);
			seq_printf(f, "%.8x: %s\n", i, buf);
		}
	}

	kfree(debug_data);
	return 0;
}

ssize_t s3c_fb_debugfs_write(struct file *file, const char *userbuf, size_t count, loff_t *off)
{
	char buf[20], *p;
	struct display_driver *dispdrv;

	dispdrv = get_display_driver();

	memset(buf,0x00,sizeof(buf));
	if (copy_from_user(buf, userbuf, min(count, sizeof(buf))))
		return -EFAULT;

	p = buf;
	while(*p) {
		if (*p == 0x0A)
			*p = 0x00;
		p++;
	}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	debug_function(dispdrv, buf);
#endif
	if (!strcmp(buf, "ops_dump"))
		dump_decon_operation_time();

	return count;
}

static int s3c_fb_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, s3c_fb_debugfs_show, inode->i_private);
}

static const struct file_operations s3c_fb_debugfs_fops = {
	.owner		= THIS_MODULE,
	.open		= s3c_fb_debugfs_open,
	.read		= seq_read,
	.write		= s3c_fb_debugfs_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int s3c_fb_debugfs_init(struct s3c_fb *sfb)
{
	sfb->debug_dentry = debugfs_create_file("s3c-fb", S_IRUGO|S_IWUSR, NULL, sfb,
			&s3c_fb_debugfs_fops);
	if (IS_ERR_OR_NULL(sfb->debug_dentry)) {
		sfb->debug_dentry = NULL;
		dev_err(sfb->dev, "debugfs_create_file() failed\n");
		return -EIO;
	}

	return 0;
}

#ifdef S3CFB_DEVICE_DRIVER_TYPE_ENABLE
static void s3c_fb_debugfs_cleanup(struct s3c_fb *sfb)
{
	if (sfb->debug_dentry)
		debugfs_remove(sfb->debug_dentry);
}
#endif

#else

static int s3c_fb_debugfs_init(struct s3c_fb *sfb) { return 0; }

#endif
/*
static int s3c_fb_inquire_version(struct s3c_fb *sfb)
{
	struct s3c_fb_platdata *pd = sfb->pdata;

	return pd->ip_version == EXYNOS5_813 ? 0 : 1;
}
*/




#ifdef CONFIG_FB_EXYNOS_FIMD_V8
static void s3c_fb_dump_registers(struct s3c_fb *sfb)
{
	pr_err("dumping registers\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4, sfb->regs,
			0x0280, false);
	pr_err("...\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			sfb->regs + SHD_VIDW_BUF_START(0), 0x74, false);
	pr_err("...\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			sfb->regs + 0x20000, 0x20, false);
}
#endif
#if 0
static int s3c_fb_clear_fb(struct s3c_fb *sfb,
		struct dma_buf *dest_buf, size_t size)
{
	size_t i;

	int ret = dma_buf_begin_cpu_access(dest_buf, 0, dest_buf->size,
			DMA_TO_DEVICE);
	if (ret < 0) {
		dev_warn(sfb->dev, "dma_buf_begin_cpu_access() failed while clearing framebuffer: %u\n",
				ret);
		return ret;
	}

	for (i = 0; i < dest_buf->size / PAGE_SIZE; i++) {
		void *to_virt = dma_buf_kmap(dest_buf, i);
		memset(to_virt, 0, PAGE_SIZE);
		dma_buf_kunmap(dest_buf, i, to_virt);
	}

	dma_buf_end_cpu_access(dest_buf, 0, size, DMA_TO_DEVICE);
	return 0;
}
#endif
#ifdef CONFIG_FB_I80_COMMAND_MODE
static int decon_fb_config_eint_for_te(struct s3c_fb *sfb)
{
	int ret;
	int gpio;
	struct device *dev = sfb->dev;

	gpio = of_get_gpio(dev->of_node, 0);
	if (gpio < 0) {
		dev_err(dev, "failed to get proper gpio number\n");
		return -EINVAL;
	}

	atomic_set(&sfb->vsync_info.eint_refcount, 1);
	sfb->irq_no = gpio_to_irq(gpio);
	ret = devm_request_irq(dev, sfb->irq_no, decon_fb_isr_for_eint,
			  IRQF_TRIGGER_RISING, "s3c_fb", sfb);

	return ret;
}
#endif

static struct decon_lcd *decon_parse_lcd_info(struct s3c_fb_platdata *pd)
{
	int i;
	struct decon_lcd *lcd_info = decon_get_lcd_info();

	for (i = 0; i < S3C_FB_MAX_WIN; i++) {
		pd->win[i]->win_mode.videomode.left_margin = lcd_info->hbp;
		pd->win[i]->win_mode.videomode.right_margin = lcd_info->hfp;
		pd->win[i]->win_mode.videomode.upper_margin = lcd_info->vbp;
		pd->win[i]->win_mode.videomode.lower_margin = lcd_info->vfp;
		pd->win[i]->win_mode.videomode.hsync_len = lcd_info->hsa;
		pd->win[i]->win_mode.videomode.vsync_len = lcd_info->vsa;
		pd->win[i]->win_mode.videomode.xres = lcd_info->xres;
		pd->win[i]->win_mode.videomode.yres = lcd_info->yres;
		pd->win[i]->virtual_x = lcd_info->xres;
		pd->win[i]->virtual_y = lcd_info->yres * 2;
		pd->win[i]->width = lcd_info->width;
		pd->win[i]->height = lcd_info->height;
	}

	return lcd_info;
}

static void s3c_fb_to_init_param(struct s3c_fb *sfb, struct decon_init_param *p)
{
	p->lcd_info = sfb->lcd_info;
	p->psr.psr_mode = sfb->psr_mode;
	p->psr.trig_mode = sfb->trig_mode;
	p->nr_windows = sfb->variant.nr_windows;
}

#if defined(CONFIG_S5P_LCD_INIT)
static void decon_set_regs_param(struct fb_info *info, struct decon_regs_data *regs)
{
	u32 line_length;

	regs->wincon = WINCONx_WSWP | WINCONx_BPPMODE_24BPP_888 |
					WINCONx_BPPMODE_24BPP_888;
	regs->winmap = 0x0;
	regs->vidosd_a = vidosd_a(0, 0);
	regs->vidosd_b = vidosd_b(0, 0, info->var.xres, info->var.yres);
	regs->vidosd_c = vidosd_c(0x0, 0x0, 0x0);
	regs->vidosd_d = vidosd_d(0xff, 0xff, 0xff);
	regs->vidw_buf_start = info->fix.smem_start;
	line_length = fb_linelength(info->var.xres_virtual, info->var.bits_per_pixel);
	regs->vidw_buf_end = info->fix.smem_start + line_length * info->var.yres;
	regs->vidw_buf_size = vidw_buf_size(info->var.xres, line_length,
			info->var.bits_per_pixel);
}
#endif

int create_decon_display_controller(struct platform_device *pdev)
{
	struct s3c_fb_driverdata *fbdrv;
	struct device *dev = &pdev->dev;
	struct s3c_fb_platdata *pd;
	struct s3c_fb *sfb;
	struct fb_info *fbinfo;
	struct display_driver *dispdrv;
	struct decon_lcd *lcd_info;
	struct decon_init_param p;
	int win;
	int default_win;
	int i;
	int ret = 0;
#if defined(CONFIG_S5P_LCD_INIT)
	struct decon_psr_info psr;
	struct decon_regs_data win_regs;
#endif

	dispdrv = get_display_driver();

	fbdrv = dispdrv->dt_ops.get_display_drvdata();
	pd = dispdrv->dt_ops.get_display_platdata();
	if (!pd) {
		dev_err(dev, "no platform data specified\n");
		return -EINVAL;
	}

	lcd_info = decon_parse_lcd_info(pd);

	if (fbdrv->variant.nr_windows > S3C_FB_MAX_WIN) {
		dev_err(dev, "too many windows, cannot attach\n");
		return -EINVAL;
	}

	sfb = devm_kzalloc(dev, sizeof(struct s3c_fb), GFP_KERNEL);
	if (!sfb) {
		dev_err(dev, "no memory for framebuffers\n");
		return -ENOMEM;
	}

	dev_dbg(dev, "allocate new framebuffer %p\n", sfb);

	sfb->dev = dev;
	sfb->pdata = pd;
	sfb->variant = fbdrv->variant;
	sfb->lcd_info = lcd_info;

#if defined(CONFIG_FB_I80_COMMAND_MODE) || defined(CONFIG_FB_I80IF)
	sfb->psr_mode = S3C_FB_MIPI_COMMAND_MODE;
#if defined(CONFIG_FB_I80_HW_TRIGGER)
	sfb->trig_mode = DECON_HW_TRIG;
#else
	sfb->trig_mode = DECON_SW_TRIG;
#endif
#elif defined(CONFIG_S5P_DP_PSR)
	sfb->psr_mode = S3C_FB_DP_PSR_MODE;
#else
	sfb->psr_mode = S3C_FB_VIDEO_MODE;
#endif
	dev_info(dev, "PSR mode is %d(0: VIDEO, 1: DP, 2: MIPI)\n", sfb->psr_mode);

	spin_lock_init(&sfb->slock);
	mutex_init(&sfb->output_lock);

	ret = s3c_fb_debugfs_init(sfb);
	if (ret)
		dev_warn(dev, "failed to initialize debugfs entry\n");

#ifdef CONFIG_ION_EXYNOS
	INIT_LIST_HEAD(&sfb->update_regs_list);
	mutex_init(&sfb->update_regs_list_lock);

	init_kthread_worker(&sfb->update_regs_worker);

	sfb->update_regs_thread = kthread_run(kthread_worker_fn,
			&sfb->update_regs_worker, "s3c-fb-update");
	if (IS_ERR(sfb->update_regs_thread)) {
		int err = PTR_ERR(sfb->update_regs_thread);
		sfb->update_regs_thread = NULL;

		dev_err(dev, "failed to run update_regs thread\n");
		return err;
	}
	init_kthread_work(&sfb->update_regs_work, s3c_fb_update_regs_handler);

	sfb->timeline = sw_sync_timeline_create("s3c-fb");
	sfb->timeline_max = 1;
	/* XXX need to cleanup on errors */
#endif
	pm_runtime_enable(sfb->dev);

	sfb->regs = devm_request_and_ioremap(dev, dispdrv->decon_driver.regs);
	if (!sfb->regs) {
		dev_err(dev, "failed to map registers\n");
		ret = -ENXIO;
		goto resource_exception;
	}

#ifndef CONFIG_FB_I80_COMMAND_MODE
	ret = devm_request_irq(dev, dispdrv->decon_driver.irq_no, s3c_fb_irq,
			  0, "s3c_fb", sfb);
	if (ret) {
		dev_err(dev, "video irq request failed\n");
		goto resource_exception;
	}
#endif
	ret = devm_request_irq(dev, dispdrv->decon_driver.fifo_irq_no,
	s3c_fb_irq, 0, "s3c_fb", sfb);
	if (ret) {
		dev_err(dev, "fifo irq request failed\n");
		goto resource_exception;
	}

#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_HW_TRIGGER)
	ret = devm_request_irq(dev, dispdrv->decon_driver.i80_irq_no,
		s3c_fb_irq, 0, "s3c_fb", sfb);
	if (ret) {
		dev_err(dev, "failed to request i80 framedone irq\n");
		goto resource_exception;
	}
#endif

	dev_dbg(dev, "got resources (regs %p), probing windows\n", sfb->regs);

	dispdrv->decon_driver.sfb = sfb;
	platform_set_drvdata(pdev, sfb);
	disp_pm_runtime_get_sync(dispdrv);

	/* setup gpio and output polarity controls */
	sfb->power_state = POWER_ON;

	/* reference assignment for power status */
	ref_power_status = &sfb->power_state;

	mutex_init(&sfb->vsync_info.irq_lock);

#ifdef CONFIG_ION_EXYNOS
	sfb->fb_ion_client = ion_client_create(ion_exynos, "fimd");
	if (IS_ERR(sfb->fb_ion_client)) {
		dev_err(sfb->dev, "failed to ion_client_create\n");
		goto resource_exception;
	}

	/* setup vmm */
	exynos_create_iovmm(&pdev->dev, 5, 0);
#endif

	default_win = sfb->pdata->default_win;
	for (win = 0; win < fbdrv->variant.nr_windows; win++) {
		if (!pd->win[win])
			continue;

		if (!pd->win[win]->win_mode.videomode.pixclock)
			decon_fb_missing_pixclock(&pd->win[win]->win_mode, sfb->psr_mode);
	}

#if defined(CONFIG_DECON_DEVFREQ)
	exynos5_update_media_layers(TYPE_RESOLUTION,
			sfb->lcd_info->xres * sfb->lcd_info->yres *\
			4 * sfb->lcd_info->fps);
	exynos5_update_media_layers(TYPE_DECON, 1);
	exynos5_update_media_layers(TYPE_GSCL_LOCAL, 0);
	prev_overlap_cnt = 1;
	prev_gsc_local_cnt = 0;
#endif

	/* we have the register setup, start allocating framebuffers */
	for (i = 0; i < fbdrv->variant.nr_windows; i++) {
		win = i;
		if (i == 0)
			win = default_win;
		if (i == default_win)
			win = 0;

		ret = s3c_fb_probe_win(sfb, win, fbdrv->win[win],
				       &sfb->windows[win]);
		if (ret < 0) {
			dev_err(dev, "failed to create window %d\n", win);
			if (win== 0)
				s3c_fb_release_win(sfb, sfb->windows[0]);
			goto resource_exception;
		}

#ifdef CONFIG_FB_EXYNOS_FIMD_MC
		/* register a window subdev as entity */
		ret = s3c_fb_register_mc_components(sfb->windows[win]);
		if (ret) {
			dev_err(sfb->dev, "failed to register s3c_fb mc entities\n");
			goto err_mc_entity_create_fail;
		}

		/* create links connected between gscaler and fimd */
		ret = s3c_fb_create_mc_links(sfb->windows[win]);
		if (ret) {
			dev_err(sfb->dev, "failed to create s3c_fb mc links\n");
			goto err_mc_link_create_fail;
		}
#endif
	}
#ifdef CONFIG_FB_EXYNOS_FIMD_MC
	ret = s3c_fb_register_mc_subdev_nodes(sfb);
	if (ret) {
			dev_err(sfb->dev, "failed to register s3c_fb mc subdev node\n");
			goto err_mc_link_create_fail;
	}
#endif

#ifdef CONFIG_FB_EXYNOS_FIMD_MC_WB
	/* register a window subdev as entity */
	ret = s3c_fb_register_mc_wb_components(sfb);
	if (ret) {
		dev_err(sfb->dev, "failed to register s3c_fb mc entities\n");
		goto err_mc_wb_entity_create_fail;
	}

	/* create links connected between gscaler and fimd */
	ret = s3c_fb_create_mc_wb_links(sfb);
	if (ret) {
		dev_err(sfb->dev, "failed to create s3c_fb mc links\n");
		goto err_mc_wb_link_create_fail;
	}

	ret = s3c_fb_register_mc_subdev_wb_nodes(sfb);
	if (ret) {
			dev_err(sfb->dev, "failed to register s3c_fb mc subdev node\n");
			goto err_mc_wb_link_create_fail;
	}
#endif

	fbinfo = sfb->windows[default_win]->fbinfo;

	if (underrun_filter_status++ == UNDERRUN_FILTER_INIT)
		INIT_DELAYED_WORK(&underrun_filter_work,
			underrun_filter_handler);

#if defined(CONFIG_S5P_LCD_INIT)
#ifdef CONFIG_ION_EXYNOS
	/* The sysmmu must be enabled before decon is started
	 * If decon is started without enabling sysmmu, decon cannot read
	 * data from memory by dma operation */
	ret = iovmm_activate(&pdev->dev);
	if (ret < 0) {
		dev_err(sfb->dev, "failed to activate vmm\n");
		goto err_iovmm;
	}
#endif

	decon_reg_shadow_protect_win(default_win, 1);

	s3c_fb_to_init_param(sfb, &p);
	decon_reg_init(&p);

	decon_set_regs_param(fbinfo, &win_regs);
	decon_reg_set_regs_data(default_win, &win_regs);

	decon_reg_shadow_protect_win(default_win, 0);

	s3c_fb_to_psr_info(sfb, &psr);
	s3c_fb_vidout_start(&psr);

	decon_reg_activate_window(default_win);

	decon_reg_set_winmap(default_win, 0x000000 /* black */, 1);

	msleep(16);
#else
	s3c_fb_to_init_param(sfb, &p);
	decon_reg_init_probe(&p);
	decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_DISABLE);
#endif

	platform_set_drvdata(pdev, sfb);

	ret = device_create_file(sfb->dev, &dev_attr_vsync);
	if (ret) {
		dev_err(sfb->dev, "failed to create vsync file\n");
		goto err_create_file;
	}

	ret = device_create_file(sfb->dev, &dev_attr_psr_info);
	if (ret) {
		dev_err(sfb->dev, "failed to create psr info file\n");
		goto err_create_psr_info_file;
	}

	sfb->vsync_info.thread = kthread_run(s3c_fb_wait_for_vsync_thread,
			sfb, "s3c-fb-vsync");
	if (sfb->vsync_info.thread == ERR_PTR(-ENOMEM)) {
		dev_err(sfb->dev, "failed to run vsync thread\n");
		sfb->vsync_info.thread = NULL;
	}

#ifdef CONFIG_FB_I80_COMMAND_MODE
	ret = decon_fb_config_eint_for_te(sfb);
	if (ret) {
		dev_err(dev, "failed to request an external interrupt for TE signal\n");
		goto resource_exception;
	}
#endif

	s3c_fb_wait_for_vsync(sfb, VSYNC_TIMEOUT_MSEC);

#if defined(CONFIG_S5P_LCD_INIT)
	/* To use PKTGO mode, PKTGO_EN & PKTGO_READY must be high.
	 * PKTGO_READY sets high when there is a TE interrupt.
	 * PACKET data will be transfered only if there is a frame data.
	 * So, the displayon command is called after DECON_ENABLE and
	 * TRIGGER_UNMASKED. */
	dispdrv->dsi_driver.dsim->dsim_lcd_drv->displayon(dispdrv->dsi_driver.dsim);
#else
#ifdef CONFIG_ION_EXYNOS
	/* If LCD is enabled and unmasked at bootloader, sysmmu must be enabled
	 * after masking trigger */
	ret = iovmm_activate(&pdev->dev);
	if (ret < 0) {
		dev_err(sfb->dev, "failed to activate vmm\n");
		goto err_iovmm;
	}
#endif
#endif

	atomic_set(&sfb->dsd_clk_ref_cnt, 0);
	sfb->output_on = true;

	dev_dbg(sfb->dev, "about to register framebuffer\n");

	/* run the check_var and set_par on our configuration. */

	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		dev_err(sfb->dev, "failed to register framebuffer\n");
		goto err_fb;
	}

	/* [W/A] prevent sleep enter during LCD on */
	ret = device_init_wakeup(sfb->dev, true);
	if (ret) {
		dev_err(sfb->dev, "failed to init wakeup device\n");
		goto err_fb;
	}

	pm_stay_awake(sfb->dev);
	dev_warn(sfb->dev, "pm_stay_awake");

	dev_info(sfb->dev, "window %d: fb %s\n", default_win, fbinfo->fix.id);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	dispdrv->decon_driver.ops->pwr_on = decon_hibernation_power_on;
	dispdrv->decon_driver.ops->pwr_off = decon_hibernation_power_off;
#endif

	return 0;

err_fb:
#ifdef CONFIG_ION_EXYNOS
	iovmm_deactivate(sfb->dev);
#endif

err_iovmm:
	device_remove_file(sfb->dev, &dev_attr_psr_info);

err_create_psr_info_file:
	device_remove_file(sfb->dev, &dev_attr_vsync);

err_create_file:
#ifdef CONFIG_FB_EXYNOS_FIMD_MC_WB
err_mc_wb_entity_create_fail:
err_mc_wb_link_create_fail:
	s3c_fb_unregister_mc_wb_entities(sfb);
#endif

#ifdef CONFIG_FB_EXYNOS_FIMD_MC
err_mc_entity_create_fail:
err_mc_link_create_fail:
	s3c_fb_unregister_mc_entities(sfb->windows[win]);
#endif

resource_exception:
	disp_pm_runtime_put_sync(dispdrv);

#ifdef CONFIG_ION_EXYNOS
	kthread_stop(sfb->update_regs_thread);
#endif
	return ret;
}

/**
 * s3c_fb_remove() - Cleanup on module finalisation
 * @pdev: The platform device we are bound to.
 *
 * Shutdown and then release all the resources that the driver allocated
 * on initialisation.
 */
#ifdef S3CFB_DEVICE_DRIVER_TYPE_ENABLE
static int s3c_fb_remove(struct platform_device *pdev)
{
	struct s3c_fb *sfb;
	struct display_driver *dispdrv;
	int win;

	dispdrv = get_display_driver();
	sfb = dispdrv->decon_driver.sfb;

	disp_pm_runtime_get_sync(dispdrv);

	unregister_framebuffer(sfb->windows[sfb->pdata->default_win]->fbinfo);

#ifdef CONFIG_ION_EXYNOS
	if (sfb->update_regs_thread)
		kthread_stop(sfb->update_regs_thread);
#endif
	s3c_fb_release_win(sfb, sfb->windows[0]);

	if (sfb->vsync_info.thread)
		kthread_stop(sfb->vsync_info.thread);

	device_remove_file(sfb->dev, &dev_attr_vsync);

	s3c_fb_debugfs_cleanup(sfb);

	disp_pm_runtime_put_sync(dispdrv);
	disp_pm_runtime_disable(dispdrv);

	return 0;
}
#endif

static int s3c_fb_disable(struct s3c_fb *sfb)
{
	int ret = 0;
	struct display_driver *dispdrv;
	struct decon_psr_info psr;

	dispdrv = get_display_driver();

	mutex_lock(&sfb->output_lock);

	if (!sfb->output_on) {
		ret = -EBUSY;
		goto err;
	}

	/* [W/A] prevent sleep enter during LCD on */
	pm_relax(sfb->dev);
	dev_warn(sfb->dev, "pm_relax");

	if (sfb->pdata->backlight_off)
		sfb->pdata->backlight_off();

	if (sfb->pdata->lcd_off)
		sfb->pdata->lcd_off();

#ifdef CONFIG_ION_EXYNOS
	flush_kthread_worker(&sfb->update_regs_worker);
#endif

	s3c_fb_disable_otf(sfb);

	s3c_fb_to_psr_info(sfb, &psr);
	ret = decon_reg_stop(&psr);

	sfb->power_state = POWER_DOWN;

#ifdef CONFIG_ION_EXYNOS
	iovmm_deactivate(sfb->dev);
#endif

	pm_runtime_put_sync(sfb->dev);
	sfb->output_on = false;

err:
	mutex_unlock(&sfb->output_lock);
	return ret;
}

/**
 * s3c_fb_enable() - Enable the main LCD output
 * @sfb: The main framebuffer state.
 */
static int s3c_fb_enable(struct s3c_fb *sfb)
{
	int ret = 0;
	struct decon_psr_info psr;
	struct decon_init_param p;
	struct display_driver *dispdrv;

	dispdrv = get_display_driver();

	mutex_lock(&sfb->output_lock);
	if (sfb->output_on) {
		ret = -EBUSY;
		goto err;
	}

	/* [W/A] prevent sleep enter during LCD on */
	pm_stay_awake(sfb->dev);
	dev_warn(sfb->dev, "pm_stay_awake");

	pm_runtime_get_sync(sfb->dev);

	sfb->power_state = POWER_ON;

#ifdef CONFIG_ION_EXYNOS
	ret = iovmm_activate(sfb->dev);
	if (ret < 0) {
		dev_err(sfb->dev, "failed to reactivate vmm\n");
		goto err;
	}
#endif

	/* decon start function by cal */
	s3c_fb_to_init_param(sfb, &p);
	decon_reg_init(&p);

	s3c_fb_to_psr_info(sfb, &psr);
	s3c_fb_vidout_start(&psr);

	/* dispdrv->dsi_driver.dsim->dsim_lcd_drv->displayon(dispdrv->dsi_driver.dsim); */

	sfb->output_on = true;
#ifdef CONFIG_FB_WINDOW_UPDATE
	sfb->update_win.x = 0;
	sfb->update_win.y = 0;
	sfb->update_win.w = 0;
	sfb->update_win.h = 0;
	sfb->need_update = false;
	sfb->full_update = true;
#endif

	ret = 0;

err:
	mutex_unlock(&sfb->output_lock);
	return ret;
}

#ifdef CONFIG_PM_SLEEP
int s3c_fb_suspend(struct device *dev)
{
	struct s3c_fb *sfb;
	struct display_driver *dispdrv;
	struct decon_psr_info psr;
	int ret = 0;

	dispdrv = get_display_driver();
	sfb = dispdrv->decon_driver.sfb;

	mutex_lock(&sfb->output_lock);
	sfb->power_state = POWER_DOWN;
	if (sfb->output_on) {
		if (sfb->pdata->backlight_off)
			sfb->pdata->backlight_off();

		if (sfb->pdata->lcd_off)
			sfb->pdata->lcd_off();

#ifdef CONFIG_ION_EXYNOS
		flush_kthread_worker(&sfb->update_regs_worker);
#endif
		if (sfb->output_on) {
			s3c_fb_to_psr_info(sfb, &psr);
			ret = decon_reg_stop(&psr);
		}

		decon_reg_update_standalone();

		GET_DISPCTL_OPS(dispdrv).disable_display_decon_clocks(sfb->dev);

#ifdef CONFIG_ION_EXYNOS
		iovmm_deactivate(dev);
#endif
		disp_pm_runtime_put_sync(dispdrv);
		sfb->output_on = false;
	}
	mutex_unlock(&sfb->output_lock);

	return ret;
}

int s3c_fb_resume(struct device *dev)
{
	struct s3c_fb *sfb;
	struct display_driver *dispdrv;
	struct s3c_fb_platdata *pd;
	struct decon_psr_info psr;
	struct decon_init_param p;
	int ret = 0;

	dispdrv = get_display_driver();
	sfb = dispdrv->decon_driver.sfb;

	pd = sfb->pdata;

	mutex_lock(&sfb->output_lock);
	if (sfb->output_on) {
		ret = -EBUSY;
		goto err;
	}
#ifdef CONFIG_PM_RUNTIME
	disp_pm_runtime_get_sync(dispdrv);
#endif
	GET_DISPCTL_OPS(dispdrv).init_display_decon_clocks(sfb->dev);

	/* setup gpio and output polarity controls */
	sfb->power_state = POWER_ON;

#ifdef CONFIG_ION_EXYNOS
	ret = iovmm_activate(dev);
	if (ret < 0) {
		dev_err(sfb->dev, "failed to reactivate vmm\n");
		goto err;
	}
#endif

	s3c_fb_to_init_param(sfb, &p);
	decon_reg_init(&p);

	s3c_fb_to_psr_info(sfb, &psr);
	s3c_fb_vidout_start(&psr);

	sfb->output_on = true;

err:
	mutex_unlock(&sfb->output_lock);
	return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME
int s3c_fb_runtime_suspend(struct device *dev)
{
	struct s3c_fb *sfb;
	struct display_driver *dispdrv;
	struct decon_psr_info psr;
	int ret = 0;

	dispdrv = get_display_driver();
	sfb = dispdrv->decon_driver.sfb;

	save_decon_operation_time(OPS_CALL_RUNTIME_SUSPEND);
	if (sfb->power_state != POWER_HIBER_DOWN)
		sfb->power_state = POWER_DOWN;

	if (sfb->output_on) {
		s3c_fb_to_psr_info(sfb, &psr);
		ret = decon_reg_stop(&psr);
		if (!ret)
			sfb->output_on = false;
	}

	GET_DISPCTL_OPS(dispdrv).disable_display_decon_clocks(sfb->dev);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	if ((sfb->irq_no != 0) && (sfb->vsync_info.irq_refcount <= 0) &&
		(atomic_read(&sfb->vsync_info.eint_refcount) == 1)) {
		disable_irq(sfb->irq_no);
		atomic_dec(&sfb->vsync_info.eint_refcount);
	}
#endif
	return ret;
}

int s3c_fb_runtime_resume(struct device *dev)
{
	struct s3c_fb *sfb;
	struct display_driver *dispdrv;
	struct s3c_fb_platdata *pd;

	dispdrv = get_display_driver();
	if (!dispdrv) {
		dev_err(dev, "Can't get display driver\n");
		return 0;
	}
	sfb = dispdrv->decon_driver.sfb;
	if (!sfb) {
		dev_err(dev, "Can't get decon platdata\n");
		return 0;
	}
	pd = sfb->pdata;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	if ((sfb->irq_no != 0) &&
		(atomic_read(&sfb->vsync_info.eint_refcount) == 0)) {
		enable_irq(sfb->irq_no);
		atomic_inc(&sfb->vsync_info.eint_refcount);
	}
#endif
	save_decon_operation_time(OPS_CALL_RUNTIME_RESUME);
	GET_DISPCTL_OPS(dispdrv).init_display_decon_clocks(dev);

	if (sfb->power_state != POWER_HIBER_DOWN)
		sfb->power_state = POWER_ON;

	writel(pd->vidcon1, sfb->regs + VIDCON1);

	return 0;
}
#endif

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int decon_hibernation_power_on(struct display_driver *dispdrv)
{
	int ret = 0;
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	mutex_lock(&sfb->output_lock);

	if (sfb->output_on) {
		pr_info("%s, DECON are already output on state\n", __func__);
		mutex_unlock(&sfb->output_lock);
		return -EBUSY;
	}

	decon_reg_reset();
	decon_reg_set_clkgate_mode(1);
	decon_reg_blend_alpha_bits(BLENDCON_NEW_8BIT_ALPHA_VALUE);
	decon_reg_set_vidout(sfb->psr_mode, 1);
	decon_reg_set_crc(1);

	if (sfb->psr_mode == S3C_FB_MIPI_COMMAND_MODE)
		decon_reg_set_fixvclk(DECON_VCLK_RUN_VDEN_DISABLE);
	else
		decon_reg_set_fixvclk(DECON_VCLK_HOLD);

#ifdef CONFIG_ION_EXYNOS
	ret = iovmm_activate(sfb->dev);
	if (ret < 0) {
		dev_err(sfb->dev, "%s: failed to reactivate vmm\n", __func__);
	}
#endif
	decon_reg_configure_trigger(sfb->trig_mode);

	if (sfb->trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(sfb->trig_mode, DECON_TRIG_DISABLE);

	/* use platform specified window as the basis for the lcd timings */
	decon_reg_configure_lcd(sfb->lcd_info);

#ifdef CONFIG_S5P_DP
	writel(DPCLKCON_ENABLE, sfb->regs + DPCLKCON);
#endif
	sfb->output_on = true;
	mutex_unlock(&sfb->output_lock);

	return ret;
}

int decon_hibernation_power_off(struct display_driver *dispdrv)
{
	int ret = 0;
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;
	struct decon_psr_info psr;

	mutex_lock(&sfb->output_lock);

	if (sfb->output_on) {
		s3c_fb_to_psr_info(sfb, &psr);
		ret = decon_reg_stop(&psr);
		if (!ret)
			sfb->output_on = false;
	}
#ifdef CONFIG_ION_EXYNOS
	iovmm_deactivate(sfb->dev);
#endif
#if defined(CONFIG_DECON_DEVFREQ)
	exynos5_update_media_layers(TYPE_DECON, 0);
	exynos5_update_media_layers(TYPE_GSCL_LOCAL, 0);
	prev_overlap_cnt = 0;
	prev_gsc_local_cnt = 0;
#endif

#ifdef CONFIG_FB_WINDOW_UPDATE
	sfb->update_win.x = 0;
	sfb->update_win.y = 0;
	sfb->update_win.w = 0;
	sfb->update_win.h = 0;
	sfb->need_update = true;
#endif
	mutex_unlock(&sfb->output_lock);

	return ret;
}
#endif

static const struct dev_pm_ops s3cfb_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(s3c_fb_suspend, s3c_fb_resume)
	SET_RUNTIME_PM_OPS(s3c_fb_runtime_suspend, s3c_fb_runtime_resume,
			   NULL)
};


MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_DESCRIPTION("Samsung S3C SoC Framebuffer driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:s3c-fb");
