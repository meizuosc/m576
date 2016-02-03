/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/dma-buf.h>
#include <linux/exynos_ion.h>
#include <linux/ion.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/exynos_iovmm.h>
#include <linux/bug.h>
#include <linux/of_address.h>
#include <linux/smc.h>

#include <mach/regs-clock.h>
#include <media/exynos_mc.h>
#include <video/mipi_display.h>
#include <video/videonode.h>
#include <media/v4l2-subdev.h>

#include "decon.h"
#include "dsim.h"
#include "decon_helper.h"
#include "./panels/lcd_ctrl.h"
#include "../../../staging/android/sw_sync.h"

#ifdef CONFIG_OF
static const struct of_device_id decon_device_table[] = {
	{ .compatible = "samsung,exynos5-decon_driver" },
	{},
};
MODULE_DEVICE_TABLE(of, decon_device_table);
#endif

int decon_log_level = DECON_LOG_LEVEL_WARN;
module_param(decon_log_level, int, 0644);

struct decon_device *decon_int_drvdata;
EXPORT_SYMBOL(decon_int_drvdata);

static int decon_runtime_resume(struct device *dev);
static int decon_runtime_suspend(struct device *dev);

void decon_dump(struct decon_device *decon)
{
	dev_err(decon->dev, "=== DECON%d SFR DUMP ===\n", decon->id);

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->regs, 0x718, false);
	dev_err(decon->dev, "=== DECON%d MIC SFR DUMP ===\n", decon->id);

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->regs + 0x2400, 0x20, false);
	dev_err(decon->dev, "=== DECON%d SHADOW SFR DUMP ===\n", decon->id);

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->regs + 0x7000, 0x718, false);

	v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_DUMP, NULL);
}

/* ---------- CHECK FUNCTIONS ----------- */
static void decon_to_regs_param(struct decon_regs_data *win_regs,
		struct decon_reg_data *regs, int idx)
{
	win_regs->wincon = regs->wincon[idx];
	win_regs->winmap = regs->winmap[idx];
	win_regs->vidosd_a = regs->vidosd_a[idx];
	win_regs->vidosd_b = regs->vidosd_b[idx];
	win_regs->vidosd_c = regs->vidosd_c[idx];
	win_regs->vidosd_d = regs->vidosd_d[idx];
	win_regs->vidw_buf_start = regs->buf_start[idx];
	win_regs->vidw_whole_w = regs->whole_w[idx];
	win_regs->vidw_whole_h = regs->whole_h[idx];
	win_regs->vidw_offset_x = regs->offset_x[idx];
	win_regs->vidw_offset_y = regs->offset_y[idx];
	win_regs->vidw_plane2_buf_start = regs->dma_buf_data[idx][1].dma_addr;
	win_regs->vidw_plane3_buf_start = regs->dma_buf_data[idx][2].dma_addr;

	if (idx)
		win_regs->blendeq = regs->blendeq[idx - 1];

	win_regs->type = regs->win_config[idx].idma_type;

	decon_dbg("decon idma_type(%d)\n", regs->win_config->idma_type);
}

static u16 fb_panstep(u32 res, u32 res_virtual)
{
	return res_virtual > res ? 1 : 0;
}

static u32 vidosd_a(int x, int y)
{
	return VIDOSD_A_TOPLEFT_X(x) |
			VIDOSD_A_TOPLEFT_Y(y);
}

static u32 vidosd_b(int x, int y, u32 xres, u32 yres)
{
	return VIDOSD_B_BOTRIGHT_X(x + xres - 1) |
		VIDOSD_B_BOTRIGHT_Y(y + yres - 1);
}

static u32 vidosd_c(u8 r0, u8 g0, u8 b0)
{
	return VIDOSD_C_ALPHA0_R_F(r0) |
		VIDOSD_C_ALPHA0_G_F(g0) |
		VIDOSD_C_ALPHA0_B_F(b0);
}

static u32 vidosd_d(u8 r1, u8 g1, u8 b1)
{
	return VIDOSD_D_ALPHA1_R_F(r1) |
		VIDOSD_D_ALPHA1_G_F(g1) |
		VIDOSD_D_ALPHA1_B_F(b1);
}

static u32 wincon(u32 bits_per_pixel, u32 transp_length, int format)
{
	u32 data = 0;

	switch (bits_per_pixel) {
	case 12:
		if (format == DECON_PIXEL_FORMAT_NV12 ||
			format == DECON_PIXEL_FORMAT_NV12M)
			data |= WINCON_BPPMODE_NV12;
		else if (format == DECON_PIXEL_FORMAT_NV21 ||
			format == DECON_PIXEL_FORMAT_NV21M)
			data |= WINCON_BPPMODE_NV21;

		data |= WINCON_INTERPOLATION_EN;
		break;
	case 16:
		data |= WINCON_BPPMODE_RGB565;
		break;
	case 24:
	case 32:
		if (transp_length > 0) {
			data |= WINCON_BLD_PIX;
			data |= WINCON_BPPMODE_ARGB8888;
		} else {
			data |= WINCON_BPPMODE_XRGB8888;
		}
		break;
	default:
		pr_err("%d bpp doesn't support\n", bits_per_pixel);
		break;
	}

	if (transp_length != 1)
		data |= WINCON_ALPHA_SEL;

	return data;
}

static inline u32 blendeq(enum decon_blending blending, u8 transp_length,
		int plane_alpha)
{
	u8 a, b;
	int is_plane_alpha = (plane_alpha < 255 && plane_alpha > 0) ? 1 : 0;

	if (transp_length == 1 && blending == DECON_BLENDING_PREMULT)
		blending = DECON_BLENDING_COVERAGE;

	switch (blending) {
	case DECON_BLENDING_NONE:
		a = BLENDE_COEF_ONE;
		b = BLENDE_COEF_ZERO;
		break;

	case DECON_BLENDING_PREMULT:
		if (!is_plane_alpha) {
			a = BLENDE_COEF_ONE;
			b = BLENDE_COEF_ONE_MINUS_ALPHA_A;
		} else {
			a = BLENDE_COEF_ALPHA0;
			b = BLENDE_COEF_ONE_MINUS_ALPHA_A;
		}
		break;

	case DECON_BLENDING_COVERAGE:
		a = BLENDE_COEF_ALPHA_A;
		b = BLENDE_COEF_ONE_MINUS_ALPHA_A;
		break;

	default:
		return 0;
	}

	return BLENDE_A_FUNC(a) |
			BLENDE_B_FUNC(b) |
			BLENDE_P_FUNC(BLENDE_COEF_ZERO) |
			BLENDE_Q_FUNC(BLENDE_COEF_ZERO);
}

