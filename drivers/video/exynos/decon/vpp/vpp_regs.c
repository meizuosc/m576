/* linux/drivers/video/exynos/decon/vpp/vpp_regs.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS5 SoC series G-scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <mach/map.h>
#include "vpp_core.h"

#define VPP_SC_RATIO_MAX	((1 << 20) * 8 / 8)
#define VPP_SC_RATIO_7_8	((1 << 20) * 8 / 7)
#define VPP_SC_RATIO_6_8	((1 << 20) * 8 / 6)
#define VPP_SC_RATIO_5_8	((1 << 20) * 8 / 5)
#define VPP_SC_RATIO_4_8	((1 << 20) * 8 / 4)
#define VPP_SC_RATIO_3_8	((1 << 20) * 8 / 3)

extern const s16 h_coef_8t[7][16][8];
extern const s16 v_coef_4t[7][16][4];

int vpp_hw_wait_op_status(struct vpp_dev *vpp)
{
	u32 cfg = 0;

	ktime_t start = ktime_get();

	do {
		cfg = readl(vpp->regs + VG_ENABLE);
		if (!(cfg & (VG_ENABLE_OP_STATUS)))
			return 0;
		udelay(10);
	} while(ktime_us_delta(ktime_get(), start) < 1000000);

	dev_err(DEV, "timeout op_status to idle\n");

	return -EBUSY;
}

void vpp_hw_wait_idle(struct vpp_dev *vpp)
{
	u32 cfg = 0;

	ktime_t start = ktime_get();

	do {
		cfg = readl(vpp->regs + VG_ENABLE);
		if (!(cfg & (VG_ENABLE_OP_STATUS)))
			return;
		dev_warn(DEV, "vpp%d is operating...\n", vpp->id);
		udelay(10);
	} while(ktime_us_delta(ktime_get(), start) < 1000000);

	dev_err(DEV, "timeout op_status to idle\n");
}

int vpp_hw_set_sw_reset(struct vpp_dev *vpp)
{
	u32 cfg = 0;
	ktime_t start;

	cfg = readl(vpp->regs + VG_ENABLE);
	cfg |= VG_ENABLE_SRESET;

	writel(cfg, vpp->regs + VG_ENABLE);

	start = ktime_get();
	do {
		cfg = readl(vpp->regs + VG_ENABLE);
		if (!(cfg & (VG_ENABLE_SRESET)))
			return 0;
		udelay(10);
	} while(ktime_us_delta(ktime_get(), start) < 1000000);

	dev_err(DEV, "timeout sw reset\n");

	return -EBUSY;
}

void vpp_hw_set_realtime_path(struct vpp_dev *vpp)
{
	u32 cfg = readl(vpp->regs + VG_ENABLE);

	cfg |= VG_ENABLE_RT_PATH_EN;

	writel(cfg, vpp->regs + VG_ENABLE);
}

void vpp_hw_set_framedone_irq(struct vpp_dev *vpp, bool enable)
{
	u32 cfg = readl(vpp->regs + VG_IRQ);

	if (enable)
		cfg |= VG_IRQ_FRAMEDONE_MASK;
	else
		cfg &= ~VG_IRQ_FRAMEDONE_MASK;

	writel(cfg, vpp->regs + VG_IRQ);
}

void vpp_hw_set_deadlock_irq(struct vpp_dev *vpp, bool enable)
{
	u32 cfg = readl(vpp->regs + VG_IRQ);

	if (enable)
		cfg |= VG_IRQ_DEADLOCK_STATUS_MASK;
	else
		cfg &= ~VG_IRQ_DEADLOCK_STATUS_MASK;

	writel(cfg, vpp->regs + VG_IRQ);
}

void vpp_hw_set_read_slave_err_irq(struct vpp_dev *vpp, bool enable)
{
	u32 cfg = readl(vpp->regs + VG_IRQ);

	if (enable)
		cfg |= VG_IRQ_READ_SLAVE_ERROR_MASK;
	else
		cfg &= ~VG_IRQ_READ_SLAVE_ERROR_MASK;

	writel(cfg, vpp->regs + VG_IRQ);
}

void vpp_hw_set_sfr_update_done_irq(struct vpp_dev *vpp, bool enable)
{
	u32 cfg = readl(vpp->regs + VG_IRQ);

	if (enable)
		cfg |= VG_IRQ_SFR_UPDATE_DONE_MASK;
	else
		cfg &= ~VG_IRQ_SFR_UPDATE_DONE_MASK;

	writel(cfg, vpp->regs + VG_IRQ);
}

void vpp_hw_set_sfr_update_force(struct vpp_dev *vpp)
{
	u32 cfg = readl(vpp->regs + VG_ENABLE);

	cfg |= VG_ENABLE_SFR_UPDATE_FORCE;

	writel(cfg, vpp->regs + VG_ENABLE);
}

void vpp_hw_set_enable_interrupt(struct vpp_dev *vpp)
{
	u32 cfg = readl(vpp->regs + VG_IRQ);

	cfg |= VG_IRQ_ENABLE;

	writel(cfg, vpp->regs + VG_IRQ);
}

void vpp_hw_set_hw_reset_done_mask(struct vpp_dev *vpp, bool enable)
{
	u32 cfg = readl(vpp->regs + VG_IRQ);

	if (enable)
		cfg |= VG_IRQ_HW_RESET_DONE_MASK;
	else
		cfg &= ~VG_IRQ_HW_RESET_DONE_MASK;

	writel(cfg, vpp->regs + VG_IRQ);
}

int vpp_hw_set_in_format(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	u32 cfg = readl(vpp->regs + VG_IN_CON);

	cfg &= ~(VG_IN_CON_IMG_FORMAT_MASK |
			VG_IN_CON_CHROMINANCE_STRIDE_EN);
	switch(config->format) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
		cfg |= VG_IN_CON_IMG_FORMAT_ARGB8888;
		break;
	case DECON_PIXEL_FORMAT_ABGR_8888:
		cfg |= VG_IN_CON_IMG_FORMAT_ABGR8888;
		break;
	case DECON_PIXEL_FORMAT_RGBA_8888:
		cfg |= VG_IN_CON_IMG_FORMAT_RGBA8888;
		break;
	case DECON_PIXEL_FORMAT_BGRA_8888:
		cfg |= VG_IN_CON_IMG_FORMAT_BGRA8888;
		break;
	case DECON_PIXEL_FORMAT_XRGB_8888:
		cfg |= VG_IN_CON_IMG_FORMAT_XRGB8888;
		break;
	case DECON_PIXEL_FORMAT_XBGR_8888:
		cfg |= VG_IN_CON_IMG_FORMAT_XBGR8888;
		break;
	case DECON_PIXEL_FORMAT_RGBX_8888:
		cfg |= VG_IN_CON_IMG_FORMAT_RGBX8888;
		break;
	case DECON_PIXEL_FORMAT_BGRX_8888:
		cfg |= VG_IN_CON_IMG_FORMAT_BGRX8888;
		break;
	case DECON_PIXEL_FORMAT_RGB_565:
		cfg |= VG_IN_CON_IMG_FORMAT_RGB565;
		break;
	case DECON_PIXEL_FORMAT_NV16:
		cfg |= VG_IN_CON_IMG_FORMAT_YUV422_2P;
		break;
	case DECON_PIXEL_FORMAT_NV61:
		cfg |= VG_IN_CON_IMG_FORMAT_YVU422_2P;
		break;
	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV12M:
		cfg |= VG_IN_CON_IMG_FORMAT_YUV420_2P;
		break;
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV21M:
		cfg |= VG_IN_CON_IMG_FORMAT_YVU420_2P;
		break;
	default:
		dev_err(DEV, "Unsupported Format\n");
		return -EINVAL ;
	}

	writel(cfg, vpp->regs + VG_IN_CON);

	return 0;
}

void vpp_hw_set_h_coef(struct vpp_dev *vpp, u32 h_ratio)
{
	int i, j, k, sc_ratio;

	if (h_ratio <= VPP_SC_RATIO_MAX)
		sc_ratio = 0;
	else if (h_ratio <= VPP_SC_RATIO_7_8)
		sc_ratio = 1;
	else if (h_ratio <= VPP_SC_RATIO_6_8)
		sc_ratio = 2;
	else if (h_ratio <= VPP_SC_RATIO_5_8)
		sc_ratio = 3;
	else if (h_ratio <= VPP_SC_RATIO_4_8)
		sc_ratio = 4;
	else if (h_ratio <= VPP_SC_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < 9; i++) {
		for (j = 0; j < 8; j++) {
			for (k = 0; k < 2; k++) {
				__raw_writel(h_coef_8t[sc_ratio][i][j],
				       vpp->regs + VG_H_COEF(i, j, k));
			}
		}
	}
}

void vpp_hw_set_v_coef(struct vpp_dev *vpp, u32 v_ratio)
{
	int i, j, k, sc_ratio;

	if (v_ratio <= VPP_SC_RATIO_MAX)
		sc_ratio = 0;
	else if (v_ratio <= VPP_SC_RATIO_7_8)
		sc_ratio = 1;
	else if (v_ratio <= VPP_SC_RATIO_6_8)
		sc_ratio = 2;
	else if (v_ratio <= VPP_SC_RATIO_5_8)
		sc_ratio = 3;
	else if (v_ratio <= VPP_SC_RATIO_4_8)
		sc_ratio = 4;
	else if (v_ratio <= VPP_SC_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < 9; i++) {
		for (j = 0; j < 4; j++) {
			for (k = 0; k < 2; k++) {
				__raw_writel(v_coef_4t[sc_ratio][i][j],
				       vpp->regs + VG_V_COEF(i, j, k));
			}
		}
	}
}

int vpp_hw_set_rotation(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	u32 cfg = readl(vpp->regs + VG_IN_CON);

	cfg &= ~VG_IN_CON_IN_ROTATION_MASK;
	cfg |= config->vpp_parm.rot << 8;

	writel(cfg, vpp->regs + VG_IN_CON);

	return 0;
}

void vpp_hw_set_scale_ratio(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct vpp_fraction *fr = &vpp->fract_val;
	u32 h_ratio, v_ratio = 0;
	u32 tmp_width, tmp_height = 0;
	u32 tmp_fr_w, tmp_fr_h = 0;

	if (is_rotation(config)) {
		tmp_width = config->src.h;
		tmp_height = config->src.w;
		tmp_fr_w = fr->h;
		tmp_fr_h = fr->w;
	} else {
		tmp_width = config->src.w;
		tmp_height = config->src.h;
		tmp_fr_w = fr->w;
		tmp_fr_h = fr->h;
	}

	h_ratio = ((tmp_width << 20) + tmp_fr_w) / config->dst.w;
	v_ratio = ((tmp_height << 20) + tmp_fr_h) / config->dst.h;

	if (vpp->h_ratio != h_ratio) {
		writel(h_ratio, vpp->regs + VG_H_RATIO);
		vpp_hw_set_h_coef(vpp, h_ratio);
	}

	if (vpp->v_ratio != v_ratio) {
		writel(v_ratio, vpp->regs + VG_V_RATIO);
		vpp_hw_set_v_coef(vpp, v_ratio);
	}

	vpp->h_ratio = h_ratio;
	vpp->v_ratio = v_ratio;

	dev_dbg(DEV, "h_ratio : %#x, v_ratio : %#x\n",
			h_ratio, v_ratio);
}

void vpp_hw_set_in_buf_addr(struct vpp_dev *vpp)
{
	struct vpp_params *vpp_parm = &vpp->config->vpp_parm;
	dev_dbg(DEV, "y : %pa, cb : %pa, cr : %pa\n",
		&vpp_parm->addr[0], &vpp_parm->addr[1], &vpp_parm->addr[2]);
	writel(vpp_parm->addr[0], vpp->regs + VG_BASE_ADDR_Y(0));
	writel(vpp_parm->addr[1], vpp->regs + VG_BASE_ADDR_CB(0));
}

void vpp_hw_set_in_size(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	u32 cfg = 0;

	/* source offset */
	cfg = VG_SRC_OFFSET_X(config->src.x);
	cfg |= VG_SRC_OFFSET_Y(config->src.y);
	writel(cfg, vpp->regs + VG_SRC_OFFSET);

	/* source full(alloc) size */
	cfg = VG_SRC_SIZE_WIDTH(config->src.f_w);
	cfg |= VG_SRC_SIZE_HEIGHT(config->src.f_h);
	writel(cfg, vpp->regs + VG_SRC_SIZE);

	/* source cropped size */
	cfg = VG_IMG_SIZE_WIDTH(config->src.w);
	cfg |= VG_IMG_SIZE_HEIGHT(config->src.h);
	writel(cfg, vpp->regs + VG_IMG_SIZE);

	if (vpp->fract_val.w)
		config->src.w--;
	if (vpp->fract_val.h)
		config->src.h--;

	/* fraction position */
	writel(vpp->fract_val.y_x, vpp->regs + VG_YHPOSITION0);
	writel(vpp->fract_val.y_y, vpp->regs + VG_YVPOSITION0);
	writel(vpp->fract_val.c_x, vpp->regs + VG_CHPOSITION0);
	writel(vpp->fract_val.c_y, vpp->regs + VG_CVPOSITION0);
}

