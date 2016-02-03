/*
 * exynos-mipi-lli-mphy.h - Exynos MIPI-LLI MPHY Header
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __DRIVERS_EXYNOS_MIPI_LLI_MPHY_H
#define __DRIVERS_EXYNOS_MIPI_LLI_MPHY_H

/* Phy Attributes */
#define PHY_TX_HSMODE_CAP(LANE)				(1024*LANE + 0x01*4)
#define PHY_TX_HSGEAR_CAP(LANE)				(1024*LANE + 0x02*4)
#define PHY_TX_PWMG0_CAP(LANE)				(1024*LANE + 0x03*4)
#define PHY_TX_PWMGEAR_CAP(LANE)			(1024*LANE + 0x04*4)
#define PHY_TX_AMP_CAP(LANE)				(1024*LANE + 0x05*4)
#define PHY_TX_EXTSYNC_CAP(LANE)			(1024*LANE + 0x06*4)
#define PHY_TX_HS_UNT_LINE_DRV_CAP(LANE)		(1024*LANE + 0x07*4)
#define PHY_TX_LS_TERM_LINE_DRV_CAP(LANE)		(1024*LANE + 0x08*4)
#define PHY_TX_MIN_SLEEP_NOCFG_TIME_CAP(LANE)		(1024*LANE + 0x09*4)
#define PHY_TX_MIN_STALL_NOCFG_TIME_CAP(LANE)		(1024*LANE + 0x0a*4)
#define PHY_TX_MIN_SAVE_CONFIG_TIME_CAP(LANE)		(1024*LANE + 0x0b*4)
#define PHY_TX_REF_CLK_SHARED_CAP(LANE)			(1024*LANE + 0x0c*4)
#define PHY_TX_PHY_MAJORMINOR_RELEASE_CAP(LANE)		(1024*LANE + 0x0d*4)
#define PHY_TX_PHY_EDITORIAL_RELEASE_CAP(LANE)		(1024*LANE + 0x0e*4)
#define PHY_TX_MODE(LANE)				(1024*LANE + 0x21*4)
#define PHY_TX_HSRATE_SERIES(LANE)			(1024*LANE + 0x22*4)
#define PHY_TX_HSGEAR(LANE)				(1024*LANE + 0x23*4)
#define PHY_TX_PWMGEAR(LANE)				(1024*LANE + 0x24*4)
#define PHY_TX_AMPLITUDE(LANE)				(1024*LANE + 0x25*4)
#define PHY_TX_HS_SLEWRATE(LANE)			(1024*LANE + 0x26*4)
#define PHY_TX_SYNC_SOURCE(LANE)			(1024*LANE + 0x27*4)
#define PHY_TX_HS_SYNC_LENGTH(LANE)			(1024*LANE + 0x28*4)
#define PHY_TX_HS_PREPARE_LENGTH(LANE)			(1024*LANE + 0x29*4)
#define PHY_TX_LS_PREPARE_LENGTH(LANE)			(1024*LANE + 0x2a*4)
#define PHY_TX_HIBERN8_CONTROL(LANE)			(1024*LANE + 0x2b*4)
#define PHY_TX_LCC_ENABLE(LANE)				(1024*LANE + 0x2C*4)
#define PHY_TX_PWM_BURST_CLOSURE_EXT(LANE)		(1024*LANE + 0x2D*4)
#define PHY_TX_BYPASS_8B10B_ENABLE(LANE)		(1024*LANE + 0x2E*4)
#define PHY_TX_DRIVER_POLARITY(LANE)			(1024*LANE + 0x2F*4)
#define PHY_TX_HS_UNT_LINE_DRV_ENABLE(LANE)		(1024*LANE + 0x30*4)
#define PHY_TX_LS_TERM_LINE_DRV_ENABLE(LANE)		(1024*LANE + 0x31*4)
#define PHY_TX_LCC_SEQUENCER(LANE)			(1024*LANE + 0x32*4)
#define PHY_TX_MIN_ACTIVATETIME(LANE)			(1024*LANE + 0x33*4)
#define PHY_TX_FSM_STATE(LANE)				(1024*LANE + 0x41*4)
#define PHY_RX_HSMODE_CAP(LANE)				(1024*LANE + 0x81*4)
#define PHY_RX_HSGEAR_CAP(LANE)				(1024*LANE + 0x82*4)
#define PHY_RX_PWMG0_CAP(LANE)				(1024*LANE + 0x83*4)
#define PHY_RX_PWMGEAR_CAP(LANE)			(1024*LANE + 0x84*4)
#define PHY_RX_HS_UNT_CAP(LANE)				(1024*LANE + 0x85*4)
#define PHY_RX_LS_TERM_CAP(LANE)			(1024*LANE + 0x86*4)
#define PHY_RX_MIN_SLEEP_NOCFG_TIME_CAP(LANE)		(1024*LANE + 0x87*4)
#define PHY_RX_MIN_STALL_NOCFG_TIME_CAP(LANE)		(1024*LANE + 0x88*4)
#define PHY_RX_MIN_SAVE_CONFIG_TIME_CAP(LANE)		(1024*LANE + 0x89*4)
#define PHY_RX_REF_CLK_SHARED_CAP(LANE)			(1024*LANE + 0x8A*4)
#define PHY_RX_HS_SYNC_LENGTH(LANE)			(1024*LANE + 0x8B*4)
#define PHY_RX_HS_PREPARE_LENGTH_CAP(LANE)		(1024*LANE + 0x8C*4)
#define PHY_RX_LS_PREPARE_LENGTH_CAP(LANE)		(1024*LANE + 0x8D*4)
#define PHY_RX_PWM_BURST_CLOSURE_LENGTH_CAP(LANE)	(1024*LANE + 0x8E*4)
#define PHY_RX_MIN_ACTIVATETIME(LANE)			(1024*LANE + 0x8F*4)
#define PHY_RX_PHY_MAJORMINOR_RELEASE_CAP(LANE)		(1024*LANE + 0x90*4)
#define PHY_RX_PHY_EDITORIAL_RELEASE_CAP(LANE)		(1024*LANE + 0x91*4)
#define PHY_RX_MODE(LANE)				(1024*LANE + 0xA1*4)
#define PHY_RX_HSRATE_SERIES(LANE)			(1024*LANE + 0xA2*4)
#define PHY_RX_HSGEAR(LANE)				(1024*LANE + 0xA3*4)
#define PHY_RX_PWMGEAR(LANE)				(1024*LANE + 0xA4*4)
#define PHY_RX_LS_TERM_ENABLE(LANE)			(1024*LANE + 0xA5*4)
#define PHY_RX_HS_UNT_ENABLE(LANE)			(1024*LANE + 0xA6*4)
#define PHY_RX_ENTER_HIBERN8(LANE)			(1024*LANE + 0xA7*4)
#define PHY_RX_BYPASS_8B10B_ENABLE(LANE)		(1024*LANE + 0xA8*4)
#define PHY_RX_FSM_STATE(LANE)				(1024*LANE + 0xC1*4)
#define PHY_RX_FILLER_INSERTION_ENABLE(LANE)		(1024*LANE + 0x16*4)