static u32 decon_red_length(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 5;

	case DECON_PIXEL_FORMAT_RGB_565:
		return 5;

	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_red_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 0;

	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGB_565:
		return 11;

	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 16;

	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return 24;

	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_green_length(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 5;

	case DECON_PIXEL_FORMAT_RGB_565:
		return 6;

	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_green_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return 16;

	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_RGB_565:
		return 5;

	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_blue_length(int format)
{
	return decon_red_length(format);
}

static u32 decon_blue_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
		return 16;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 10;

	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return 8;

	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return 24;

	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 0;

	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_transp_length(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 1;

	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 0;

	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_transp_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return 24;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 15;

	case DECON_PIXEL_FORMAT_RGBX_8888:
		return decon_blue_offset(format);

	case DECON_PIXEL_FORMAT_BGRX_8888:
		return decon_red_offset(format);

	case DECON_PIXEL_FORMAT_RGB_565:
		return 0;

	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_padding(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return 0;

	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}

}

/* DECON_PIXEL_FORMAT_RGBA_8888 and WINCON_BPPMODE_ABGR8888 are same format
 * A[31:24] : B[23:16] : G[15:8] : R[7:0] */
static u32 decon_rgborder(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
		return WINCON_BPPMODE_ABGR8888;
	case DECON_PIXEL_FORMAT_RGBX_8888:
		return WINCON_BPPMODE_XBGR8888;
	case DECON_PIXEL_FORMAT_RGB_565:
		return WINCON_BPPMODE_RGB565;
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return WINCON_BPPMODE_ARGB8888;
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return WINCON_BPPMODE_XRGB8888;
	case DECON_PIXEL_FORMAT_ARGB_8888:
		return WINCON_BPPMODE_BGRA8888;
	case DECON_PIXEL_FORMAT_ABGR_8888:
		return WINCON_BPPMODE_RGBA8888;
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return WINCON_BPPMODE_BGRX8888;
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return WINCON_BPPMODE_RGBX8888;

	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

bool decon_validate_x_alignment(struct decon_device *decon, int x, u32 w,
		u32 bits_per_pixel)
{
	uint8_t pixel_alignment = 32 / bits_per_pixel;

	if (x % pixel_alignment) {
		decon_err("left X coordinate not properly aligned to %u-pixel boundary (bpp = %u, x = %u)\n",
				pixel_alignment, bits_per_pixel, x);
		return 0;
	}
	if ((x + w) % pixel_alignment) {
		decon_err("right X coordinate not properly aligned to %u-pixel boundary (bpp = %u, x = %u, w = %u)\n",
				pixel_alignment, bits_per_pixel, x, w);
		return 0;
	}

	return 1;
}

static unsigned int decon_calc_bandwidth(u32 w, u32 h, u32 bits_per_pixel, int fps)
{
	unsigned int bw = w * h;

#ifndef CONFIG_DECON_PM_QOS_REQUESTS
	bw *= DIV_ROUND_UP(bits_per_pixel, 8);
	bw *= fps;
#endif

	return bw;
}

/* ---------- OVERLAP COUNT CALCULATION ----------- */
static bool decon_intersect(struct decon_rect *r1, struct decon_rect *r2)
{
	return !(r1->left > r2->right || r1->right < r2->left ||
		r1->top > r2->bottom || r1->bottom < r2->top);
}

static int decon_intersection(struct decon_rect *r1,
				struct decon_rect *r2, struct decon_rect *r3)
{
	r3->top = max(r1->top, r2->top);
	r3->bottom = min(r1->bottom, r2->bottom);
	r3->left = max(r1->left, r2->left);
	r3->right = min(r1->right, r2->right);
	return 0;
}

static void decon_set_win_blocking_mode(struct decon_device *decon, struct decon_win *win,
		struct decon_win_config *win_config, struct decon_reg_data *regs)
{
	struct decon_rect r1, r2, overlap_rect, block_rect;
	unsigned int overlap_size, blocking_size = 0;
	int j;
	bool enabled = false;

	r1.left = win_config->dst.x;
	r1.top = win_config->dst.y;
	r1.right = r1.left + win_config->dst.w - 1;
	r1.bottom = r1.top + win_config->dst.h - 1;

	memset(&block_rect, 0, sizeof(struct decon_rect));
	for (j = win->index + 1; j < decon->pdata->max_win; j++) {
		struct decon_win_config *config = &win_config[j];
		if (config->state != DECON_WIN_STATE_BUFFER)
			continue;

		/* Support only XRGB */
		if ((config->format == DECON_PIXEL_FORMAT_ARGB_8888) ||
				(config->format == DECON_PIXEL_FORMAT_ABGR_8888) ||
				(config->format == DECON_PIXEL_FORMAT_RGBA_8888) ||
				(config->format == DECON_PIXEL_FORMAT_BGRA_8888) ||
				(config->format == DECON_PIXEL_FORMAT_RGBA_5551))
			continue;

		r2.left = config->dst.x;
		r2.top = config->dst.y;
		r2.right = r2.left + config->dst.w - 1;
		r2.bottom = r2.top + config->dst.h - 1;
		/* overlaps or not */
		if (decon_intersect(&r1, &r2)) {
			decon_intersection(&r1, &r2, &overlap_rect);

			/* Minimum size of blocking : 144 x 10 */
			if (overlap_rect.right - overlap_rect.left + 1 <
					MIN_BLK_MODE_WIDTH ||
				overlap_rect.bottom - overlap_rect.top + 1 <
					MIN_BLK_MODE_HEIGHT)
				continue;

			overlap_size = (overlap_rect.right - overlap_rect.left) *
					(overlap_rect.bottom - overlap_rect.top);

			if (overlap_size > blocking_size) {
				memcpy(&block_rect, &overlap_rect,
						sizeof(struct decon_rect));
				blocking_size = (block_rect.right - block_rect.left) *
						(block_rect.bottom - block_rect.top);
				enabled = true;
			}
		}
	}

	if (enabled) {
		regs->block_rect[win->index].w = block_rect.right - block_rect.left + 1;
		regs->block_rect[win->index].h = block_rect.bottom - block_rect.top + 1;
		regs->block_rect[win->index].x = block_rect.left;
		regs->block_rect[win->index].y = block_rect.top;

		memcpy(&win_config->block_area, &regs->block_rect[win->index],
				sizeof(struct decon_win_rect));
	}
}

static void decon_enable_blocking_mode(struct decon_device *decon,
		struct decon_reg_data *regs, u32 win_idx)
{
	struct decon_win_rect rect = regs->block_rect[win_idx];
	bool enable = false;

	/* TODO: Check a DECON H/W limitation */
	enable = (rect.w * rect.h) ? true : false;

	if (enable) {
		decon_reg_set_block_mode(decon->id, win_idx, rect.x, rect.y,
						rect.w, rect.h, true);
		decon_dbg("win[%d] blocking_mode:(%d,%d,%d,%d)\n", win_idx,
				rect.x, rect.y, rect.w, rect.h);
	} else {
		decon_reg_set_block_mode(decon->id, win_idx, 0, 0, 0, 0, false);
	}
}

#if defined(CONFIG_DECON_DEVFREQ)
static int decon_get_overlap_cnt(struct decon_device *decon,
				struct decon_win_config *win_config)
{
	struct decon_rect overlaps2[10];
	struct decon_rect overlaps3[6];
	struct decon_rect overlaps4[3];
	struct decon_rect r1, r2;
	struct decon_win_config *win_cfg1, *win_cfg2;
	int overlaps2_cnt = 0;
	int overlaps3_cnt = 0;
	int overlaps4_cnt = 0;
	int i, j;

	int overlap_max_cnt = 1;

	for (i = 1; i < decon->pdata->max_win; i++) {
		win_cfg1 = &win_config[i];
		if (win_cfg1->state != DECON_WIN_STATE_BUFFER)
			continue;
		r1.left = win_cfg1->dst.x;
		r1.top = win_cfg1->dst.y;
		r1.right = r1.left + win_cfg1->dst.w - 1;
		r1.bottom = r1.top + win_cfg1->dst.h - 1;
		for (j = 0; j < overlaps4_cnt; j++) {
			/* 5 window overlaps */
			if (decon_intersect(&r1, &overlaps4[j])) {
				overlap_max_cnt = 5;
				break;
			}
		}
		for (j = 0; (j < overlaps3_cnt) && (overlaps4_cnt < 3); j++) {
			/* 4 window overlaps */
			if (decon_intersect(&r1, &overlaps3[j])) {
				decon_intersection(&r1, &overlaps3[j], &overlaps4[overlaps4_cnt]);
				overlaps4_cnt++;
			}
		}
		for (j = 0; (j < overlaps2_cnt) && (overlaps3_cnt < 6); j++) {
			/* 3 window overlaps */
			if (decon_intersect(&r1, &overlaps2[j])) {
				decon_intersection(&r1, &overlaps2[j], &overlaps3[overlaps3_cnt]);
				overlaps3_cnt++;
			}
		}
		for (j = 0; (j < i) && (overlaps2_cnt < 10); j++) {
			win_cfg2 = &win_config[j];
			if (win_cfg2->state != DECON_WIN_STATE_BUFFER)
				continue;
			r2.left = win_cfg2->dst.x;
			r2.top = win_cfg2->dst.y;
			r2.right = r2.left + win_cfg2->dst.w - 1;
			r2.bottom = r2.top + win_cfg2->dst.h - 1;
			/* 2 window overlaps */
			if (decon_intersect(&r1, &r2)) {
				decon_intersection(&r1, &r2, &overlaps2[overlaps2_cnt]);
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

	return overlap_max_cnt;
}
#endif

/* ---------- FB_BLANK INTERFACE ----------- */
int decon_enable(struct decon_device *decon)
{
	struct decon_psr_info psr;
	struct decon_init_param p;
	int ret = 0;
#if defined(CONFIG_DECON_DEVFREQ)
	enum devfreq_media_type media_type;
#endif

	decon_dbg("enable decon-%s\n", "int");
	exynos_ss_printk("%s:state %d: active %d:+\n", __func__,
				decon->state, pm_runtime_active(decon->dev));
	mutex_lock(&decon->output_lock);

	if (decon->state == DECON_STATE_ON) {
		decon_warn("decon%d already enabled\n", decon->id);
		goto err;
	}

#if defined(CONFIG_DECON_DEVFREQ)
	if (!decon->id)
		media_type = TYPE_DECON;
	else
		media_type = TYPE_TV;
	exynos7_update_media_layers(media_type, 1);
	decon->cur_overlap_cnt = 1;
#endif

	if (decon->state == DECON_STATE_LPD_EXIT_REQ) {
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
				DSIM_IOC_ENTER_ULPS, (unsigned long *)0);
		if (ret) {
			decon_err("%s: failed to exit ULPS state for %s\n",
					__func__, decon->output_sd->name);
			goto err;
		}
	} else if (decon->out_type == DECON_OUT_DSI) {
		pm_stay_awake(decon->dev);
		dev_warn(decon->dev, "pm_stay_awake");
		ret = v4l2_subdev_call(decon->output_sd, video, s_stream, 1);
		if (ret) {
			decon_err("starting stream failed for %s\n",
					decon->output_sd->name);
			goto err;
		}
	}

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_get_sync(decon->dev);
#else
	decon_runtime_resume(decon->dev);
#endif

	ret = iovmm_activate(decon->dev);
	if (ret < 0) {
		decon_err("failed to reactivate vmm\n");
		goto err;
	}
	ret = 0;

	decon_to_init_param(decon, &p);
	decon_reg_init(decon->id, decon->pdata->dsi_mode, &p);

	decon->prev_overlap_cnt = 1;

	decon_to_psr_info(decon, &psr);
	if (decon->state != DECON_STATE_LPD_EXIT_REQ) {
	/* In case of resume*/
		if (decon->out_type == DECON_OUT_DSI)
			decon_reg_start(decon->id, decon->pdata->dsi_mode, &psr);
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		if (!decon->id) {
			ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL);
			if (ret)
				decon_err("Failed to call DSIM packet go enable!\n");
		}
#endif
	}

	decon->state = DECON_STATE_ON;

#ifdef CONFIG_FB_WINDOW_UPDATE
	if ((decon->update_win.x != 0) || (decon->update_win.y != 0) ||
			(decon->update_win.w != decon->lcd_info->xres) ||
			(decon->update_win.h != decon->lcd_info->yres)) {
		decon->update_win.x = 0;
		decon->update_win.y = 0;
		decon->update_win.w = 0;
		decon->update_win.h = 0;
		decon->need_update = true;
	}
#endif
err:
	exynos_ss_printk("%s:state %d: active %d:-\n", __func__,
				decon->state, pm_runtime_active(decon->dev));
	mutex_unlock(&decon->output_lock);
	return ret;
}

int decon_disable(struct decon_device *decon)
{
	struct decon_psr_info psr;
	int ret = 0;
	unsigned long irq_flags;
	int state = decon->state;
#if defined(CONFIG_DECON_DEVFREQ)
	enum devfreq_media_type media_type;
#endif

	exynos_ss_printk("disable decon-%s, state(%d) cnt %d\n", "int",
				decon->state, pm_runtime_active(decon->dev));
	mutex_lock(&decon->output_lock);

	if (decon->state == DECON_STATE_OFF) {
		decon_info("decon%d already disabled\n", decon->id);
		goto err;
	} else if (decon->state == DECON_STATE_LPD) {
#ifdef DECON_LPD_OPT
		decon_lcd_off(decon);
		decon_info("decon is LPD state. only lcd is off\n");
#endif
		goto err;
	}

	flush_kthread_worker(&decon->update_regs_worker);

	decon_to_psr_info(decon, &psr);
	decon_reg_stop(decon->id, decon->pdata->dsi_mode, &psr);
	decon_reg_clear_int(decon->id);
	iovmm_deactivate(decon->dev);

	/* Synchronize the decon->state with irq_handler */
	spin_lock_irqsave(&decon->slock, irq_flags);
	if (state == DECON_STATE_LPD_ENT_REQ)
		decon->state = DECON_STATE_LPD;
	spin_unlock_irqrestore(&decon->slock, irq_flags);
#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_put_sync(decon->dev);
#else
	decon_runtime_suspend(decon->dev);
#endif

	if (state == DECON_STATE_LPD_ENT_REQ) {
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
				DSIM_IOC_ENTER_ULPS, (unsigned long *)1);
		if (ret) {
			decon_err("%s: failed to enter ULPS state for %s\n",
					__func__, decon->output_sd->name);
			goto err;
		}
		decon->state = DECON_STATE_LPD;
	} else if (decon->out_type == DECON_OUT_DSI) {
		/* stop output device (mipi-dsi) */
		ret = v4l2_subdev_call(decon->output_sd, video, s_stream, 0);
		if (ret) {
			decon_err("stopping stream failed for %s\n",
					decon->output_sd->name);
			goto err;
		}

		pm_relax(decon->dev);
		dev_warn(decon->dev, "pm_relax");

		decon->state = DECON_STATE_OFF;
	}

#if defined(CONFIG_DECON_DEVFREQ)
	if (!decon->id)
		media_type = TYPE_DECON;
	else
		media_type = TYPE_TV;
	exynos7_update_media_layers(media_type, 0);
	decon->cur_overlap_cnt = 0;
#endif

err:
	exynos_ss_printk("%s:state %d: active%d:-\n", __func__,
				decon->state, pm_runtime_active(decon->dev));
	mutex_unlock(&decon->output_lock);
	return ret;
}

static int decon_blank(int blank_mode, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	int ret = 0;

	decon_info("decon-%s %s mode: %dtype (0: DSI)\n", "int",
			blank_mode == FB_BLANK_UNBLANK ? "UNBLANK" : "POWERDOWN",
			decon->out_type);

	decon_lpd_block_exit(decon);

	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
		ret = decon_disable(decon);
		if (ret) {
			decon_err("failed to disable decon\n");
			goto blank_exit;
		}
		break;
	case FB_BLANK_UNBLANK:
		ret = decon_enable(decon);
		if (ret) {
			decon_err("failed to enable decon\n");
			goto blank_exit;
		}
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	default:
		ret = -EINVAL;
	}

blank_exit:
	decon_lpd_unblock(decon);
	decon_info("%s -\n", __func__);
	return ret;
}

/* ---------- FB_IOCTL INTERFACE ----------- */
static void decon_activate_vsync(struct decon_device *decon)
{
	struct decon_psr_info psr;
	int prev_refcount;

	mutex_lock(&decon->vsync_info.irq_lock);

	prev_refcount = decon->vsync_info.irq_refcount++;
	if (!prev_refcount) {
		decon_to_psr_info(decon, &psr);
		decon_reg_set_int(decon->id, &psr, DSI_MODE_SINGLE, 1);
	}

	mutex_unlock(&decon->vsync_info.irq_lock);
}

static void decon_deactivate_vsync(struct decon_device *decon)
{
	struct decon_psr_info psr;
	int new_refcount;

	mutex_lock(&decon->vsync_info.irq_lock);

	new_refcount = --decon->vsync_info.irq_refcount;
	WARN_ON(new_refcount < 0);
	if (!new_refcount) {
		decon_to_psr_info(decon, &psr);
		decon_reg_set_int(decon->id, &psr, DSI_MODE_SINGLE, 0);
	}

	mutex_unlock(&decon->vsync_info.irq_lock);
}

static int decon_wait_for_vsync(struct decon_device *decon, u32 timeout)
{
	ktime_t timestamp;
	int ret;

	timestamp = decon->vsync_info.timestamp;
	if (decon->pdata->trig_mode == DECON_SW_TRIG)
		decon_activate_vsync(decon);

	if (timeout) {
		ret = wait_event_interruptible_timeout(decon->vsync_info.wait,
				!ktime_equal(timestamp,
						decon->vsync_info.timestamp),
				msecs_to_jiffies(timeout));
	} else {
		ret = wait_event_interruptible(decon->vsync_info.wait,
				!ktime_equal(timestamp,
						decon->vsync_info.timestamp));
	}

	if (decon->pdata->trig_mode == DECON_SW_TRIG)
		decon_deactivate_vsync(decon);

	if (timeout && ret == 0) {
		decon_err("decon%d wait for vsync timeout", decon->id);
		return -ETIMEDOUT;
	}

	return 0;
}

int decon_set_window_position(struct fb_info *info,
				struct decon_user_window user_window)
{
	return 0;
}

int decon_set_plane_alpha_blending(struct fb_info *info,
				struct s3c_fb_user_plane_alpha user_alpha)
{
	return 0;
}

int decon_set_chroma_key(struct fb_info *info,
			struct s3c_fb_user_chroma user_chroma)
{
	return 0;
}

int decon_set_vsync_int(struct fb_info *info, bool active)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	bool prev_active = decon->vsync_info.active;

	decon->vsync_info.active = active;
	smp_wmb();

	if (active && !prev_active)
		decon_activate_vsync(decon);
	else if (!active && prev_active)
		decon_deactivate_vsync(decon);

	return 0;
}

static unsigned int decon_map_ion_handle(struct decon_device *decon,
		struct device *dev, struct decon_dma_buf_data *dma,
		struct ion_handle *ion_handle, struct dma_buf *buf, int win_no)
{
	void __iomem *addr;

	dma->fence = NULL;
	dma->dma_buf = buf;

	dma->attachment = dma_buf_attach(dma->dma_buf, dev);
	if (IS_ERR_OR_NULL(dma->attachment)) {
		decon_err("dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma->attachment));
		goto err_buf_map_attach;
	}

	dma->sg_table = dma_buf_map_attachment(dma->attachment,
			DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dma->sg_table)) {
		decon_err("dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma->sg_table));
		goto err_buf_map_attachment;
	}

	dma->dma_addr = ion_iovmm_map(dma->attachment, 0,
			dma->dma_buf->size, DMA_TO_DEVICE, win_no);
	if (!dma->dma_addr || IS_ERR_VALUE(dma->dma_addr)) {
		decon_err("iovmm_map() failed: %pa\n", &dma->dma_addr);
		goto err_iovmm_map;
	}

	exynos_ion_sync_dmabuf_for_device(dev, dma->dma_buf, dma->dma_buf->size,
			DMA_TO_DEVICE);

	dma->ion_handle = ion_handle;
	addr = ion_map_kernel(decon->ion_client, dma->ion_handle);
	decon_dbg("vaddr(0x%pa)\n", addr);

	return dma->dma_buf->size;

err_iovmm_map:
	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);
err_buf_map_attachment:
	dma_buf_detach(dma->dma_buf, dma->attachment);
err_buf_map_attach:
	return 0;
}


static void decon_free_dma_buf(struct decon_device *decon,
		struct decon_dma_buf_data *dma)
{
	if (!dma->dma_addr)
		return;

	if (dma->fence)
		sync_fence_put(dma->fence);

	ion_iovmm_unmap(dma->attachment, dma->dma_addr);

	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);
#if 1 /* TODO: It will be added after merging ion patches */
	exynos_ion_sync_dmabuf_for_cpu(decon->dev, dma->dma_buf,
					dma->dma_buf->size, DMA_FROM_DEVICE);
	dma_buf_detach(dma->dma_buf, dma->attachment);
	dma_buf_put(dma->dma_buf);
	ion_free(decon->ion_client, dma->ion_handle);
	memset(dma, 0, sizeof(struct decon_dma_buf_data));
#endif
}