void vpp_hw_set_in_block_size(struct vpp_dev *vpp, bool enable)
{
	struct decon_win_config *config = vpp->config;
	u32 cfg = 0;

	if (!enable) {
		cfg = readl(vpp->regs + VG_IN_CON);
		cfg &= ~VG_IN_CON_BLOCKING_FEATURE_EN;
		writel(cfg, vpp->regs + VG_IN_CON);
		return;
	}

	/* blocking area offset */
	cfg = VG_BLK_OFFSET_X(config->block_area.x);
	cfg |= VG_BLK_OFFSET_Y(config->block_area.y);
	writel(cfg, vpp->regs + VG_BLK_OFFSET);

	/* blocking area size */
	cfg = VG_BLK_SIZE_WIDTH(config->block_area.w);
	cfg |= VG_BLK_SIZE_HEIGHT(config->block_area.h);
	writel(cfg, vpp->regs + VG_BLK_SIZE);

	cfg = readl(vpp->regs + VG_IN_CON);
	cfg |= VG_IN_CON_BLOCKING_FEATURE_EN;
	writel(cfg, vpp->regs + VG_IN_CON);

	dev_dbg(DEV, "block x : %d, y : %d, w : %d, h : %d\n",
			config->block_area.x, config->block_area.y,
			config->block_area.w, config->block_area.h);
}

