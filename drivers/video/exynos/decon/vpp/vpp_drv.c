/* linux/drivers/video/exynos/decon/vpp/vpp_drv.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS5 SoC series VPP driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/of.h>
#include <linux/exynos_iovmm.h>
#include <linux/smc.h>

#include <plat/cpu.h>
#include <mach/devfreq.h>

#include "vpp_core.h"
#include "../decon_helper.h"
/*
 * Gscaler constraints
 * This is base of without rotation.
 */

#define SRC_OFFSET_MULTIPLE	2
#define SRC_SIZE_MULTIPLE	2
#define SRC_WIDTH_MAX		8190
#define SRC_HEIGHT_MAX		4096
#define SRC_WIDTH_MIN		64
#define SRC_HEIGHT_MIN		32


#define IMG_SIZE_MULTIPLE	2
#define IMG_WIDTH_MAX		4096
#define IMG_HEIGHT_MAX		4096
#define IMG_WIDTH_MIN		64
#define IMG_HEIGHT_MIN		32

#define BLK_WIDTH_MAX		4096
#define BLK_HEIGHT_MAX		4096
#define BLK_WIDTH_MIN		144
#define BLK_HEIGHT_MIN		10

#define SCALED_SIZE_MULTIPLE	2
#define SCALED_WIDTH_MAX	4096
#define SCALED_HEIGHT_MAX	4096
#define SCALED_WIDTH_MIN	32
#define SCALED_HEIGHT_MIN	16

#define check_align(width, height, align)\
	(IS_ALIGNED(width, align) && IS_ALIGNED(height, align))
#define is_err_irq(irq) ((irq == VG_IRQ_DEADLOCK_STATUS) ||\
			(irq == VG_IRQ_READ_SLAVE_ERROR))

#define MIF_LV1			(2912000/2)
#define INT_LV7			(400000)

#define MEM_FAULT_VPP_MASTER            0
#define MEM_FAULT_VPP_CFW               1
#define MEM_FAULT_PROT_EXCEPT_0         2
#define MEM_FAULT_PROT_EXCEPT_1         3
#define MEM_FAULT_PROT_EXCEPT_2         4
#define MEM_FAULT_PROT_EXCEPT_3         5

static struct vpp_dev *vpp_for_decon;

static void vpp_dump_cfw_register(void)
{
	u32 smc_val;
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_VPP_MASTER, 0, 0);
	pr_err("=== vpp_master:0x%x \n", smc_val);
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_VPP_CFW, 0, 0);
	pr_err("=== vpp_cfw:0x%x \n", smc_val);
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_PROT_EXCEPT_0, 0, 0);
	pr_err("=== vpp_except_0:0x%x \n", smc_val);
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_PROT_EXCEPT_1, 0, 0);
	pr_err("=== vpp_except_1:0x%x \n", smc_val);
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_PROT_EXCEPT_2, 0, 0);
	pr_err("=== vpp_except_2:0x%x \n", smc_val);
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_PROT_EXCEPT_3, 0, 0);
	pr_err("=== vpp_except_3:0x%x \n", smc_val);
}

static void vpp_dump_registers(struct vpp_dev *vpp)
{
	unsigned long flags;
	dev_info(DEV, "=== VPP%d SFR DUMP ===\n", vpp->id);
	dev_info(DEV, "start count : %d, done count : %d\n",
			vpp->start_count, vpp->done_count);

	if (!test_bit(VPP_RUNNING, &vpp->state)) {
		dev_err(DEV, "vpp clocks are disabled\n");
		return;
	}

	spin_lock_irqsave(&vpp->slock, flags);
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			vpp->regs, 0xB0, false);
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			vpp->regs + 0x5B0, 0x20, false);
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			vpp->regs + 0xA48, 0x10, false);
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			vpp->regs + 0xB00, 0xB0, false);

	vpp_dump_cfw_register();
	spin_unlock_irqrestore(&vpp->slock, flags);
}

void vpp_op_timer_handler(unsigned long arg)
{
	struct vpp_dev *vpp = (struct vpp_dev *)arg;

	vpp_dump_registers(vpp);

	dev_info(DEV, "VPP[%d] irq hasn't been occured", vpp->id);
}

static int vpp_wait_for_update(struct vpp_dev *vpp)
{
	int update_cnt;
	int ret;

	if (test_bit(VPP_POWER_ON, &vpp->state)) {
		update_cnt = vpp->update_cnt_prev;
		ret = wait_event_interruptible_timeout(vpp->update_queue,
				(update_cnt != vpp->update_cnt),
				msecs_to_jiffies(17));
		if (ret == 0) {
			dev_err(DEV, "timeout of shadow update(%d, %d)\n",
				update_cnt, vpp->update_cnt);
			return -ETIMEDOUT;
		}
		DISP_SS_EVENT_LOG(DISP_EVT_VPP_UPDATE_DONE, vpp->sd, ktime_set(0, 0));
	}
	return 0;
}

static void vpp_get_align_variant(struct decon_win_config *config,
	u32 *offs, u32 *src_f, u32 *src_cr, u32 *dst_cr)
{
	if (is_rotation(config)) {
		if (is_yuv(config)) {
			*offs = *src_f = 4;
			*src_cr = 2;
			*dst_cr = 1;
		} else {
			*offs = *src_f = 2;
			*src_cr = *dst_cr = 1;
		}
	} else {
		if (is_yuv(config)) {
			*offs = *src_f = 2;
			*src_cr = 2;
			*dst_cr = 1;
		} else {
			*offs = *src_f = 1;
			*src_cr = *dst_cr = 1;
		}
	}
}