static int decon_get_plane_cnt(enum decon_pixel_format format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_RGB_565:
		return 1;

	case DECON_PIXEL_FORMAT_NV16:
	case DECON_PIXEL_FORMAT_NV61:
	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 2;

	case DECON_PIXEL_FORMAT_YVU422_3P:
	case DECON_PIXEL_FORMAT_YUV420:
	case DECON_PIXEL_FORMAT_YVU420:
	case DECON_PIXEL_FORMAT_YUV420M:
	case DECON_PIXEL_FORMAT_YVU420M:
		return 3;

	default:
		decon_err("invalid format(%d)\n", format);
		return 1;
	}

}

static void decon_set_protected_content(struct decon_device *decon,
				struct decon_reg_data *regs)
{
	int i, ret = 0;
	u32 change = 0;
	bool en;
	int idma_protected = 0;

	for (i = 0; i < MAX_DMA_TYPE; i++) {
		change = (decon->cur_protection_bitmask & (1 << i)) ^
			(decon->prev_protection_bitmask & (1 << i));
		if (change) {
			en = (decon->cur_protection_bitmask & (1 << i)) >> i;
			if (i == DECON_ODMA_WB && en && !idma_protected)
				continue;
			ret = exynos_smc(SMC_PROTECTION_SET, 0,
					i + DECON_TZPC_OFFSET, en);
			if (!ret) {
				WARN(1, "decon%d IDMA-%d smc call fail\n",
						decon->id, i);
			} else {
				idma_protected++;
				decon_info("decon%d IDMA-%d protection %s\n",
					decon->id, i, en ? "enabled" : "disabled");
			}
		}
	}
	decon->prev_protection_bitmask = decon->cur_protection_bitmask;
}

static int decon_set_win_buffer(struct decon_device *decon, struct decon_win *win,
		struct decon_win_config *win_config, struct decon_reg_data *regs)
{
	struct ion_handle *handle;
	struct fb_var_screeninfo prev_var = win->fbinfo->var;
	struct dma_buf *buf[MAX_BUF_PLANE_CNT];
	struct decon_dma_buf_data dma_buf_data[MAX_BUF_PLANE_CNT];
	unsigned short win_no = win->index;
	int ret, i;
	size_t buf_size = 0, window_size;
	u8 alpha0, alpha1;
	int plane_cnt;
	u32 format;

	if (win_config->format >= DECON_PIXEL_FORMAT_MAX) {
		decon_err("unknown pixel format %u\n", win_config->format);
		return -EINVAL;
	}

	if (win_config->blending >= DECON_BLENDING_MAX) {
		decon_err("unknown blending %u\n", win_config->blending);
		return -EINVAL;
	}

	if (win_no == 0 && win_config->blending != DECON_BLENDING_NONE) {
		decon_err("blending not allowed on window 0\n");
		return -EINVAL;
	}

	if (win_config->dst.w == 0 || win_config->dst.h == 0 ||
			win_config->dst.x < 0 || win_config->dst.y < 0) {
		decon_err("win[%d] size is abnormal (w:%d, h:%d, x:%d, y:%d)\n",
				win_no, win_config->dst.w, win_config->dst.h,
				win_config->dst.x, win_config->dst.y);
		return -EINVAL;
	}

	format = win_config->format;

	win->fbinfo->var.red.length = decon_red_length(format);
	win->fbinfo->var.red.offset = decon_red_offset(format);
	win->fbinfo->var.green.length = decon_green_length(format);
	win->fbinfo->var.green.offset = decon_green_offset(format);
	win->fbinfo->var.blue.length = decon_blue_length(format);
	win->fbinfo->var.blue.offset = decon_blue_offset(format);
	win->fbinfo->var.transp.length =
			decon_transp_length(format);
	win->fbinfo->var.transp.offset =
			decon_transp_offset(format);
	win->fbinfo->var.bits_per_pixel = win->fbinfo->var.red.length +
			win->fbinfo->var.green.length +
			win->fbinfo->var.blue.length +
			win->fbinfo->var.transp.length +
			decon_padding(format);

	if (format <= DECON_PIXEL_FORMAT_RGB_565) {
		win->fbinfo->var.bits_per_pixel = win->fbinfo->var.red.length +
			win->fbinfo->var.green.length +
			win->fbinfo->var.blue.length +
			win->fbinfo->var.transp.length +
			decon_padding(format);
	} else {
		win->fbinfo->var.bits_per_pixel = 12;
	}

	if (win_config->dst.w * win->fbinfo->var.bits_per_pixel / 8 < 128) {
		decon_err("window wide < 128bytes, width = %u, bpp = %u)\n",
				win_config->dst.w,
				win->fbinfo->var.bits_per_pixel);
		ret = -EINVAL;
		goto err_invalid;
	}

	if (win_config->src.f_w < win_config->dst.w) {
		decon_err("f_width(%u) < width(%u),\
			bpp = %u\n", win_config->src.f_w,
			win_config->dst.w,
			win->fbinfo->var.bits_per_pixel);
		ret = -EINVAL;
		goto err_invalid;
	}

	if ((format <= DECON_PIXEL_FORMAT_RGB_565) &&
			(decon_validate_x_alignment(decon, win_config->dst.x,
			win_config->dst.w,
			win->fbinfo->var.bits_per_pixel) == false)) {
		ret = -EINVAL;
		goto err_invalid;
	}

	plane_cnt = decon_get_plane_cnt(win_config->format);
	for (i = 0; i < plane_cnt; ++i) {
		handle = ion_import_dma_buf(decon->ion_client, win_config->fd_idma[i]);
		if (IS_ERR(handle)) {
			decon_err("failed to import fd\n");
			ret = PTR_ERR(handle);
			goto err_invalid;
		}

		buf[i] = dma_buf_get(win_config->fd_idma[i]);
		if (IS_ERR_OR_NULL(buf[i])) {
			decon_err("dma_buf_get() failed: %ld\n", PTR_ERR(buf[i]));
			ret = PTR_ERR(buf[i]);
			goto err_buf_get;
		}
		buf_size = decon_map_ion_handle(decon, decon->dev,
				&dma_buf_data[i], handle, buf[i], win_no);

		if (!buf_size) {
			ret = -ENOMEM;
			goto err_map;
		}
		win_config->vpp_parm.addr[i] = dma_buf_data[i].dma_addr;
		handle = NULL;
		buf[i] = NULL;
	}
	if (win_config->fence_fd >= 0) {
		dma_buf_data[0].fence = sync_fence_fdget(win_config->fence_fd);
		if (!dma_buf_data[0].fence) {
			decon_err("failed to import fence fd\n");
			ret = -EINVAL;
			goto err_offset;
		}
		decon_dbg("%s(%d): fence_fd(%d), fence(%lx)\n", __func__, __LINE__,
				win_config->fence_fd, (ulong)dma_buf_data[0].fence);
	}