#define PHY_TRSV_POWER_DOWN_LOW                         (0x0138)
#define PHY_TRSV_POWER_DOWN_HIGH                        (0x013C)
#define PHY_CMN_POWER_DOWN                              (0x0054)

static const int phy_std_debug_info[] = {
	PHY_TX_MODE(0),
	PHY_TX_HSRATE_SERIES(0),
	PHY_TX_HSGEAR(0),
	PHY_TX_PWMGEAR(0),
	PHY_TX_HIBERN8_CONTROL(0),
	PHY_TX_LCC_ENABLE(0),
	PHY_TX_BYPASS_8B10B_ENABLE(0),
	PHY_TX_LCC_SEQUENCER(0),
	PHY_TX_FSM_STATE(0),
	PHY_RX_MODE(0),
	PHY_RX_HSRATE_SERIES(0),
	PHY_RX_HSGEAR(0),
	PHY_RX_PWMGEAR(0),
	PHY_RX_ENTER_HIBERN8(0),
	PHY_RX_BYPASS_8B10B_ENABLE(0),
	PHY_RX_FSM_STATE(0),
};

static const int phy_cmn_debug_info[] = {
	0x00*4, /* [7] r_read_l_lane
		   [6] r_cfg_ov_tm
		   [5:3] r_ln_num
		   [2:0] r_txrx_lane */
	0x06*4, /* [6:4] r_freq_mode[2:0]
		   [3:0] o_pdiv [3:0] */
	0x07*4, /* [7:0] r_mdiv [7:0] */
	0x08*4, /* [4] r_mdiv_ctrl_by_i2c
		   [3:0] o_abt_sel[11:8] */
	0x0A*4, /* [3:2] o_irext_sel[1:0]
		   [1:0] o_irpoly_sel[1:0] */
	0x10*4, /* [7] r_vco_band_reuse
		   [6] r_cmn_afc_state_en
		   [5] r_afc_state_en
		   [4] r_ov_en_afc
		   [3:0] o_force_vco_band */
	0x11*4, /* [2:0] o_kvco_tune[2:0] */
	0x12*4, /* [1:0] o_ireg_ctrl[1:0](regulator_current) */
	0x13*4, /* [1:0] o_icpi_ctrl[1:0] */
	0x14*4, /* [2:0] o_icpp_ctrl[2:0] */
	0x16*4, /* [2:0] o_lpf_rsel[2:0] */
	0x17*4, /* [0] o_pd_bgr_clk  00(on)*4, 01(off) */
	0x19*4, /* [7:4] o_vci_ctrl[3:0]
		   [3:0] o_lpf_cpsel[3:0] */
	0x1A*4,
	0x1C*4, /* [3:1] o_vcm_boost_en[2:0] : 000 -> 101
		   [0] o_vcm_short_en 1->0 */
	0x21*4, /* [6] r_cmn_block_en */
	0x26*4, /* Lock Read */
	0x31*4, /* [4:0] r_pd_mask_reg [4:0] */
	0x44*4, /* [5] o_afc_clk_div_en 1'b0
		   [4] o_afc_vforce 1'b0
		   [3:0] o_afc_code_start 4'b0111 -> 4'b0000 */
	0x4E*4, /* [1] pd_refclk_out */
	0x4D*4, /* refclk_out strength */
};