static void vpp_get_min_max_variant(struct decon_win_config *config,
		u32 *max_src, u32 *min_src_w, u32 *min_src_h,
		u32 *max_dst, u32 *min_dst_w, u32 *min_dst_h)
{
	if (is_rotation(config)) {
		if (is_yuv(config)) {
			*max_src = 2560;
			*max_dst = 4096;
			*min_src_w = 32;
			*min_src_h = 64;
			*min_dst_w = 16;
			*min_dst_h = 8;
		} else {
			*max_src = 2560;
			*max_dst = 4096;
			*min_src_w = 16;
			*min_src_h = 32;
			*min_dst_w = 16;
			*min_dst_h = 8;
		}
	} else {
		if (is_yuv(config)) {
			*max_src = 4096;
			*max_dst = 4096;
			*min_src_w = 64;
			*min_src_h = 32;
			*min_dst_w = 16;
			*min_dst_h = 8;
		} else {
			*max_src = 4096;
			*max_dst = 4096;
			*min_src_w = 32;
			*min_src_h = 16;
			*min_dst_w = 16;
			*min_dst_h = 8;
		}
	}
}

static void vpp_separate_fraction_value(struct vpp_dev *vpp,
			int *integer, u32 *fract_val)
{
	/*
	 * [30:15] : fraction val, [14:0] : integer val.
	 */
	*fract_val = (*integer >> 15) << 4;
	if (*fract_val & ~VG_POSITION_F_MASK) {
		dev_warn(DEV, "%d is unsupported value",
					*fract_val);
		*fract_val &= VG_POSITION_F_MASK;
	}

	*integer = (*integer & 0x7fff);
}

static void vpp_set_initial_phase(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct vpp_fraction *fr = &vpp->fract_val;

	if (is_fraction(src->x)) {
		vpp_separate_fraction_value(vpp, &src->x, &fr->y_x);
		fr->c_x = fr->y_x >> 1;
	}

	if (is_fraction(src->y)) {
		vpp_separate_fraction_value(vpp, &src->y, &fr->y_y);
		fr->c_y = fr->y_y >> 1;
	}

	if (is_fraction(src->w)) {
		vpp_separate_fraction_value(vpp, &src->w, &fr->w);
		src->w++;
	}

	if (is_fraction(src->h)) {
		vpp_separate_fraction_value(vpp, &src->h, &fr->h);
		src->h++;
	}
}

static int vpp_check_size(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;

	u32 offs, src_f, src_cr, dst_cr;
	u32 max_src, min_src_w, min_src_h;
	u32 max_dst, min_dst_w, min_dst_h;

	vpp_get_align_variant(config, &offs, &src_f,
					&src_cr, &dst_cr);
	vpp_get_min_max_variant(config, &max_src, &min_src_w,
		&min_src_h, &max_dst, &min_dst_w, &min_dst_h);

	if ((!check_align(src->x, src->y, offs)) ||
	   (!check_align(src->f_w, src->f_h, src_f)) ||
	   (!check_align(src->w, src->h, src_cr)) ||
	   (!check_align(dst->w, dst->h, dst_cr))) {
		dev_err(DEV, "Alignment error\n");
		goto err;
	}

	if (src->w > max_src || src->w < min_src_w ||
		src->h > max_src || src->h < min_src_h) {
		dev_err(DEV, "Unsupported source size\n");
		goto err;
	}

	if (dst->w > max_dst || dst->w < min_dst_w ||
		dst->h > max_dst || dst->h < min_dst_h) {
		dev_err(DEV, "Unsupported dest size\n");
		goto err;
	}

	return 0;
err:
	dev_err(DEV, "offset x : %d, offset y: %d\n", src->x, src->y);
	dev_err(DEV, "src f_w : %d, src f_h: %d\n", src->f_w, src->f_h);
	dev_err(DEV, "src w : %d, src h: %d\n", src->w, src->h);
	dev_err(DEV, "dst w : %d, dst h: %d\n", dst->w, dst->h);

	return -EINVAL;
}

static int vpp_check_scale_ratio(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	u32 sc_up_max_w;
	u32 sc_up_max_h;
	u32 sc_down_min_w;
	u32 sc_down_min_h;

	if (is_rotation(config)) {
		sc_up_max_w = config->dst.h << 1;
		sc_up_max_h = config->dst.w << 1;
		sc_down_min_w = config->src.h >> 3;
		sc_down_min_h = config->src.w >> 3;
	} else {
		sc_up_max_w = config->dst.w << 1;
		sc_up_max_h = config->dst.h << 1;
		sc_down_min_w = config->src.w >> 3;
		sc_down_min_h = config->src.h >> 3;
	}

	if (src->w > sc_up_max_w || src->h > sc_up_max_h) {
		dev_err(DEV, "Unsupported max(2x) scale ratio\n");
		goto err;
	}

	if (dst->w < sc_down_min_w || dst->h < sc_down_min_h) {
		dev_err(DEV, "Unsupported min(1/8x) scale ratio\n");
		goto err;
	}

	return 0;
err:
	dev_err(DEV, "src w : %d, src h: %d\n", src->w, src->h);
	dev_err(DEV, "dst w : %d, dst h: %d\n", dst->w, dst->h);
	return -EINVAL;
}

static int vpp_set_scale_info(struct vpp_dev *vpp)
{
	if (vpp_check_scale_ratio(vpp))
		return -EINVAL;
	vpp_hw_set_scale_ratio(vpp);

	return 0;
}

static int vpp_check_rotation(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	enum vpp_rotate rot = config->vpp_parm.rot;
	if (!is_vgr(vpp) && (rot > VPP_ROT_NORMAL)) {
		dev_err(DEV, "vpp-%d can't rotate\n", vpp->id);
		return -EINVAL;
	}

	return 0;
}