	if (format <= DECON_PIXEL_FORMAT_RGB_565) {
		window_size = win_config->dst.w * win_config->dst.h *
			win->fbinfo->var.bits_per_pixel / 8;
		if (window_size > buf_size) {
			decon_err("window size(%zu) > buffer size(%zu)\n",
					window_size, buf_size);
			ret = -EINVAL;
			goto err_offset;
		}
	}

	win->fbinfo->fix.smem_start = dma_buf_data[0].dma_addr;
	win->fbinfo->fix.smem_len = buf_size;
	win->fbinfo->var.xres = win_config->dst.w;
	win->fbinfo->var.xres_virtual = win_config->dst.f_w;
	win->fbinfo->var.yres = win_config->dst.h;
	win->fbinfo->var.yres_virtual = win_config->dst.f_h;
	win->fbinfo->var.xoffset = win_config->src.x;
	win->fbinfo->var.yoffset = win_config->src.y;

	win->fbinfo->fix.line_length = win_config->src.f_w *
			win->fbinfo->var.bits_per_pixel / 8;
	win->fbinfo->fix.xpanstep = fb_panstep(win_config->dst.w,
			win->fbinfo->var.xres_virtual);
	win->fbinfo->fix.ypanstep = fb_panstep(win_config->dst.h,
			win->fbinfo->var.yres_virtual);

	plane_cnt = decon_get_plane_cnt(win_config->format);
	for (i = 0; i < plane_cnt; ++i)
		regs->dma_buf_data[win_no][i] = dma_buf_data[i];
	regs->buf_start[win_no] = win->fbinfo->fix.smem_start;

	regs->vidosd_a[win_no] = vidosd_a(win_config->dst.x, win_config->dst.y);
	regs->vidosd_b[win_no] = vidosd_b(win_config->dst.x, win_config->dst.y,

	win_config->dst.w, win_config->dst.h);
	regs->whole_w[win_no] = win_config->src.f_w;
	regs->whole_h[win_no] = win_config->src.f_h;
	regs->offset_x[win_no] = win_config->src.x;
	regs->offset_y[win_no] = win_config->src.y;

	if ((win_config->plane_alpha > 0) && (win_config->plane_alpha < 0xFF)) {
		alpha0 = win_config->plane_alpha;
		alpha1 = 0;
	} else if (win->fbinfo->var.transp.length == 1 &&
			win_config->blending == DECON_BLENDING_NONE) {
		alpha0 = 0xff;
		alpha1 = 0xff;
	} else {
		alpha0 = 0;
		alpha1 = 0xff;
	}
	regs->vidosd_c[win_no] = vidosd_c(alpha0, alpha0, alpha0);
	regs->vidosd_d[win_no] = vidosd_d(alpha1, alpha1, alpha1);

	regs->wincon[win_no] = wincon(win->fbinfo->var.bits_per_pixel,
			win->fbinfo->var.transp.length, format);
	regs->wincon[win_no] |= decon_rgborder(format);
	regs->protection[win_no] = win_config->protection;

	if (win_no) {
		if ((win_config->plane_alpha > 0) && (win_config->plane_alpha < 0xFF)) {
			if (win->fbinfo->var.transp.length) {
				if (win_config->blending != DECON_BLENDING_NONE)
					regs->wincon[win_no] |= WINCON_ALPHA_MUL;
			} else {
				regs->wincon[win_no] &= (~WINCON_ALPHA_SEL);
				if (win_config->blending == DECON_BLENDING_PREMULT)
					win_config->blending = DECON_BLENDING_COVERAGE;
			}
		}
		regs->blendeq[win_no - 1] = blendeq(win_config->blending,
				win->fbinfo->var.transp.length, win_config->plane_alpha);
	}

	decon_dbg("win[%d] SRC:(%d,%d) %dx%d  DST:(%d,%d) %dx%d\n", win_no,
			win_config->src.x, win_config->src.y,
			win_config->src.f_w, win_config->src.f_h,
			win_config->dst.x, win_config->dst.y,
			win_config->dst.w, win_config->dst.h);

	return 0;

err_offset:
	for (i = 0; i < plane_cnt; ++i)
		decon_free_dma_buf(decon, &dma_buf_data[i]);
err_map:
	for (i = 0; i < plane_cnt; ++i)
		dma_buf_put(buf[i]);
err_buf_get:
	if (handle)
		ion_free(decon->ion_client, handle);
err_invalid:
	win->fbinfo->var = prev_var;
	return ret;
}

#ifdef CONFIG_FB_WINDOW_UPDATE
static void decon_set_win_update_config(struct decon_device *decon,
		struct decon_win_config *win_config,
		struct decon_reg_data *regs)
{
	int i;
	struct decon_win_config *update_config = &win_config[DECON_WIN_UPDATE_IDX];
	struct decon_win_config temp_config;
	struct decon_rect r1, r2;
	struct decon_lcd *lcd_info = decon->lcd_info;

	update_config->dst.w = ((update_config->dst.w + 7) >> 3) << 3;
	/* TODO: with partial width */
	update_config->dst.w = lcd_info->xres;
	if (update_config->dst.x + update_config->dst.w > lcd_info->xres)
		update_config->dst.x = lcd_info->xres - update_config->dst.w;

	if ((update_config->state == DECON_WIN_STATE_UPDATE) &&
			((update_config->dst.x != decon->update_win.x) ||
			 (update_config->dst.y != decon->update_win.y) ||
			 (update_config->dst.w != decon->update_win.w) ||
			 (update_config->dst.h != decon->update_win.h))) {
		decon->update_win.x = update_config->dst.x;
		decon->update_win.y = update_config->dst.y;
		decon->update_win.w = update_config->dst.w;
		decon->update_win.h = update_config->dst.h;
		decon->need_update = true;
		regs->need_update = true;
		regs->update_win.x = update_config->dst.x;
		regs->update_win.y = update_config->dst.y;
		regs->update_win.w = update_config->dst.w;
		regs->update_win.h = update_config->dst.h;

		decon_win_update_dbg("[WIN_UPDATE]need_update_1: [%d %d %d %d]\n",
				update_config->dst.x, update_config->dst.y, update_config->dst.w, update_config->dst.h);
	} else if (decon->need_update &&
			(update_config->state != DECON_WIN_STATE_UPDATE)) {
		decon->update_win.x = 0;
		decon->update_win.y = 0;
		decon->update_win.w = lcd_info->xres;
		decon->update_win.h = lcd_info->yres;
		decon->need_update = false;
		regs->need_update = true;
		regs->update_win.w = lcd_info->xres;
		regs->update_win.h = lcd_info->yres;
		decon_win_update_dbg("[WIN_UPDATE]update2org: [%d %d %d %d]\n",
				decon->update_win.x, decon->update_win.y, decon->update_win.w, decon->update_win.h);
	}

	if (update_config->state != DECON_WIN_STATE_UPDATE)
		return;

	/* TODO: support window update for scaling scenarios */
	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win_config *config = &win_config[i];
		if (config->state == DECON_WIN_STATE_BUFFER) {
			if ((config->src.w != config->dst.w) ||
				(config->src.h != config->dst.h)) {
				decon->need_update = false;
				regs->need_update = false;
				return;
			}
		}
	}
	r1.left = update_config->dst.x;
	r1.top = update_config->dst.y;
	r1.right = r1.left + update_config->dst.w - 1;
	r1.bottom = r1.top + update_config->dst.h - 1;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win_config *config = &win_config[i];
		if (config->state == DECON_WIN_STATE_DISABLED)
			continue;
		r2.left = config->dst.x;
		r2.top = config->dst.y;
		r2.right = r2.left + config->dst.w - 1;
		r2.bottom = r2.top + config->dst.h - 1;
		if (!decon_intersect(&r1, &r2)) {
			config->state = DECON_WIN_STATE_DISABLED;
			continue;
		}
		memcpy(&temp_config, config, sizeof(struct decon_win_config));
		if (update_config->dst.x > config->dst.x)
			config->dst.w = min(update_config->dst.w,
					config->dst.x + config->dst.w - update_config->dst.x);
		else if (update_config->dst.x + update_config->dst.w < config->dst.x + config->dst.w)
			config->dst.w = min(config->dst.w,
					update_config->dst.w + update_config->dst.x - config->dst.x);

		if (update_config->dst.y > config->dst.y)
			config->dst.h = min(update_config->dst.h,
					config->dst.y + config->dst.h - update_config->dst.y);
		else if (update_config->dst.y + update_config->dst.h < config->dst.y + config->dst.h)
			config->dst.h = min(config->dst.h,
					update_config->dst.h + update_config->dst.y - config->dst.y);

		config->dst.x = max(config->dst.x - update_config->dst.x, 0);
		config->dst.y = max(config->dst.y - update_config->dst.y, 0);
		if (update_config->dst.y > temp_config.dst.y) {
			config->src.y += (update_config->dst.y - temp_config.dst.y);
			config->src.h -= (update_config->dst.y - temp_config.dst.y);
		}

		if (update_config->dst.x > temp_config.dst.x) {
			config->src.x += (update_config->dst.x - temp_config.dst.x);
			config->src.w -= (update_config->dst.x - temp_config.dst.x);
		}

		if (regs->need_update == true)
			decon_win_update_dbg("[WIN_UPDATE]win_idx %d: idma_type %d: dst[%d %d %d %d] -> [%d %d %d %d], src[%d %d %d %d] -> [%d %d %d %d]\n",
					i, temp_config.idma_type,
					temp_config.dst.x, temp_config.dst.y, temp_config.dst.w, temp_config.dst.h,
					config->dst.x, config->dst.y, config->dst.w, config->dst.h,
					temp_config.src.x, temp_config.src.y, temp_config.src.w, temp_config.src.h,
					config->src.x, config->src.y, config->src.w, config->src.h);
	}

	return;
}
#endif

void decon_reg_chmap_validate(struct decon_device *decon, struct decon_reg_data *regs)
{
	unsigned short i, bitmap = 0;

	for (i = 0; i < decon->pdata->max_win; i++) {
		if (regs->wincon[i] & WINCON_ENWIN) {
			if (bitmap & (1 << regs->win_config[i].idma_type)) {
				decon_warn("Channel-%d is mapped to multiple windows\n",
					regs->win_config[i].idma_type);
				regs->wincon[i] &= (~WINCON_ENWIN);
			}
			bitmap |= 1 << regs->win_config[i].idma_type;
		}
	}
}