void vpp_hw_set_out_size(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	u32 cfg = 0;

	/* destination scaled size */
	cfg = VG_SCALED_SIZE_WIDTH(config->dst.w);
	cfg |= VG_SCALED_SIZE_HEIGHT(config->dst.h);
	writel(cfg, vpp->regs + VG_SCALED_SIZE);
}

void vpp_hw_set_rgb_type(struct vpp_dev *vpp)
{
	u32 cfg = VG_OUT_CON_RGB_TYPE_601_WIDE;

	writel(cfg, vpp->regs + VG_OUT_CON);
}

void vpp_hw_set_plane_alpha(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	u32 cfg = readl(vpp->regs + VG_OUT_CON);

	if (config->plane_alpha > 0xFF)
		dev_warn(DEV, "%d is too much value\n",
				config->plane_alpha);
	cfg &= ~VG_OUT_CON_FRAME_ALPHA_MASK;
	cfg |= VG_OUT_CON_FRAME_ALPHA(config->plane_alpha);

	writel(cfg, vpp->regs + VG_OUT_CON);
}

void vpp_hw_set_plane_alpha_fixed(struct vpp_dev *vpp)
{
	u32 cfg = readl(vpp->regs + VG_OUT_CON);

	cfg &= ~VG_OUT_CON_FRAME_ALPHA_MASK;
	cfg |= VG_OUT_CON_FRAME_ALPHA(0xFF);

	writel(cfg, vpp->regs + VG_OUT_CON);
}

void vpp_hw_set_smart_if_pix_num(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;

	u32 cfg = readl(vpp->regs + VG_SMART_IF_PIXEL_NUM);
	cfg = config->dst.w * config->dst.h;
	writel(cfg, vpp->regs + VG_SMART_IF_PIXEL_NUM);
}

void vpp_hw_set_lookup_table(struct vpp_dev *vpp)
{
	writel(0x44444444, vpp->regs + VG_QOS_LUT07_00);
	writel(0x44444444, vpp->regs + VG_QOS_LUT15_08);
}

void vpp_hw_set_dynamic_clock_gating(struct vpp_dev *vpp)
{
	writel(0x3F, vpp->regs + VG_DYNAMIC_GATING_ENABLE);
}