static int vpp_clk_enable(struct vpp_dev *vpp)
{
	int ret;

	ret = clk_enable(vpp->res.gate);
	if (ret) {
		dev_err(DEV, "Failed res.gate clk enable\n");
		return ret;
	}

	ret = clk_enable(vpp->res.pclk_vpp);
	if (ret) {
		dev_err(DEV, "Failed res.pclk clk enable\n");
		goto err_0;
	}

	ret = clk_enable(vpp->res.lh_vpp);
	if (ret) {
		dev_err(DEV, "Failed res.pclk clk enable\n");
		goto err_1;
	}

	if (is_vpp0_series(vpp)) {
		ret = clk_enable(vpp->res.aclk_vpp_sfw0);
		if(ret) {
			dev_err(DEV, "Failed res.aclk_vpp_sfw0 clk enable\n");
			goto err_2;
		}
		ret = clk_enable(vpp->res.pclk_vpp_sfw0);
		if(ret) {
			dev_err(DEV, "Failed res.pclk_vpp_sfw0 clk enable\n");
			goto err_3;
		}
	} else {
		ret = clk_enable(vpp->res.aclk_vpp_sfw1);
		if(ret) {
			dev_err(DEV, "Failed res.aclk_vpp_sfw1 clk enable\n");
			goto err_2;
		}
		ret = clk_enable(vpp->res.pclk_vpp_sfw1);
		if(ret) {
			dev_err(DEV, "Failed res.pclk_vpp_sfw1 clk enable\n");
			goto err_3;
		}
	}

	return 0;

err_3:
	if (is_vpp0_series(vpp))
		clk_disable(vpp->res.aclk_vpp_sfw0);
	else
		clk_disable(vpp->res.aclk_vpp_sfw1);
err_2:
	clk_disable(vpp->res.lh_vpp);
err_1:
	clk_disable(vpp->res.pclk_vpp);
err_0:
	clk_disable(vpp->res.gate);

	return ret;
}

static void vpp_clk_disable(struct vpp_dev *vpp)
{
	clk_disable(vpp->res.gate);
	clk_disable(vpp->res.pclk_vpp);
	clk_disable(vpp->res.lh_vpp);
	if (is_vpp0_series(vpp)) {
		clk_disable(vpp->res.aclk_vpp_sfw0);
		clk_disable(vpp->res.pclk_vpp_sfw0);
	} else {
		clk_disable(vpp->res.aclk_vpp_sfw1);
		clk_disable(vpp->res.pclk_vpp_sfw1);
	}
}

static int vpp_init(struct vpp_dev *vpp)
{
	int ret = 0;

	ret = vpp_clk_enable(vpp);
	if (ret)
		return ret;

	vpp_hw_set_realtime_path(vpp);

	vpp_hw_set_framedone_irq(vpp, false);
	vpp_hw_set_deadlock_irq(vpp, false);
	vpp_hw_set_read_slave_err_irq(vpp, false);
	vpp_hw_set_hw_reset_done_mask(vpp, false);
	vpp_hw_set_sfr_update_done_irq(vpp, false);
	vpp_hw_set_enable_interrupt(vpp);
	vpp_hw_set_lookup_table(vpp);
	vpp_hw_set_rgb_type(vpp);
	vpp_hw_set_dynamic_clock_gating(vpp);
	vpp_hw_set_plane_alpha_fixed(vpp);

	vpp->h_ratio = vpp->v_ratio = 0;
	vpp->fract_val.y_x = vpp->fract_val.y_y = 0;
	vpp->fract_val.c_x = vpp->fract_val.c_y = 0;

	vpp->start_count = 0;
	vpp->done_count = 0;

	vpp->prev_read_order = SYSMMU_PBUFCFG_ASCENDING;

	set_bit(VPP_POWER_ON, &vpp->state);

	return 0;
}

#ifdef CONFIG_PM_DEVFREQ
#define MULTI_FACTOR (1 << 10)
static void vpp_get_bts_scale_factor(struct vpp_dev *vpp, bool is_mif)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	u32 src_w = is_rotation(config) ? src->h : src->w;
	u32 src_h = is_rotation(config) ? src->w : src->h;

	if (is_mif) {
		vpp->sc_w = MULTI_FACTOR * src_w / dst->w;
		vpp->sc_h = MULTI_FACTOR * src_h / dst->h;
	} else {
		vpp->sc_w = (src_w <= dst->w) ?
			MULTI_FACTOR : MULTI_FACTOR * src_w / dst->w;
		vpp->sc_h = (src_h <= dst->h) ?
			MULTI_FACTOR : MULTI_FACTOR * src_h / dst->h;
	}
}

static int vpp_get_min_int_lock(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *dst = &config->dst;
	struct decon_device *decon = get_decon_drvdata(0);
	u32 vclk_mic = (clk_get_rate(decon->res.vclk) / MHZ) * 2;
	u32 lcd_width = decon->lcd_info->xres;
	int ret = 0;

	vpp_get_bts_scale_factor(vpp, false);

	if ((vpp->sc_w == MULTI_FACTOR) && (vpp->sc_h == MULTI_FACTOR)) {
		vpp->cur_int = vclk_mic / 2 * KHZ;
	} else {
		u64 scale_factor = ((u64)vclk_mic * (u64)vpp->sc_w * (u64)vpp->sc_h) / 2;
		u64 dst_factor = ((u64)dst->w * (u64)MULTI_FACTOR) / (u64)lcd_width ;

		vpp->cur_int = (scale_factor * dst_factor * KHZ) /
				(MULTI_FACTOR * MULTI_FACTOR * MULTI_FACTOR);
		if (vpp->cur_int > INT_LV7) {
			dev_err(DEV, "Bandwidth has exceeded 400MHz %s\n", __func__);
			dev_err(DEV, "scale_factor %lld, dst_factor %lld, cur_int %d \n"
					,scale_factor, dst_factor, vpp->cur_int);
			ret = -ERANGE;
		}
	}

	dev_dbg(DEV, "vpp-%d int get: %d\n", vpp->id, vpp->cur_int);

	return ret;
}