#ifdef CONFIG_FB_WINDOW_UPDATE
static void decon_reg_set_win_update_config(struct decon_device *decon, struct decon_reg_data *regs)
{
	struct decon_lcd lcd_info;
	struct decon_win_rect win_rect;
	struct decon_lcd *lcd_info_org = decon->lcd_info;
	int ret;

	if (regs->need_update) {
		memcpy(&lcd_info, lcd_info_org, sizeof(struct decon_lcd));
		/* TODO: need to set DSI_IDX */
		decon_reg_wait_linecnt_is_zero_timeout(decon->id, 0, 35 * 1000);
		/* Partial Command */
		win_rect.x = regs->update_win.x;
		win_rect.y = regs->update_win.y;
		/* w is right & h is bottom */
		win_rect.w = regs->update_win.x + regs->update_win.w - 1;
		win_rect.h = regs->update_win.y + regs->update_win.h - 1;
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_DISABLE, NULL);
		if (ret)
			decon_err("Failed to disable Packet-go in %s\n", __func__);
#endif
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
				DSIM_IOC_PARTIAL_CMD, &win_rect);
		if (ret) {
			decon_err("%s: partial_area CMD is failed  %s\n",
					__func__, decon->output_sd->name);
		}
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL); /* Don't care failure or success */
#endif
		lcd_info.xres = regs->update_win.w;
		lcd_info.yres = regs->update_win.h;
		lcd_info.vsa = lcd_info_org->vsa;
		lcd_info.vbp = lcd_info_org->vbp;

		lcd_info.vfp = lcd_info_org->vfp;

		if (regs->update_win.w + lcd_info_org->hfp < (lcd_info_org->xres / 4))
			lcd_info.hfp = (lcd_info_org->xres / 4) - regs->update_win.w;
		else
			lcd_info.hfp = lcd_info_org->hfp;

		lcd_info.hbp = lcd_info_org->hbp;
		lcd_info.hsa = lcd_info_org->hsa;

		v4l2_set_subdev_hostdata(decon->output_sd, &lcd_info);
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_SET_PORCH, NULL);
		if (ret)
			decon_err("failed to set porch values of DSIM\n");

		if (lcd_info.mic_enabled)
			decon_reg_config_mic(decon->id, 0, &lcd_info);
		decon_reg_set_porch(decon->id, 0, &lcd_info);

		decon_win_update_dbg("[WIN_UPDATE]%s : vfp %d vbp %d vsa %d hfp %d hbp %d hsa %d w %d h %d\n",
				__func__,
				lcd_info.vfp, lcd_info.vbp, lcd_info.vsa,
				lcd_info.hfp, lcd_info.hbp, lcd_info.hsa,
				regs->update_win.w, regs->update_win.h);

	}
}
#endif

static void __decon_update_regs(struct decon_device *decon, struct decon_reg_data *regs)
{
	unsigned short i, j;
	struct decon_regs_data win_regs;
	struct decon_psr_info psr;
	int plane_cnt;
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	int ret;
#endif

	memset(&win_regs, 0, sizeof(struct decon_regs_data));

	decon->cur_protection_bitmask = 0;

	if (!decon->id && decon->pdata->trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
				decon->pdata->trig_mode, DECON_TRIG_DISABLE);

	for (i = 0; i < decon->pdata->max_win; i++)
		decon_reg_shadow_protect_win(decon->id, decon->windows[i]->index, 1);

	decon_reg_chmap_validate(decon, regs);

#ifdef CONFIG_FB_WINDOW_UPDATE
	if (decon->out_type == DECON_OUT_DSI)
		decon_reg_set_win_update_config(decon, regs);
#endif

	for (i = 0; i < decon->pdata->max_win; i++) {
		decon_to_regs_param(&win_regs, regs, i);
		decon_reg_set_regs_data(decon->id, i, &win_regs);
		decon->cur_protection_bitmask |=
			regs->protection[i] << regs->win_config[i].idma_type;
		plane_cnt = decon_get_plane_cnt(regs->win_config[i].format);
		for (j = 0; j < plane_cnt; ++j)
			decon->windows[i]->dma_buf_data[j] = regs->dma_buf_data[i][j];
		if (IS_ENABLED(CONFIG_DECON_BLOCKING_MODE))
			decon_enable_blocking_mode(decon, regs, i);
	}
	decon_set_protected_content(decon, regs);

	for (i = 0; i < decon->pdata->max_win; i++)
		decon_reg_shadow_protect_win(decon->id, decon->windows[i]->index, 0);

	decon_to_psr_info(decon, &psr);
	decon_reg_start(decon->id, decon->pdata->dsi_mode, &psr);
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	if (!decon->id) {
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL);
		if (ret)
			decon_err("Failed to call DSIM packet go enable in %s!\n", __func__);
	}
#endif
}

static void decon_fence_wait(struct sync_fence *fence)
{
	int err = sync_fence_wait(fence, 900);
	if (err >= 0)
		return;

	if (err < 0)
		decon_warn("error waiting on acquire fence: %d\n", err);
}

#ifdef CONFIG_DECON_PM_QOS_REQUESTS

static u32 int_opp_list[] = {
	400000,
	334000,
	267000,
	222000,
	160000,
	133000,
	114000,
	100000,
};

static u32 mif_opp_list[] = {
	825000,
	667000,
	416000,
	334000,
	275000,
	222000,
	160000,
	134000,
	111000,
};

static u32 decon_find_ceil_freq(u32 *list, u32 list_sz, u32 freq)
{
	int i;

	if (freq < list[list_sz - 1])
		return list[list_sz - 1];

	for (i = list_sz - 1; i >= 0; i--) {
		if (list[i] >= freq)
			return list[i];
	}

	return list[0];
}

void decon_update_qos_requests(struct decon_device *decon,
		struct decon_reg_data *regs, bool is_default_qos)
{
	unsigned long int mif_diff = MIF_DVFS_LVL_MAX - MIF_DVFS_LVL_MIN;
	unsigned long int int_diff = INT_DVFS_LVL_MAX - INT_DVFS_LVL_MIN;
	unsigned long int mif_freq = MIF_DVFS_LVL_MIN;
	unsigned long int int_freq = INT_DVFS_LVL_MIN;
	unsigned long int max_bw;

	if (!is_default_qos) {
		max_bw = decon->lcd_info->xres * decon->lcd_info->yres *
				decon->pdata->max_win;

		mif_freq += (((mif_diff * regs->bandwidth) / max_bw) *
					MIF_SCALING_FACTOR)/10;
		int_freq += (((int_diff * regs->bandwidth) / max_bw) *
					INT_SCALING_FACTOR)/10;

		mif_freq = decon_find_ceil_freq(mif_opp_list,
				sizeof(mif_opp_list)/sizeof(u32),
				mif_freq);

		int_freq = decon_find_ceil_freq(int_opp_list,
				sizeof(int_opp_list)/sizeof(u32),
				int_freq);
	} else {
		mif_freq = PM_QOS_DEFAULT_VALUE;
		int_freq = PM_QOS_DEFAULT_VALUE;
	}

	decon_dbg("decon mif(%lu), int(%lu)\n", mif_freq , int_freq);

	pm_qos_update_request(&decon->int_qos, int_freq);
	pm_qos_update_request(&decon->mif_qos, mif_freq);
}
#endif

static void decon_update_regs(struct decon_device *decon, struct decon_reg_data *regs)
{
	struct decon_dma_buf_data old_dma_bufs[decon->pdata->max_win][MAX_BUF_PLANE_CNT];
	int i, j;
	int plane_cnt;
#if defined(CONFIG_DECON_DEVFREQ)
	enum devfreq_media_type media_type;
#endif

	if (decon->state == DECON_STATE_LPD)
		decon_exit_lpd(decon);

	for (i = 0; i < decon->pdata->max_win; i++)
		for (j = 0; j < MAX_BUF_PLANE_CNT; ++j)
			memset(&old_dma_bufs[i][j], 0, sizeof(struct decon_dma_buf_data));

	for (i = 0; i < decon->pdata->max_win; i++) {
		plane_cnt = decon_get_plane_cnt(regs->win_config[i].format);
		for (j = 0; j < plane_cnt; ++j) {
			old_dma_bufs[i][j] = decon->windows[i]->dma_buf_data[j];
		}
		if (regs->dma_buf_data[i][0].fence) {
			decon_fence_wait(regs->dma_buf_data[i][0].fence);
		}
	}

#if defined(CONFIG_DECON_DEVFREQ)
	if (!decon->id)
		media_type = TYPE_DECON;
	else
		media_type = TYPE_TV;
	if (decon->prev_overlap_cnt < regs->win_overlap_cnt) {
		exynos7_update_media_layers(media_type, regs->win_overlap_cnt);
		decon->cur_overlap_cnt = regs->win_overlap_cnt;
	}
#endif

#ifdef CONFIG_DECON_PM_QOS_REQUESTS
	decon_update_qos_requests(decon, regs, false);
#endif
	__decon_update_regs(decon, regs);
	decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
	if (decon_reg_wait_for_update_timeout(decon->id, 300 * 1000) < 0)
		decon_dump(decon);

	if (!decon->id && decon->pdata->trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
				decon->pdata->trig_mode, DECON_TRIG_DISABLE);

	for (i = 0; i < decon->pdata->max_win; i++) {
		plane_cnt = decon_get_plane_cnt(regs->win_config[i].format);
		for (j = 0; j < plane_cnt; ++j)
			decon_free_dma_buf(decon, &old_dma_bufs[i][j]);
	}

	sw_sync_timeline_inc(decon->timeline, 1);

#if defined(CONFIG_DECON_DEVFREQ)
	if (decon->prev_overlap_cnt > regs->win_overlap_cnt) {
		exynos7_update_media_layers(media_type, regs->win_overlap_cnt);
		decon->cur_overlap_cnt = regs->win_overlap_cnt;
	}
	decon->prev_overlap_cnt = regs->win_overlap_cnt;
#endif
}

static void decon_update_regs_handler(struct kthread_work *work)
{
	struct decon_device *decon =
			container_of(work, struct decon_device, update_regs_work);
	struct decon_reg_data *data, *next;
	struct list_head saved_list;

	if (decon->state == DECON_STATE_LPD)
		decon_warn("%s: LPD state: %d\n", __func__, decon_get_lpd_block_cnt(decon));

	mutex_lock(&decon->update_regs_list_lock);
	saved_list = decon->update_regs_list;
	list_replace_init(&decon->update_regs_list, &saved_list);
	mutex_unlock(&decon->update_regs_list_lock);

	list_for_each_entry_safe(data, next, &saved_list, list) {
		decon_update_regs(decon, data);
		decon_lpd_unblock(decon);
		list_del(&data->list);
		kfree(data);
	}
}

static int decon_set_win_config(struct decon_device *decon,
		struct decon_win_config_data *win_data)
{
	struct decon_win_config *win_config = win_data->config;
	int ret = 0;
	unsigned short i, j;
	struct decon_reg_data *regs;
	struct sync_fence *fence;
	struct sync_pt *pt;
	int fd;
	unsigned int bw = 0;
	int plane_cnt = 0;

	fd = get_unused_fd();
	if (fd < 0)
		return fd;

	mutex_lock(&decon->output_lock);

	if (decon->state == DECON_STATE_OFF) {
		decon->timeline_max++;
		pt = sw_sync_pt_create(decon->timeline, decon->timeline_max);
		fence = sync_fence_create("display", pt);
		sync_fence_install(fence, fd);
		win_data->fence = fd;

		sw_sync_timeline_inc(decon->timeline, 1);
		goto err;
	}

