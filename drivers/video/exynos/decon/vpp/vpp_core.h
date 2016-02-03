/* linux/drivers/video/exynos/decon/vpp_core.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * header file for Samsung EXYNOS5 SoC series VPP driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef VPP_CORE_H_
#define VPP_CORE_H_

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/exynos_iovmm.h>
#include <video/videonode.h>
#include <mach/bts.h>
#include <media/exynos_mc.h>
#include <mach/regs-clock-exynos7420.h>

#include "regs-vpp.h"
#include "../decon.h"

#define VPP_PADS_NUM	1
#define DEV		(&vpp->pdev->dev)

#define is_vgr(vpp) ((vpp->id == 2) || (vpp->id == 3))
#define is_rotation(config) (config->vpp_parm.rot >= VPP_ROT_90)
#define is_yuv(config) ((config->format >= DECON_PIXEL_FORMAT_NV16) && (config->format < DECON_PIXEL_FORMAT_MAX))
#define is_yuv422(config) ((config->format >= DECON_PIXEL_FORMAT_NV16) && (config->format <= DECON_PIXEL_FORMAT_YVU422_3P))
#define is_yuv420(config) ((config->format >= DECON_PIXEL_FORMAT_NV12) && (config->format <= DECON_PIXEL_FORMAT_YVU420M))
#define is_rgb(config) ((config->format >= DECON_PIXEL_FORMAT_ARGB_8888) && (config->format <= DECON_PIXEL_FORMAT_RGB_565))
#define is_rgb16(config) ((config->format == DECON_PIXEL_FORMAT_RGB_565))
#define is_ayv12(config) (config->format == DECON_PIXEL_FORMAT_YVU420)
#define is_fraction(x) ((x) >> 15)
#define is_vpp0_series(vpp) ((vpp->id == 0 || vpp->id == 2))
#define is_scaling(vpp) ((vpp->h_ratio != (1 << 20)) || (vpp->v_ratio != (1 << 20)))
#define is_scale_down(vpp) ((vpp->h_ratio > (1 << 20)) || (vpp->v_ratio > (1 << 20)))

enum vpp_dev_state {
	VPP_OPENED,
	VPP_RUNNING,
	VPP_POWER_ON,
	VPP_STOPPING,
};

struct vpp_resources {
	struct clk *gate;
	struct clk *lh_vpp;
	struct clk *d_pclk_vpp;
	struct clk *pclk_vpp;
	struct clk *aclk_vpp_sfw0;
	struct clk *aclk_vpp_sfw1;
	struct clk *pclk_vpp_sfw0;
	struct clk *pclk_vpp_sfw1;
};

struct vpp_fraction {
	u32 y_x;
	u32 y_y;
	u32 c_x;
	u32 c_y;
	u32 w;
	u32 h;
};

struct vpp_minlock_entry {
	bool rotation;
	u32 scalefactor;
	u32 dvfsfreq;
};

struct vpp_minlock_table {
	struct list_head node;
	u16 width;
	u16 height;
	int num_entries;
	struct vpp_minlock_entry entries[0];
};

struct vpp_dev {
	int				id;
	struct platform_device		*pdev;
	spinlock_t			slock;
	struct media_pad		pad;
	struct exynos_md		*mdev;
	struct v4l2_subdev		*sd;
	void __iomem			*regs;
	unsigned long			state;
	struct clk			*gate_clk;
	struct vpp_resources		res;
	wait_queue_head_t		stop_queue;
	wait_queue_head_t		update_queue;
	struct timer_list		op_timer;
	u32				start_count;
	u32				done_count;
	u32				update_cnt;
	u32				update_cnt_prev;
	struct decon_win_config	*config;
	struct pm_qos_request		vpp_int_qos;
	struct pm_qos_request		vpp_mif_qos;
	u32				h_ratio;
	u32				v_ratio;
	struct vpp_fraction		fract_val;
	struct mutex			mlock;
	u32				clk_cnt;
	u32				aclk_vpp[10];
	u32				pclk_vpp[10];
	u32				bus_vpp[10];
	unsigned int			irq;
	u32				prev_read_order;
	u32				pbuf_num;
	u32				cur_bw;
	u32				prev_bw;
	u32				cur_int;
	u32				prev_int;
	u32				sc_w;
	u32				sc_h;
};

static inline int vpp_hw_get_irq_status(struct vpp_dev *vpp)
{
	u32 cfg = readl(vpp->regs + VG_IRQ);
	cfg &= (VG_IRQ_SFR_UPDATE_DONE | VG_IRQ_HW_RESET_DONE |
		VG_IRQ_READ_SLAVE_ERROR | VG_IRQ_DEADLOCK_STATUS |
		VG_IRQ_FRAMEDONE);

	return cfg;
}

static inline void vpp_hw_clear_irq(struct vpp_dev *vpp, int irq)
{
	u32 cfg = readl(vpp->regs + VG_IRQ);
	cfg |= irq;
	writel(cfg, vpp->regs + VG_IRQ);
}

int vpp_hw_set_sw_reset(struct vpp_dev *vpp);
void vpp_hw_set_realtime_path(struct vpp_dev *vpp);
void vpp_hw_set_framedone_irq(struct vpp_dev *vpp, bool enable);
void vpp_hw_set_deadlock_irq(struct vpp_dev *vpp, bool enable);
void vpp_hw_set_read_slave_err_irq(struct vpp_dev *vpp, bool enable);
void vpp_hw_set_sfr_update_done_irq(struct vpp_dev *vpp, bool enable);
void vpp_hw_set_hw_reset_done_mask(struct vpp_dev *vpp, bool enable);
void vpp_hw_set_in_size(struct vpp_dev *vpp);
void vpp_hw_set_out_size(struct vpp_dev *vpp);
void vpp_hw_set_scale_ratio(struct vpp_dev *vpp);
int vpp_hw_set_in_format(struct vpp_dev *vpp);
void vpp_hw_set_in_block_size(struct vpp_dev *vpp, bool enable);
void vpp_hw_set_in_buf_addr(struct vpp_dev *vpp);
void vpp_hw_set_smart_if_pix_num(struct vpp_dev *vpp);
void vpp_hw_set_lookup_table(struct vpp_dev *vpp);
void vpp_hw_set_enable_interrupt(struct vpp_dev *vpp);
void vpp_hw_set_sfr_update_force(struct vpp_dev *vpp);
int vpp_hw_wait_op_status(struct vpp_dev *vpp);
int vpp_hw_set_rotation(struct vpp_dev *vpp);
void vpp_hw_set_rgb_type(struct vpp_dev *vpp);
void vpp_hw_set_plane_alpha(struct vpp_dev *vpp);
void vpp_hw_set_dynamic_clock_gating(struct vpp_dev *vpp);
void vpp_hw_set_plane_alpha_fixed(struct vpp_dev *vpp);
void vpp_hw_wait_idle(struct vpp_dev *vpp);
#endif