static void vpp_set_min_int_lock(struct vpp_dev *vpp, bool enable)
{
	if (enable) {
		pm_qos_update_request(&vpp->vpp_int_qos,
				vpp_get_int_freq(vpp->cur_int));
	} else {
		pm_qos_update_request(&vpp->vpp_int_qos, 0);
		vpp->prev_int = vpp->cur_int = 0;
	}

	dev_dbg(DEV, "vpp-%d int set : %d\n", vpp->id, vpp->cur_int);
}

static void vpp_get_min_mif_lock(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_device *decon = get_decon_drvdata(0);
	u32 vclk_mic = (clk_get_rate(decon->res.vclk) / MHZ) * 2;
	u8 bpl, rot_factor = 0;
	u32 scale_factor = 0;

	rot_factor = is_rotation(config) ? 4 : 1;

	if (is_rgb(config))
		bpl = is_rgb16(config) ? 4 : 8;
	else if (is_yuv422(config))
		bpl = 4;
	else
		bpl = 3;

	vpp_get_bts_scale_factor(vpp, true);

	scale_factor = ((vclk_mic * vpp->sc_w * vpp->sc_h) /
			(MULTI_FACTOR *	MULTI_FACTOR) * bpl) / 2;
	vpp->cur_bw = scale_factor * rot_factor * KHZ;

	dev_dbg(DEV, "vpp-%d bw get: %d\n", vpp->id, vpp->cur_bw);
}

static void vpp_set_min_mif_lock(struct vpp_dev *vpp, bool enable)
{
	struct decon_win_config *config = vpp->config;
	u8 vpp_type;
	enum vpp_bw_type bw_type;

	if (is_rotation(config)){
		if (config->src.w * config->src.h >= FULLHD_SRC)
			bw_type = BW_FULLHD_ROT;
		else
			bw_type = BW_ROT;
	} else {
		bw_type = BW_DEFAULT;
	}

	vpp_type = vpp->id + 2;

	if (enable) {
		exynos7_update_media_scenario(vpp_type, vpp->cur_bw,
				bw_type);
	} else {
		exynos7_update_media_scenario(vpp_type, 0, BW_DEFAULT);
		vpp->prev_bw = vpp->cur_bw = 0;
	}

	dev_dbg(DEV, "vpp-%d bw set: %d\n", vpp->id, vpp->cur_bw);
}
#else
#define vpp_get_min_mif_lock(vpp) do {} while (0)
#define vpp_get_min_int_lock(vpp) do {} while (0)
#define vpp_set_min_mif_lock(vpp, enalbe) do {} while (0)
#define vpp_set_min_int_lock(vpp, enalbe) do {} while (0)
#endif

static int vpp_deinit(struct vpp_dev *vpp, bool do_sw_reset)
{
	unsigned int vpp_irq = 0;

	clear_bit(VPP_POWER_ON, &vpp->state);

	vpp_irq = vpp_hw_get_irq_status(vpp);
	vpp_hw_clear_irq(vpp, vpp_irq);

	vpp_hw_set_framedone_irq(vpp, true);
	vpp_hw_set_deadlock_irq(vpp, true);
	vpp_hw_set_read_slave_err_irq(vpp, true);
	vpp_hw_set_hw_reset_done_mask(vpp, true);
	vpp_hw_set_sfr_update_done_irq(vpp, true);
	if (do_sw_reset)
		vpp_hw_set_sw_reset(vpp);

	vpp_clk_disable(vpp);

	return 0;
}

static bool vpp_check_block_mode(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	u32 b_w = config->block_area.w;
	u32 b_h = config->block_area.h;

	if (config->vpp_parm.rot != VPP_ROT_NORMAL)
		return false;
	if (is_scaling(vpp))
		return false;
	if (!is_rgb(config))
		return false;
	if (b_w < BLK_WIDTH_MIN || b_h < BLK_HEIGHT_MIN)
		return false;

	return true;
}

static int vpp_set_read_order(struct vpp_dev *vpp)
{
	int ret = 0;
	u32 cur_read_order;

	switch (vpp->config->vpp_parm.rot) {
	case VPP_ROT_NORMAL:
	case VPP_ROT_YFLIP:
	case VPP_ROT_90_YFLIP:
	case VPP_ROT_270:
		cur_read_order = SYSMMU_PBUFCFG_ASCENDING;
		break;
	case VPP_ROT_XFLIP:
	case VPP_ROT_180:
	case VPP_ROT_90:
	case VPP_ROT_90_XFLIP:
		cur_read_order = SYSMMU_PBUFCFG_DESCENDING;
		break;
	default:
		BUG();
	}

	if (cur_read_order != vpp->prev_read_order) {
		struct device *dev = &vpp->pdev->dev;
		u32 ipoption[vpp->pbuf_num];
		ipoption[0] = SYSMMU_PBUFCFG_ASCENDING_INPUT;
		ipoption[2] = SYSMMU_PBUFCFG_ASCENDING_INPUT;

		if (cur_read_order == SYSMMU_PBUFCFG_ASCENDING) {
			ipoption[1] = SYSMMU_PBUFCFG_ASCENDING_INPUT;
			ipoption[3] = SYSMMU_PBUFCFG_ASCENDING_INPUT;
		} else {
			ipoption[1] = SYSMMU_PBUFCFG_DESCENDING_INPUT;
			ipoption[3] = SYSMMU_PBUFCFG_DESCENDING_INPUT;
		}

		pm_qos_update_request(&vpp->vpp_mif_qos, MIF_LV1);
		ret = sysmmu_set_prefetch_buffer_property(dev,
			vpp->pbuf_num, 0, ipoption, NULL);
		if (ret)
			dev_err(DEV, "failed set pbuf\n");
	} else {
		pm_qos_update_request(&vpp->vpp_mif_qos, 0);
	}

	vpp->prev_read_order = cur_read_order;

	return ret;
}