	regs = kzalloc(sizeof(struct decon_reg_data), GFP_KERNEL);
	if (!regs) {
		decon_err("could not allocate decon_reg_data\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < decon->pdata->max_win; i++) {
		decon->windows[i]->prev_fix =
			decon->windows[i]->fbinfo->fix;
		decon->windows[i]->prev_var =
			decon->windows[i]->fbinfo->var;

	}

#ifdef CONFIG_FB_WINDOW_UPDATE
	if (decon->out_type == DECON_OUT_DSI)
		decon_set_win_update_config(decon, win_config, regs);
#endif

	for (i = 0; i < decon->pdata->max_win && !ret; i++) {
		struct decon_win_config *config = &win_config[i];
		struct decon_win *win = decon->windows[i];

		bool enabled = 0;
		u32 color_map = WIN_MAP_MAP | WIN_MAP_MAP_COLOUR(0);

		switch (config->state) {
		case DECON_WIN_STATE_DISABLED:
			break;
		case DECON_WIN_STATE_COLOR:
			enabled = 1;
			color_map |= WIN_MAP_MAP_COLOUR(config->color);
			regs->vidosd_a[i] = vidosd_a(config->dst.x, config->dst.y);
			regs->vidosd_b[i] = vidosd_b(config->dst.x, config->dst.y,
					config->dst.w, config->dst.h);
			break;
		case DECON_WIN_STATE_BUFFER:
			if (IS_ENABLED(CONFIG_DECON_BLOCKING_MODE))
				decon_set_win_blocking_mode(decon, win,
					win_config, regs);

			ret = decon_set_win_buffer(decon, win, config, regs);
			if (!ret) {
				enabled = 1;
				color_map = 0;
			}
			break;
		default:
			decon_warn("unrecognized window state %u",
					config->state);
			ret = -EINVAL;
			break;
		}
		if (enabled)
			regs->wincon[i] |= WINCON_ENWIN;
		else
			regs->wincon[i] &= ~WINCON_ENWIN;

		/*
		 * Because BURSTLEN field does not have shadow register,
		 * this bit field should be retain always.
		 * exynos7580 must be set 16 burst
		 */
		regs->wincon[i] |= WINCON_BURSTLEN_16WORD;

		regs->winmap[i] = color_map;

		if (enabled && config->state == DECON_WIN_STATE_BUFFER) {
			bw += decon_calc_bandwidth(config->dst.w, config->dst.h,
					win->fbinfo->var.bits_per_pixel,
					win->fps);
		}
	}

	for (i = 0; i < decon->pdata->max_win; i++)
		memcpy(&regs->win_config[i], &win_config[i],
				sizeof(struct decon_win_config));


	regs->bandwidth = bw;

	decon_dbg("Total BW = %d Mbits, Max BW per window = %d Mbits\n",
			bw / (1024 * 1024), MAX_BW_PER_WINDOW / (1024 * 1024));

#ifdef CONFIG_DECON_DEVFREQ
	regs->win_overlap_cnt = decon_get_overlap_cnt(decon, win_config);
#endif

	if (ret) {
		for (i = 0; i < decon->pdata->max_win; i++) {
			decon->windows[i]->fbinfo->fix = decon->windows[i]->prev_fix;
			decon->windows[i]->fbinfo->var = decon->windows[i]->prev_var;

			plane_cnt = decon_get_plane_cnt(regs->win_config[i].format);
			for (j = 0; j < plane_cnt; ++j)
				decon_free_dma_buf(decon, &regs->dma_buf_data[i][j]);
		}
		put_unused_fd(fd);
		kfree(regs);
	} else if (decon->out_type == DECON_OUT_DSI) {
		decon_lpd_block(decon);
		mutex_lock(&decon->update_regs_list_lock);
		decon->timeline_max++;
		pt = sw_sync_pt_create(decon->timeline, decon->timeline_max);
		fence = sync_fence_create("display", pt);
		sync_fence_install(fence, fd);
		win_data->fence = fd;

		list_add_tail(&regs->list, &decon->update_regs_list);
		mutex_unlock(&decon->update_regs_list_lock);
		queue_kthread_work(&decon->update_regs_worker,
				&decon->update_regs_work);
	}
err:
	mutex_unlock(&decon->output_lock);
	return ret;
}

static int decon_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	int ret;
	u32 crtc;

	/* enable lpd only when system is ready to interact with driver */
	decon_lpd_enable();

	decon_lpd_block_exit(decon);

	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		if (get_user(crtc, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		if (crtc == 0)
			ret = decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		else
			ret = -ENODEV;

		break;

	case S3CFB_WIN_POSITION:
		if (copy_from_user(&decon->ioctl_data.user_window,
				(struct decon_user_window __user *)arg,
				sizeof(decon->ioctl_data.user_window))) {
			ret = -EFAULT;
			break;
		}

		if (decon->ioctl_data.user_window.x < 0)
			decon->ioctl_data.user_window.x = 0;
		if (decon->ioctl_data.user_window.y < 0)
			decon->ioctl_data.user_window.y = 0;

		ret = decon_set_window_position(info, decon->ioctl_data.user_window);
		break;

	case S3CFB_WIN_SET_PLANE_ALPHA:
		if (copy_from_user(&decon->ioctl_data.user_alpha,
				(struct s3c_fb_user_plane_alpha __user *)arg,
				sizeof(decon->ioctl_data.user_alpha))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_plane_alpha_blending(info, decon->ioctl_data.user_alpha);
		break;

	case S3CFB_WIN_SET_CHROMA:
		if (copy_from_user(&decon->ioctl_data.user_chroma,
				   (struct s3c_fb_user_chroma __user *)arg,
				   sizeof(decon->ioctl_data.user_chroma))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_chroma_key(info, decon->ioctl_data.user_chroma);
		break;

	case S3CFB_SET_VSYNC_INT:
		if (get_user(decon->ioctl_data.vsync, (int __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_vsync_int(info, decon->ioctl_data.vsync);
		break;

	case S3CFB_WIN_CONFIG:
		if (copy_from_user(&decon->ioctl_data.win_data,
				   (struct decon_win_config_data __user *)arg,
				   sizeof(decon->ioctl_data.win_data))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_win_config(decon, &decon->ioctl_data.win_data);
		if (ret)
			break;

		if (copy_to_user(&((struct decon_win_config_data __user *)arg)->fence,
				 &decon->ioctl_data.win_data.fence,
				 sizeof(decon->ioctl_data.win_data.fence))) {
			ret = -EFAULT;
			break;
		}
		break;

	default:
		ret = -ENOTTY;
	}

	decon_lpd_unblock(decon);
	return ret;
}

int decon_release(struct fb_info *info, int user)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;

	decon_info("%s +\n", __func__);

	if (decon->id && decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY) {
		find_subdev_mipi(decon);
		decon_info("output device of decon%d is changed to %s\n",
				decon->id, decon->output_sd->name);
	}

	decon_info("%s -\n", __func__);

	return 0;
}

extern int decon_set_par(struct fb_info *info);
extern int decon_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);
extern int decon_setcolreg(unsigned regno,
			    unsigned red, unsigned green, unsigned blue,
			    unsigned transp, struct fb_info *info);
extern int decon_mmap(struct fb_info *info, struct vm_area_struct *vma);

/* ---------- FREAMBUFFER INTERFACE ----------- */
static struct fb_ops decon_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= decon_check_var,
	.fb_set_par	= decon_set_par,
	.fb_blank	= decon_blank,
	.fb_setcolreg	= decon_setcolreg,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
	.fb_ioctl	= decon_ioctl,
	.fb_pan_display	= decon_pan_display,
	.fb_mmap	= decon_mmap,
	.fb_release	= decon_release,
};

/* ---------- POWER MANAGEMENT ----------- */
void decon_clocks_info(struct decon_device *decon)
{
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.pclk),
				clk_get_rate(decon->res.pclk) / MHZ);
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.aclk),
				clk_get_rate(decon->res.aclk) / MHZ);
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.eclk),
				clk_get_rate(decon->res.eclk) / MHZ);
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.vclk),
				clk_get_rate(decon->res.vclk) / MHZ);
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.aclk_disp),
				clk_get_rate(decon->res.aclk_disp) / MHZ);
}

void decon_put_clocks(struct decon_device *decon)
{
	clk_put(decon->res.pclk);
	clk_put(decon->res.aclk);
	clk_put(decon->res.eclk);
	clk_put(decon->res.vclk);
	clk_put(decon->res.aclk_disp);
	clk_put(decon->res.mif_pll);
}

static int decon_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct decon_device *decon = platform_get_drvdata(pdev);

	decon_dbg("decon%d %s +\n", decon->id, __func__);
	mutex_lock(&decon->mutex);

	decon_int_set_clocks(decon);

	clk_prepare_enable(decon->res.pclk);
	clk_prepare_enable(decon->res.aclk);
	clk_prepare_enable(decon->res.eclk);
	clk_prepare_enable(decon->res.vclk);
	clk_prepare_enable(decon->res.aclk_disp);

	if (decon->state == DECON_STATE_INIT)
		decon_clocks_info(decon);

	mutex_unlock(&decon->mutex);
	decon_dbg("decon%d %s -\n", decon->id, __func__);

	return 0;
}

static int decon_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct decon_device *decon = platform_get_drvdata(pdev);

	decon_dbg("decon%d %s +\n", decon->id, __func__);
	mutex_lock(&decon->mutex);

	clk_disable_unprepare(decon->res.pclk);
	clk_disable_unprepare(decon->res.aclk);
	clk_disable_unprepare(decon->res.eclk);
	clk_disable_unprepare(decon->res.vclk);
	clk_disable_unprepare(decon->res.aclk_disp);

	mutex_unlock(&decon->mutex);
	decon_dbg("decon%d %s -\n", decon->id, __func__);

	return 0;
}

static const struct dev_pm_ops decon_pm_ops = {
	.runtime_suspend = decon_runtime_suspend,
	.runtime_resume	 = decon_runtime_resume,
};

/* ---------- MEDIA CONTROLLER MANAGEMENT ----------- */
static long decon_sd_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	long ret = 0;

	switch (cmd) {
	case DECON_IOC_LPD_EXIT_LOCK:
		decon_lpd_block_exit(decon);
		break;
	case DECON_IOC_LPD_UNLOCK:
		decon_lpd_unblock(decon);
		break;
	default:
		dev_err(decon->dev, "unsupported ioctl");
		ret = -EINVAL;
	}
	return ret;
}

static int decon_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int decon_s_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *format)
{
	decon_err("unsupported ioctl");
	return -EINVAL;
}

static const struct v4l2_subdev_core_ops decon_sd_core_ops = {
	.ioctl = decon_sd_ioctl,
};

static const struct v4l2_subdev_video_ops decon_sd_video_ops = {
	.s_stream = decon_s_stream,
};

static const struct v4l2_subdev_pad_ops	decon_sd_pad_ops = {
	.set_fmt = decon_s_fmt,
};

static const struct v4l2_subdev_ops decon_sd_ops = {
	.video = &decon_sd_video_ops,
	.core = &decon_sd_core_ops,
	.pad = &decon_sd_pad_ops,
};

static int decon_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations decon_entity_ops = {
	.link_setup = decon_link_setup,
};

static int decon_register_subdev_nodes(struct decon_device *decon,
					struct exynos_md *md)
{
	int ret = v4l2_device_register_subdev_nodes(&md->v4l2_dev);
	if (ret) {
		decon_err("failed to make nodes for subdev\n");
		return ret;
	}