static const int phy_ovtm_debug_info[] = {
	0x75*4, /* [5:0]r_tx_amplitude_ctrl */
	0x76*4,
	0x78*4, /* [5] o_use_internal_clk
		   [4] o_line_lb_sel_1
		   [3] o_line_lb_sel_0
		   [2] o_ser_lb_en_pwm
		   [1] o_line_lb_en
		   [0] o_ser_lb_en */
	0x79*4, /* [3] o_protocol_lb
		   [2:0] o_lb_pattern */
	0x84*4, /* [0] o_pin_rsv_tx[8] */
	0x85*4, /* [7:4] up    4'b0000 -> 4'b1000
		   [3:0] down  4'b0000 -> 4'b1000 */
	0x86*4, /* [7]: Enable RES_TUNE[1:0],
		   [3:2] : HS_sync option default on
		   [1:0] : RES_TUNE[1:0] */
	0x0A*4, /* [7:5] o_lc_sw[2:0]
		   [4] o_pulse_rej_en
		   [3] o_ser_lb_sel_0
		   [2] o_sel_lb_sel_0_pwm
		   [1] o_line_lb_en
		   [0] o_ser_lb_en */
	0x0B*4, /* [3] o_protocol_lb
		   [2:0] o_lb_pattern */
	0x16*4, /* [5] o_ov_align_ctl_en
		   [4:2] o_ov_align_ctl_cnt[2:0]
		   [1] o_rx_filler_en
		   [0] o_rx_align */
	0x1A*4, /* [7] r_rcal_inv_code */
	0x29*4, /* [4] r_ov_pwm_burst_end_wa2
		   [3] r_ov_pwm_gear_minus_1
		   [2] r_ov_pwm_gear_plus_1
		   [1] r_ov_pwm_closure_sel
		   [0] o_pwm_align_en */
	0x2E*4, /* [7:6] o_pwm_cur_ctrl[1:0]
		   [5] o_hs_data_inv
		   [4] o_hs_data_pol
		   [3] o_pwm_data_inv
		   [2] o_pwm_data_pol
		   [1] o_sys_data_inv
		   [0] o_sys_data_pol */
	0x2F*4, /* [7:6] o_pwm_clk_dly2[1:0]
		   [5:0] o_pwm_clk_dly1[5] [4:3] [2] [1:0] */
	0x30*4, /* [7:4] o_pwm_clk_dly2[5:2]
		   [3] o_hs_pol
		   [2:0] o_sq_time_ctrl[2:0] */
};

enum phy_sfr_type {
	LOCAL_SFR, REMOTE_SFR,
};

enum phy_mode_type {
	GEAR_1 = 0x1,
	GEAR_2 = 0x2,
	GEAR_3 = 0x3,
	GEAR_4 = 0x4,
	GEAR_5 = 0x5,
	GEAR_6 = 0x6,
	GEAR_7 = 0x7,
	OPMODE_PWM = 0x10,
	OPMODE_HS = 0x20,
	HS_RATE_A = 0x100,
	HS_RATE_B = 0x200,
};

struct exynos_mphy;

struct exynos_mphy_driver {
	int (*init)(struct exynos_mphy *phy);
	int (*cmn_init)(struct exynos_mphy *phy);
	int (*ovtm_init)(struct exynos_mphy *phy);
	int (*pma_init)(struct exynos_mphy *phy);
	int (*shutdown)(struct exynos_mphy *phy);
};

struct exynos_mphy {
	struct device	*dev;
	struct clk	*clk;
	void __iomem	*loc_regs;
	void __iomem	*rem_regs;
	void __iomem	*pma_regs;
	u8		lane;
	spinlock_t	lock;

	enum phy_mode_type default_mode;
	bool is_shared_clk;
	int afc_val;

	struct exynos_mphy_driver *driver;
};

struct device *exynos_get_mphy(void);
int exynos_mphy_init(struct exynos_mphy *phy);
int exynos_mphy_init2(struct exynos_mphy *phy);
int exynos_mphy_shared_clock(struct exynos_mphy *phy);
int exynos_mphy_block_powerdown(struct device *lli_mphy, int enable);

#endif /* __DRIVERS_EXYNOS_MIPI_LLI_MPHY_H */