static int vpp_set_config(struct vpp_dev *vpp)
{
	int ret = -EINVAL;
	unsigned long flags;

	if (test_bit(VPP_STOPPING, &vpp->state)) {
		dev_warn(DEV, "vpp is ongoing stop(%d)\n", vpp->id);
		return 0;
	}

	if (!test_bit(VPP_RUNNING, &vpp->state)) {
		dev_dbg(DEV, "vpp start(%d)\n", vpp->id);
		ret = pm_runtime_get_sync(DEV);
		if (ret < 0) {
			dev_err(DEV, "Failed runtime_get(), %d\n", ret);
			return ret;
		}
		spin_lock_irqsave(&vpp->slock, flags);
		ret = vpp_init(vpp);
		if (ret < 0) {
			dev_err(DEV, "Failed to initiailze clk\n");
			spin_unlock_irqrestore(&vpp->slock, flags);
			pm_runtime_put_sync(DEV);
			return ret;
		}
		/* The state need to be set here to handle err case */
		set_bit(VPP_RUNNING, &vpp->state);
		spin_unlock_irqrestore(&vpp->slock, flags);
		enable_irq(vpp->irq);
	}

	ret = vpp_hw_set_in_format(vpp);
	if (ret)
		goto err;

	vpp_set_initial_phase(vpp);

	ret = vpp_check_size(vpp);
	if (ret)
		goto err;

	ret = vpp_check_rotation(vpp);
	if (ret)
		goto err;

	DISP_SS_EVENT_LOG(DISP_EVT_VPP_WINCON, vpp->sd, ktime_set(0, 0));
	vpp_hw_set_in_size(vpp);
	vpp_hw_set_out_size(vpp);

	vpp_hw_set_rotation(vpp);
	if (is_vgr(vpp)) {
		ret = vpp_set_read_order(vpp);
		if (ret)
			goto err;
	}
	vpp_hw_set_in_buf_addr(vpp);
	vpp_hw_set_smart_if_pix_num(vpp);

	ret = vpp_set_scale_info(vpp);
	if (ret)
		goto err;

	if (vpp_check_block_mode(vpp))
		vpp_hw_set_in_block_size(vpp, true);
	else
		vpp_hw_set_in_block_size(vpp, false);

	vpp->op_timer.expires = (jiffies + 1 * HZ);
	mod_timer(&vpp->op_timer, vpp->op_timer.expires);

	vpp->start_count++;

	vpp->update_cnt_prev = vpp->update_cnt;
	return 0;
err:
	dev_err(DEV, "failed to set config\n");
	return ret;
}

static long vpp_subdev_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);
	int ret = 0;
	unsigned long flags;
	unsigned long state = (unsigned long)arg;
	bool need_reset;
	BUG_ON(!vpp);

	mutex_lock(&vpp->mlock);
	switch (cmd) {
	case VPP_WIN_CONFIG:
		vpp->config = (struct decon_win_config *)arg;
		ret = vpp_set_config(vpp);
		if (ret)
			dev_err(DEV, "Failed vpp-%d configuration\n",
					vpp->id);
		break;

	case VPP_STOP:
		if (!test_bit(VPP_RUNNING, &vpp->state)) {
			dev_warn(DEV, "vpp-%d is already stopped\n",
					vpp->id);
			goto err;
		}
		set_bit(VPP_STOPPING, &vpp->state);
		if (state != VPP_STOP_ERR) {
			ret = vpp_hw_wait_op_status(vpp);
			if (ret < 0) {
				dev_err(DEV, "%s : vpp-%d is working\n",
						__func__, vpp->id);
				goto err;
			}
		}
		need_reset = (state > 0) ? true : false;
		DISP_SS_EVENT_LOG(DISP_EVT_VPP_STOP, vpp->sd, ktime_set(0, 0));
		clear_bit(VPP_RUNNING, &vpp->state);
		disable_irq(vpp->irq);
		spin_lock_irqsave(&vpp->slock, flags);
		del_timer(&vpp->op_timer);
		vpp_deinit(vpp, need_reset);
		spin_unlock_irqrestore(&vpp->slock, flags);
		vpp_set_min_mif_lock(vpp, 0);
		vpp_set_min_int_lock(vpp, 0);
		pm_qos_update_request(&vpp->vpp_mif_qos, 0);

		pm_runtime_put_sync(DEV);
		dev_dbg(DEV, "vpp stop(%d)\n", vpp->id);
		clear_bit(VPP_STOPPING, &vpp->state);
		break;

	case VPP_GET_BTS_VAL:
		vpp->config = (struct decon_win_config *)arg;
		vpp_get_min_mif_lock(vpp);
		ret = vpp_get_min_int_lock(vpp);
		break;

	case VPP_SET_BW:
		vpp->config = (struct decon_win_config *)arg;
		vpp_set_min_mif_lock(vpp, true);
		break;

	case VPP_SET_MIN_INT:
		vpp->config = (struct decon_win_config *)arg;
		vpp_set_min_int_lock(vpp, true);
		break;

	case VPP_WAIT_FOR_UPDATE:
		vpp_wait_for_update(vpp);
		break;

	case VPP_DUMP:
		vpp_dump_registers(vpp);
		break;

	case VPP_WAIT_IDLE:
		vpp_hw_wait_idle(vpp);
		break;

	default:
		break;
	}