	decon_info("Register V4L2 subdev nodes for DECON\n");

	return 0;

}

static int decon_create_links(struct decon_device *decon,
					struct exynos_md *md)
{
	int ret;
	char err[80];

	decon_info("decon%d create links\n", decon->id);
	memset(err, 0, sizeof(err));

	/* link creation: decon <-> output */
	if (!decon->id)
		ret = create_link_mipi(decon);
	else if (decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY)
		ret = create_link_mipi(decon);

	return ret;
}

static void decon_unregister_entity(struct decon_device *decon)
{
	v4l2_device_unregister_subdev(&decon->sd);
}

static int decon_register_entity(struct decon_device *decon)
{
	struct v4l2_subdev *sd = &decon->sd;
	struct media_pad *pads = decon->pads;
	struct media_entity *me = &sd->entity;
	struct exynos_md *md;
	int i, n_pad, ret = 0;

	/* init DECON sub-device */
	v4l2_subdev_init(sd, &decon_sd_ops);
	sd->owner = THIS_MODULE;
	snprintf(sd->name, sizeof(sd->name), "exynos-decon%d", decon->id);

	/* DECON sub-device can be opened in user space */
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	/* init DECON sub-device as entity */
	n_pad = decon->n_sink_pad + decon->n_src_pad;
	for (i = 0; i < decon->n_sink_pad; i++)
		pads[i].flags = MEDIA_PAD_FL_SINK;
	for (i = decon->n_sink_pad; i < n_pad; i++)
		pads[i].flags = MEDIA_PAD_FL_SOURCE;

	me->ops = &decon_entity_ops;
	ret = media_entity_init(me, n_pad, pads, 0);
	if (ret) {
		decon_err("failed to initialize media entity\n");
		return ret;
	}

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		decon_err("failed to get output media device\n");
		return -ENODEV;
	}

	ret = v4l2_device_register_subdev(&md->v4l2_dev, sd);
	if (ret) {
		decon_err("failed to register DECON subdev\n");
		return ret;
	}
	decon_info("%s entity init\n", sd->name);

	if (!decon->id)
		ret = find_subdev_mipi(decon);
	else if (decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY)
		ret = find_subdev_mipi(decon);

	return ret;
}

static void decon_release_windows(struct decon_win *win)
{
	if (win->fbinfo)
		framebuffer_release(win->fbinfo);
}

static int decon_fb_alloc_memory(struct decon_device *decon, struct decon_win *win)
{
	struct decon_fb_pd_win *windata = &win->windata;
	unsigned int real_size, virt_size, size;
	struct fb_info *fbi = win->fbinfo;
	struct ion_handle *handle;
	dma_addr_t map_dma;
	struct dma_buf *buf;
	void *vaddr;
	unsigned int ret;

	dev_info(decon->dev, "allocating memory for display\n");

	real_size = windata->win_mode.videomode.xres * windata->win_mode.videomode.yres;
	virt_size = windata->virtual_x * windata->virtual_y;

	dev_info(decon->dev, "real_size=%u (%u.%u), virt_size=%u (%u.%u)\n",
		real_size, windata->win_mode.videomode.xres, windata->win_mode.videomode.yres,
		virt_size, windata->virtual_x, windata->virtual_y);

	size = (real_size > virt_size) ? real_size : virt_size;
	size *= (windata->max_bpp > 16) ? 32 : windata->max_bpp;
	size /= 8;

	fbi->fix.smem_len = size;
	size = PAGE_ALIGN(size);

	dev_info(decon->dev, "want %u bytes for window[%d]\n", size, win->index);

#if defined(CONFIG_ION_EXYNOS)
	handle = ion_alloc(decon->ion_client, (size_t)size, 0,
					EXYNOS_ION_HEAP_SYSTEM_MASK, 0);
	if (IS_ERR(handle)) {
		dev_err(decon->dev, "failed to ion_alloc\n");
		return -ENOMEM;
	}

	buf = ion_share_dma_buf(decon->ion_client, handle);
	if (IS_ERR_OR_NULL(buf)) {
		dev_err(decon->dev, "ion_share_dma_buf() failed\n");
		goto err_share_dma_buf;
	}

	vaddr = ion_map_kernel(decon->ion_client, handle);

	fbi->screen_base = vaddr;

	win->dma_buf_data[1].fence = NULL;
	win->dma_buf_data[2].fence = NULL;
	ret = decon_map_ion_handle(decon, decon->dev, &win->dma_buf_data[0],
			handle, buf, win->index);
	if (!ret)
		goto err_map;
	map_dma = win->dma_buf_data[0].dma_addr;

	dev_info(decon->dev, "alloated memory\n");
#else
	fbi->screen_base = dma_alloc_writecombine(decon->dev, size,
						  &map_dma, GFP_KERNEL);
	if (!fbi->screen_base)
		return -ENOMEM;

	dev_dbg(decon->dev, "mapped %x to %p\n",
		(unsigned int)map_dma, fbi->screen_base);

	memset(fbi->screen_base, 0x0, size);
#endif
	fbi->fix.smem_start = map_dma;

	dev_info(decon->dev, "fb start addr = 0x%x\n", (u32)fbi->fix.smem_start);

	return 0;

#ifdef CONFIG_ION_EXYNOS
err_map:
	dma_buf_put(buf);
err_share_dma_buf:
	ion_free(decon->ion_client, handle);
	return -ENOMEM;
#endif
}

static void decon_missing_pixclock(struct decon_fb_videomode *win_mode)
{
	u64 pixclk = 1000000000000ULL;
	u32 div;
	u32 width, height;

	width = win_mode->videomode.xres;
	height = win_mode->videomode.yres;

	div = width * height * (win_mode->videomode.refresh ? : 60);

	do_div(pixclk, div);
	win_mode->videomode.pixclock = pixclk;
}

static int decon_acquire_windows(struct decon_device *decon, int idx)
{
	struct decon_win *win;
	struct fb_info *fbinfo;
	struct fb_var_screeninfo *var;
	struct decon_lcd *lcd_info = NULL;
	int ret, i;

	decon_dbg("acquire DECON window%d\n", idx);

	fbinfo = framebuffer_alloc(sizeof(struct decon_win), decon->dev);
	if (!fbinfo) {
		decon_err("failed to allocate framebuffer\n");
		return -ENOENT;
	}

	win = fbinfo->par;
	decon->windows[idx] = win;
	var = &fbinfo->var;
	win->fbinfo = fbinfo;
	/* fbinfo->fbops = &decon_fb_ops; */
	/* fbinfo->flags = FBINFO_FLAG_DEFAULT; */
	win->decon = decon;
	win->index = idx;

	win->windata.default_bpp = 32;
	win->windata.max_bpp = 32;
	if (!decon->id || decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY) {
		lcd_info = decon->lcd_info;
		win->windata.virtual_x = lcd_info->xres;
		win->windata.virtual_y = lcd_info->yres * 2;
		win->windata.width = lcd_info->xres;
		win->windata.height = lcd_info->yres;
		win->windata.win_mode.videomode.left_margin = lcd_info->hbp;
		win->windata.win_mode.videomode.right_margin = lcd_info->hfp;
		win->windata.win_mode.videomode.upper_margin = lcd_info->vbp;
		win->windata.win_mode.videomode.lower_margin = lcd_info->vfp;
		win->windata.win_mode.videomode.hsync_len = lcd_info->hsa;
		win->windata.win_mode.videomode.vsync_len = lcd_info->vsa;
		win->windata.win_mode.videomode.xres = lcd_info->xres;
		win->windata.win_mode.videomode.yres = lcd_info->yres;
		decon_missing_pixclock(&win->windata.win_mode);
	}

	for (i = 0; i < MAX_BUF_PLANE_CNT; ++i)
		memset(&win->dma_buf_data[i], 0, sizeof(win->dma_buf_data));

	if ((!decon->id || decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY)
			&& win->index == decon->pdata->default_win) {
		ret = decon_fb_alloc_memory(decon, win);
		if (ret) {
			dev_err(decon->dev, "failed to allocate display memory\n");
			return ret;
		}
	}

	fb_videomode_to_var(&fbinfo->var, &win->windata.win_mode.videomode);

	fbinfo->fix.type	= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.accel	= FB_ACCEL_NONE;
	fbinfo->var.activate	= FB_ACTIVATE_NOW;
	fbinfo->var.vmode	= FB_VMODE_NONINTERLACED;
	fbinfo->var.bits_per_pixel = win->windata.default_bpp;
	fbinfo->var.width	= win->windata.width;
	fbinfo->var.height	= win->windata.height;
	fbinfo->fbops		= &decon_fb_ops;
	fbinfo->flags		= FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette  = &win->pseudo_palette;

	ret = decon_check_var(&fbinfo->var, fbinfo);
	if (ret < 0) {
		dev_err(decon->dev, "check_var failed on initial video params\n");
		return ret;
	}

	ret = fb_alloc_cmap(&fbinfo->cmap, 256 /* palette size */, 1);
	if (ret == 0)
		fb_set_cmap(&fbinfo->cmap, fbinfo);
	else
		dev_err(decon->dev, "failed to allocate fb cmap\n");

	decon_info("decon%d window[%d] create\n", decon->id, idx);
	return 0;
}

static int decon_acquire_window(struct decon_device *decon)
{
	int i, ret;

	for (i = 0; i < decon->n_sink_pad; i++) {
		ret = decon_acquire_windows(decon, i);
		if (ret < 0) {
			decon_err("failed to create decon-int window[%d]\n", i);
			for (; i >= 0; i--)
				decon_release_windows(decon->windows[i]);
			return ret;
		}
	}

	return 0;
}

static void decon_parse_pdata(struct decon_device *decon, struct device *dev)
{
	if (dev->of_node) {
		decon->id = of_alias_get_id(dev->of_node, "decon");
		of_property_read_u32(dev->of_node, "ip_ver",
					&decon->pdata->ip_ver);
		of_property_read_u32(dev->of_node, "n_sink_pad",
					&decon->n_sink_pad);
		of_property_read_u32(dev->of_node, "n_src_pad",
					&decon->n_src_pad);
		of_property_read_u32(dev->of_node, "max_win",
					&decon->pdata->max_win);
		of_property_read_u32(dev->of_node, "default_win",
					&decon->pdata->default_win);
		/* video mode: 0, dp: 1 mipi command mode: 2 */
		of_property_read_u32(dev->of_node, "psr_mode",
					&decon->pdata->psr_mode);
		/* H/W trigger: 0, S/W trigger: 1 */
		of_property_read_u32(dev->of_node, "trig_mode",
					&decon->pdata->trig_mode);
		decon_info("decon-%s: ver%d, max win%d, %s mode, %s trigger\n",
			"int", decon->pdata->ip_ver,
			decon->pdata->max_win,
			decon->pdata->psr_mode ? "command" : "video",
			decon->pdata->trig_mode ? "sw" : "hw");

		/* single DSI: 0, dual DSI: 1 */
		of_property_read_u32(dev->of_node, "dsi_mode",
				&decon->pdata->dsi_mode);
		/* disp_pll */
		of_property_read_u32(dev->of_node, "disp-pll-clk",
				&decon->pdata->disp_pll_clk);
		decon_info("dsi mode(%d). 0: single 1: dual dsi 2: dual display\n",
				decon->pdata->dsi_mode);
	} else {
		decon_warn("no device tree information\n");
	}
}

