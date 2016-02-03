/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.

 * Alternatively, this program is free software in case of open source projec;
 * you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 */

#ifndef ___SAMSUNG_DECON_COMMON_H__
#define ___SAMSUNG_DECON_COMMON_H__

#include "./panels/decon_lcd.h"

#define MAX_VPP_SUBDEV		4

enum decon_dsi_mode {
	DSI_MODE_SINGLE = 0,
	DSI_MODE_DUAL,
	DSI_MODE_DUAL_DISPLAY
};

enum decon_trig_mode {
	DECON_HW_TRIG = 0,
	DECON_SW_TRIG
};

enum decon_hold_scheme {
	DECON_VCLK_HOLD = 0x00,
	DECON_VCLK_RUNNING = 0x01,
	DECON_VCLK_RUN_VDEN_DISABLE = 0x3,
};

enum decon_rgb_order {
	DECON_RGB = 0x0,
	DECON_GBR = 0x1,
	DECON_BRG = 0x2,
	DECON_BGR = 0x4,
	DECON_RBG = 0x5,
	DECON_GRB = 0x6,
};

enum decon_set_trig {
	DECON_TRIG_DISABLE = 0,
	DECON_TRIG_ENABLE
};

enum decon_idma_type {
	IDMA_G0 = 0x0,
	IDMA_G1,
	IDMA_VG0,
	IDMA_VG1,
	IDMA_VGR0,
	IDMA_VGR1,
	IDMA_G2,
	IDMA_G3,
};

enum decon_output_type {
	DECON_OUT_DSI = 0,
	DECON_OUT_HDMI,
	DECON_OUT_WB,
	DECON_OUT_TUI
};

struct decon_psr_info {
	enum decon_psr_mode psr_mode;
	enum decon_trig_mode trig_mode;
	enum decon_output_type out_type;
};

struct decon_init_param {
	struct decon_psr_info psr;
	struct decon_lcd *lcd_info;
	u32 nr_windows;
};

struct decon_regs_data {
	u32 wincon;
	u32 winmap;
	u32 vidosd_a;
	u32 vidosd_b;
	u32 vidosd_c;
	u32 vidosd_d;
	u32 vidosd_e;
	u32 vidw_buf_start;
	u32 vidw_whole_w;
	u32 vidw_whole_h;
	u32 vidw_offset_x;
	u32 vidw_offset_y;
	u32 blendeq;

	enum decon_idma_type type;
};

/* CAL APIs list */
void decon_reg_init(u32 id, enum decon_dsi_mode dsi_mode, struct decon_init_param *p);
void decon_reg_init_probe(u32 id, enum decon_dsi_mode dsi_mode, struct decon_init_param *p);
void decon_reg_start(u32 id, enum decon_dsi_mode dsi_mode, struct decon_psr_info *psr);
int decon_reg_stop(u32 id, enum decon_dsi_mode dsi_mode, struct decon_psr_info *psr);
void decon_reg_set_regs_data(u32 id, int win_idx, struct decon_regs_data *regs);
void decon_reg_set_int(u32 id, struct decon_psr_info *psr, enum decon_dsi_mode dsi_mode, u32 en);
void decon_reg_set_trigger(u32 id, enum decon_dsi_mode dsi_mode,
			enum decon_trig_mode trig, enum decon_set_trig en);
int decon_reg_wait_for_update_timeout(u32 id, unsigned long timeout);
void decon_reg_shadow_protect_win(u32 id, u32 win_idx, u32 protect);
void decon_reg_activate_window(u32 id, u32 index);

/* CAL raw functions list */
int decon_reg_reset(u32 id);
void decon_reg_set_default_win_channel(u32 id);
void decon_reg_set_clkgate_mode(u32 id, u32 en);
void decon_reg_blend_alpha_bits(u32 id, u32 alpha_bits);
void decon_reg_set_vidout(u32 id, struct decon_psr_info *psr, enum decon_dsi_mode dsi_mode, u32 en);
void decon_reg_set_crc(u32 id, u32 en);
void decon_reg_set_fixvclk(u32 id, int dsi_idx, enum decon_hold_scheme mode);
void decon_reg_clear_win(u32 id, int win_idx);
void decon_reg_set_rgb_order(u32 id, int dsi_idx, enum decon_rgb_order order);
void decon_reg_set_porch(u32 id, int dsi_idx, struct decon_lcd *info);
void decon_reg_set_linecnt_op_threshold(u32 id, int dsi_idx, u32 th);
void decon_reg_set_clkval(u32 id, u32 clkdiv);
void decon_reg_direct_on_off(u32 id, u32 en);
void decon_reg_per_frame_off(u32 id);
void decon_reg_set_freerun_mode(u32 id, u32 en);
void decon_reg_update_standalone(u32 id);
void decon_reg_set_wb_frame(u32 id, u32 width, u32 height, dma_addr_t addr);
void decon_reg_wb_swtrigger(u32 id);
void decon_reg_configure_lcd(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info);
void decon_reg_configure_trigger(u32 id, enum decon_trig_mode mode);
void decon_reg_set_winmap(u32 id, u32 idx, u32 color, u32 en);
u32 decon_reg_get_linecnt(u32 id, int dsi_idx);
u32 decon_reg_get_vstatus(u32 id, int dsi_idx);
int decon_reg_wait_linecnt_is_zero_timeout(u32 id, int dsi_idx, unsigned long timeout);
u32 decon_reg_get_stop_status(u32 id);
int decon_reg_wait_stop_status_timeout(u32 id, unsigned long timeout);
int decon_reg_is_win_enabled(u32 id, int win_idx);
int decon_reg_is_shadow_updated(u32 id);
void decon_reg_config_mic(u32 id, int dsi_idx, struct decon_lcd *lcd_info);
void decon_reg_clear_int(u32 id);
void decon_reg_config_win_channel(u32 id, u32 win_idx, enum decon_idma_type type);
u32 decon_reg_get_lineval(u32 id, int dsi_idx, struct decon_lcd *lcd_info);
u32 decon_reg_get_hozval(u32 id, int dsi_idx, struct decon_lcd *lcd_info);
void decon_exit_lpd_state(void);
void update_frame_for_ad(void);
#if defined(CONFIG_EXYNOS_DECON_DPU)
void decon_reg_enable_apb_clk(u32 id, u32 en);
void decon_reg_set_pixel_count_se(u32 id, u32 width, u32 height);
void decon_reg_set_image_size_se(u32 id, u32 width, u32 height);
void decon_reg_set_porch_se(u32 id, u32 vfp, u32 vsa, u32 vbp, u32 hfp, u32 hsa, u32 hbp);
void decon_reg_set_bit_order_se(u32 id, u32 out_order, u32 in_order);
void decon_reg_enable_dpu(u32 id, u32 en);
#endif

#endif /* ___SAMSUNG_DECON_COMMON_H__ */