err:
	mutex_unlock(&vpp->mlock);

	return ret;
}

static int vpp_sd_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);

	dev_dbg(DEV, "vpp%d is opened\n", vpp->id);

	return 0;
}

static int vpp_sd_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);

	dev_dbg(DEV, "vpp%d is closed\n", vpp->id);

	return 0;
}

static int vpp_link_setup(struct media_entity *entity,
			const struct media_pad *local,
			const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations vpp_media_ops = {
	.link_setup = vpp_link_setup,
};

static const struct v4l2_subdev_internal_ops vpp_internal_ops = {
	.open = vpp_sd_open,
	.close = vpp_sd_close,
};

static const struct v4l2_subdev_core_ops vpp_subdev_core_ops = {
	.ioctl = vpp_subdev_ioctl,
};

static struct v4l2_subdev_ops vpp_subdev_ops = {
	.core = &vpp_subdev_core_ops,
};

static int check_mdev_for_output(struct device *dev, void *id)
{
	struct platform_device *pdev = to_platform_device(dev);

	if (pdev->id == (enum mdev_node)id)
		return -1;

	return 0;
}

static int vpp_find_media_device(struct vpp_dev *vpp)
{
	struct device_driver *driver;
	struct device *dev;
	struct exynos_md *md;

	driver = driver_find(MDEV_MODULE_NAME, &platform_bus_type);
	if (likely(driver)) {
		dev = driver_find_device(driver, NULL,
			(void *)MDEV_OUTPUT, check_mdev_for_output);
		md = (struct exynos_md *)dev_get_drvdata(dev);
		vpp->mdev = md;
	} else {
		dev_err(DEV, "failed to find output media device\n");
		return -EINVAL;
	}

	return 0;
}

static int vpp_create_subdev(struct vpp_dev *vpp)
{
	struct v4l2_subdev *sd;
	int ret;

	sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!sd)
	       return -ENOMEM;

	v4l2_subdev_init(sd, &vpp_subdev_ops);

	vpp->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", "vpp-sd", vpp->id);
	sd->grp_id = vpp->id;
	ret = media_entity_init(&sd->entity, VPP_PADS_NUM,
				&vpp->pad, 0);
	if (ret) {
		dev_err(DEV, "Failed to initialize VPP media entity");
		goto error;
	}

	sd->entity.ops = &vpp_media_ops;
	sd->internal_ops = &vpp_internal_ops;

	ret = v4l2_device_register_subdev(&vpp->mdev->v4l2_dev, sd);
	if (ret) {
		media_entity_cleanup(&sd->entity);
		goto error;
	}

	vpp->mdev->vpp_sd[vpp->id] = sd;
	vpp->mdev->vpp_dev[vpp->id] = &vpp->pdev->dev;
	dev_info(DEV, "vpp_sd[%d] = %08lx\n", vpp->id,
			(ulong)vpp->mdev->vpp_sd[vpp->id]);

	vpp->sd = sd;
	v4l2_set_subdevdata(sd, vpp);

	return 0;
error:
	kfree(sd);
	return ret;
}

static int vpp_resume(struct device *dev)
{
	return 0;
}

static int vpp_suspend(struct device *dev)
{
	return 0;
}

static irqreturn_t vpp_irq_handler(int irq, void *priv)
{
	struct vpp_dev *vpp = priv;
	int vpp_irq = 0;

	DISP_SS_EVENT_START();
	spin_lock(&vpp->slock);
	if (test_bit(VPP_POWER_ON, &vpp->state)) {
		vpp->aclk_vpp[vpp->clk_cnt] =
			readl(EXYNOS7420_ENABLE_ACLK_VPP);
		vpp->pclk_vpp[vpp->clk_cnt] =
			readl(EXYNOS7420_ENABLE_PCLK_VPP);
		vpp->bus_vpp[vpp->clk_cnt] =
			readl(EXYNOS7420_ENABLE_ACLK_BUS0);
		vpp->clk_cnt++;
		if (vpp->clk_cnt == 10)
			vpp->clk_cnt = 0;

		vpp_irq = vpp_hw_get_irq_status(vpp);
		vpp_hw_clear_irq(vpp, vpp_irq);

		if (is_err_irq(vpp_irq)) {
			dev_err(DEV, "Error interrupt (0x%x)\n", vpp_irq);
			vpp_dump_registers(vpp);
			exynos_sysmmu_show_status(&vpp->pdev->dev);
			goto err;
		}
	}

	if (vpp_irq & VG_IRQ_FRAMEDONE) {
		vpp->done_count++;
		DISP_SS_EVENT_LOG(DISP_EVT_VPP_FRAMEDONE, vpp->sd, start);
	}

	if (vpp_irq & VG_IRQ_SFR_UPDATE_DONE) {
		vpp->update_cnt++;
		wake_up_interruptible_all(&vpp->update_queue);
		DISP_SS_EVENT_LOG(DISP_EVT_VPP_SHADOW_UPDATE, vpp->sd, ktime_set(0, 0));
	}

	dev_dbg(DEV, "irq status : 0x%x\n", vpp_irq);
err:
	del_timer(&vpp->op_timer);
	spin_unlock(&vpp->slock);

	return IRQ_HANDLED;
}
static void vpp_clk_info(struct vpp_dev *vpp)
{
	dev_info(DEV, "%s: %ld Mhz\n", __clk_get_name(vpp->res.gate),
				clk_get_rate(vpp->res.gate) / MHZ);
	dev_info(DEV, "%s: %ld Mhz\n", __clk_get_name(vpp->res.pclk_vpp),
				clk_get_rate(vpp->res.pclk_vpp) / MHZ);
	dev_info(DEV, "%s: %ld Mhz\n", __clk_get_name(vpp->res.lh_vpp),
				clk_get_rate(vpp->res.lh_vpp) / MHZ);
}