/* --------- DRIVER INITIALIZATION ---------- */
static int decon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct decon_device *decon;
	struct resource *res;
	struct fb_info *fbinfo;
	int ret = 0;
	char device_name[MAX_NAME_SIZE];
	struct decon_psr_info psr;
	struct decon_init_param p;
	struct decon_regs_data win_regs;
	struct dsim_device *dsim;
	struct exynos_md *md;
	struct device_node *cam_stat;
	int win_idx = 0;
#if defined(CONFIG_DECON_DEVFREQ)
	enum devfreq_media_type media_type;
	unsigned int bw = 0;
#endif

	dev_info(dev, "%s start\n", __func__);

	decon = devm_kzalloc(dev, sizeof(struct decon_device), GFP_KERNEL);
	if (!decon) {
		decon_err("no memory for decon device\n");
		return -ENOMEM;
	}

	/* setup pointer to master device */
	decon->dev = dev;
	decon->pdata = devm_kzalloc(dev, sizeof(struct exynos_decon_platdata),
						GFP_KERNEL);
	if (!decon->pdata) {
		decon_err("no memory for DECON platdata\n");
		return -ENOMEM;
	}

	/* store platform data ptr to decon_tv context */
	decon_parse_pdata(decon, dev);
	win_idx = decon->pdata->default_win;

	/* init clock setting for decon */
	decon_int_drvdata = decon;
	decon_int_get_clocks(decon);

	/* Get memory resource and map SFR region. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	decon->regs = devm_request_and_ioremap(dev, res);
	if (decon->regs == NULL) {
		decon_err("failed to claim register region\n");
		return -ENOENT;
	}

	spin_lock_init(&decon->slock);
	init_waitqueue_head(&decon->vsync_info.wait);
	mutex_init(&decon->vsync_info.irq_lock);

	/* Get IRQ resource and register IRQ, create thread */
	ret = decon_int_register_irq(pdev, decon);
	if (ret)
		goto fail;
	ret = decon_int_create_vsync_thread(decon);
	if (ret)
		goto fail;
	ret = decon_int_create_psr_thread(decon);
	if (ret)
		goto fail_vsync_thread;
	ret = decon_fb_config_eint_for_te(pdev, decon);
	if (ret)
		goto fail_psr_thread;
	ret = decon_int_register_lpd_work(decon);
	if (ret)
		goto fail_psr_thread;

	snprintf(device_name, MAX_NAME_SIZE, "decon%d", decon->id);
	decon->ion_client = ion_client_create(ion_exynos, device_name);
	if (IS_ERR(decon->ion_client)) {
		decon_err("failed to ion_client_create\n");
		goto fail_thread;
	}

	/* exynos_create_iovmm(dev, decon->n_sink_pad, 0); */ /* It doesn't need any more */
	ret = iovmm_activate(decon->dev);
	if (ret < 0) {
		decon_err("failed to reactivate vmm\n");
		goto fail_iovmm;
	}

	/* register internal and external DECON as entity */
	ret = decon_register_entity(decon);
	if (ret)
		goto fail_psr_thread;

	/* configure windows */
	ret = decon_acquire_window(decon);
	if (ret)
		goto fail_entity;

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		decon_err("failed to get output media device\n");
		goto fail_entity;
	}

	decon->mdev = md;

	/* link creation: vpp <-> decon / decon <-> output */
	ret = decon_create_links(decon, md);
	if (ret)
		goto fail_entity;

	ret = decon_register_subdev_nodes(decon, md);
	if (ret)
		goto fail_entity;


	/* register framebuffer */
	fbinfo = decon->windows[decon->pdata->default_win]->fbinfo;
	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		decon_err("failed to register framebuffer\n");
		goto fail_entity;
	}

	/* mutex mechanism */
	mutex_init(&decon->output_lock);
	mutex_init(&decon->mutex);

	/* init work thread for update registers */
	INIT_LIST_HEAD(&decon->update_regs_list);
	mutex_init(&decon->update_regs_list_lock);
	init_kthread_worker(&decon->update_regs_worker);

	decon->update_regs_thread = kthread_run(kthread_worker_fn,
			&decon->update_regs_worker, device_name);
	if (IS_ERR(decon->update_regs_thread)) {
		ret = PTR_ERR(decon->update_regs_thread);
		decon->update_regs_thread = NULL;
		decon_err("failed to run update_regs thread\n");
		goto fail_entity;
	}
	init_kthread_work(&decon->update_regs_work, decon_update_regs_handler);

	decon->timeline = sw_sync_timeline_create(device_name);
	decon->timeline_max = 1;
	snprintf(device_name, MAX_NAME_SIZE, "decon%d-wb", decon->id);
	decon->wb_timeline = sw_sync_timeline_create(device_name);
	decon->wb_timeline_max = 0;

	if (!decon->id || decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY) {
		ret = decon_int_set_lcd_config(decon);
		if (ret) {
			decon_err("failed to set lcd information\n");
			goto fail_iovmm;
		}
	}
	platform_set_drvdata(pdev, decon);
	pm_runtime_enable(dev);

	if (!decon->id || decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY) {
#if defined(CONFIG_DECON_DEVFREQ)
		if (!decon->id) {
			bw = decon_calc_bandwidth(fbinfo->var.xres, fbinfo->var.yres,
					fbinfo->var.bits_per_pixel,
					decon->windows[decon->pdata->default_win]->fps);
			exynos7_update_media_layers(TYPE_RESOLUTION, bw);
			media_type = TYPE_DECON;
		} else {
			media_type = TYPE_TV;
		}
		exynos7_update_media_layers(media_type, 1);
#endif

#ifdef CONFIG_DECON_PM_QOS_REQUESTS
		pm_qos_add_request(&decon->mif_qos,
					PM_QOS_BUS_THROUGHPUT,
					PM_QOS_DEFAULT_VALUE);
		pm_qos_add_request(&decon->int_qos,
					PM_QOS_DEVICE_THROUGHPUT,
					PM_QOS_DEFAULT_VALUE);
#endif

		decon->cur_overlap_cnt = 1;
		decon->prev_overlap_cnt = 1;
#if defined(CONFIG_PM_RUNTIME)
		pm_runtime_get_sync(decon->dev);
#else
		decon_runtime_resume(decon->dev);
#endif

		decon_to_init_param(decon, &p);

		/* DECON does not need to start, if DECON is already
		 * running(enabled in LCD_ON_UBOOT) */
		if (decon_reg_get_stop_status(decon->id)) {
			decon_reg_init_probe(decon->id, decon->pdata->dsi_mode, &p);
			if (decon->pdata->trig_mode == DECON_HW_TRIG)
				decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
						decon->pdata->trig_mode, DECON_TRIG_DISABLE);
			goto decon_init_done;
		}

		decon_reg_shadow_protect_win(decon->id, win_idx, 1);

		decon_reg_init(decon->id, decon->pdata->dsi_mode, &p);

		win_regs.wincon = WINCON_BPPMODE_ARGB8888;
		win_regs.winmap = 0x0;
		win_regs.vidosd_a = vidosd_a(0, 0);
		win_regs.vidosd_b = vidosd_b(0, 0, fbinfo->var.xres, fbinfo->var.yres);
		win_regs.vidosd_c = vidosd_c(0x0, 0x0, 0x0);
		win_regs.vidosd_d = vidosd_d(0xff, 0xff, 0xff);
		win_regs.vidw_buf_start = fbinfo->fix.smem_start;
		win_regs.vidw_whole_w = fbinfo->var.xres_virtual;
		win_regs.vidw_whole_h = fbinfo->var.yres_virtual;
		win_regs.vidw_offset_x = fbinfo->var.xoffset;
		win_regs.vidw_offset_y = fbinfo->var.yoffset;
		win_regs.type = IDMA_G0;

		decon_reg_set_regs_data(decon->id, win_idx, &win_regs);

		decon_reg_shadow_protect_win(decon->id, win_idx, 0);

		decon_to_psr_info(decon, &psr);
		decon_reg_start(decon->id, decon->pdata->dsi_mode, &psr);

		decon_reg_activate_window(decon->id, win_idx);

		decon_reg_set_winmap(decon->id, win_idx, 0x000000 /* black */, 1);

		if (decon->pdata->trig_mode == DECON_HW_TRIG)
			decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
					decon->pdata->trig_mode, DECON_TRIG_ENABLE);

		dsim = container_of(decon->output_sd, struct dsim_device, sd);
		call_panel_ops(dsim, displayon, dsim);

decon_init_done:

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		if (!decon->id) {
			ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL);
			if (ret)
				decon_err("Failed to call DSIM packet go enable in %s!\n", __func__);
		}
#endif
		decon->state = DECON_STATE_ON;

		pm_stay_awake(decon->dev);
		dev_warn(decon->dev, "pm_stay_awake");
		cam_stat = of_get_child_by_name(decon->dev->of_node, "cam-stat");
		if (!cam_stat) {
			decon_info("No DT node for cam-stat\n");
		} else {
			decon->cam_status[0] = of_iomap(cam_stat, 0);
			if (!decon->cam_status[0])
				decon_info("Failed to get CAM0-STAT Reg\n");

			decon->cam_status[1] = of_iomap(cam_stat, 1);
			if (!decon->cam_status[1])
				decon_info("Failed to get CAM1-STAT Reg\n");
		}
	} else { /* DECON-EXT(only single-dsi) is off at probe */
		decon->state = DECON_STATE_INIT;
	}

	decon_info("decon%d registered successfully", decon->id);

	return 0;

fail_iovmm:
	iovmm_deactivate(dev);

fail_thread:
	if (decon->update_regs_thread)
		kthread_stop(decon->update_regs_thread);

fail_entity:
	decon_unregister_entity(decon);

fail_psr_thread:
	decon_int_destroy_psr_thread(decon);

fail_vsync_thread:
	decon_int_destroy_vsync_thread(decon);

fail:
	decon_err("decon probe fail");
	return ret;
}

static int decon_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct decon_device *decon = platform_get_drvdata(pdev);
	int i;

#ifdef CONFIG_DECON_PM_QOS_REQUESTS
	pm_qos_remove_request(&decon->int_qos);
	pm_qos_remove_request(&decon->mif_qos);
#endif

	pm_runtime_disable(dev);
	decon_put_clocks(decon);

	iovmm_deactivate(dev);
	unregister_framebuffer(decon->windows[0]->fbinfo);

	if (decon->update_regs_thread)
		kthread_stop(decon->update_regs_thread);

	for (i = 0; i < decon->pdata->max_win; i++)
		decon_release_windows(decon->windows[i]);

	kfree(decon);

	decon_info("remove sucessful\n");
	return 0;
}

static struct platform_driver decon_driver __refdata = {
	.probe		= decon_probe,
	.remove		= decon_remove,
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm	= &decon_pm_ops,
		.of_match_table = of_match_ptr(decon_device_table),
	}
};

static int exynos_decon_register(void)
{
	platform_driver_register(&decon_driver);

	return 0;
}

static void exynos_decon_unregister(void)
{
	platform_driver_unregister(&decon_driver);
}
late_initcall(exynos_decon_register);
module_exit(exynos_decon_unregister);

MODULE_AUTHOR("Ayoung Sim <a.sim@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS Soc DECON driver");
MODULE_LICENSE("GPL");