static int vpp_clk_get(struct vpp_dev *vpp)
{
	struct device *dev = &vpp->pdev->dev;
	int ret;

	vpp->res.gate = devm_clk_get(dev, "gate");
	if (IS_ERR(vpp->res.gate)) {
		dev_err(dev, "vpp-%d clock get failed\n", vpp->id);
		return PTR_ERR(vpp->res.gate);
	}

	vpp->res.pclk_vpp = devm_clk_get(dev, "pclk_vpp");
	if (IS_ERR(vpp->res.pclk_vpp)) {
		dev_err(dev, "vpp-%d clock pclk_vpp get failed\n", vpp->id);
		return PTR_ERR(vpp->res.pclk_vpp);
	}

	if (is_vpp0_series(vpp)) {
		vpp->res.lh_vpp = devm_clk_get(dev, "aclk_lh_vpp0");
		if (IS_ERR(vpp->res.lh_vpp)) {
			dev_err(dev, "vpp-%d clock lh_vpp get failed\n", vpp->id);
			return PTR_ERR(vpp->res.lh_vpp);
		}
	} else {
		vpp->res.lh_vpp = devm_clk_get(dev, "aclk_lh_vpp1");
		if (IS_ERR(vpp->res.lh_vpp)) {
			dev_err(dev, "vpp-%d clock lh_vpp get failed\n", vpp->id);
			return PTR_ERR(vpp->res.lh_vpp);
		}
	}

	if (is_vpp0_series(vpp)) {
		vpp->res.aclk_vpp_sfw0 = devm_clk_get(dev, "aclk_smmu_vpp_sfw0");
		if(IS_ERR(vpp->res.aclk_vpp_sfw0 )) {
			dev_err(dev, "vpp-%d clock aclk_smmu_vpp_sfw0 get failed\n", vpp->id);
			return PTR_ERR(vpp->res.aclk_vpp_sfw0);
		}
		vpp->res.pclk_vpp_sfw0 = devm_clk_get(dev, "pclk_smmu_vpp_sfw0");
		if(IS_ERR(vpp->res.pclk_vpp_sfw0 )) {
			dev_err(dev, "vpp-%d clock pclk_smmu_vpp_sfw0 get failed\n", vpp->id);
			return PTR_ERR(vpp->res.pclk_vpp_sfw0);
		}
	} else {
		vpp->res.aclk_vpp_sfw1 = devm_clk_get(dev, "aclk_smmu_vpp_sfw1");
		if(IS_ERR(vpp->res.aclk_vpp_sfw1 )) {
			dev_err(dev, "vpp-%d clock aclk_smmu_vpp_sfw1 get failed\n", vpp->id);
			return PTR_ERR(vpp->res.aclk_vpp_sfw1);
		}
		vpp->res.pclk_vpp_sfw1 = devm_clk_get(dev, "pclk_smmu_vpp_sfw1");
		if(IS_ERR(vpp->res.pclk_vpp_sfw1 )) {
			dev_err(dev, "vpp-%d clock pclk_smmu_vpp_sfw1 get failed\n", vpp->id);
			return PTR_ERR(vpp->res.pclk_vpp_sfw1);
		}
	}

	decon_clk_set_rate(dev, "d_pclk_vpp", 134 * MHZ);

	ret = clk_prepare(vpp->res.gate);
	if (ret < 0) {
		dev_err(dev, "vpp-%d clock prepare failed\n", vpp->id);
		return ret;
	}

	ret = clk_prepare(vpp->res.pclk_vpp);
	if (ret < 0) {
		dev_err(dev, "vpp-%d pclk_vpp clock prepare failed\n", vpp->id);
		goto err_0;
	}

	ret = clk_prepare(vpp->res.lh_vpp);
	if (ret < 0) {
		dev_err(dev, "vpp-%d lh_vpp clock prepare failed\n", vpp->id);
		goto err_1;
	}

	if (is_vpp0_series(vpp)) {
		ret = clk_prepare(vpp->res.aclk_vpp_sfw0);
		if(ret < 0) {
			dev_err(dev, "vpp-%d aclk_smmu_vpp_sfw0 clock prepare failed\n", vpp->id);
			goto err_2;
		}
		ret = clk_prepare(vpp->res.pclk_vpp_sfw0);
		if(ret < 0) {
			dev_err(dev, "vpp-%d pclk_smmu_vpp_sfw0 clock prepare failed\n", vpp->id);
			goto err_3;
		}
	} else {
		ret = clk_prepare(vpp->res.aclk_vpp_sfw1);
		if(ret < 0) {
			dev_err(dev, "vpp-%d aclk_smmu_vpp_sfw1 clock prepare failed\n", vpp->id);
			goto err_2;
		}
		ret = clk_prepare(vpp->res.pclk_vpp_sfw1);
		if(ret < 0) {
			dev_err(dev, "vpp-%d pclk_smmu_vpp_sfw1 clock prepare failed\n", vpp->id);
			goto err_3;
		}
	}
	return 0;
err_3:
	if (is_vpp0_series(vpp))
		clk_unprepare(vpp->res.aclk_vpp_sfw0);
	else
		clk_unprepare(vpp->res.aclk_vpp_sfw1);
err_2:
	clk_unprepare(vpp->res.lh_vpp);
err_1:
	clk_unprepare(vpp->res.pclk_vpp);
err_0:
	clk_unprepare(vpp->res.gate);

	return ret;
}

static void vpp_clk_put(struct vpp_dev *vpp)
{
	clk_unprepare(vpp->res.gate);
	clk_put(vpp->res.gate);

	clk_put(vpp->res.d_pclk_vpp);

	clk_unprepare(vpp->res.pclk_vpp);
	clk_put(vpp->res.pclk_vpp);

	clk_unprepare(vpp->res.lh_vpp);
	clk_put(vpp->res.lh_vpp);
}

static int vpp_sysmmu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token)
{
	struct vpp_dev *vpp = dev_get_drvdata(dev);

	if (test_bit(VPP_POWER_ON, &vpp->state)) {
		dev_info(DEV, "vpp%d sysmmu fault handler\n", vpp->id);
		vpp_dump_registers(vpp);
	}

	return 0;
}

static int vpp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vpp_dev *vpp;
	struct resource *res;
	int irq;
	int ret = 0;

	vpp = devm_kzalloc(dev, sizeof(*vpp), GFP_KERNEL);
	if (!vpp) {
		dev_err(dev, "Failed to allocate local vpp mem\n");
		return -ENOMEM;
	}
	vpp_for_decon = vpp;
	vpp->id = of_alias_get_id(dev->of_node, "vpp");

	ret = of_property_read_u32(dev->of_node, "#pb-id-cells",
			&vpp->pbuf_num);
	if (ret) {
		dev_err(DEV, "failed to get PB info\n");
		return ret;
	}
	vpp->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	vpp->regs = devm_request_and_ioremap(dev, res);
	if (!vpp->regs) {
		dev_err(DEV, "Failed to map registers\n");
		ret = -EADDRNOTAVAIL;
		return ret;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(DEV, "Failed to get IRQ resource\n");
		return irq;
	}

	ret = devm_request_irq(dev, irq, vpp_irq_handler,
				0, pdev->name, vpp);
	if (ret) {
		dev_err(DEV, "Failed to install irq\n");
		return ret;
	}

	vpp->irq = irq;
	disable_irq(vpp->irq);

	ret = vpp_find_media_device(vpp);
	if (ret) {
		dev_err(DEV, "Failed find media device\n");
		return ret;
	}

	ret = vpp_create_subdev(vpp);
	if (ret) {
		dev_err(DEV, "Failed create sub-device\n");
		return ret;
	}

	ret = vpp_clk_get(vpp);
	if (ret) {
		dev_err(DEV, "Failed to get clk\n");
		return ret;
	}

	vpp_clk_info(vpp);

	init_waitqueue_head(&vpp->stop_queue);
	init_waitqueue_head(&vpp->update_queue);

	platform_set_drvdata(pdev, vpp);
	pm_runtime_enable(dev);

	ret = iovmm_activate(dev);
	if (ret < 0) {
		dev_err(DEV, "failed to reactivate vmm\n");
		return ret;
	}

	setup_timer(&vpp->op_timer, vpp_op_timer_handler,
			(unsigned long)vpp);

	if (IS_ENABLED(CONFIG_PM_DEVFREQ)) {
		pm_qos_add_request(&vpp->vpp_int_qos,
			PM_QOS_DEVICE_THROUGHPUT, 0);
		pm_qos_add_request(&vpp->vpp_mif_qos,
			PM_QOS_BUS_THROUGHPUT, 0);
	}

	mutex_init(&vpp->mlock);
	spin_lock_init(&vpp->slock);
	vpp->clk_cnt = 0;

	iovmm_set_fault_handler(dev, vpp_sysmmu_fault_handler, NULL);

	dev_info(DEV, "VPP%d is probed successfully\n", vpp->id);

	return 0;
}

static int vpp_remove(struct platform_device *pdev)
{
	struct vpp_dev *vpp =
		(struct vpp_dev *)platform_get_drvdata(pdev);

	iovmm_deactivate(&vpp->pdev->dev);
	vpp_clk_put(vpp);

	if (IS_ENABLED(CONFIG_PM_DEVFREQ)) {
		pm_qos_remove_request(&vpp->vpp_int_qos);
		pm_qos_remove_request(&vpp->vpp_mif_qos);
	}

	dev_info(DEV, "%s driver unloaded\n", pdev->name);

	return 0;
}

static const struct of_device_id vpp_device_table[] = {
	{
		.compatible = "samsung,exynos7-vpp",
	},
	{},
};

static const struct dev_pm_ops vpp_pm_ops = {
	.suspend		= vpp_suspend,
	.resume			= vpp_resume,
};

static struct platform_driver vpp_driver __refdata = {
	.probe		= vpp_probe,
	.remove		= vpp_remove,
	.driver = {
		.name	= "exynos-vpp",
		.owner	= THIS_MODULE,
		.pm	= &vpp_pm_ops,
		.of_match_table = of_match_ptr(vpp_device_table),
	}
};

static int vpp_register(void)
{
	return platform_driver_register(&vpp_driver);
}

module_init(vpp_register);

MODULE_AUTHOR("Sungchun, Kang <sungchun.kang@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS Soc VPP driver");
MODULE_LICENSE("GPL");
