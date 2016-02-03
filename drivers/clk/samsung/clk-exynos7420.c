/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file is for exynos7420 clocks.
 */

#include <linux/of.h>
#include <linux/of_address.h>

#include <dt-bindings/clock/exynos7420.h>

#include <mach/regs-clock-exynos7420.h>
#include <mach/regs-pmu-exynos7420.h>
#include "composite.h"

/* please define clocks only used in device tree */
enum exynos7420_clks {
	none,

	oscclk = 1,
	g3d_pll = 10, disp_pll,
	mout_isp_pll,
	sclk_isp_sensor0 = 16, sclk_isp_sensor1, sclk_isp_sensor2,
	dout_sclk_isp_sensor0 = 20, dout_sclk_isp_sensor1, dout_sclk_isp_sensor2,
	mout_sclk_isp_sensor0, mout_sclk_isp_sensor1, mout_sclk_isp_sensor2,
	mout_isp_pll_ctrl, mout_cam_pll_ctrl,
	mout_aud_pll, mout_top0_mfc_pll, mout_top0_cci_pll, mout_top0_bus1_pll,
	mout_top0_bus0_pll, dout_sclk_isp_mpwm, dout_sclk_isp_pwm,

	isp_pll = 35, cam_pll, bus0_pll, bus1_pll, cci_pll, mfc_pll, aud_pll, mif_pll,

	//clkout = 45,
	bus_pll_g3d = 50,
	/* number for uart driver starts from 100 */
	baud0 = 100, baud1, baud2, puart0, suart0,
	puart1, suart1, puart2, suart2, mct,
	baud3 = 110, puart3, suart3, sclk_spi1, sclk_spi2,
	sclk_spi3, sclk_spi4, sclk_spi5, sclk_spi0,

	/* number for i2c driver starts from 120 */
	hsi2c0 = 120, hsi2c1, hsi2c2, hsi2c3, hsi2c4,
	hsi2c5, hsi2c6, hsi2c7, hsi2c8, hsi2c9,
	hsi2c10 = 130, hsi2c11, pclk_spi0, pclk_spi1, pclk_spi2,
	pclk_spi3, pclk_spi4, pclk_spi5,
	/* number for rest of peric starts from 140 */
	wdt_apl = 140, pclk_pwm, pclk_adcif, cec,
	/* number for g3d starts from 160 */
	g3d = 160, dout_g3d, mout_g3d, sclk_hpm_g3d,
	/* number for mmc starts from 180, dma from 183 */
	/* number for rtc starts from 185 */
	usermux_aclk_fsys1_200 = 180, sclk_mmc0, dout_mmc0, pdma0, pdma1,
	rtc, 
	aclk_mmc1, sclk_mmc1, dout_mmc1,
	sclk_mmc2 = 190, dout_mmc2, usermux_aclk_fsys0_200,
	/* number for disp starts from 200 to 299 */
	d_pclk_disp = 200, aclk_decon0, m_decon0_eclk, d_decon0_eclk, decon0_eclk,
	m_decon0_vclk, d_decon0_vclk, decon0_vclk, mipi0_rx, mipi0_bit,
	aclk_decon1 = 210, pclk_decon1, um_decon1_eclk, m_decon1_eclk, d_decon1_eclk,
	decon1_eclk, decon1_vclk, hdmi_pixel, hdmi_tmds, sclk_dsd,
	pclk_decon0 = 220, pclk_dsim0, pclk_dsim1, pclk_hdmi, pclk_hdmiphy,
	um_decon1_vclk, m_decon1_vclk, d_decon1_vclk,
	aclk_lh_disp0 = 230, aclk_lh_disp1, aclk_disp, pclk_disp, rgb_vclk0,
	rgb_vclk1, mipi1_rx, mipi1_bit,
	disp_last = 299,

	/* number for ccore 300 */
	/* number for audio starts from 400 */
	aclk_dmac = 400, aclk_sramc, aclk_audnp_133,
	aclk_acel_lh_async_si_top_133, aclk_smmu_aud,
	aclk_xiu_lpassx, aclk_intr, aclk_atclk_aud,
	pclk_smmu_aud = 410, pclk_sfr0, pclk_sfr1, pclk_timer,
	pclk_i2s, pclk_pcm, pclk_uart, pclk_slimbus, pclk_wdt0, pclk_wdt1,
	pclk_pmu_aud, pclk_gpio_aud, pclk_dbg_aud,
	sclk_ca5 = 430, sclk_slimbus, sclk_uart, sclk_i2s, sclk_pcm,
	sclk_slimbus_clkin, sclk_i2s_bclk,

	mout_aud_pll_user = 440, mout_sclk_pcm, mout_sclk_i2s,
	dout_pclk_dbg_aud = 450, dout_aclk_aud, dout_aud_ca5,
	dout_aud_cdclk, dout_sclk_slimbus, dout_sclk_uart,
	dout_sclk_pcm, dout_sclk_i2s, dout_atclk_aud,
	pclk_spdif = 460, pclk_pcm1, pclk_i2s1,
	sclk_spdif = 465, sclk_pcm1, sclk_i2s1, sclk_i2s1_bclk,
	mout_aud_pll_ctrl = 470, dout_sclk_aud_pll, mout_aud_pll_user_top, mout_aud_pll_top0,
	mout_sclk_spdif = 475, mout_sclk_pcm1, mout_sclk_i2s1,
	dout_sclk_spdif = 480, dout_sclk_pcm1, dout_sclk_i2s1,

	/* number for vpp starts from 500 to 599 */
	aclk_vg0 = 500, aclk_vg1, aclk_vgr0, aclk_vgr1,	aclk_lh_vpp0,
	aclk_lh_vpp1, d_pclk_vpp, pclk_vpp, aclk_smmu_vpp_sfw0,
	aclk_smmu_vpp_sfw1, pclk_smmu_vpp_sfw0, pclk_smmu_vpp_sfw1,
	vpp_last = 599,

	/*
	 * clk id for MSCL: 600 ~ 699
	 * NOTE: clock ID of MSCL block is defined in
	 * include/dt-bindings/clock/exynos7420.h
	 */
	mscl_last = 699,

	/* clk id for g2d: 700 ~ 799
	 * NOTE: clock ID of G2D block is defined in
	 * include/dt-bindings/clock/exynos7420.h
	 */
	g2d_last = 799,

	/* extra peric and peris 800 */
	/* extra fsys 900 */

	/* number for mfc starts from 1000 */
	aclk_mfc = 1000, pclk_mfc, dout_aclk_mfc_532, dout_pclk_mfc,
	aclk_lh_s_mfc_0 = 1010, aclk_lh_s_mfc_1,aclk_noc_bus1_nrt,
	pclk_gpio_bus1 = 1020, aclk_lh_mfc0, aclk_lh_mfc1,
	top_aclk_mfc_532,

	/* clk id for sysmmu: 1500 ~ 1599
	 * NOTE: clock IDs of sysmmus are defined in
	 * include/dt-bindings/clock/exynos7420.h
	 */
	sysmmu_last = 1599,

	/* clk id for isp: 2000 ~ 4000
	 * NOTE: clock ID of ISP block is defined in
	 * include/dt-bindings/clock/exynos7420.h
	 */
	/* top0 - isp0 */
	dout_aclk_isp0_isp0_590 = 2000, dout_aclk_isp0_tpu_590, dout_aclk_isp0_trex_532,

	/* top0 - isp1 */
	dout_aclk_isp1_isp1_468 = 2005, dout_aclk_isp1_ahb_117,

	/* top0 - cam0 */
	dout_aclk_cam0_csis0_690 = 2010, dout_aclk_cam0_bnsa_690, dout_aclk_cam0_bnsb_690,
	dout_aclk_cam0_bnsd_690, dout_aclk_cam0_csis1_174, dout_aclk_cam0_3aa0_690,
	dout_aclk_cam0_3aa1_468, dout_aclk_cam0_trex_532, dout_aclk_cam0_nocp_133,

	/* top0 - cam1 */
	dout_aclk_cam1_sclvra_491 = 2020, dout_aclk_cam1_arm_668, dout_aclk_cam1_busperi_334,
	dout_aclk_cam1_bnscsis_133, dout_aclk_cam1_nocp_133, dout_aclk_cam1_trex_532,

	/* top0 - cam1 special clock */
	dout_sclk_isp_spi0 = 2030, dout_sclk_isp_spi1, dout_sclk_isp_uart,

	/* top0 - mout_isp0 */
	mout_aclk_isp0_isp0_590 = 2035, mout_aclk_isp0_tpu_590, mout_aclk_isp0_trex_532,

	/* top0 - mout_isp1 */
	mout_aclk_isp1_isp1_468 = 2040, mout_aclk_isp1_ahb_117,

	/* top0 - mout_cam0 */
	mout_aclk_cam0_csis0_690 = 2045, mout_aclk_cam0_bnsa_690, mout_aclk_cam0_bnsb_690,
	mout_aclk_cam0_bnsd_690, mout_aclk_cam0_csis1_174, mout_aclk_cam0_3aa0_690,
	mout_aclk_cam0_3aa1_468, mout_aclk_cam0_trex_532, mout_aclk_cam0_nocp_133,

	/* top0 - mout_cam1 */
	mout_aclk_cam1_sclvra_491 = 2055, mout_aclk_cam1_arm_668, mout_aclk_cam1_busperi_334,
	mout_aclk_cam1_bnscsis_133, mout_aclk_cam1_nocp_133, mout_aclk_cam1_trex_532,

	/* top0 - mout_cam1 special clock */
	mout_sclk_isp_spi0 = 2065, mout_sclk_isp_spi1, mout_sclk_isp_uart,

	/* cam0 ip gate */
	gate_aclk_csis0_i_wrap = 3005,

	gate_aclk_fimc_bns_a = 3010, gate_aclk_trex_a_5x1_bns_a,

	gate_pclk_fimc_bns_a = 3015, gate_cclk_asyncapb_socp_fimc_bns_a,

	gate_aclk_fimc_bns_b = 3020, gate_aclk_trex_a_5x1_bns_b,

	gate_pclk_fimc_bns_b = 3025, gate_cclk_asyncapb_socp_fimc_bns_b,

	gate_aclk_fimc_bns_d = 3030, gate_aclk_trex_a_5x1_bns_d,

	gate_pclk_fimc_bns_d = 3035, gate_cclk_asyncapb_socp_fimc_bns_d,

	gate_aclk_csis1_i_wrap = 3040, gate_aclk_csis3_i_wrap,

	gate_aclk_fimc_3aa0 = 3045, gate_aclk_trex_a_5x1_aa0, gate_aclk_pxl_asbs_fimc_bns_c,
	gate_aclk_pxl_asbs_3aa0_in,

	gate_pclk_fimc_3aa0 = 3050, gate_cclk_asyncapb_socp_3aa0,

	gate_aclk_fimc_3aa1 = 3055, gate_aclk_trex_a_5x1_aa1, gate_aclk_pxl_asbs_3aa1_in,

	gate_pclk_fimc_3aa1 = 3060, gate_cclk_asyncapb_socp_3aa1,

	gate_aclk_trex_a_5x1 = 3065, gate_aclk_axi_lh_async_si_top_cam0,

	gate_pclk_csis0 = 3070, gate_pclk_csis1, gate_pclk_csis3,
	gate_aclk_axi2apb_bridge_is0p,
	gate_pclk_asyncapb_socp_3aa0, gate_pclk_asyncapb_socp_3aa1,
	gate_pclk_asyncapb_socp_fimc_bns_a, gate_pclk_asyncapb_socp_fimc_bns_b, gate_pclk_asyncapb_socp_fimc_bns_d,
	gate_pclk_freeruncnt, gate_aclk_xiu_is0x, gate_aclk_axi2ahb_is0p,
	gate_hclk_ahbsyncdn_cam0, gate_aclk_xiu_async_mi_cam0, gate_pclk_xiu_async_mi_cam0,
	gate_aclk_xiu_async_mi_is0x,

	gate_pclk_xiu_async_mi_is0x = 3086, gate_pclk_pmu_cam0,
	gate_hclk_ahb2apb_bridge_is0p, gate_pclk_trex_a_5x1,

	gate_aclk_pxl_asbs_fimc_bns_c_int = 3090, gate_aclk_pxl_asbm_fimc_bns_c_int,

	gate_aclk_200_cam0_noc_p_cam0 = 3095, gate_aclk_xiu_async_si_is0x,

	gate_user_phyclk_rxbyteclkhs0_s2a,

	gate_user_phyclk_rxbyteclkhs0_s4,

	gate_user_phyclk_rxbyteclkhs1_s4,

	gate_user_phyclk_rxbyteclkhs2_s4,

	gate_user_phyclk_rxbyteclkhs3_s4,

	/* cam1 ip gate */
	gate_aclk_fimc_scaler = 3105, gate_aclk_fimc_vra, gate_aclk_pxl_asbs_from_blkc,
	gate_clk_fimc_scaler, gate_clk_fimc_vra,
	gate_clk_scaler_trex_b, gate_clk_vra_trex_b,

	gate_pclk_fimc_scaler = 3115, gate_pclk_fimc_vra,
	gate_cclk_asyncapb_socp_fimc_scaler, gate_cclk_asyncapb_socp_fimc_vra_s0, gate_cclk_asyncapb_socp_fimc_vra_s1,

	gate_aclk_xiu_n_async_si_cortex = 3120, gate_clk_mcu_isp_400_isp_arm_sys,

	gate_atclks_asatbslv_cam1_cssys = 3125, gate_pclkdbg_asapbmst_cssys_cam1,
	gate_clk_csatbdownsizer_cam1,

	gate_aclk_axi2apb_bridge_is3p = 3130, gate_aclk_axispcx, gate_aclk_axisphx,
	gate_aclk_gic_isp_arm_sys,
	gate_aclk_r_axispcx, gate_aclk_r_axisphx, gate_aclk_xiu_ispx_1x4,
	gate_aclk_xiu_n_async_mi_cortex, gate_aclk_xiu_n_async_si_cam1,
	gate_aclk_xiu_n_async_si_to_blkc, gate_aclk_xiu_n_async_si_to_blkd,
	gate_clk_isp_cpu_trex_b, gate_hclk_ahbsyncdn_isp_peri,
	gate_hclk_ahbsyncdn_isp2h, gate_hclkm_asyncahb_cam1,

	gate_pclk_asyncapb_socp_fimc_bns_c = 3150, gate_pclk_asyncapb_socp_fimc_scaler,
	gate_pclk_asyncapb_socp_fimc_vra_s0, gate_pclk_asyncapb_socp_fimc_vra_s1,
	gate_pclk_csis2, gate_pclk_fimc_is_b_glue, gate_pclk_xiu_n_async_mi_cortex,

	gate_hclk_ahb_sfrisp2h = 3165, gate_hclk_ahb2apb_bridge_is3p, gate_hclk_ahb2apb_bridge_is5p,
	gate_hclk_asyncahbslave_to_blkc, gate_hclk_asyncahbslave_to_blkd,
	gate_pclk_i2c0_isp, gate_pclk_i2c1_isp, gate_pclk_i2c2_isp,
	gate_pclk_mcuctl_isp, gate_pclk_mpwm_isp, gate_pclk_mtcadc_isp,
	gate_pclk_pwm_isp, gate_pclk_spi0_isp, gate_pclk_spi1_isp,
	gate_pclk_trex_b, gate_pclk_uart_isp, gate_pclk_wdt_isp, gate_pclk_xiu_n_async_mi_from_blkd,
	gate_pclk_pmu_cam1, gate_sclk_isp_pwm, gate_sclk_isp_mpwm,

	gate_pclk_fimc_bns_c = 3190, gate_aclk_fimc_bns_c, gate_cclk_asyncapb_socp_fimc_bns_c,
	gate_clk_bns_c_trex_b, gate_aclk_wrap_csis2,

	gate_clk_133_cam1_noc_p_cam1 = 3200, gate_hclks_asyncahb_cam1,

	gate_clk_axlh_async_si_top_cam1 = 3205,
	gate_aclk_xiu_n_async_mi_from_blkd, gate_clk_b_trex_b,

	gate_user_sclk_isp_spi0,

	gate_user_sclk_isp_spi1,

	gate_user_sclk_isp_uart,

	gate_sclk_isp_mtcadc,

	gate_sclk_i2c0_isp, gate_sclk_i2c1_isp, gate_sclk_i2c2_isp,

	gate_user_phyclk_rxbyteclkhs0_s2b,

	/* isp0 ip gate */
	gate_clk_isp0_trex_c = 3220, gate_aclk_isp_v4,
	gate_clk_isp_v4, gate_clk_pxl_asb_s_in,

	gate_cclk_asyncapb_isp = 3225, gate_pclk_isp_v4,

	gate_clk_tpu_trex_c = 3230, gate_aclk_tpu_v1,
	gate_clk_tpu_v1,

	gate_cclk_asyncapb_tpu = 3235, gate_pclk_tpu_v1,

	gate_clk_axi_lh_async_si_top_isp0 = 3240, gate_clk_c_trex_c,

	gate_pclk_asyncapb_isp = 3245, gate_pclk_asyncapb_tpu,
	gate_aclk_axi2apb_bridge, gate_aclk_xiu_async_m, gate_pclk_pmu_isp0,

	gate_hclk_ahb2apb_bridge = 3255, gate_pclk_trex_c, gate_hclkm_ahb_async_m,

	/* isp1 ip gate */
	gate_aclk_fimc_isp1 = 3260, gate_clk_fimc_isp1,
	gate_aclk_pxl_asbs, gate_aclk_xiu_n_async_si,

	gate_pclk_fimc_isp1 = 3265, gate_aclk_xiu_n_async_mi,
	gate_aclk_axi2apb_bridge_is2p,

	gate_hclk_ahb2apb_bridge_is2p = 3270, gate_hclkm_asyncahbmaster,
	gate_pclk_pmu_isp1,

	/* MUX */
	/* isp & cam mux */
	mout_user_mux_aclk_cam0_csis0_690 = 3275,
	mout_user_mux_aclk_cam0_bnsa_690,
	mout_user_mux_aclk_cam0_bnsb_690,
	mout_user_mux_aclk_cam0_bnsd_690,
	mout_user_mux_aclk_cam0_csis1_174,
	mout_user_mux_aclk_cam0_3aa0_690,
	mout_user_mux_aclk_cam0_3aa1_468,
	mout_user_mux_aclk_cam0_trex_532,
	mout_user_mux_aclk_cam0_csis3_133,
	mout_user_mux_aclk_cam0_nocp_133,
	mout_user_mux_phyclk_rxbyteclkhs0_s2a,
	mout_user_mux_phyclk_rxbyteclkhs0_s4,
	mout_user_mux_phyclk_rxbyteclkhs1_s4,
	mout_user_mux_phyclk_rxbyteclkhs2_s4,
	mout_user_mux_phyclk_rxbyteclkhs3_s4,

	mout_user_mux_aclk_cam1_sclvra_491 = 3290,
	mout_user_mux_aclk_cam1_arm_668,
	mout_user_mux_aclk_cam1_busperi_334,
	mout_user_mux_aclk_cam1_bnscsis_133,
	mout_user_mux_aclk_cam1_nocp_133,
	mout_user_mux_aclk_cam1_trex_532,
	mout_user_mux_sclk_isp_spi0,
	mout_user_mux_sclk_isp_spi1,
	mout_user_mux_sclk_isp_uart,
	mout_user_mux_phyclk_hs0_csis2_rx_byte,

	mout_user_mux_aclk_isp0_isp0_590 = 3300,
	mout_user_mux_aclk_isp0_tpu_590,
	mout_user_mux_aclk_isp0_trex_532,

	mout_user_mux_aclk_isp1_isp1_468,
	mout_user_mux_aclk_isp1_ahb_117,

	/* DIV */
	/* isp & cam div */
	dout_clkdiv_pclk_cam0_bnsa_345 = 3305,
	dout_clkdiv_pclk_cam0_bnsb_345,
	dout_clkdiv_pclk_cam0_bnsd_345,
	dout_clkdiv_pclk_cam0_3aa0_345,
	dout_clkdiv_pclk_cam0_3aa1_234,
	dout_clkdiv_pclk_cam0_trex_266,
	dout_clkdiv_pclk_cam0_trex_133,

	dout_clkdiv_pclk_cam1_sclvra_246 = 3315,
	dout_clkdiv_pclk_cam1_arm_167,
	dout_clkdiv_pclk_cam1_busperi_167,
	dout_clkdiv_pclk_cam1_busperi_84,

	dout_clkdiv_pclk_isp0_isp0_295 = 3320,
	dout_clkdiv_pclk_isp0_tpu_295,
	dout_clkdiv_pclk_isp0_trex_266,
	dout_clkdiv_pclk_isp0_trex_133,

	dout_clkdiv_pclk_isp1_isp1_234,

	/* top isp0 gate */
	aclk_isp0_isp0_590 = 3325, aclk_isp0_tpu_590, aclk_isp0_trex_532,

	/* top0 isp1 gate */
	aclk_isp1_isp1_468 = 3330, aclk_isp1_ahb_117,

	/* top0 cam0 gate */
	aclk_cam0_csis0_690 = 3335, aclk_cam0_bnsa_690, aclk_cam0_bnsb_690,
	aclk_cam0_bnsd_690, aclk_cam0_csis1_174, aclk_cam0_csis3_133, aclk_cam0_3aa0_690,
	aclk_cam0_3aa1_468, aclk_cam0_trex_532, aclk_cam0_nocp_133,

	/* top0 cam1 gate */
	aclk_cam1_sclvra_491 = 3345, aclk_cam1_arm_668, aclk_cam1_busperi_334,
	aclk_cam1_bnscsis_133, aclk_cam1_nocp_133, aclk_cam1_trex_532,

	/* top0 - cam1 gate special clock */
	sclk_isp_spi0 = 3355, sclk_isp_spi1, sclk_isp_uart,

	/* ETC gate */
	phyclk_rxbyteclkhs0_s2a = 3360, phyclk_rxbyteclkhs0_s4, phyclk_rxbyteclkhs1_s4,
	phyclk_rxbyteclkhs2_s4, phyclk_rxbyteclkhs3_s4, phyclk_hs0_csis2_rx_byte,

	/* bus0 */
	gate_aclk_lh_cam0 = 3370, gate_aclk_lh_cam1, gate_aclk_lh_isp, gate_aclk_noc_bus0_nrt,

	/* MIPI-LLI gate */
	aclk_xiu_llisfrx = 3500, aclk_axius_lli_be, aclk_axius_lli_ll,
	aclk_axius_llisfrx_llill, aclk_axius_llisfrx_llibe,
	aclk_lli_ll_init = 3510, aclk_lli_ll_targ,
	aclk_lli_be_init, aclk_lli_be_targ,
	aclk_lli_svc_loc, aclk_lli_svc_rem,
	user_phyclk_lli_tx0_symbol, user_phyclk_lli_rx0_symbol,

	/* Modem common */
	aclk_xiu_modemx = 3600, aclk_combo_phy_modem_pcs_pclk,
	sclk_phy_fsys0, sclk_combo_phy_modem_26m, pclk_async_combo_phy_modem,

	/* pcie(Modem) gate */
	aclk_pcie_modem_mstr_aclk = 3650, aclk_pcie_modem_slv_aclk, aclk_pcie_modem_dbi_aclk,
	sclk_pcie_modem_gated, phyclk_pcie_tx0_gated, phyclk_pcie_rx0_gated,

	/* pcie gate */
	aclk_xiu_wifi1x = 3700, aclk_ahb2axi_pcie_wifi1, aclk_pcie_wifi1_mstr_aclk,
	aclk_pcie_wifi1_slv_aclk, aclk_pcie_wifi1_dbi_aclk, aclk_combo_phy_pcs_pclk_wifi1,
	pclk_async_combo_phy_wifi1, sclk_phy_fsys1_gated, sclk_pcie_link_wifi1_gated,
	phyclk_pcie_wifi1_tx0_gated, phyclk_pcie_wifi1_rx0_gated, sclk_combo_phy_wifi1_26m_gated,

	aclk_usbdrd300 = 5010, aclk_axius_usbdrd30x_fsys0x,
	sclk_usbdrd300_ref_clk, user_sclk_usbdrd300,
	oscclk_phy_clkout_usb300_phy,
	user_phyclk_usbdrd300_udrd30_phyclock,
	user_phyclk_usbdrd300_udrd30_pipe_pclk,
	aclk_ahb_usbdrd300_linkh,

	aclk_usbhost20 = 5020, aclk_axius_usbhs_fsys0x,
	aclk_ahb_usbhs, aclk_ahb2axi_usbhs_modemx,
	sclk_usbhost20_clk48mohci,
	sclk_usb20phy_hsic_pll480mclk,
	oscclk_phy_clkout_usb20_hsic_phy,
	user_phyclk_usbhost20_phy_freeclk__hsic1,
	user_phyclk_usbhost20_phy_phyclk__hsic1,

	/* ufs */
	aclk_ufs20_link = 5030, sclk_ufsunipro20_gated,
	phyclk_ufs20_tx0_symbol_gated, phyclk_ufs20_tx1_symbol_gated,
	phyclk_ufs20_rx0_symbol_gated, phyclk_ufs20_rx1_symbol_gated,
	oscclk_phy_clkout_embedded_combo_phy,
	sclk_combo_phy_embedded_26m_gated, mout_fsys1_phyclk_sel1, top_sclk_phy_fsys1_26m,

	nr_clks,
};

/*
 * list of controller registers to be saved and restored during a
 * suspend/resume cycle.
 */
static __initdata void *exynos7420_clk_regs[] = {

	HSIC_LOCK,
	HSIC_CON0,
	HSIC_CON1,

	EXYNOS7420_ENABLE_ACLK_FSYS00,
	EXYNOS7420_ENABLE_ACLK_FSYS01,
	EXYNOS7420_ENABLE_PCLK_FSYS0,
	EXYNOS7420_ENABLE_SCLK_FSYS01,
	EXYNOS7420_ENABLE_SCLK_FSYS02,
	EXYNOS7420_ENABLE_SCLK_FSYS03,
	EXYNOS7420_ENABLE_SCLK_FSYS04,
	EXYNOS7420_DIV_FSYS0,
	EXYNOS7420_MUX_SEL_FSYS00,
	EXYNOS7420_MUX_SEL_FSYS01,
	EXYNOS7420_MUX_SEL_FSYS02,
	EXYNOS7420_MUX_SEL_FSYS03,

	EXYNOS7420_ENABLE_ACLK_FSYS1,
	EXYNOS7420_ENABLE_PCLK_FSYS1,
	EXYNOS7420_ENABLE_SCLK_FSYS11,
	EXYNOS7420_ENABLE_SCLK_FSYS12,
	EXYNOS7420_ENABLE_SCLK_FSYS13,
	EXYNOS7420_DIV_FSYS1,
	EXYNOS7420_MUX_SEL_FSYS10,
	EXYNOS7420_MUX_SEL_FSYS11,
	EXYNOS7420_MUX_SEL_FSYS12,

	EXYNOS7420_ENABLE_ACLK_PERIC0,
	EXYNOS7420_ENABLE_PCLK_PERIC0,
	EXYNOS7420_ENABLE_SCLK_PERIC0,
	EXYNOS7420_MUX_SEL_PERIC0,

	EXYNOS7420_ENABLE_ACLK_PERIC1,
	EXYNOS7420_ENABLE_PCLK_PERIC1,
	EXYNOS7420_ENABLE_SCLK_PERIC10,
	EXYNOS7420_ENABLE_SCLK_PERIC11,
	EXYNOS7420_MUX_SEL_PERIC10,
	EXYNOS7420_MUX_SEL_PERIC11,

	EXYNOS7420_ENABLE_ACLK_PERIS,
	EXYNOS7420_ENABLE_PCLK_PERIS,
	EXYNOS7420_ENABLE_SCLK_PERIS,
	EXYNOS7420_MUX_SEL_PERIS,

	EXYNOS7420_ENABLE_ACLK_CCORE0,
	EXYNOS7420_ENABLE_ACLK_CCORE1,
	EXYNOS7420_ENABLE_PCLK_CCORE,
	EXYNOS7420_DIV_CCORE,

	EXYNOS7420_ENABLE_ACLK_IMEM0,
	EXYNOS7420_ENABLE_ACLK_IMEM1,
	EXYNOS7420_MUX_SEL_IMEM,

	EXYNOS7420_MUX_SEL_BUS0,
	EXYNOS7420_ENABLE_ACLK_BUS0,
	EXYNOS7420_MUX_SEL_BUS1,
	EXYNOS7420_ENABLE_ACLK_BUS1,

	EXYNOS7420_DIV_MIF0,
	EXYNOS7420_DIV_MIF1,
	EXYNOS7420_DIV_MIF2,
	EXYNOS7420_DIV_MIF3,

	CAM_CON,
	ISP_CON,
	EXYNOS7420_ENABLE_ACLK_TOP02,
	EXYNOS7420_ENABLE_ACLK_TOP03,
	EXYNOS7420_ENABLE_ACLK_TOP04,
	EXYNOS7420_ENABLE_ACLK_TOP05,
	EXYNOS7420_ENABLE_ACLK_TOP06,
	EXYNOS7420_ENABLE_ACLK_TOP07,
	EXYNOS7420_ENABLE_SCLK_TOP0_DISP,
	EXYNOS7420_ENABLE_SCLK_TOP0_CAM10,
	EXYNOS7420_ENABLE_SCLK_TOP0_CAM11,
	EXYNOS7420_ENABLE_SCLK_TOP0_PERIC0,
	EXYNOS7420_ENABLE_SCLK_TOP0_PERIC1,
	EXYNOS7420_ENABLE_SCLK_TOP0_PERIC2,
	EXYNOS7420_ENABLE_SCLK_TOP0_PERIC3,

	EXYNOS7420_DIV_TOP03,
	EXYNOS7420_DIV_TOP04,
	EXYNOS7420_DIV_TOP05,
	EXYNOS7420_DIV_TOP06,
	EXYNOS7420_DIV_TOP07,
	EXYNOS7420_DIV_TOP0_DISP,
	EXYNOS7420_DIV_TOP0_CAM10,
	EXYNOS7420_DIV_TOP0_CAM11,
	EXYNOS7420_DIV_TOP0_PERIC0,
	EXYNOS7420_DIV_TOP0_PERIC1,
	EXYNOS7420_DIV_TOP0_PERIC2,
	EXYNOS7420_DIV_TOP0_PERIC3,
	EXYNOS7420_MUX_SEL_TOP00,
	EXYNOS7420_MUX_SEL_TOP01,
	EXYNOS7420_MUX_SEL_TOP03,
	EXYNOS7420_MUX_SEL_TOP04,
	EXYNOS7420_MUX_SEL_TOP05,
	EXYNOS7420_MUX_SEL_TOP06,
	EXYNOS7420_MUX_SEL_TOP07,
	EXYNOS7420_MUX_SEL_TOP0_DISP,
	EXYNOS7420_MUX_SEL_TOP0_CAM10,
	EXYNOS7420_MUX_SEL_TOP0_CAM11,
	EXYNOS7420_MUX_SEL_TOP0_PERIC0,
	EXYNOS7420_MUX_SEL_TOP0_PERIC1,
	EXYNOS7420_MUX_SEL_TOP0_PERIC2,
	EXYNOS7420_MUX_SEL_TOP0_PERIC3,

	EXYNOS7420_ENABLE_ACLK_TOP12,
	EXYNOS7420_ENABLE_ACLK_TOP13,
	EXYNOS7420_ENABLE_SCLK_TOP1_FSYS0,
	EXYNOS7420_ENABLE_SCLK_TOP1_FSYS1,
	EXYNOS7420_ENABLE_SCLK_TOP1_FSYS11,

	EXYNOS7420_DIV_TOP13,
	EXYNOS7420_DIV_TOP1_FSYS0,
	EXYNOS7420_DIV_TOP1_FSYS1,
	EXYNOS7420_DIV_TOP1_FSYS11,
	EXYNOS7420_MUX_SEL_TOP10,
	EXYNOS7420_MUX_SEL_TOP11,
	EXYNOS7420_MUX_SEL_TOP13,
	EXYNOS7420_MUX_SEL_TOP1_FSYS0,
	EXYNOS7420_MUX_SEL_TOP1_FSYS1,
	EXYNOS7420_MUX_SEL_TOP1_FSYS11,

	EXYNOS7420_ENABLE_ACLK_TOPC0,
	EXYNOS7420_ENABLE_ACLK_TOPC1,
	EXYNOS7420_ENABLE_ACLK_TOPC2,
	EXYNOS7420_ENABLE_SCLK_TOPC0,
	EXYNOS7420_ENABLE_SCLK_TOPC1,

	EXYNOS7420_DIV_TOPC0,
	EXYNOS7420_DIV_TOPC1,
	EXYNOS7420_DIV_TOPC2,
	EXYNOS7420_DIV_TOPC3,
	EXYNOS7420_MUX_SEL_TOPC0,
	EXYNOS7420_MUX_SEL_TOPC1,
	EXYNOS7420_MUX_SEL_TOPC2,
	EXYNOS7420_MUX_SEL_TOPC3,
	EXYNOS7420_MUX_SEL_TOPC4,
	EXYNOS7420_MUX_SEL_TOPC5,

	AUD_CON,
	AUD_CON1,

	EXYNOS7420_MUX_SEL_ATLAS1,
	EXYNOS7420_ATLAS_PWR_CTRL,
	EXYNOS7420_ATLAS_PWR_CTRL2,
	EXYNOS7420_MUX_SEL_APOLLO1,
	EXYNOS7420_APOLLO_PWR_CTRL,
	EXYNOS7420_APOLLO_PWR_CTRL2,
	EXYNOS7420_ATLAS_SMPL_CTRL0
};

/*
 * table for pll clocks
 */
struct samsung_pll_rate_table table_atlas[] = {
	{2496000000U,	2, 208, 0, 0},
	{2400000000U,	2, 200, 0, 0},
	{2304000000U,	2, 192, 0, 0},
	{2200000000U,	3, 275, 0, 0},
	{2100000000U,	2, 175, 0, 0},
	{2000000000U,	3, 250, 0, 0},
	{1896000000U,	2, 158, 0, 0},
	{1800000000U,	2, 150, 0, 0},
	{1704000000U,	2, 142, 0, 0},
	{1600000000U,	3, 200, 0, 0},
	{1500000000U,	2, 250, 1, 0},
	{1400000000U,	3, 350, 1, 0},
	{1300000000U,	3, 325, 1, 0},
	{1200000000U,	2, 200, 1, 0},
	{1100000000U,	3, 275, 1, 0},
	{1000000000U,	3, 250, 1, 0},
	{900000000U,	2, 150, 1, 0},
	{800000000U,	3, 200, 1, 0},
	{700000000U,	3, 350, 2, 0},
	{600000000U,	2, 200, 2, 0},
	{500000000U,	3, 250, 2, 0},
	{400000000U,	3, 200, 2, 0},
	{300000000U,	2, 200, 3, 0},
	{200000000U,	3, 200, 3, 0},
	{0,		0, 0, 0, 0},
};
struct samsung_pll_rate_table table_apollo[] = {
	{1800000000U,	2, 150, 0, 0},
	{1704000000U,	2, 142, 0, 0},
	{1600000000U,	3, 200, 0, 0},
	{1500000000U,	4, 250, 0, 0},
	{1400000000U,	3, 175, 0, 0},
	{1296000000U,	2, 108, 0, 0},
	{1200000000U,	2, 100, 0, 0},
	{1104000000U,	2, 92, 0, 0},
	{1000000000U,	3, 125, 0, 0},
	{900000000U,	2, 150, 1, 0},
	{800000000U,	3, 200, 1, 0},
	{700000000U,	3, 175, 1, 0},
	{600000000U,	2, 100, 1, 0},
	{500000000U,	3, 125, 1, 0},
	{400000000U,	3, 200, 2, 0},
	{300000000U,	2, 100, 2, 0},
	{200000000U,	3, 200, 3, 0},
	{0,		0, 0, 0, 0},
};
struct samsung_pll_rate_table table_aud[] = {
	{491520000U,	1, 20,	0, 31457},
	{196608000U,	1, 33,	2, -15204},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_bus0[] = {
	{1600000000U,	3, 200, 0, 0},
	{800000000U,	3, 200, 1, 0},
	{400000000U,	3, 200, 2, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_bus1[] = {
	{668000000U,	6, 167, 0, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_cci[] = {
	{532000000U,	6, 133, 0, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_mfc[] = {
	{468000000U,	6, 234, 1, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_g3d[] = {
	{772000000U,	6, 193, 0, 0},
	{700000000U,	6, 175, 0, 0},
	{600000000U,	3, 75, 0, 0},
	{544000000U,	3, 68, 0, 0},
	{420000000U,	3, 105, 1, 0},
	{350000000U,	6, 175, 1, 0},
	{266000000U,	6, 133, 1, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_disp[] = {
	{266000000U,	3, 67, 1, -32768},
	{252000000U,	2, 42, 1, 0},
	{240000000U,	1, 20, 1, 0},
	{133000000U,	3, 67, 2, -32768},
	{126000000U,	1, 21, 2, 0},
	{96000000U,	1, 32, 3, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_isp[] = {
	{590000000U,	6, 295, 1, 0},
	{588000000U,	2, 98, 1, 0},
	{558000000U,	2, 93, 1, 0},
	{552000000U,	2, 92, 1, 0},
	{432000000U,	2, 144, 2, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_cam[] = {
	{690000000U,	2, 115, 1, 0},
	{660000000U,	3, 165, 1, 0},
	{532000000U,	3, 133, 1, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_mif[] = {
	{3104000000U,	3, 388, 0, 0},
	{2912000000U,	3, 364, 0, 0},
	{2528000000U,	3, 316, 0, 0},
	{2136000000U,	2, 178, 0, 0},
	{2052000000U, 	2, 171, 0, 0},
	{1656000000U,	3, 207, 0, 0},
	{1264000000U,	3, 316, 1, 0},
	{1086000000U,	2, 181, 1, 0},
	{832000000U,	3, 208, 1, 0},
	{696000000U,	3, 348, 2, 0},
	{552000000U,	3, 276, 2, 0},
	{334000000U,	3, 334, 3, 0},
	{266000000U,	3, 266, 3, 0},
	{200000000U,	3, 200, 3, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_hsic[] = {
	{480000000U,	6, 240, 1, 0},
	{0,		0, 0, 0, 0},
};

/*
 * parent names are defined as array like below.
 * it is for mux clocks.
 */
/* topc block */
PNAME(mout_bus0_pll_cmuc_p) = {"bus0_pll", "ffac_topc_bus0_pll_div2",
				"ffac_topc_bus0_pll_div4"};
PNAME(mout_bus1_pll_cmuc_p) = {"bus1_pll", "ffac_topc_bus1_pll_div2"};
PNAME(mout_cci_pll_cmuc_p) = {"cci_pll", "ffac_topc_cci_pll_div2"};
PNAME(mout_mfc_pll_cmuc_p) = {"mfc_pll", "ffac_topc_mfc_pll_div2"};
PNAME(mout_sclk_bus0_pll_out_p) = {"bus0_pll", "ffac_topc_bus0_pll_div2"};
PNAME(mout_aud_pll_ctrl_p) = {"fin_pll", "aud_pll"};
PNAME(topc_group0) = {"bus0_pll", "ffac_topc_bus0_pll_div2",
			"bus1_pll", "cci_pll"};
PNAME(topc_group1) = {"mout_bus0_pll_cmuc", "mout_bus1_pll_cmuc",
			"mout_cci_pll_cmuc", "mout_mfc_pll_cmuc"};
PNAME(topc_group2) = {"mout_bus0_pll_cmuc", "bus1_pll", "cci_pll", "mfc_pll",
			"ffac_mout_bus0_pll_cmuc_div2", "ffac_topc_bus1_pll_div2",
			"ffac_topc_cci_pll_div2", "ffac_topc_mfc_pll_div2",
			"ffac_mout_bus0_pll_cmuc_div4", "ffac_topc_bus1_pll_div4",
			"ffac_topc_cci_pll_div4", "ffac_topc_mfc_pll_div4"};
/* top0 block */
PNAME(mout_bus0_pll_top0_p) = {"usermux_bus0_pll_top0", "ffac_top0_bus0_pll_div2"};
PNAME(mout_bus1_pll_top0_p) = {"usermux_bus1_pll_top0", "ffac_top0_bus1_pll_div2"};
PNAME(mout_cci_pll_top0_p) = {"usermux_cci_pll_top0", "ffac_top0_cci_pll_div2"};
PNAME(mout_mfc_pll_top0_p) = {"usermux_mfc_pll_top0", "ffac_top0_mfc_pll_div2"};
PNAME(mout_aclk_disp_400_p) = {"usermux_bus0_pll_top0", "usermux_bus1_pll_top0",
			"usermux_cci_pll_top0", "usermux_mfc_pll_top0",
			"ffac_top0_bus0_pll_div2", "ffac_top0_bus1_pll_div2",
			"ffac_top0_cci_pll_div2", "ffac_top0_mfc_pll_div2"};
PNAME(mout_spdif_p) = {"ioclk_audiocdclk0", "ioclk_audiocdclk1",
			"ioclk_spdif_extclk", "usermux_aud_pll",
			"mout_bus0_pll_top0", "mout_bus1_pll_top0"};
PNAME(mout_sclk_isp_sensor_p) = {"mout_bus0_pll_top0", "mout_bus1_pll_top0",
			"mout_cci_pll_top0", "fin_pll"};
PNAME(top0_group1) = {"mout_bus0_pll_top0", "mout_bus1_pll_top0",
			"mout_cci_pll_top0", "mout_mfc_pll_top0"};
PNAME(top0_group2) = {"mout_bus0_pll_top0", "mout_bus1_pll_top0",
			"mout_cci_pll_top0", "mout_mfc_pll_top0", "usermux_aud_pll",
			"isp_pll", "cam_pll"};
PNAME(top0_group3) = {"ioclk_audiocdclk1", "usermux_aud_pll",
			"mout_bus0_pll_top0", "mout_bus1_pll_top0"};
/* top1 block */
PNAME(mout_bus0_pll_top1_p) = {"usermux_bus0_pll_top1", "ffac_top1_bus0_pll_div2"};
PNAME(mout_bus1_pll_top1_p) = {"usermux_bus1_pll_top1", "ffac_top1_bus1_pll_div2"};
PNAME(mout_cci_pll_top1_p) = {"usermux_cci_pll_top1", "ffac_top1_cci_pll_div2"};
PNAME(mout_mfc_pll_top1_p) = {"usermux_mfc_pll_top1", "ffac_top1_mfc_pll_div2"};
PNAME(top1_group1) = {"mout_bus0_pll_top1", "mout_bus1_pll_top1",
			"mout_cci_pll_top1", "mout_mfc_pll_top1"};
/* g3d block */
PNAME(mout_g3d_p) = {"g3d_pll", "fin_pll"};
/* disp block */
PNAME(mout_sub_sclk_decon_int_eclk_p) = {"usermux_sclk_decon_int_eclk", "disp_pll"};
PNAME(mout_sub_sclk_decon_ext_eclk_p) = {"usermux_sclk_decon_ext_eclk", "disp_pll"};
PNAME(mout_sub_sclk_decon_vclk_p) = {"usermux_sclk_decon_vclk", "disp_pll"};
PNAME(mout_decon_ext_vclk_p) = {"usermux_hdmiphy_pixel", "dout_sub_sclk_decon_ext_vclk"};
/* aud block */
PNAME(mout_sclk_i2s_p) = { "dout_aud_cdclk", "ioclk_audiocdclk0" };
PNAME(mout_sclk_pcm_p) = { "dout_aud_cdclk", "ioclk_audiocdclk0" };
/* atlas block */
PNAME(mout_atlas_p) = {"atlas_pll", "usermux_bus0_pll_atlas"};
/* apollo block */
PNAME(mout_apollo_p) = {"apollo_pll", "usermux_bus0_pll_apollo"};

/* mif */
PNAME(mout_aclk_mif_pll_p) = {"sclk_bus0_pll_mif", "mif_pll"};
PNAME(mout_cell_clksel_p) = {"ffac_mout_aclk_mif_pll_lp4phy_div8", "ffac_mout_aclk_mif_pll_lp4phy_div4"};
PNAME(mout_pclk_mif_p) = {"mout_aclk_mif_pll", "ffac_mout_aclk_mif_pll_div2", "ffac_mout_aclk_mif_pll_div4", "ffac_mout_aclk_mif_pll_div8"};

/* fsys0 */
PNAME(mout_fsys0_phyclk_sel0_p) = {"fin_pll", "fin_pll_26m", "top_sclk_phy_fsys0_26m"};
PNAME(mout_fsys1_phyclk_sel0_p) = {"fin_pll", "fin_pll_26m", "top_sclk_phy_fsys1_26m"};
PNAME(mout_fsys1_phyclk_sel1_p) = {"fin_pll_26m", "fin_pll", "top_sclk_phy_fsys1_26m"};

static struct samsung_fixed_rate exynos7420_fixed_rate_ext_clks[] __initdata = {
	FRATE(oscclk, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "samsung,exynos7420-oscclk", .data = (void *)0, },
};

static struct samsung_composite_pll exynos7420_pll_clks[] __refdata = {
	PLL(aud_pll, "aud_pll", pll_1460x, AUD_LOCK, AUD_CON, AUD_CON, 31, \
			EXYNOS7420_MUX_SEL_TOPC1, 0, \
			EXYNOS7420_MUX_STAT_TOPC1, 0, \
			table_aud, 0, CLK_IGNORE_UNUSED, NULL),
	PLL(none, "bus0_pll", pll_1451x, BUS0_LOCK, BUS0_CON, BUS0_CON, 31, \
			EXYNOS7420_MUX_SEL_TOPC0, 0, \
			EXYNOS7420_MUX_STAT_TOPC0, 0, \
			table_bus0, 0, CLK_IGNORE_UNUSED, "bus0_pll"),
	PLL(none, "bus1_pll", pll_1452x, BUS1_LOCK, BUS1_CON, BUS1_CON, 31, \
			EXYNOS7420_MUX_SEL_TOPC0, 4, \
			EXYNOS7420_MUX_STAT_TOPC0, 4, \
			table_bus1, 0, CLK_IGNORE_UNUSED, "bus1_pll"),
	PLL(none, "cci_pll", pll_1452x, CCI_LOCK, CCI_CON, CCI_CON, 31, \
			EXYNOS7420_MUX_SEL_TOPC0, 8, \
			EXYNOS7420_MUX_STAT_TOPC0, 8, \
			table_cci, 0, CLK_IGNORE_UNUSED, "cci_pll"),
	PLL(none, "mfc_pll", pll_1452x, MFC_LOCK, MFC_CON, MFC_CON, 31, \
			EXYNOS7420_MUX_SEL_TOPC0, 12, \
			EXYNOS7420_MUX_STAT_TOPC0, 12, \
			table_mfc, 0, CLK_IGNORE_UNUSED, "mfc_pll"),
	PLL(g3d_pll, "g3d_pll", pll_1452x, G3D_LOCK, G3D_CON, G3D_CON, 31, \
			EXYNOS7420_MUX_SEL_G3D, 0, \
			EXYNOS7420_MUX_STAT_G3D, 0, \
			table_g3d, 0, CLK_IGNORE_UNUSED | CLK_GET_RATE_NOCACHE, NULL),
	PLL(disp_pll, "disp_pll", pll_1460x, DISP_LOCK, DISP_CON, DISP_CON, 31, \
			EXYNOS7420_MUX_SEL_DISP0, 28, \
			EXYNOS7420_MUX_STAT_DISP0, 28, \
			table_disp, 0, CLK_IGNORE_UNUSED, NULL),
	PLL(isp_pll, "isp_pll", pll_1451x, ISP_LOCK, ISP_CON, ISP_CON, 31, \
			EXYNOS7420_MUX_SEL_TOP00, 24, \
			EXYNOS7420_MUX_STAT_TOP00, 24, \
			table_isp, 0, 0, "isp_pll"),
	PLL(cam_pll, "cam_pll", pll_1451x, CAM_LOCK, CAM_CON, CAM_CON, 31, \
			EXYNOS7420_MUX_SEL_TOP00, 20, \
			EXYNOS7420_MUX_STAT_TOP00, 20, \
			table_cam, 0, 0, "cam_pll"),
	PLL(none, "atlas_pll", pll_1450x, ATLAS_LOCK, ATLAS_CON, ATLAS_CON, 31, \
			EXYNOS7420_MUX_SEL_ATLAS0, 0, \
			EXYNOS7420_MUX_STAT_ATLAS0, 0, \
			table_atlas, 0, CLK_IGNORE_UNUSED | CLK_GET_RATE_NOCACHE, "mout_atlas_pll"),
	PLL(none, "apollo_pll", pll_1451x, APOLLO_LOCK, APOLLO_CON, APOLLO_CON, 31, \
			EXYNOS7420_MUX_SEL_APOLLO0, 0, \
			EXYNOS7420_MUX_STAT_APOLLO0, 0, \
			table_apollo, 0, CLK_IGNORE_UNUSED | CLK_GET_RATE_NOCACHE, "mout_apollo_pll"),
	PLL(mif_pll, "mif_pll", pll_1450x, MIF_LOCK, MIF_CON, MIF_CON, 31, \
			EXYNOS7420_MUX_SEL_TOPC5, 16, \
			EXYNOS7420_MUX_STAT_TOPC5, 20, \
			table_mif, 0, CLK_IGNORE_UNUSED, "mif_pll"),
	PLL(none, "hsic_pll", pll_1452x, HSIC_LOCK, HSIC_CON0, HSIC_CON0, 31, \
			EXYNOS7420_MUX_SEL_FSYS00, 28, \
			EXYNOS7420_MUX_STAT_FSYS00, 28, \
			table_hsic, 0, CLK_IGNORE_UNUSED, NULL),
};

static struct samsung_fixed_rate exynos7420_fixed_rate_clks[] __initdata = {
	FRATE(none, "ioclk_audiocdclk0", NULL, CLK_IS_ROOT, 83400000),
	FRATE(none, "ioclk_audiocdclk1", NULL, CLK_IS_ROOT, 83400000),
	FRATE(none, "ioclk_spdif_extclk", NULL, CLK_IS_ROOT, 50000000),
	FRATE(none, "ioclk_slimbus_clk", NULL, CLK_IS_ROOT, 50000000),
	FRATE(none, "ioclk_i2s_bclk", NULL, CLK_IS_ROOT, 50000000),

	FRATE(none, "sclk_rgb_vclk0", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "sclk_rgb_vclk1", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_mipidphy0_rx", NULL, CLK_IS_ROOT, 20000000),
	FRATE(none, "phyclk_mipidphy0_bit", NULL, CLK_IS_ROOT, 187500000),
	FRATE(none, "phyclk_mipidphy1_rx", NULL, CLK_IS_ROOT, 20000000),
	FRATE(none, "phyclk_mipidphy1_bit", NULL, CLK_IS_ROOT, 187500000),
	FRATE(none, "phyclk_hdmiphy_pixel_clko", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_hdmiphy_tmds_clko", NULL, CLK_IS_ROOT, 300000000),

	FRATE(phyclk_rxbyteclkhs0_s2a, "phyclk_rxbyteclkhs0_s2a", NULL, CLK_IS_ROOT, 250000000),
	FRATE(phyclk_rxbyteclkhs0_s4, "phyclk_rxbyteclkhs0_s4", NULL, CLK_IS_ROOT, 250000000),
	FRATE(phyclk_rxbyteclkhs1_s4, "phyclk_rxbyteclkhs1_s4", NULL, CLK_IS_ROOT, 250000000),
	FRATE(phyclk_rxbyteclkhs2_s4, "phyclk_rxbyteclkhs2_s4", NULL, CLK_IS_ROOT, 250000000),
	FRATE(phyclk_rxbyteclkhs3_s4, "phyclk_rxbyteclkhs3_s4", NULL, CLK_IS_ROOT, 250000000),
	FRATE(none, "phyclk_usbdrd300_udrd30_phyclock", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_usbdrd300_udrd30_pipe_pclk", NULL, CLK_IS_ROOT, 125000000),
	FRATE(none, "fin_pll_26m", NULL, CLK_IS_ROOT, 26000000),
	FRATE(none, "phyclk_ufs_tx0_symbol", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_ufs_rx0_symbol", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_usbhost20_phy_freeclk_hsic1", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_usbhost20_phy_phyclk_hsic1", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_lli_tx0", NULL, CLK_IS_ROOT, 150000000),
	FRATE(none, "phyclk_lli_rx0", NULL, CLK_IS_ROOT, 150000000),
	FRATE(none, "phyclk_pcie_tx0", NULL, CLK_IS_ROOT, 250000000),
	FRATE(none, "phyclk_pcie_rx0", NULL, CLK_IS_ROOT, 250000000),

	FRATE(none, "ioclk_i2s1_bclk", NULL, CLK_IS_ROOT, 25000000),
	FRATE(none, "phyclk_ufs20_tx0_symbol", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_ufs20_rx0_symbol", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_ufs20_rx1_symbol", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_pcie_wifi1_tx0", NULL, CLK_IS_ROOT, 250000000),
	FRATE(none, "phyclk_pcie_wifi1_rx0", NULL, CLK_IS_ROOT, 250000000),

	FRATE(none, "ioclk_spi0_clk", NULL, CLK_IS_ROOT, 100000000),
	FRATE(none, "ioclk_spi1_clk", NULL, CLK_IS_ROOT, 100000000),
	FRATE(none, "ioclk_spi2_clk", NULL, CLK_IS_ROOT, 100000000),
	FRATE(none, "ioclk_spi3_clk", NULL, CLK_IS_ROOT, 100000000),
	FRATE(none, "ioclk_spi4_clk", NULL, CLK_IS_ROOT, 100000000),
	FRATE(none, "ioclk_spi5_clk", NULL, CLK_IS_ROOT, 100000000),

	FRATE(phyclk_hs0_csis2_rx_byte, "phyclk_hs0_csis2_rx_byte", NULL, CLK_IS_ROOT, 250000000),
};

static struct samsung_fixed_factor exynos7420_fixed_factor_clks[] __initdata = {
	/* topc block */
	FFACTOR(none, "ffac_topc_bus0_pll_div2", "bus0_pll", 1, 2, 0),
	FFACTOR(none, "ffac_topc_bus1_pll_div2", "bus1_pll", 1, 2, 0),
	FFACTOR(none, "ffac_topc_cci_pll_div2", "cci_pll", 1, 2, 0),
	FFACTOR(none, "ffac_topc_mfc_pll_div2", "mfc_pll", 1, 2, 0),
	FFACTOR(none, "ffac_topc_bus0_pll_div4", "ffac_topc_bus0_pll_div2", 1, 2, 0),
	FFACTOR(none, "ffac_topc_bus1_pll_div4", "ffac_topc_bus1_pll_div2", 1, 2, 0),
	FFACTOR(none, "ffac_topc_cci_pll_div4", "ffac_topc_cci_pll_div2", 1, 2, 0),
	FFACTOR(none, "ffac_topc_mfc_pll_div4", "ffac_topc_mfc_pll_div2", 1, 2, 0),
	FFACTOR(none, "ffac_mout_bus0_pll_cmuc_div2", "mout_bus0_pll_cmuc", 1, 2, 0),
	FFACTOR(none, "ffac_mout_bus0_pll_cmuc_div4", "ffac_mout_bus0_pll_cmuc_div2", 1, 2, 0),
	/* top0 block */
	FFACTOR(none, "ffac_top0_bus0_pll_div2", "usermux_bus0_pll_top0", 1, 2, 0),
	FFACTOR(none, "ffac_top0_bus1_pll_div2", "usermux_bus1_pll_top0", 1, 2, 0),
	FFACTOR(none, "ffac_top0_cci_pll_div2", "usermux_cci_pll_top0", 1, 2, 0),
	FFACTOR(none, "ffac_top0_mfc_pll_div2", "usermux_mfc_pll_top0", 1, 2, 0),
	/* top1 block */
	FFACTOR(none, "ffac_top1_bus0_pll_div2", "usermux_bus0_pll_top1", 1, 2, 0),
	FFACTOR(none, "ffac_top1_bus1_pll_div2", "usermux_bus1_pll_top1", 1, 2, 0),
	FFACTOR(none, "ffac_top1_cci_pll_div2", "usermux_cci_pll_top1", 1, 2, 0),
	FFACTOR(none, "ffac_top1_mfc_pll_div2", "usermux_mfc_pll_top1", 1, 2, 0),

	/* MIF block */
	FFACTOR(none, "ffac_mout_aclk_mif_pll_lp4phy_div4", "mout_aclk_mif_pll", 1, 4, 0),
	FFACTOR(none, "ffac_mout_aclk_mif_pll_lp4phy_div8", "mout_aclk_mif_pll", 1, 8, 0),
	FFACTOR(none, "ffac_mout_aclk_mif_pll_div2", "mout_aclk_mif_pll", 1, 2, 0),
	FFACTOR(none, "ffac_mout_aclk_mif_pll_div4", "mout_aclk_mif_pll", 1, 4, 0),
	FFACTOR(none, "ffac_mout_aclk_mif_pll_div8", "mout_aclk_mif_pll", 1, 8, 0),
	FFACTOR(none, "pwm-clock", "pwm_clock", 1, 1, CLK_SET_RATE_PARENT),

	/* fsys block */
	FFACTOR(none, "oscclk_phy_div2", "fin_pll", 1, 2, 0),
};

static struct samsung_composite_mux exynos7420_mux_clks[] __refdata = {
	/* topc block */
	MUX(none, "mout_bus0_pll_cmuc", mout_bus0_pll_cmuc_p, \
			EXYNOS7420_MUX_SEL_TOPC0, 16, 2, \
			EXYNOS7420_MUX_STAT_TOPC0, 16, 4, 0, "mout_bus0_pll_cmuc"),
	MUX(none, "mout_bus1_pll_cmuc", mout_bus1_pll_cmuc_p, \
			EXYNOS7420_MUX_SEL_TOPC0, 20, 1, \
			EXYNOS7420_MUX_STAT_TOPC0, 20, 3, 0, "mout_bus1_pll_cmuc"),
	MUX(none, "mout_cci_pll_cmuc", mout_cci_pll_cmuc_p, \
			EXYNOS7420_MUX_SEL_TOPC0, 24, 1, \
			EXYNOS7420_MUX_STAT_TOPC0, 24, 3, 0, "mout_cci_pll_cmuc"),
	MUX(none, "mout_mfc_pll_cmuc", mout_mfc_pll_cmuc_p, \
			EXYNOS7420_MUX_SEL_TOPC0, 28, 1, \
			EXYNOS7420_MUX_STAT_TOPC0, 28, 3, 0, "mout_mfc_pll_cmuc"),
	MUX(mout_aud_pll_ctrl, "mout_aud_pll_ctrl", mout_aud_pll_ctrl_p, \
			EXYNOS7420_MUX_SEL_TOPC1, 0, 1, \
			EXYNOS7420_MUX_STAT_TOPC1, 0, 3, 0, NULL),
	MUX(none, "mout_sclk_bus0_pll_out", mout_sclk_bus0_pll_out_p, \
			EXYNOS7420_MUX_SEL_TOPC1, 16, 1, \
			EXYNOS7420_MUX_STAT_TOPC1, 16, 3, 0, NULL),
	MUX(none, "mout_sclk_bus_pll_g3d", topc_group0, \
			EXYNOS7420_MUX_SEL_TOPC1, 20, 2, \
			EXYNOS7420_MUX_STAT_TOPC1, 20, 4, 0, NULL),
	MUX(none, "mout_sclk_bus0_pll_mif", topc_group0, \
			EXYNOS7420_MUX_SEL_TOPC1, 12, 2, \
			EXYNOS7420_MUX_STAT_TOPC1, 12, 4, 0, "mout_sclk_bus0_pll_mif"),
	MUX(none, "mout_sclk_bus0_pll_apollo", topc_group0, \
			EXYNOS7420_MUX_SEL_TOPC1, 8, 2, \
			EXYNOS7420_MUX_STAT_TOPC1, 8, 4, 0, NULL),
	MUX(none, "mout_sclk_bus0_pll_atlas", topc_group0, \
			EXYNOS7420_MUX_SEL_TOPC1, 4, 2, \
			EXYNOS7420_MUX_STAT_TOPC1, 4, 4, 0, NULL),
	MUX(none, "mout_aclk_imem_100", topc_group1, \
			EXYNOS7420_MUX_SEL_TOPC2, 28, 2, \
			EXYNOS7420_MUX_STAT_TOPC2, 28, 4, 0, "mout_aclk_imem_100"),
	MUX(none, "mout_aclk_imem_200", topc_group1, \
			EXYNOS7420_MUX_SEL_TOPC2, 24, 2, \
			EXYNOS7420_MUX_STAT_TOPC2, 24, 4, 0, "mout_aclk_imem_200"),
	MUX(none, "mout_aclk_imem_266", topc_group1, \
			EXYNOS7420_MUX_SEL_TOPC2, 20, 2, \
			EXYNOS7420_MUX_STAT_TOPC2, 20, 4, 0, "mout_aclk_imem_266"),
	MUX(none, "mout_aclk_bus1_200", topc_group1, \
			EXYNOS7420_MUX_SEL_TOPC2, 16, 2, \
			EXYNOS7420_MUX_STAT_TOPC2, 16, 4, 0, "mout_aclk_bus1_200"),
	MUX(none, "mout_aclk_bus1_532", topc_group2, \
			EXYNOS7420_MUX_SEL_TOPC2, 12, 4, \
			EXYNOS7420_MUX_STAT_TOPC2, 12, 4, 0, "mout_aclk_bus1_532"),
	MUX(none, "mout_aclk_bus0_532", topc_group2, \
			EXYNOS7420_MUX_SEL_TOPC2, 8, 4, \
			EXYNOS7420_MUX_STAT_TOPC2, 8, 4, 0, "mout_aclk_bus0_532"),
	MUX(none, "mout_aclk_ccore_133", topc_group1, \
			EXYNOS7420_MUX_SEL_TOPC2, 4, 2, \
			EXYNOS7420_MUX_STAT_TOPC2, 4, 4, 0, "mout_aclk_ccore_133"),
	MUX(none, "mout_aclk_ccore_532", topc_group2, \
			EXYNOS7420_MUX_SEL_TOPC2, 0, 4, \
			EXYNOS7420_MUX_SEL_TOPC2, 0, 4, 0, "mout_aclk_ccore_532"),
	MUX(none, "mout_pclk_bus01_133", topc_group1, \
			EXYNOS7420_MUX_SEL_TOPC3, 28, 2, \
			EXYNOS7420_MUX_STAT_TOPC3, 28, 4, 0, "mout_pclk_bus01_133"),
	MUX(none, "mout_aclk_peris_66", topc_group1, \
			EXYNOS7420_MUX_SEL_TOPC3, 24, 2, \
			EXYNOS7420_MUX_STAT_TOPC3, 24, 4, 0, "mout_aclk_peris_66"),
	MUX(none, "mout_aclk_mscl_532", topc_group1, \
			EXYNOS7420_MUX_SEL_TOPC3, 20, 2, \
			EXYNOS7420_MUX_STAT_TOPC3, 20, 4, 0, "mout_aclk_mscl_532"),
	MUX(none, "mout_aclk_mfc_532", topc_group1, \
			EXYNOS7420_MUX_SEL_TOPC3, 8, 2, \
			EXYNOS7420_MUX_STAT_TOPC3, 8, 4, 0, "mout_aclk_mfc_532"),
	/* top0 block */
	MUX(none, "mout_bus0_pll_top0", mout_bus0_pll_top0_p, \
			EXYNOS7420_MUX_SEL_TOP01, 16, 1, \
			EXYNOS7420_MUX_STAT_TOP01, 16, 3, 0, "mout_bus0_pll_top0"),
	MUX(none, "mout_bus1_pll_top0", mout_bus1_pll_top0_p, \
			EXYNOS7420_MUX_SEL_TOP01, 12, 1, \
			EXYNOS7420_MUX_STAT_TOP01, 12, 3, 0, "mout_bus1_pll_top0"),
	MUX(none, "mout_cci_pll_top0", mout_cci_pll_top0_p, \
			EXYNOS7420_MUX_SEL_TOP01, 8, 1, \
			EXYNOS7420_MUX_STAT_TOP01, 8, 3, 0, "mout_cci_pll_top0"),
	MUX(none, "mout_mfc_pll_top0", mout_mfc_pll_top0_p, \
			EXYNOS7420_MUX_SEL_TOP01, 4, 1, \
			EXYNOS7420_MUX_STAT_TOP01, 4, 3, 0, "mout_mfc_pll_top0"),
	MUX(none, "mout_aclk_peric0_66", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP03, 20, 2, \
			EXYNOS7420_MUX_STAT_TOP03, 20, 4, 0, "mout_aclk_peric0_66"),
	MUX(none, "mout_aclk_peric1_66", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP03, 12, 2, \
			EXYNOS7420_MUX_STAT_TOP03, 12, 4, 0, "mout_aclk_peric1_66"),
	MUX(none, "mout_aclk_vpp0_400", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP03, 8, 2, \
			EXYNOS7420_MUX_STAT_TOP03, 8, 4, 0, "mout_aclk_vpp0_400"),
	MUX(none, "mout_aclk_vpp1_400", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP03, 4, 2, \
			EXYNOS7420_MUX_STAT_TOP03, 4, 4, 0, "mout_aclk_vpp1_400"),
	MUX(none, "mout_aclk_disp_400", mout_aclk_disp_400_p, \
			EXYNOS7420_MUX_SEL_TOP03, 28, 3, \
			EXYNOS7420_MUX_STAT_TOP03, 28, 4, 0, "mout_aclk_disp_400"),
	/* top0 block isp and cam */
	MUX(mout_aclk_isp0_isp0_590, "mout_aclk_isp0_isp0_590", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP04, 28, 3, \
			EXYNOS7420_MUX_STAT_TOP04, 28, 4, 0, "mout_aclk_isp0_isp0_590"),
	MUX(mout_aclk_isp0_tpu_590, "mout_aclk_isp0_tpu_590", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP04, 24, 3, \
			EXYNOS7420_MUX_STAT_TOP04, 24, 4, 0, "mout_aclk_isp0_tpu_590"),
	MUX(mout_aclk_isp0_trex_532, "mout_aclk_isp0_trex_532", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP04, 20, 3, \
			EXYNOS7420_MUX_STAT_TOP04, 20, 4, 0, "mout_aclk_isp0_trex_532"),
	MUX(mout_aclk_isp1_isp1_468, "mout_aclk_isp1_isp1_468", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP04, 16, 3, \
			EXYNOS7420_MUX_STAT_TOP04, 16, 4, 0, "mout_aclk_isp1_isp1_468"),
	MUX(mout_aclk_isp1_ahb_117, "mout_aclk_isp1_ahb_117", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP04, 12, 3, \
			EXYNOS7420_MUX_STAT_TOP04, 12, 4, 0, "mout_aclk_isp1_ahb_117"),
	MUX(mout_aclk_cam0_csis0_690, "mout_aclk_cam0_csis0_690", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP05, 28, 3, \
			EXYNOS7420_MUX_STAT_TOP05, 28, 4, 0, "mout_aclk_cam0_csis0_690"),
	MUX(mout_aclk_cam0_bnsa_690, "mout_aclk_cam0_bnsa_690", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP05, 24, 3, \
			EXYNOS7420_MUX_STAT_TOP05, 24, 4, 0, "mout_aclk_cam0_bnsa_690"),
	MUX(mout_aclk_cam0_bnsb_690, "mout_aclk_cam0_bnsb_690", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP05, 20, 3, \
			EXYNOS7420_MUX_STAT_TOP05, 20, 4, 0, "mout_aclk_cam0_bnsb_690"),
	MUX(mout_aclk_cam0_bnsd_690, "mout_aclk_cam0_bnsd_690", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP05, 16, 3, \
			EXYNOS7420_MUX_STAT_TOP05, 16, 4, 0, "mout_aclk_cam0_bnsd_690"),
	MUX(mout_aclk_cam0_csis1_174, "mout_aclk_cam0_csis1_174", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP05, 12, 3, \
			EXYNOS7420_MUX_STAT_TOP05, 12, 4, 0, "mout_aclk_cam0_csis1_174"),
	MUX(none, "mout_aclk_cam0_csis3_133", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP05, 8, 3, \
			EXYNOS7420_MUX_STAT_TOP05, 8, 4, 0, "mout_aclk_cam0_csis3_133"),
	MUX(mout_aclk_cam0_3aa0_690, "mout_aclk_cam0_3aa0_690", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP06, 28, 3, \
			EXYNOS7420_MUX_STAT_TOP06, 28, 4, 0, "mout_aclk_cam0_3aa0_690"),
	MUX(mout_aclk_cam0_3aa1_468, "mout_aclk_cam0_3aa1_468", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP06, 24, 3, \
			EXYNOS7420_MUX_STAT_TOP06, 24, 4, 0, "mout_aclk_cam0_3aa1_468"),
	MUX(mout_aclk_cam0_trex_532, "mout_aclk_cam0_trex_532", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP06, 20, 3, \
			EXYNOS7420_MUX_STAT_TOP06, 20, 4, 0, "mout_aclk_cam0_trex_532"),
	MUX(mout_aclk_cam0_nocp_133, "mout_aclk_cam0_nocp_133", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP06, 16, 3, \
			EXYNOS7420_MUX_STAT_TOP06, 16, 4, 0, "mout_aclk_cam0_nocp_133"),
	MUX(mout_aclk_cam1_trex_532, "mout_aclk_cam1_trex_532", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP07, 8, 3, \
			EXYNOS7420_MUX_STAT_TOP07, 8, 4, 0, "mout_aclk_cam1_trex_532"),
	MUX(mout_aclk_cam1_nocp_133, "mout_aclk_cam1_nocp_133", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP07, 12, 3, \
			EXYNOS7420_MUX_STAT_TOP07, 12, 4, 0, "mout_aclk_cam1_nocp_133"),
	MUX(mout_aclk_cam1_bnscsis_133, "mout_aclk_cam1_bnscsis_133", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP07, 16, 3, \
			EXYNOS7420_MUX_STAT_TOP07, 16, 4, 0, "mout_aclk_cam1_bnscsis_133"),
	MUX(mout_aclk_cam1_busperi_334, "mout_aclk_cam1_busperi_334", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP07, 20, 3, \
			EXYNOS7420_MUX_STAT_TOP07, 20, 4, 0, "mout_aclk_cam1_busperi_334"),
	MUX(mout_aclk_cam1_arm_668, "mout_aclk_cam1_arm_668", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP07, 24, 3, \
			EXYNOS7420_MUX_STAT_TOP07, 24, 4, 0, "mout_aclk_cam1_arm_668"),
	MUX(mout_aclk_cam1_sclvra_491, "mout_aclk_cam1_sclvra_491", top0_group2, \
			EXYNOS7420_MUX_SEL_TOP07, 28, 3, \
			EXYNOS7420_MUX_STAT_TOP07, 28, 4, 0, "mout_aclk_cam1_sclvra_491"),
	/* top0 block sclk */
	MUX(none, "mout_sclk_uart0", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC3, 16, 2, \
			EXYNOS7420_MUX_STAT_TOP0_PERIC3, 16, 4, 0, NULL),
	MUX(none, "mout_sclk_uart1", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC3, 12, 2, \
			EXYNOS7420_MUX_STAT_TOP0_PERIC3, 12, 4, 0, NULL),
	MUX(none, "mout_sclk_uart2", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC3, 8, 2, \
			EXYNOS7420_MUX_STAT_TOP0_PERIC3, 8, 4, 0, NULL),
	MUX(none, "mout_sclk_uart3", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC3, 4, 2, \
			EXYNOS7420_MUX_STAT_TOP0_PERIC3, 4, 4, 0, NULL),
	MUX(none, "mout_sclk_decon_int_eclk", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_DISP, 28, 2, \
			EXYNOS7420_MUX_STAT_TOP0_DISP, 28, 4, 0, "m_sclk_decon0_eclk"),
	MUX(none, "mout_sclk_decon_ext_eclk", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_DISP, 24, 2, \
			EXYNOS7420_MUX_STAT_TOP0_DISP, 24, 4, 0, NULL),
	MUX(none, "mout_sclk_decon_vclk", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_DISP, 20, 2, \
			EXYNOS7420_MUX_STAT_TOP0_DISP, 20, 4, 0, NULL),
	MUX(none, "mout_sclk_dsd", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_DISP, 16, 2, \
			EXYNOS7420_MUX_STAT_TOP0_DISP, 16, 4, 0, NULL),
	MUX(none, "mout_sclk_hdmi_spdif", mout_spdif_p, \
			EXYNOS7420_MUX_SEL_TOP0_DISP, 12, 3, \
			EXYNOS7420_MUX_STAT_TOP0_DISP, 12, 4, 0, NULL),
	MUX(mout_sclk_isp_uart, "mout_sclk_isp_uart", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_CAM10, 4, 2, \
			EXYNOS7420_MUX_STAT_TOP0_CAM10, 4, 4, 0, NULL),
	MUX(mout_sclk_isp_spi1, "mout_sclk_isp_spi1", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_CAM10, 8, 2, \
			EXYNOS7420_MUX_STAT_TOP0_CAM10, 8, 4, 0, NULL),
	MUX(mout_sclk_isp_spi0, "mout_sclk_isp_spi0", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_CAM10, 20, 2, \
			EXYNOS7420_MUX_STAT_TOP0_CAM10, 20, 4, 0, NULL),
	MUX(mout_sclk_isp_sensor2, "mout_sclk_isp_sensor2", mout_sclk_isp_sensor_p, \
			EXYNOS7420_MUX_SEL_TOP0_CAM11, 0, 2, \
			EXYNOS7420_MUX_STAT_TOP0_CAM11, 0, 4, 0, NULL),
	MUX(mout_sclk_isp_sensor1, "mout_sclk_isp_sensor1", mout_sclk_isp_sensor_p, \
			EXYNOS7420_MUX_SEL_TOP0_CAM11, 8, 2, \
			EXYNOS7420_MUX_STAT_TOP0_CAM11, 8, 4, 0, NULL),
	MUX(mout_sclk_isp_sensor0, "mout_sclk_isp_sensor0", mout_sclk_isp_sensor_p, \
			EXYNOS7420_MUX_SEL_TOP0_CAM11, 16, 2, \
			EXYNOS7420_MUX_STAT_TOP0_CAM11, 16, 4, 0, NULL),
	MUX(mout_sclk_spdif, "mout_sclk_spdif", mout_spdif_p, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC0, 4, 3, \
			0, 0, 0, 0, NULL),
	MUX(mout_sclk_pcm1, "mout_sclk_pcm1", top0_group3, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC0, 8, 2, \
			0, 0, 0, 0, NULL),
	MUX(mout_sclk_i2s1, "mout_sclk_i2s1", top0_group3, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC0, 20, 2, \
			0, 0, 0, 0, NULL),
	MUX(none, "mout_sclk_spi0", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC1, 20, 2, \
			EXYNOS7420_MUX_STAT_TOP0_PERIC1, 20, 4, 0, NULL),
	MUX(none, "mout_sclk_spi1", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC1, 8, 2, \
			EXYNOS7420_MUX_STAT_TOP0_PERIC1, 8, 4, 0, NULL),
	MUX(none, "mout_sclk_spi2", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC2, 20, 2, \
			EXYNOS7420_MUX_STAT_TOP0_PERIC2, 20, 4, 0, NULL),
	MUX(none, "mout_sclk_spi3", top0_group1,\
			EXYNOS7420_MUX_SEL_TOP0_PERIC2, 8, 2, \
			EXYNOS7420_MUX_STAT_TOP0_PERIC2, 8, 4, 0, NULL),
	MUX(none, "mout_sclk_spi4", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC3, 20, 2, \
			EXYNOS7420_MUX_STAT_TOP0_PERIC3, 20, 4, 0, NULL),
	MUX(none, "mout_sclk_spi5", top0_group1, \
			EXYNOS7420_MUX_SEL_TOP0_PERIC4, 20, 2, \
			EXYNOS7420_MUX_STAT_TOP0_PERIC4, 20, 4, 0, NULL),
	/* top1 block */
	MUX(none, "mout_bus0_pll_top1", mout_bus0_pll_top1_p, \
			EXYNOS7420_MUX_SEL_TOP11, 16, 1, \
			EXYNOS7420_MUX_STAT_TOP11, 16, 3, 0, "mout_bus0_pll_top1"),
	MUX(none, "mout_bus1_pll_top1", mout_bus1_pll_top1_p, \
			EXYNOS7420_MUX_SEL_TOP11, 12, 1, \
			EXYNOS7420_MUX_STAT_TOP11, 12, 3, 0, "mout_bus1_pll_top1"),
	MUX(none, "mout_cci_pll_top1", mout_cci_pll_top1_p, \
			EXYNOS7420_MUX_SEL_TOP11, 8, 1, \
			EXYNOS7420_MUX_STAT_TOP11, 8, 3, 0, "mout_cci_pll_top1"),
	MUX(none, "mout_mfc_pll_top1", mout_mfc_pll_top1_p, \
			EXYNOS7420_MUX_SEL_TOP11, 4, 1, \
			EXYNOS7420_MUX_STAT_TOP11, 4, 3, 0, "mout_mfc_pll_top1"),
	MUX(none, "mout_aclk_fsys0_200", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP13, 28, 2, \
			EXYNOS7420_MUX_STAT_TOP13, 28, 4, 0, "mout_aclk_fsys0_200"),
	MUX(none, "mout_aclk_fsys1_200", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP13, 24, 2, \
			EXYNOS7420_MUX_STAT_TOP13, 24, 4, 0, "mout_aclk_fsys1_200"),
	MUX(none, "mout_sclk_mmc2", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP1_FSYS0, 16, 2, \
			EXYNOS7420_MUX_STAT_TOP1_FSYS0, 16, 4, 0, NULL),
	MUX(none, "mout_sclk_mmc1", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP1_FSYS11, 0, 2, \
			EXYNOS7420_MUX_STAT_TOP1_FSYS11, 0, 4, 0, NULL),
	MUX(none, "mout_sclk_mmc0", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP1_FSYS11, 12, 2, \
			EXYNOS7420_MUX_STAT_TOP1_FSYS11, 12, 4, 0, NULL),
	MUX(none, "mout_sclk_phy_fsys0_26m", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP1_FSYS0, 0, 2, \
			EXYNOS7420_MUX_STAT_TOP1_FSYS0, 0, 4, 0, NULL),
	MUX(none, "mout_sclk_phy_fsys1_26m", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP1_FSYS11, 24, 2, \
			EXYNOS7420_MUX_STAT_TOP1_FSYS11, 24, 4, 0, NULL),
	MUX(none, "mout_sclk_phy_fsys0", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP1_FSYS0, 8, 2, \
			EXYNOS7420_MUX_STAT_TOP1_FSYS0, 8, 4, 0, NULL),
	MUX(none, "mout_sclk_phy_fsys1", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP1_FSYS1, 0, 2, \
			EXYNOS7420_MUX_STAT_TOP1_FSYS1, 0, 4, 0, NULL),
	MUX(none, "mout_sclk_usbdrd300", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP1_FSYS0, 28, 2, \
			EXYNOS7420_MUX_STAT_TOP1_FSYS0, 28, 4, 0, NULL),
	MUX(none, "mout_sclk_tlx400_wifi1", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP1_FSYS1, 8, 2, \
			EXYNOS7420_MUX_STAT_TOP1_FSYS1, 8, 4, 0, NULL),
	MUX(none, "mout_sclk_ufsunipro20", top1_group1, \
			EXYNOS7420_MUX_SEL_TOP1_FSYS1, 16, 2, \
			EXYNOS7420_MUX_STAT_TOP1_FSYS1, 16, 4, 0, NULL),
	/* aud block */
	MUX(mout_sclk_pcm, "mout_sclk_pcm", mout_sclk_pcm_p, \
			EXYNOS7420_MUX_SEL_AUD, 16, 1,
			0, 0, 0, 0, "mout_sclk_pcm"),
	MUX(mout_sclk_i2s, "mout_sclk_i2s", mout_sclk_i2s_p, \
			EXYNOS7420_MUX_SEL_AUD, 12, 1,
			0, 0, 0, 0, "mout_sclk_i2s"),
	/* g3d block */
	MUX(mout_g3d, "mout_g3d", mout_g3d_p, \
			EXYNOS7420_MUX_SEL_G3D, 8, 1, \
			EXYNOS7420_MUX_STAT_G3D, 8, 3, 0, NULL),
	/* disp block */
	MUX(m_decon0_eclk, "mout_sub_sclk_decon_int_eclk", mout_sub_sclk_decon_int_eclk_p, \
			EXYNOS7420_MUX_SEL_DISP3, 20, 1, \
			EXYNOS7420_MUX_STAT_DISP3, 20, 3, 0, NULL),
	MUX(m_decon0_vclk, "mout_sub_sclk_decon_int_vclk", mout_sub_sclk_decon_vclk_p, \
			EXYNOS7420_MUX_SEL_DISP3, 12, 1, \
			EXYNOS7420_MUX_STAT_DISP3, 12, 3, 0, NULL),
	MUX(m_decon1_eclk, "mout_sub_sclk_decon_ext_eclk", mout_sub_sclk_decon_ext_eclk_p, \
			EXYNOS7420_MUX_SEL_DISP3, 16, 1, \
			EXYNOS7420_MUX_STAT_DISP3, 16, 3, 0, NULL),
	MUX(um_decon1_vclk, "mout_sub_sclk_decon_ext_vclk", mout_sub_sclk_decon_vclk_p, \
			EXYNOS7420_MUX_SEL_DISP3, 8, 1, \
			EXYNOS7420_MUX_STAT_DISP3, 8, 3, 0, NULL),
	MUX(m_decon1_vclk, "mout_decon_ext_vclk", mout_decon_ext_vclk_p, \
			EXYNOS7420_MUX_SEL_DISP5, 28, 1, \
			0, 0, 0, 0, NULL),
	/* mif */
	MUX(none, "mout_aclk_mif_pll", mout_aclk_mif_pll_p, \
			EXYNOS7420_MUX_SEL_TOPC5, 20, 1,
			0, 0, 0, CLK_IGNORE_UNUSED, "mout_aclk_mif_pll"),
	MUX(none, "mout_cell_clksel",  mout_cell_clksel_p, \
			EXYNOS7420_MUX_SEL_TOPC_LP4_PHY_DIV, 0, 1,
			0, 0, 0, 0, "mout_cell_clksel"),
	MUX(none, "mout_pclk_mif0",  mout_pclk_mif_p, \
			EXYNOS7420_MUX_SEL_MIF0, 4, 2,
			EXYNOS7420_MUX_STAT_MIF0, 4, 4, 0, "mout_pclk_mif0"),
	MUX(none, "mout_pclk_mif1",  mout_pclk_mif_p, \
			EXYNOS7420_MUX_SEL_MIF1, 4, 2,
			EXYNOS7420_MUX_STAT_MIF1, 4, 4, 0, "mout_pclk_mif1"),
	MUX(none, "mout_pclk_mif2",  mout_pclk_mif_p, \
			EXYNOS7420_MUX_SEL_MIF2, 4, 2,
			EXYNOS7420_MUX_STAT_MIF2, 4, 4, 0, "mout_pclk_mif2"),
	MUX(none, "mout_pclk_mif3",  mout_pclk_mif_p, \
			EXYNOS7420_MUX_SEL_MIF3, 4, 2,
			EXYNOS7420_MUX_STAT_MIF3, 4, 4, 0, "mout_pclk_mif3"),
	/* atlas block */
	MUX(none, "mout_atlas", mout_atlas_p, \
			EXYNOS7420_MUX_SEL_ATLAS2, 0, 1, \
			EXYNOS7420_MUX_STAT_ATLAS2, 0, 3, 0, "mout_atlas"),
	/* apollo block */
	MUX(none, "mout_apollo", mout_apollo_p, \
			EXYNOS7420_MUX_SEL_APOLLO2, 0, 1, \
			EXYNOS7420_MUX_STAT_APOLLO2, 0, 3, 0, "mout_apollo"),
	/* fsys0 block */
	MUX(none, "mout_fsys0_phyclk_sel0", mout_fsys0_phyclk_sel0_p, \
			EXYNOS7420_MUX_SEL_FSYS00, 20, 2, \
			0, 0, 0, 0, NULL),
	/* fsys1 block */
	MUX(none, "mout_fsys1_phyclk_sel0", mout_fsys1_phyclk_sel0_p, \
			EXYNOS7420_MUX_SEL_FSYS10, 20, 2, \
			0, 0, 0, 0, NULL),
	MUX(mout_fsys1_phyclk_sel1, "mout_fsys1_phyclk_sel1", mout_fsys1_phyclk_sel1_p, \
			EXYNOS7420_MUX_SEL_FSYS10, 16, 2, \
			0, 0, 0, 0, NULL),
};

static struct samsung_composite_divider exynos7420_div_clks[] __refdata = {
	/* topc block */
	DIV(dout_sclk_aud_pll, "dout_sclk_aud_pll", "aud_pll", \
			EXYNOS7420_DIV_TOPC3, 28, 4, \
			EXYNOS7420_DIV_STAT_TOPC3, 28, 1, 0, NULL),
	DIV(none, "dout_sclk_bus0_pll", "mout_sclk_bus0_pll_out", \
			EXYNOS7420_DIV_TOPC3, 0, 4, \
			EXYNOS7420_DIV_STAT_TOPC3, 0, 1, 0, NULL),
	DIV(none, "dout_sclk_bus1_pll", "bus1_pll", \
			EXYNOS7420_DIV_TOPC3, 8, 4, \
			EXYNOS7420_DIV_STAT_TOPC3, 8, 1, 0, NULL),
	DIV(none, "dout_sclk_cci_pll", "cci_pll", \
			EXYNOS7420_DIV_TOPC3, 12, 4, \
			EXYNOS7420_DIV_STAT_TOPC3, 12, 1, 0, NULL),
	DIV(none, "dout_sclk_mfc_pll", "mfc_pll", \
			EXYNOS7420_DIV_TOPC3, 16, 4, \
			EXYNOS7420_DIV_STAT_TOPC3, 16, 1, 0, NULL),
	DIV(none, "dout_sclk_bus_pll_g3d", "mout_sclk_bus_pll_g3d", \
			EXYNOS7420_DIV_TOPC3, 20, 4, \
			EXYNOS7420_DIV_STAT_TOPC3, 20, 1, 0, NULL),
	DIV(none, "dout_aclk_imem_100", "mout_aclk_imem_100", \
			EXYNOS7420_DIV_TOPC0, 28, 4, \
			EXYNOS7420_DIV_STAT_TOPC0, 28, 1, 0, "dout_aclk_imem_100"),
	DIV(none, "dout_aclk_imem_200", "mout_aclk_imem_200", \
			EXYNOS7420_DIV_TOPC0, 24, 4, \
			EXYNOS7420_DIV_STAT_TOPC0, 24, 1, 0, "dout_aclk_imem_200"),
	DIV(none, "dout_aclk_imem_266", "mout_aclk_imem_266", \
			EXYNOS7420_DIV_TOPC0, 20, 4, \
			EXYNOS7420_DIV_STAT_TOPC0, 20, 1, 0, "dout_aclk_imem_266"),
	DIV(none, "dout_aclk_bus1_200", "mout_aclk_bus1_200", \
			EXYNOS7420_DIV_TOPC0, 16, 4, \
			EXYNOS7420_DIV_STAT_TOPC0, 16, 1, 0, "dout_aclk_bus1_200"),
	DIV(none, "dout_aclk_bus1_532", "mout_aclk_bus1_532", \
			EXYNOS7420_DIV_TOPC0, 12, 4, \
			EXYNOS7420_DIV_STAT_TOPC0, 12, 1, 0, "dout_aclk_bus1_532"),
	DIV(none, "dout_aclk_bus0_532", "mout_aclk_bus0_532", \
			EXYNOS7420_DIV_TOPC0, 8, 4, \
			EXYNOS7420_DIV_STAT_TOPC0, 8, 1, 0, "dout_aclk_bus0_532"),
	DIV(none, "dout_aclk_ccore_133", "mout_aclk_ccore_133", \
			EXYNOS7420_DIV_TOPC0, 4, 4, \
			EXYNOS7420_DIV_STAT_TOPC0, 4, 1, 0, "dout_aclk_ccore_133"),
	DIV(none, "dout_aclk_ccore_266", "usermux_aclk_ccore_532", \
			EXYNOS7420_DIV_CCORE, 0, 3, \
			EXYNOS7420_DIV_STAT_CCORE, 0, 1, 0, "dout_aclk_ccore_266"),
	DIV(none, "dout_aclk_ccore_532", "mout_aclk_ccore_532", \
			EXYNOS7420_DIV_TOPC0, 0, 4, \
			EXYNOS7420_DIV_STAT_TOPC0, 0, 1, 0, "dout_aclk_ccore_532"),
	DIV(none, "dout_pclk_bus01_133", "mout_pclk_bus01_133", \
			EXYNOS7420_DIV_TOPC1, 28, 4, \
			EXYNOS7420_DIV_STAT_TOPC1, 28, 1, 0, "dout_pclk_bus01_133"),
	DIV(none, "dout_aclk_peris_66", "mout_aclk_peris_66", \
			EXYNOS7420_DIV_TOPC1, 24, 4, \
			EXYNOS7420_DIV_STAT_TOPC1, 24, 1, 0, "dout_aclk_peris_66"),
	DIV(none, "dout_aclk_mscl_532", "mout_aclk_mscl_532", \
			EXYNOS7420_DIV_TOPC1, 20, 4, \
			EXYNOS7420_DIV_STAT_TOPC1, 20, 1, 0, "dout_aclk_mscl_532"),
	DIV(dout_aclk_mfc_532, "dout_aclk_mfc_532", "mout_aclk_mfc_532", \
			EXYNOS7420_DIV_TOPC1, 8, 4, \
			EXYNOS7420_DIV_STAT_TOPC1, 8, 1, 0, "dout_aclk_mfc_532"),
	/* top0 block */
	DIV(none, "dout_sclk_bus0_pll_mif", "mout_sclk_bus0_pll_mif", \
			EXYNOS7420_DIV_TOPC3, 4, 3, \
			EXYNOS7420_DIV_STAT_TOP03, 4, 1, 0, "dout_sclk_bus0_pll_mif" ),
	DIV(none, "dout_aclk_disp_400", "mout_aclk_disp_400", \
			EXYNOS7420_DIV_TOP03, 28, 4, \
			EXYNOS7420_DIV_STAT_TOP03, 28, 1, 0, "dout_aclk_disp_400"),
	DIV(none, "dout_aclk_peric0_66", "mout_aclk_peric0_66", \
			EXYNOS7420_DIV_TOP03, 20, 6, \
			EXYNOS7420_DIV_STAT_TOP03, 20, 1, 0, "dout_aclk_peric0_66"),
	DIV(none, "dout_aclk_peric1_66", "mout_aclk_peric1_66", \
			EXYNOS7420_DIV_TOP03, 12, 6, \
			EXYNOS7420_DIV_STAT_TOP03, 12, 1, 0, "dout_aclk_peric1_66"),
	DIV(none, "dout_aclk_vpp0_400", "mout_aclk_vpp0_400", \
			EXYNOS7420_DIV_TOP03, 8, 4, \
			EXYNOS7420_DIV_STAT_TOP03, 8, 1, 0, "dout_aclk_vpp0_400"),
	DIV(none, "dout_aclk_vpp1_400", "mout_aclk_vpp1_400", \
			EXYNOS7420_DIV_TOP03, 4, 4, \
			EXYNOS7420_DIV_STAT_TOP03, 4, 1, 0, "dout_aclk_vpp1_400"),
	/* top0 block isp and cam */
	DIV(dout_aclk_isp0_isp0_590, "dout_aclk_isp0_isp0_590", "mout_aclk_isp0_isp0_590", \
			EXYNOS7420_DIV_TOP04, 28, 4, \
			EXYNOS7420_DIV_STAT_TOP04, 28, 1, 0, "dout_aclk_isp0_isp0_590"),
	DIV(dout_aclk_isp0_tpu_590, "dout_aclk_isp0_tpu_590", "mout_aclk_isp0_tpu_590", \
			EXYNOS7420_DIV_TOP04, 24, 4, \
			EXYNOS7420_DIV_STAT_TOP04, 24, 1, 0, "dout_aclk_isp0_tpu_590"),
	DIV(dout_aclk_isp0_trex_532, "dout_aclk_isp0_trex_532", "mout_aclk_isp0_trex_532", \
			EXYNOS7420_DIV_TOP04, 20, 4, \
			EXYNOS7420_DIV_STAT_TOP04, 20, 1, 0, "dout_aclk_isp0_trex_532"),
	DIV(dout_aclk_isp1_isp1_468, "dout_aclk_isp1_isp1_468", "mout_aclk_isp1_isp1_468", \
			EXYNOS7420_DIV_TOP04, 16, 4, \
			EXYNOS7420_DIV_STAT_TOP04, 16, 1, 0, "dout_aclk_isp1_isp1_468"),
	DIV(dout_aclk_isp1_ahb_117, "dout_aclk_isp1_ahb_117", "mout_aclk_isp1_ahb_117", \
			EXYNOS7420_DIV_TOP04, 12, 4, \
			EXYNOS7420_DIV_STAT_TOP04, 12, 1, 0, "dout_aclk_isp1_ahb_117"),
	DIV(dout_aclk_cam0_csis0_690, "dout_aclk_cam0_csis0_690", "mout_aclk_cam0_csis0_690", \
			EXYNOS7420_DIV_TOP05, 28, 4, \
			EXYNOS7420_DIV_STAT_TOP05, 28, 1, 0, "dout_aclk_cam0_csis0_690"),
	DIV(dout_aclk_cam0_bnsa_690, "dout_aclk_cam0_bnsa_690", "mout_aclk_cam0_bnsa_690", \
			EXYNOS7420_DIV_TOP05, 24, 4, \
			EXYNOS7420_DIV_STAT_TOP05, 24, 1, 0, "dout_aclk_cam0_bnsa_690"),
	DIV(dout_aclk_cam0_bnsb_690, "dout_aclk_cam0_bnsb_690", "mout_aclk_cam0_bnsb_690", \
			EXYNOS7420_DIV_TOP05, 20, 4, \
			EXYNOS7420_DIV_STAT_TOP05, 20, 1, 0, "dout_aclk_cam0_bnsb_690"),
	DIV(dout_aclk_cam0_bnsd_690, "dout_aclk_cam0_bnsd_690", "mout_aclk_cam0_bnsd_690", \
			EXYNOS7420_DIV_TOP05, 16, 4, \
			EXYNOS7420_DIV_STAT_TOP05, 16, 1, 0, "dout_aclk_cam0_bnsd_690"),
	DIV(dout_aclk_cam0_csis1_174, "dout_aclk_cam0_csis1_174", "mout_aclk_cam0_csis1_174", \
			EXYNOS7420_DIV_TOP05, 12, 4, \
			EXYNOS7420_DIV_STAT_TOP05, 12, 1, 0, "dout_aclk_cam0_csis1_174"),
	DIV(none, "dout_aclk_cam0_csis3_133", "mout_aclk_cam0_csis3_133", \
			EXYNOS7420_DIV_TOP05, 8, 4, \
			EXYNOS7420_DIV_STAT_TOP05, 8, 1, 0, "dout_aclk_cam0_csis3_133"),
	DIV(dout_aclk_cam0_3aa0_690, "dout_aclk_cam0_3aa0_690", "mout_aclk_cam0_3aa0_690", \
			EXYNOS7420_DIV_TOP06, 28, 4, \
			EXYNOS7420_DIV_STAT_TOP06, 28, 1, 0, "dout_aclk_cam0_3aa0_690"),
	DIV(dout_aclk_cam0_3aa1_468, "dout_aclk_cam0_3aa1_468", "mout_aclk_cam0_3aa1_468", \
			EXYNOS7420_DIV_TOP06, 24, 4, \
			EXYNOS7420_DIV_STAT_TOP06, 24, 1, 0, "dout_aclk_cam0_3aa1_468"),
	DIV(dout_aclk_cam0_trex_532, "dout_aclk_cam0_trex_532", "mout_aclk_cam0_trex_532", \
			EXYNOS7420_DIV_TOP06, 20, 4, \
			EXYNOS7420_DIV_STAT_TOP06, 20, 1, 0, "dout_aclk_cam0_trex_532"),
	DIV(dout_aclk_cam0_nocp_133, "dout_aclk_cam0_nocp_133", "mout_aclk_cam0_nocp_133", \
			EXYNOS7420_DIV_TOP06, 16, 4, \
			EXYNOS7420_DIV_STAT_TOP06, 16, 1, 0, "dout_aclk_cam0_nocp_133"),
	DIV(dout_aclk_cam1_trex_532, "dout_aclk_cam1_trex_532", "mout_aclk_cam1_trex_532", \
			EXYNOS7420_DIV_TOP07, 8, 4, \
			EXYNOS7420_DIV_STAT_TOP07, 8, 1, 0, "dout_aclk_cam1_trex_532"),
	DIV(dout_aclk_cam1_nocp_133, "dout_aclk_cam1_nocp_133", "mout_aclk_cam1_nocp_133", \
			EXYNOS7420_DIV_TOP07, 12, 4, \
			EXYNOS7420_DIV_STAT_TOP07, 12, 1, 0, "dout_aclk_cam1_nocp_133"),
	DIV(dout_aclk_cam1_bnscsis_133, "dout_aclk_cam1_bnscsis_133", "mout_aclk_cam1_bnscsis_133", \
			EXYNOS7420_DIV_TOP07, 16, 4, \
			EXYNOS7420_DIV_STAT_TOP07, 16, 1, 0, "dout_aclk_cam1_bnscsis_133"),
	DIV(dout_aclk_cam1_busperi_334, "dout_aclk_cam1_busperi_334", "mout_aclk_cam1_busperi_334", \
			EXYNOS7420_DIV_TOP07, 20, 4, \
			EXYNOS7420_DIV_STAT_TOP07, 20, 1, 0, "dout_aclk_cam1_busperi_334"),
	DIV(dout_aclk_cam1_arm_668, "dout_aclk_cam1_arm_668", "mout_aclk_cam1_arm_668", \
			EXYNOS7420_DIV_TOP07, 24, 4,
			EXYNOS7420_DIV_STAT_TOP07, 24, 1, 0, "dout_aclk_cam1_arm_668"),
	DIV(dout_aclk_cam1_sclvra_491, "dout_aclk_cam1_sclvra_491", "mout_aclk_cam1_sclvra_491", \
			EXYNOS7420_DIV_TOP07, 28, 4, \
			EXYNOS7420_DIV_STAT_TOP07, 28, 1, 0, "dout_aclk_cam1_sclvra_491"),
	/* top0 block sclk */
	DIV(baud0, "dout_sclk_uart0", "mout_sclk_uart0", \
			EXYNOS7420_DIV_TOP0_PERIC3, 16, 4, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC3, 16, 1, 0, NULL),
	DIV(baud1, "dout_sclk_uart1", "mout_sclk_uart1", \
			EXYNOS7420_DIV_TOP0_PERIC3, 12, 4, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC3, 12, 1, 0, NULL),
	DIV(baud2, "dout_sclk_uart2", "mout_sclk_uart2", \
			EXYNOS7420_DIV_TOP0_PERIC3, 8, 4, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC3, 8, 1, 0, NULL),
	DIV(baud3, "dout_sclk_uart3", "mout_sclk_uart3", \
			EXYNOS7420_DIV_TOP0_PERIC3, 4, 4, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC3, 4, 1, 0, NULL),
	DIV(none, "dout_sclk_decon_int_eclk", "mout_sclk_decon_int_eclk", \
			EXYNOS7420_DIV_TOP0_DISP, 28, 4, \
			EXYNOS7420_DIV_STAT_TOP0_DISP, 28, 1, 0, "dout_sclk_decon_int_eclk"),
	DIV(none, "dout_sclk_decon_ext_eclk", "mout_sclk_decon_ext_eclk", \
			EXYNOS7420_DIV_TOP0_DISP, 24, 4, \
			EXYNOS7420_DIV_STAT_TOP0_DISP, 24, 1, 0, "dout_sclk_decon_ext_eclk"),
	DIV(none, "dout_sclk_decon_vclk", "mout_sclk_decon_vclk", \
			EXYNOS7420_DIV_TOP0_DISP, 20, 4, \
			EXYNOS7420_DIV_STAT_TOP0_DISP, 20, 1, 0, "dout_sclk_decon_vclk"),
	DIV(none, "dout_sclk_dsd", "mout_sclk_dsd", \
			EXYNOS7420_DIV_TOP0_DISP, 16, 4, \
			EXYNOS7420_DIV_STAT_TOP0_DISP, 16, 1, 0, "dout_sclk_dsd"),
	DIV(none, "dout_sclk_hdmi_spdif", "mout_sclk_hdmi_spdif", \
			EXYNOS7420_DIV_TOP0_DISP, 12, 4, \
			EXYNOS7420_DIV_STAT_TOP0_DISP, 12, 1, 0, "dout_sclk_hdmi_spdif"),
	DIV(dout_sclk_isp_uart, "dout_sclk_isp_uart", "mout_sclk_isp_uart", \
			EXYNOS7420_DIV_TOP0_CAM10, 4, 4, \
			EXYNOS7420_DIV_STAT_TOP0_CAM10, 4, 1, 0, NULL),
	DIV(dout_sclk_isp_spi1, "dout_sclk_isp_spi1", "mout_sclk_isp_spi1", \
			EXYNOS7420_DIV_TOP0_CAM10, 8, 12, \
			EXYNOS7420_DIV_STAT_TOP0_CAM10, 8, 1, 0, NULL),
	DIV(dout_sclk_isp_spi0, "dout_sclk_isp_spi0", "mout_sclk_isp_spi0", \
			EXYNOS7420_DIV_TOP0_CAM10, 20, 12, \
			EXYNOS7420_DIV_STAT_TOP0_CAM10, 20, 1, 0, NULL),
	/* top0 block for external sensor */
	DIV(dout_sclk_isp_sensor2, "dout_sclk_isp_sensor2", "mout_sclk_isp_sensor2", \
			EXYNOS7420_DIV_TOP0_CAM11, 0, 8, \
			EXYNOS7420_DIV_STAT_TOP0_CAM11, 0, 1, 0, NULL),
	DIV(dout_sclk_isp_sensor1, "dout_sclk_isp_sensor1", "mout_sclk_isp_sensor1", \
			EXYNOS7420_DIV_TOP0_CAM11, 8, 8, \
			EXYNOS7420_DIV_STAT_TOP0_CAM11, 8, 1, 0, NULL),
	DIV(dout_sclk_isp_sensor0, "dout_sclk_isp_sensor0", "mout_sclk_isp_sensor0", \
			EXYNOS7420_DIV_TOP0_CAM11, 16, 8, \
			EXYNOS7420_DIV_STAT_TOP0_CAM11, 16, 1, 0, NULL),
	/* top0 block for peric */
	DIV(dout_sclk_spdif, "dout_sclk_spdif", "mout_sclk_spdif", \
			EXYNOS7420_DIV_TOP0_PERIC0, 4, 4, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC0, 4, 1, 0, NULL),
	DIV(dout_sclk_pcm1, "dout_sclk_pcm1", "mout_sclk_pcm1", \
			EXYNOS7420_DIV_TOP0_PERIC0, 8, 12, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC0, 8, 1, 0, NULL),
	DIV(dout_sclk_i2s1, "dout_sclk_i2s1", "mout_sclk_i2s1", \
			EXYNOS7420_DIV_TOP0_PERIC0, 20, 10, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC0, 20, 1, 0, NULL),
	DIV(none, "dout_sclk_spi0", "mout_sclk_spi0", \
			EXYNOS7420_DIV_TOP0_PERIC1, 20, 12, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC1, 20, 1, 0, NULL),
	DIV(none, "dout_sclk_spi1", "mout_sclk_spi1", \
			EXYNOS7420_DIV_TOP0_PERIC1, 8, 12, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC1, 8, 1, 0, NULL),
	DIV(none, "dout_sclk_spi2", "mout_sclk_spi2", \
			EXYNOS7420_DIV_TOP0_PERIC2, 20, 12, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC2, 20, 1, 0, NULL),
	DIV(none, "dout_sclk_spi3", "mout_sclk_spi3", \
			EXYNOS7420_DIV_TOP0_PERIC2, 8, 12, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC2, 8, 1, 0, NULL),
	DIV(none, "dout_sclk_spi4", "mout_sclk_spi4", \
			EXYNOS7420_DIV_TOP0_PERIC3, 20, 12, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC2, 20, 1, 0, NULL),
	DIV(none, "dout_sclk_spi5", "mout_sclk_spi5", \
			EXYNOS7420_DIV_TOP0_PERIC4, 20, 12, \
			EXYNOS7420_DIV_STAT_TOP0_PERIC4, 20, 1, 0, NULL),

	/* top1 block */
	DIV(none, "dout_aclk_fsys0_200", "mout_aclk_fsys0_200", \
			EXYNOS7420_DIV_TOP13, 28, 4, \
			EXYNOS7420_DIV_STAT_TOP13, 28, 1, 0, "dout_aclk_fsys0_200"),
	DIV(none, "dout_aclk_fsys1_200", "mout_aclk_fsys1_200", \
			EXYNOS7420_DIV_TOP13, 24, 4, \
			EXYNOS7420_DIV_STAT_TOP13, 24, 1, 0, "dout_aclk_fsys1_200"),
	DIV(dout_mmc2, "dout_sclk_mmc2", "mout_sclk_mmc2", \
			EXYNOS7420_DIV_TOP1_FSYS0, 16, 10, \
			EXYNOS7420_DIV_STAT_TOP1_FSYS0, 16, 1, 0, NULL),
	DIV(dout_mmc1, "dout_sclk_mmc1", "mout_sclk_mmc1", \
			EXYNOS7420_DIV_TOP1_FSYS11, 0, 10, \
			EXYNOS7420_DIV_STAT_TOP1_FSYS11, 0, 1, 0, NULL),
	DIV(dout_mmc0, "dout_sclk_mmc0", "mout_sclk_mmc0", \
			EXYNOS7420_DIV_TOP1_FSYS11, 12, 10, \
			EXYNOS7420_DIV_STAT_TOP1_FSYS11, 12, 1, 0, NULL),
	DIV(none, "dout_sclk_phy_fsys0_26m", "mout_sclk_phy_fsys0_26m", \
			EXYNOS7420_DIV_TOP1_FSYS0, 0, 6, \
			EXYNOS7420_DIV_STAT_TOP1_FSYS0, 0, 1, 0, NULL),
	DIV(none, "dout_sclk_phy_fsys1_26m", "mout_sclk_phy_fsys1_26m", \
			EXYNOS7420_DIV_TOP1_FSYS11, 24, 6, \
			EXYNOS7420_DIV_STAT_TOP1_FSYS11, 24, 1, 0, NULL),
	DIV(none, "dout_sclk_phy_fsys0", "mout_sclk_phy_fsys0", \
			EXYNOS7420_DIV_TOP1_FSYS0, 8, 6, \
			EXYNOS7420_DIV_STAT_TOP1_FSYS0, 8, 1, 0, NULL),
	DIV(none, "dout_sclk_phy_fsys1", "mout_sclk_phy_fsys1", \
			EXYNOS7420_DIV_TOP1_FSYS1, 0, 6, \
			EXYNOS7420_DIV_STAT_TOP1_FSYS1, 0, 1, 0, NULL),
	DIV(none, "dout_sclk_usbdrd300", "mout_sclk_usbdrd300", \
			EXYNOS7420_DIV_TOP1_FSYS0, 28, 4, \
			EXYNOS7420_DIV_STAT_TOP1_FSYS0, 28, 1, 0, NULL),
	DIV(none, "dout_sclk_tlx400_wifi1", "mout_sclk_tlx400_wifi1", \
			EXYNOS7420_DIV_TOP1_FSYS1, 8, 6, \
			EXYNOS7420_DIV_STAT_TOP1_FSYS1, 8, 1, 0, NULL),
	DIV(none, "dout_sclk_ufsunipro20", "mout_sclk_ufsunipro20", \
			EXYNOS7420_DIV_TOP1_FSYS1, 16, 6, \
			EXYNOS7420_DIV_STAT_TOP1_FSYS1, 16, 1, 0, NULL),
	/* aud block */
	DIV(dout_aud_ca5, "dout_aud_ca5", "usermux_aud_pll_cmu", \
			EXYNOS7420_DIV_AUD0, 0, 4, \
			EXYNOS7420_DIV_STAT_AUD0, 0, 1, 0, NULL),
	DIV(dout_aclk_aud, "dout_aclk_aud", "dout_aud_ca5", \
			EXYNOS7420_DIV_AUD0, 4, 4, \
			EXYNOS7420_DIV_STAT_AUD0, 4, 1, 0, NULL),
	DIV(dout_pclk_dbg_aud, "dout_pclk_dbg_aud", "dout_aud_ca5", \
			EXYNOS7420_DIV_AUD0, 8, 4, \
			EXYNOS7420_DIV_STAT_AUD0, 8, 1, 0, NULL),
	DIV(dout_atclk_aud, "dout_atclk_aud", "dout_aud_ca5", \
			EXYNOS7420_DIV_AUD0, 12, 4, \
			EXYNOS7420_DIV_STAT_AUD0, 12, 1, 0, NULL),
	DIV(dout_sclk_i2s, "dout_sclk_i2s", "mout_sclk_i2s", \
			EXYNOS7420_DIV_AUD1, 0, 4, \
			EXYNOS7420_DIV_STAT_AUD1, 0, 1, 0, NULL),
	DIV(dout_sclk_pcm, "dout_sclk_pcm", "mout_sclk_pcm", \
			EXYNOS7420_DIV_AUD1, 4, 8, \
			EXYNOS7420_DIV_STAT_AUD1, 4, 1, 0, NULL),
	DIV(dout_sclk_uart, "dout_sclk_uart", "dout_aud_cdclk", \
			EXYNOS7420_DIV_AUD1, 12, 4, \
			EXYNOS7420_DIV_STAT_AUD1, 12, 1, 0, NULL),
	DIV(dout_sclk_slimbus, "dout_sclk_slimbus", "dout_aud_cdclk", \
			EXYNOS7420_DIV_AUD1, 16, 4, \
			EXYNOS7420_DIV_STAT_AUD1, 16, 1, 0, NULL),
	DIV(dout_aud_cdclk, "dout_aud_cdclk", "usermux_aud_pll_cmu", \
			EXYNOS7420_DIV_AUD1, 24, 4, \
			EXYNOS7420_DIV_STAT_AUD1, 24, 1, 0, NULL),

	/* g3d block */
	DIV(sclk_hpm_g3d, "sclk_hpm_g3d", "mout_g3d", \
			EXYNOS7420_DIV_G3D, 8, 2, \
			EXYNOS7420_DIV_STAT_G3D, 8, 1, 0, NULL),

	/* disp block */
	DIV(d_pclk_disp, "dout_pclk_disp", "usermux_aclk_disp_400", \
			EXYNOS7420_DIV_DISP, 0, 2, \
			EXYNOS7420_DIV_STAT_DISP, 0, 1, 0, NULL),
	DIV(d_decon0_eclk, "dout_sub_sclk_decon_int_eclk", "mout_sub_sclk_decon_int_eclk", \
			EXYNOS7420_DIV_DISP, 20, 3, \
			EXYNOS7420_DIV_STAT_DISP, 20, 1, 0, NULL),
	DIV(d_decon0_vclk, "dout_sub_sclk_decon_int_vclk", "mout_sub_sclk_decon_int_vclk", \
			EXYNOS7420_DIV_DISP, 12, 3, \
			EXYNOS7420_DIV_STAT_DISP, 12, 1, 0, NULL),
	DIV(d_decon1_eclk, "dout_sub_sclk_decon_ext_eclk", "mout_sub_sclk_decon_ext_eclk", \
			EXYNOS7420_DIV_DISP, 16, 3, \
			EXYNOS7420_DIV_STAT_DISP, 16, 1, 0, NULL),
	DIV(d_decon1_vclk, "dout_sub_sclk_decon_ext_vclk", "mout_sub_sclk_decon_ext_vclk", \
			EXYNOS7420_DIV_DISP, 8, 3, \
			EXYNOS7420_DIV_STAT_DISP, 8, 1, 0, NULL),
	/* vpp block */
	DIV(d_pclk_vpp, "dout_pclk_vpp_133", "usermux_aclk_vpp0_400", \
			EXYNOS7420_DIV_VPP, 0, 3, \
			EXYNOS7420_DIV_STAT_VPP, 0, 1, 0, NULL),
	/* mfc block */
	DIV(dout_pclk_mfc, "dout_pclk_mfc", "usermux_aclk_mfc_532", \
			EXYNOS7420_DIV_MFC, 0, 2, \
			EXYNOS7420_DIV_STAT_MFC, 0, 1, 0, NULL),
	/* mif block */
	DIV(none, "dout_pclk_mif0", "mout_pclk_mif0", \
			EXYNOS7420_DIV_MIF0, 0, 3, \
			EXYNOS7420_DIV_STAT_MIF0, 0, 0, 0, "dout_pclk_mif0"),
	DIV(none, "dout_pclk_mif1", "mout_pclk_mif1", \
			EXYNOS7420_DIV_MIF1, 0, 3, \
			EXYNOS7420_DIV_STAT_MIF1, 0, 0, 0, "dout_pclk_mif1"),
	DIV(none, "dout_pclk_mif2", "mout_pclk_mif2", \
			EXYNOS7420_DIV_MIF2, 0, 3, \
			EXYNOS7420_DIV_STAT_MIF2, 0, 0, 0, "dout_pclk_mif2"),
	DIV(none, "dout_pclk_mif3", "mout_pclk_mif3", \
			EXYNOS7420_DIV_MIF3, 0, 3, \
			EXYNOS7420_DIV_STAT_MIF3, 0, 0, 0, "dout_pclk_mif3"),
	/* MSCL block */
	DIV(none, "dout_pclk_mscl", "usermux_aclk_mscl_532",
			EXYNOS7420_DIV_MSCL, 0, 3,
			EXYNOS7420_DIV_STAT_MSCL, 0, 1, 0, NULL),
	/* cam0 block */
	DIV(dout_clkdiv_pclk_cam0_bnsa_345, "dout_clkdiv_pclk_cam0_bnsa_345", "mout_user_mux_aclk_cam0_bnsa_690", \
			EXYNOS7420_DIV_CAM0, 0, 3, \
			EXYNOS7420_DIV_STAT_CAM0, 0, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_cam0_bnsb_345, "dout_clkdiv_pclk_cam0_bnsb_345", "mout_user_mux_aclk_cam0_bnsb_690", \
			EXYNOS7420_DIV_CAM0, 4, 3, \
			EXYNOS7420_DIV_STAT_CAM0, 4, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_cam0_bnsd_345, "dout_clkdiv_pclk_cam0_bnsd_345", "mout_user_mux_aclk_cam0_bnsd_690", \
			EXYNOS7420_DIV_CAM0, 8, 3, \
			EXYNOS7420_DIV_STAT_CAM0, 8, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_cam0_3aa0_345, "dout_clkdiv_pclk_cam0_3aa0_345", "mout_user_mux_aclk_cam0_3aa0_690", \
			EXYNOS7420_DIV_CAM0, 12, 3, \
			EXYNOS7420_DIV_STAT_CAM0, 12, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_cam0_3aa1_234, "dout_clkdiv_pclk_cam0_3aa1_234", "mout_user_mux_aclk_cam0_3aa1_468", \
			EXYNOS7420_DIV_CAM0, 16, 3, \
			EXYNOS7420_DIV_STAT_CAM0, 16, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_cam0_trex_266, "dout_clkdiv_pclk_cam0_trex_266", "mout_user_mux_aclk_cam0_trex_532", \
			EXYNOS7420_DIV_CAM0, 20, 3, \
			EXYNOS7420_DIV_STAT_CAM0, 20, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_cam0_trex_133, "dout_clkdiv_pclk_cam0_trex_133", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_DIV_CAM0, 24, 3, \
			EXYNOS7420_DIV_STAT_CAM0, 24, 1, 0, NULL),

	/* cam1 block */
	DIV(dout_clkdiv_pclk_cam1_sclvra_246, "dout_clkdiv_pclk_cam1_sclvra_246", "mout_user_mux_aclk_cam1_sclvra_491", \
			EXYNOS7420_DIV_CAM1, 12, 3, \
			EXYNOS7420_DIV_STAT_CAM1, 12, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_cam1_arm_167, "dout_clkdiv_pclk_cam1_arm_167", "mout_user_mux_aclk_cam1_arm_668", \
			EXYNOS7420_DIV_CAM1, 0, 3, \
			EXYNOS7420_DIV_STAT_CAM1, 0, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_cam1_busperi_167, "dout_clkdiv_pclk_cam1_busperi_167", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_DIV_CAM1, 4, 3, \
			EXYNOS7420_DIV_STAT_CAM1, 4, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_cam1_busperi_84, "dout_clkdiv_pclk_cam1_busperi_84", "dout_clkdiv_pclk_cam1_busperi_167", \
			EXYNOS7420_DIV_CAM1, 8, 3, \
			EXYNOS7420_DIV_STAT_CAM1, 8, 1, 0, NULL),

	/* isp0 block */
	DIV(dout_clkdiv_pclk_isp0_isp0_295, "dout_clkdiv_pclk_isp0_isp0_295", "mout_user_mux_aclk_isp0_isp0_590", \
			EXYNOS7420_DIV_ISP0, 0, 3, \
			EXYNOS7420_DIV_STAT_ISP0, 0, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_isp0_tpu_295, "dout_clkdiv_pclk_isp0_tpu_295", "mout_user_mux_aclk_isp0_tpu_590", \
			EXYNOS7420_DIV_ISP0, 4, 3, \
			EXYNOS7420_DIV_STAT_ISP0, 4, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_isp0_trex_266, "dout_clkdiv_pclk_isp0_trex_266", "mout_user_mux_aclk_isp0_trex_532", \
			EXYNOS7420_DIV_ISP0, 8, 3, \
			EXYNOS7420_DIV_STAT_ISP0, 8, 1, 0, NULL),
	DIV(dout_clkdiv_pclk_isp0_trex_133, "dout_clkdiv_pclk_isp0_trex_133", "mout_user_mux_aclk_isp0_trex_532", \
			EXYNOS7420_DIV_ISP0, 12, 3, \
			EXYNOS7420_DIV_STAT_ISP0, 12, 1, 0, NULL),

	/* isp1 block */
	DIV(dout_clkdiv_pclk_isp1_isp1_234, "dout_clkdiv_pclk_isp1_isp1_234", "mout_user_mux_aclk_isp1_isp1_468", \
			EXYNOS7420_DIV_ISP1, 0, 3, \
			EXYNOS7420_DIV_STAT_ISP1, 0, 1, 0, NULL),
	/* fsys block */
	DIV(none, "dout_pclk_combo_phy_modem", "usermux_aclk_fsys0_200", \
			EXYNOS7420_DIV_FSYS0, 8, 2, \
			EXYNOS7420_DIV_STAT_FSYS0, 8, 1, 0, NULL),
	DIV(none, "usb20_div", "hsic_pll", \
			EXYNOS7420_DIV_FSYS0, 4, 4, \
			EXYNOS7420_DIV_STAT_FSYS0, 4, 1, 0, NULL),
	DIV(none, "dout_pclk_combo_phy_wifi1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_DIV_FSYS1, 8, 2, \
			EXYNOS7420_DIV_STAT_FSYS1, 8, 1, 0, NULL),
	DIV(none, "dout_pclk_fsys1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_DIV_FSYS1, 0, 2, \
			EXYNOS7420_DIV_STAT_FSYS1, 0, 1, 0, NULL),
};

static struct samsung_usermux exynos7420_usermux_clks[] __initdata = {
	/* top0 */
	USERMUX(none, "usermux_bus0_pll_top0", "sclk_bus0_pll_a", \
			EXYNOS7420_MUX_SEL_TOP00, 16, \
			EXYNOS7420_MUX_STAT_TOP00, 16, 0, "usermux_bus0_pll_top0"),
	USERMUX(none, "usermux_bus1_pll_top0", "sclk_bus1_pll_a", \
			EXYNOS7420_MUX_SEL_TOP00, 12, \
			EXYNOS7420_MUX_STAT_TOP00, 12, 0, "usermux_bus1_pll_top0"),
	USERMUX(none, "usermux_cci_pll_top0", "sclk_cci_pll_a", \
			EXYNOS7420_MUX_SEL_TOP00, 8, \
			EXYNOS7420_MUX_STAT_TOP00, 8, 0, "usermux_cci_pll_top0"),
	USERMUX(none, "usermux_mfc_pll_top0", "sclk_mfc_pll_a", \
			EXYNOS7420_MUX_SEL_TOP00, 4, \
			EXYNOS7420_MUX_STAT_TOP00, 4, 0, "usermux_mfc_pll_top0"),
	USERMUX(mout_aud_pll_top0, "usermux_aud_pll", "top_sclk_aud_pll", \
			EXYNOS7420_MUX_SEL_TOP00, 0, \
			EXYNOS7420_MUX_STAT_TOP00, 0, 0, NULL),
	/* top1 */
	USERMUX(none, "usermux_bus0_pll_top1", "sclk_bus0_pll_b", \
			EXYNOS7420_MUX_SEL_TOP10, 16, \
			EXYNOS7420_MUX_STAT_TOP10, 16, 0, NULL),
	USERMUX(none, "usermux_bus1_pll_top1", "sclk_bus1_pll_b", \
			EXYNOS7420_MUX_SEL_TOP10, 12, \
			EXYNOS7420_MUX_STAT_TOP10, 12, 0, NULL),
	USERMUX(none, "usermux_cci_pll_top1", "sclk_cci_pll_b", \
			EXYNOS7420_MUX_SEL_TOP10, 8, \
			EXYNOS7420_MUX_STAT_TOP10, 8, 0, NULL),
	USERMUX(none, "usermux_mfc_pll_top1", "sclk_mfc_pll_b", \
			EXYNOS7420_MUX_SEL_TOP10, 4, \
			EXYNOS7420_MUX_STAT_TOP10, 4, 0, NULL),
	/* bus0 */
	USERMUX(none, "usermux_aclk_bus0_532", "top_aclk_bus0_532", \
			EXYNOS7420_MUX_SEL_BUS0, 0, \
			EXYNOS7420_MUX_STAT_BUS0, 0, 0, NULL),
	USERMUX(none, "usermux_pclk_bus0_133", "top_pclk_bus0_133", \
			EXYNOS7420_MUX_SEL_BUS0, 4, \
			EXYNOS7420_MUX_STAT_BUS0, 4, CLK_IGNORE_UNUSED, NULL),
	/* bus1 */
	USERMUX(none, "usermux_aclk_bus1_532", "top_aclk_bus1_532", \
			EXYNOS7420_MUX_SEL_BUS1, 0, \
			EXYNOS7420_MUX_STAT_BUS1, 0, 0, NULL),
	USERMUX(none, "usermux_aclk_bus1_200", "top_aclk_bus1_200", \
			EXYNOS7420_MUX_SEL_BUS1, 4, \
			EXYNOS7420_MUX_STAT_BUS1, 4, 0, NULL),
	USERMUX(none, "usermux_pclk_bus1_133", "top_pclk_bus1_133", \
			EXYNOS7420_MUX_SEL_BUS1, 8, \
			EXYNOS7420_MUX_STAT_BUS1, 8, 0, NULL),
	/* aud */
	USERMUX(mout_aud_pll_user, "usermux_aud_pll_cmu", "aud_pll", \
			EXYNOS7420_MUX_SEL_AUD, 20, \
			EXYNOS7420_MUX_STAT_AUD, 20, 0, NULL),
	/* g3d */
	/* peris */
	USERMUX(none, "usermux_aclk_peris_66", "top_aclk_peris_66", \
			EXYNOS7420_MUX_SEL_PERIS, 0, \
			EXYNOS7420_MUX_STAT_PERIS, 0, 0, NULL),
	/* peric0 */
	USERMUX(none, "usermux_aclk_peric0_66", "top_aclk_peric0_66", \
			EXYNOS7420_MUX_SEL_PERIC0, 0, \
			EXYNOS7420_MUX_STAT_PERIC0, 0, 0, NULL),
	USERMUX(none, "usermux_sclk_uart0", "top_sclk_uart0", \
			EXYNOS7420_MUX_SEL_PERIC0, 16, \
			EXYNOS7420_MUX_STAT_PERIC0, 16, 0, NULL),
	/* peric1 */
	USERMUX(none, "usermux_aclk_peric1_66", "top_aclk_peric1_66", \
			EXYNOS7420_MUX_SEL_PERIC10, 0, \
			EXYNOS7420_MUX_STAT_PERIC10, 0, 0, NULL),
	USERMUX(none, "usermux_sclk_uart1", "top_sclk_uart1", \
			EXYNOS7420_MUX_SEL_PERIC11, 20, \
			EXYNOS7420_MUX_STAT_PERIC11, 20, 0, NULL),
	USERMUX(none, "usermux_sclk_uart2", "top_sclk_uart2", \
			EXYNOS7420_MUX_SEL_PERIC11, 24, \
			EXYNOS7420_MUX_STAT_PERIC11, 24, 0, NULL),
	USERMUX(none, "usermux_sclk_uart3", "top_sclk_uart3", \
			EXYNOS7420_MUX_SEL_PERIC11, 28, \
			EXYNOS7420_MUX_STAT_PERIC11, 28, 0, NULL),
	USERMUX(none, "usermux_sclk_spi0", "top_sclk_spi0", \
			EXYNOS7420_MUX_SEL_PERIC11, 0, \
			EXYNOS7420_MUX_STAT_PERIC11, 0, CLK_SET_RATE_PARENT, NULL),
	USERMUX(none, "usermux_sclk_spi1", "top_sclk_spi1", \
			EXYNOS7420_MUX_SEL_PERIC11, 4, \
			EXYNOS7420_MUX_STAT_PERIC11, 4, CLK_SET_RATE_PARENT, NULL),
	USERMUX(none, "usermux_sclk_spi2", "top_sclk_spi2", \
			EXYNOS7420_MUX_SEL_PERIC11, 8, \
			EXYNOS7420_MUX_STAT_PERIC11, 8, CLK_SET_RATE_PARENT, NULL),
	USERMUX(none, "usermux_sclk_spi3", "top_sclk_spi3", \
			EXYNOS7420_MUX_SEL_PERIC11, 12, \
			EXYNOS7420_MUX_STAT_PERIC11, 12, CLK_SET_RATE_PARENT, NULL),
	USERMUX(none, "usermux_sclk_spi4", "top_sclk_spi4", \
			EXYNOS7420_MUX_SEL_PERIC11, 16, \
			EXYNOS7420_MUX_STAT_PERIC11, 16, CLK_SET_RATE_PARENT, NULL),
	USERMUX(none, "usermux_sclk_spi5", "top_sclk_spi5", \
			EXYNOS7420_MUX_SEL_PERIC12, 0, \
			EXYNOS7420_MUX_STAT_PERIC12, 0, CLK_SET_RATE_PARENT, NULL),
	/* disp */
	USERMUX(none, "usermux_aclk_disp_400", "top_aclk_disp_400", \
			EXYNOS7420_MUX_SEL_DISP0, 24, \
			EXYNOS7420_MUX_STAT_DISP0, 24, 0, NULL),
	USERMUX(none, "usermux_sclk_dsd", "top_sclk_dsd", \
			EXYNOS7420_MUX_SEL_DISP1, 16, \
			EXYNOS7420_MUX_STAT_DISP1, 16, 0, NULL),
	USERMUX(none, "usermux_sclk_decon_int_eclk", "top_sclk_decon_int_eclk", \
			EXYNOS7420_MUX_SEL_DISP1, 28, \
			EXYNOS7420_MUX_STAT_DISP1, 28, 0, "um_decon0_eclk"),
	USERMUX(none, "usermux_sclk_decon_vclk", "top_sclk_decon_vclk", \
			EXYNOS7420_MUX_SEL_DISP1, 20, \
			EXYNOS7420_MUX_STAT_DISP1, 20, 0, NULL),
	USERMUX(none, "usermux_mipidphy0_rx", "phyclk_mipidphy0_rx", \
			EXYNOS7420_MUX_SEL_DISP2, 12, \
			0, 0, 0, NULL),
	USERMUX(none, "usermux_mipidphy0_bit", "phyclk_mipidphy0_bit", \
			EXYNOS7420_MUX_SEL_DISP2, 8, \
			0, 0, 0, NULL),
	USERMUX(none, "usermux_mipidphy1_rx", "phyclk_mipidphy1_rx", \
			EXYNOS7420_MUX_SEL_DISP2, 4, \
			0, 0, 0, NULL),
	USERMUX(none, "usermux_mipidphy1_bit", "phyclk_mipidphy1_bit", \
			EXYNOS7420_MUX_SEL_DISP2, 0, \
			0, 0, 0, NULL),
	USERMUX(um_decon1_eclk, "usermux_sclk_decon_ext_eclk", "top_sclk_decon_ext_eclk", \
			EXYNOS7420_MUX_SEL_DISP1, 24, \
			EXYNOS7420_MUX_STAT_DISP1, 24, 0, NULL),
	USERMUX(hdmi_pixel, "usermux_hdmiphy_pixel", "phyclk_hdmiphy_pixel_clko", \
			EXYNOS7420_MUX_SEL_DISP2, 20, \
			EXYNOS7420_MUX_STAT_DISP2, 20, 0, NULL),
	USERMUX(hdmi_tmds, "usermux_hdmiphy_tmds", "phyclk_hdmiphy_tmds_clko", \
			EXYNOS7420_MUX_SEL_DISP2, 16, \
			EXYNOS7420_MUX_STAT_DISP2, 16, 0, NULL),
	/* fsys0 */
	USERMUX(usermux_aclk_fsys0_200, "usermux_aclk_fsys0_200", "top_aclk_fsys0_200", \
			EXYNOS7420_MUX_SEL_FSYS00, 24, \
			EXYNOS7420_MUX_STAT_FSYS00, 24, 0, NULL),
	USERMUX(none, "usermux_sclk_usbdrd300", "top_sclk_usbdrd300", \
			EXYNOS7420_MUX_SEL_FSYS01, 28, \
			EXYNOS7420_MUX_STAT_FSYS01, 28, 0, NULL),
	USERMUX(none, "usermux_sclk_mmc2", "top_sclk_mmc2", \
			EXYNOS7420_MUX_SEL_FSYS01, 24, \
			EXYNOS7420_MUX_STAT_FSYS01, 24, 0, NULL),
	USERMUX(none, "usermux_sclk_phy_fsys0", "top_sclk_phy_fsys0", \
			EXYNOS7420_MUX_SEL_FSYS01, 16, \
			EXYNOS7420_MUX_STAT_FSYS01, 16, 0, NULL),
	USERMUX(none, "usermux_phyclk_usbdrd300_udrd30_phyclock", \
			"phyclk_usbdrd300_udrd30_phyclock", \
			EXYNOS7420_MUX_SEL_FSYS02, 28, \
			0, 0, 0, NULL),
	USERMUX(none, "usermux_phyclk_usbdrd300_udrd30_pipe_pclk", \
			"phyclk_usbdrd300_udrd30_pipe_pclk", \
			EXYNOS7420_MUX_SEL_FSYS02, 24, \
			0, 0, 0, NULL),
	USERMUX(none, "usermux_phyclk_usbhost20_phy_freeclk_hsic1", \
			"phyclk_usbhost20_phy_freeclk_hsic1", \
			EXYNOS7420_MUX_SEL_FSYS02, 20, \
			0, 0, 0, NULL),
	USERMUX(none, "usermux_phyclk_usbhost20_phy_phyclk_hsic1", \
			"phyclk_usbhost20_phy_phyclk_hsic1", \
			EXYNOS7420_MUX_SEL_FSYS02, 16, \
			0, 0, 0, NULL),
	USERMUX(none, "usermux_phyclk_lli_tx0", "phyclk_lli_tx0", \
			EXYNOS7420_MUX_SEL_FSYS03, 20, \
			0, 0, 0, NULL),
	USERMUX(none, "usermux_phyclk_lli_rx0", "phyclk_lli_rx0", \
			EXYNOS7420_MUX_SEL_FSYS03, 16, \
			0, 0, 0, NULL),
	USERMUX(none, "usermux_phyclk_pcie_tx0", "phyclk_pcie_tx0", \
			EXYNOS7420_MUX_SEL_FSYS03, 12, \
			0, 0, 0, NULL),
	USERMUX(none, "usermux_phyclk_pcie_rx0", "phyclk_pcie_rx0", \
			EXYNOS7420_MUX_SEL_FSYS03, 8, \
			0, 0, 0, NULL),
	/* fsys1 */
	USERMUX(usermux_aclk_fsys1_200, "usermux_aclk_fsys1_200", "top_aclk_fsys1_200", \
			EXYNOS7420_MUX_SEL_FSYS10, 28, \
			EXYNOS7420_MUX_STAT_FSYS10, 28, 0, NULL),
	USERMUX(none, "usermux_sclk_mmc0", "top_sclk_mmc0", \
			EXYNOS7420_MUX_SEL_FSYS11, 28, \
			EXYNOS7420_MUX_STAT_FSYS11, 28, 0, NULL),
	USERMUX(none, "usermux_sclk_mmc1", "top_sclk_mmc1", \
			EXYNOS7420_MUX_SEL_FSYS11, 24, \
			EXYNOS7420_MUX_STAT_FSYS11, 24, 0, NULL),
	USERMUX(none, "usermux_sclk_ufsunipro20", "top_sclk_ufsunipro20", \
			EXYNOS7420_MUX_SEL_FSYS11, 20, \
			EXYNOS7420_MUX_STAT_FSYS11, 20, 0, NULL),
	USERMUX(none, "usermux_sclk_phy_fsys1", "top_sclk_phy_fsys1", \
			EXYNOS7420_MUX_SEL_FSYS11, 16, \
			EXYNOS7420_MUX_STAT_FSYS11, 16, 0, NULL),
	USERMUX(none, "usermux_sclk_tlx400_wifi1", "top_sclk_tlx400_wifi1", \
			EXYNOS7420_MUX_SEL_FSYS11, 12, \
			EXYNOS7420_MUX_STAT_FSYS11, 12, 0, NULL),
	USERMUX(none, "usermux_phyclk_ufs20_tx0_symbol", "phyclk_ufs20_tx0_symbol", \
			EXYNOS7420_MUX_SEL_FSYS12, 28, \
			EXYNOS7420_MUX_STAT_FSYS12, 28, 0, NULL),
	USERMUX(none, "usermux_phyclk_ufs20_rx0_symbol", "phyclk_ufs20_rx0_symbol", \
			EXYNOS7420_MUX_SEL_FSYS12, 24, \
			EXYNOS7420_MUX_STAT_FSYS12, 24, 0, NULL),
	USERMUX(none, "usermux_phyclk_ufs20_rx1_symbol", "phyclk_ufs20_rx1_symbol", \
			EXYNOS7420_MUX_SEL_FSYS12, 16, \
			EXYNOS7420_MUX_STAT_FSYS12, 16, 0, NULL),
	USERMUX(none, "usermux_phyclk_pcie_wifi1_tx0", "phyclk_pcie_wifi1_tx0", \
			EXYNOS7420_MUX_SEL_FSYS12, 12, \
			EXYNOS7420_MUX_STAT_FSYS12, 12, 0, NULL),
	USERMUX(none, "usermux_phyclk_pcie_wifi1_rx0", "phyclk_pcie_wifi1_rx0", \
			EXYNOS7420_MUX_SEL_FSYS12, 8, \
			EXYNOS7420_MUX_STAT_FSYS12, 8, 0, NULL),
	/* ccore */
	USERMUX(none, "usermux_aclk_ccore_532", "dout_aclk_ccore_532", \
			EXYNOS7420_MUX_SEL_CCORE, 0, \
			EXYNOS7420_MUX_STAT_CCORE, 0, 0, NULL),
	USERMUX(none, "usermux_aclk_ccore_133", "top_aclk_ccore_133", \
			EXYNOS7420_MUX_SEL_CCORE, 1, \
			EXYNOS7420_MUX_STAT_CCORE, 4, 0, NULL),
	/* vpp */
	USERMUX(none, "usermux_aclk_vpp1_400", "top_aclk_vpp1_400", \
			EXYNOS7420_MUX_SEL_VPP, 4, \
			EXYNOS7420_MUX_STAT_VPP, 4, 0, NULL),
	USERMUX(none, "usermux_aclk_vpp0_400", "top_aclk_vpp0_400", \
			EXYNOS7420_MUX_SEL_VPP, 0, \
			EXYNOS7420_MUX_STAT_VPP, 0, 0, NULL),
	/* mfc */
	USERMUX(none, "usermux_aclk_mfc_532", "top_aclk_mfc_532", \
			EXYNOS7420_MUX_SEL_MFC, 0, \
			EXYNOS7420_MUX_STAT_MFC, 0, 0, NULL),
	/* atlas */
	USERMUX(none, "usermux_bus0_pll_atlas", "mout_sclk_bus0_pll_atlas", \
			EXYNOS7420_MUX_SEL_ATLAS1, 0, \
			EXYNOS7420_MUX_STAT_ATLAS1, 0, 0, "mout_bus0_pll_atlas"),
	/* apollo */
	USERMUX(none, "usermux_bus0_pll_apollo", "mout_sclk_bus0_pll_apollo", \
			EXYNOS7420_MUX_SEL_APOLLO1, 0, \
			EXYNOS7420_MUX_STAT_APOLLO1, 0, 0, "mout_bus0_pll_apollo"),
	/* mscl */
	USERMUX(none, "usermux_aclk_mscl_532", "top_aclk_mscl_532",
			EXYNOS7420_MUX_SEL_MSCL, 0,
			EXYNOS7420_MUX_STAT_MSCL, 0, 0, NULL),
	/* cam0 */
	USERMUX(mout_user_mux_aclk_cam0_csis0_690, "mout_user_mux_aclk_cam0_csis0_690", "aclk_cam0_csis0_690", \
			EXYNOS7420_MUX_SEL_CAM00, 0, \
			EXYNOS7420_MUX_STAT_CAM00, 0, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam0_bnsa_690, "mout_user_mux_aclk_cam0_bnsa_690", "aclk_cam0_bnsa_690", \
			EXYNOS7420_MUX_SEL_CAM00, 4, \
			EXYNOS7420_MUX_STAT_CAM00, 4, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam0_bnsb_690, "mout_user_mux_aclk_cam0_bnsb_690", "aclk_cam0_bnsb_690", \
			EXYNOS7420_MUX_SEL_CAM00, 8, \
			EXYNOS7420_MUX_STAT_CAM00, 8, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam0_bnsd_690, "mout_user_mux_aclk_cam0_bnsd_690", "aclk_cam0_bnsd_690", \
			EXYNOS7420_MUX_SEL_CAM00, 12, \
			EXYNOS7420_MUX_STAT_CAM00, 12, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam0_csis1_174, "mout_user_mux_aclk_cam0_csis1_174", "aclk_cam0_csis1_174", \
			EXYNOS7420_MUX_SEL_CAM00, 16, \
			EXYNOS7420_MUX_STAT_CAM00, 16, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam0_3aa0_690, "mout_user_mux_aclk_cam0_3aa0_690", "aclk_cam0_3aa0_690", \
			EXYNOS7420_MUX_SEL_CAM00, 20, \
			EXYNOS7420_MUX_STAT_CAM00, 20, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam0_3aa1_468, "mout_user_mux_aclk_cam0_3aa1_468", "aclk_cam0_3aa1_468", \
			EXYNOS7420_MUX_SEL_CAM00, 24, \
			EXYNOS7420_MUX_STAT_CAM00, 24, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam0_trex_532, "mout_user_mux_aclk_cam0_trex_532", "aclk_cam0_trex_532", \
			EXYNOS7420_MUX_SEL_CAM00, 28, \
			EXYNOS7420_MUX_STAT_CAM00, 28, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam0_csis3_133, "mout_user_mux_aclk_cam0_csis3_133", "aclk_cam0_csis3_133", \
			EXYNOS7420_MUX_SEL_CAM01, 4, \
			EXYNOS7420_MUX_STAT_CAM01, 4, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam0_nocp_133, "mout_user_mux_aclk_cam0_nocp_133", "aclk_cam0_3aa1_468", \
			EXYNOS7420_MUX_SEL_CAM01, 0, \
			EXYNOS7420_MUX_STAT_CAM01, 0, 0, NULL),
	USERMUX(mout_user_mux_phyclk_rxbyteclkhs0_s2a, "mout_user_mux_phyclk_rxbyteclkhs0_s2a", "phyclk_rxbyteclkhs0_s2a", \
			EXYNOS7420_MUX_SEL_CAM02, 0, \
			0, 0, 0, NULL),
	USERMUX(mout_user_mux_phyclk_rxbyteclkhs0_s4, "mout_user_mux_phyclk_rxbyteclkhs0_s4", "phyclk_rxbyteclkhs0_s4", \
			EXYNOS7420_MUX_SEL_CAM02, 4, \
			0, 0, 0, NULL),
	USERMUX(mout_user_mux_phyclk_rxbyteclkhs1_s4, "mout_user_mux_phyclk_rxbyteclkhs1_s4", "phyclk_rxbyteclkhs1_s4", \
			EXYNOS7420_MUX_SEL_CAM02, 8, \
			0, 0, 0, NULL),
	USERMUX(mout_user_mux_phyclk_rxbyteclkhs2_s4, "mout_user_mux_phyclk_rxbyteclkhs2_s4", "phyclk_rxbyteclkhs2_s4", \
			EXYNOS7420_MUX_SEL_CAM02, 12, \
			0, 0, 0, NULL),
	USERMUX(mout_user_mux_phyclk_rxbyteclkhs3_s4, "mout_user_mux_phyclk_rxbyteclkhs3_s4", "phyclk_rxbyteclkhs3_s4", \
			EXYNOS7420_MUX_SEL_CAM02, 16, \
			0, 0, 0, NULL),

	/* cam1 */
	USERMUX(mout_user_mux_aclk_cam1_sclvra_491, "mout_user_mux_aclk_cam1_sclvra_491", "aclk_cam1_sclvra_491", \
			EXYNOS7420_MUX_SEL_CAM10, 0, \
			EXYNOS7420_MUX_STAT_CAM10, 0, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam1_arm_668, "mout_user_mux_aclk_cam1_arm_668", "aclk_cam1_arm_668", \
			EXYNOS7420_MUX_SEL_CAM10, 4, \
			EXYNOS7420_MUX_STAT_CAM10, 4, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam1_busperi_334, "mout_user_mux_aclk_cam1_busperi_334", "aclk_cam1_busperi_334", \
			EXYNOS7420_MUX_SEL_CAM10, 8, \
			EXYNOS7420_MUX_STAT_CAM10, 8, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam1_bnscsis_133, "mout_user_mux_aclk_cam1_bnscsis_133", "aclk_cam1_bnscsis_133", \
			EXYNOS7420_MUX_SEL_CAM10, 12, \
			EXYNOS7420_MUX_STAT_CAM10, 12, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam1_nocp_133, "mout_user_mux_aclk_cam1_nocp_133", "aclk_cam1_nocp_133", \
			EXYNOS7420_MUX_SEL_CAM10, 16, \
			EXYNOS7420_MUX_STAT_CAM10, 16, 0, NULL),
	USERMUX(mout_user_mux_aclk_cam1_trex_532, "mout_user_mux_aclk_cam1_trex_532", "aclk_cam1_trex_532", \
			EXYNOS7420_MUX_SEL_CAM10, 20, \
			EXYNOS7420_MUX_STAT_CAM10, 20, 0, NULL),
	USERMUX(mout_user_mux_sclk_isp_spi0, "mout_user_mux_sclk_isp_spi0", "sclk_isp_spi0", \
			EXYNOS7420_MUX_SEL_CAM11, 0, \
			EXYNOS7420_MUX_STAT_CAM11, 0, 0, NULL),
	USERMUX(mout_user_mux_sclk_isp_spi1, "mout_user_mux_sclk_isp_spi1", "sclk_isp_spi1", \
			EXYNOS7420_MUX_SEL_CAM11, 4, \
			EXYNOS7420_MUX_STAT_CAM11, 4, 0, NULL),
	USERMUX(mout_user_mux_sclk_isp_uart, "mout_user_mux_sclk_isp_uart", "sclk_isp_uart", \
			EXYNOS7420_MUX_SEL_CAM11, 8, \
			EXYNOS7420_MUX_STAT_CAM11, 8, 0, NULL),
	USERMUX(mout_user_mux_phyclk_hs0_csis2_rx_byte, "mout_user_mux_phyclk_hs0_csis2_rx_byte", "phyclk_hs0_csis2_rx_byte", \
			EXYNOS7420_MUX_SEL_CAM11, 28, \
			0, 0, 0, NULL),

	/* isp0 */
	USERMUX(mout_user_mux_aclk_isp0_isp0_590, "mout_user_mux_aclk_isp0_isp0_590", "aclk_isp0_isp0_590", \
			EXYNOS7420_MUX_SEL_ISP0, 0, \
			EXYNOS7420_MUX_STAT_ISP0, 0, 0, NULL),
	USERMUX(mout_user_mux_aclk_isp0_tpu_590, "mout_user_mux_aclk_isp0_tpu_590", "aclk_isp0_tpu_590", \
			EXYNOS7420_MUX_SEL_ISP0, 4, \
			EXYNOS7420_MUX_STAT_ISP0, 4, 0, NULL),
	USERMUX(mout_user_mux_aclk_isp0_trex_532, "mout_user_mux_aclk_isp0_trex_532", "aclk_isp0_trex_532", \
			EXYNOS7420_MUX_SEL_ISP0, 8, \
			EXYNOS7420_MUX_STAT_ISP0, 8, 0, NULL),

	/* isp1 */
	USERMUX(mout_user_mux_aclk_isp1_isp1_468, "mout_user_mux_aclk_isp1_isp1_468", "aclk_isp1_isp1_468", \
			EXYNOS7420_MUX_SEL_ISP1, 0, \
			EXYNOS7420_MUX_STAT_ISP1, 0, 0, NULL),
	USERMUX(mout_user_mux_aclk_isp1_ahb_117, "mout_user_mux_aclk_isp1_ahb_117", "aclk_isp1_ahb_117", \
			EXYNOS7420_MUX_SEL_ISP1, 4, \
			EXYNOS7420_MUX_STAT_ISP1, 4, 0, NULL),

	/* imem */
	USERMUX(none, "mout_user_mux_aclk_imem_266", "aclk_imem_266", \
			EXYNOS7420_MUX_SEL_IMEM, 0, \
			EXYNOS7420_MUX_STAT_IMEM, 0, CLK_IGNORE_UNUSED, NULL),
	USERMUX(none, "mout_user_mux_aclk_imem_200", "aclk_imem_200", \
			EXYNOS7420_MUX_SEL_IMEM, 4, \
			EXYNOS7420_MUX_STAT_IMEM, 4, CLK_IGNORE_UNUSED, NULL),
};

static struct samsung_gate exynos7420_gate_clks[] __initdata = {
	/* topc clock */
	GATE(none, "top_sclk_aud_pll", "dout_sclk_aud_pll", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 20, 0, NULL),
	GATE(none, "top_sclk_bus_pll_g3d", "dout_sclk_bus_pll_g3d", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "top_aclk_peris_66", "dout_aclk_peris_66", \
			EXYNOS7420_ENABLE_ACLK_TOPC1, 24, 0, NULL),
	GATE(none, "sclk_bus0_pll_mif", "dout_sclk_bus0_pll_mif", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 6, 0, "sclk_bus0_pll_mif"),
	GATE(none, "sclk_bus0_pll_a", "dout_sclk_bus0_pll", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_bus0_pll_b", "dout_sclk_bus0_pll", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_bus1_pll_a", "dout_sclk_bus1_pll", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 12, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_bus1_pll_b", "dout_sclk_bus1_pll", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 13, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_cci_pll_a", "dout_sclk_cci_pll", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 0, 0, NULL),
	GATE(none, "sclk_cci_pll_b", "dout_sclk_cci_pll", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 1, 0, NULL),
	GATE(none, "sclk_mfc_pll_a", "dout_sclk_mfc_pll", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 16, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_mfc_pll_b", "dout_sclk_mfc_pll", \
			EXYNOS7420_ENABLE_SCLK_TOPC1, 17, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "top_aclk_ccore_133", "dout_aclk_ccore_133", \
			EXYNOS7420_ENABLE_ACLK_TOPC0, 4, 0, NULL),
	GATE(none, "top_aclk_bus0_532", "dout_aclk_bus0_532", \
			EXYNOS7420_ENABLE_ACLK_TOPC0, 8, CLK_GATE_ENABLE, NULL),
	GATE(none, "top_aclk_bus1_532", "dout_aclk_bus1_532", \
			EXYNOS7420_ENABLE_ACLK_TOPC0, 12, CLK_GATE_ENABLE, NULL),
	GATE(none, "top_aclk_bus1_200", "dout_aclk_bus1_200", \
			EXYNOS7420_ENABLE_ACLK_TOPC0, 16, CLK_GATE_ENABLE, NULL),
	GATE(none, "aclk_imem_266", "dout_aclk_imem_266", \
			EXYNOS7420_ENABLE_ACLK_TOPC0, 20, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_200", "dout_aclk_imem_200", \
			EXYNOS7420_ENABLE_ACLK_TOPC0, 24, CLK_IGNORE_UNUSED, NULL),
	GATE(top_aclk_mfc_532, "top_aclk_mfc_532", "dout_aclk_mfc_532", \
			EXYNOS7420_ENABLE_ACLK_TOPC1, 8, 0, NULL),
	GATE(none, "top_aclk_mscl_532", "dout_aclk_mscl_532", \
			EXYNOS7420_ENABLE_ACLK_TOPC1, 20, 0, NULL),
	GATE(none, "top_pclk_bus0_133", "dout_pclk_bus01_133", \
			EXYNOS7420_ENABLE_ACLK_TOPC1, 28, CLK_GATE_ENABLE, NULL),
	GATE(none, "top_pclk_bus1_133", "dout_pclk_bus01_133", \
			EXYNOS7420_ENABLE_ACLK_TOPC1, 29, CLK_GATE_ENABLE, NULL),

	/* top0 clock */
	GATE(none, "top_aclk_peric0_66", "dout_aclk_peric0_66", \
			EXYNOS7420_ENABLE_ACLK_TOP03, 20, 0, NULL),
	GATE(none, "top_aclk_peric1_66", "dout_aclk_peric1_66", \
			EXYNOS7420_ENABLE_ACLK_TOP03, 12, 0, NULL),
	GATE(none, "top_aclk_vpp0_400", "dout_aclk_vpp0_400", \
			EXYNOS7420_ENABLE_ACLK_TOP03, 8, 0, NULL),
	GATE(none, "top_aclk_vpp1_400", "dout_aclk_vpp1_400",\
			EXYNOS7420_ENABLE_ACLK_TOP03, 4, 0, NULL),
	GATE(none, "top_sclk_uart0", "dout_sclk_uart0", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC3, 16, 0, NULL),
	GATE(none, "top_sclk_uart1", "dout_sclk_uart1", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC3, 12, 0, NULL),
	GATE(none, "top_sclk_uart2", "dout_sclk_uart2", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC3, 8, 0, NULL),
	GATE(none, "top_sclk_uart3", "dout_sclk_uart3", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC3, 4, 0, NULL),
	GATE(none, "top_sclk_spi0", "dout_sclk_spi0", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC1, 20, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "top_sclk_spi1", "dout_sclk_spi1", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC1, 8, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "top_sclk_spi2", "dout_sclk_spi2", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC2, 20, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "top_sclk_spi3", "dout_sclk_spi3", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC2, 8, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "top_sclk_spi4", "dout_sclk_spi4", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC3, 20, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "top_sclk_spi5", "dout_sclk_spi5", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC4, 20, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "top_aclk_disp_400", "dout_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_TOP03, 28, 0, NULL),
	GATE(none, "top_sclk_decon_int_eclk", "dout_sclk_decon_int_eclk", \
			EXYNOS7420_ENABLE_SCLK_TOP0_DISP, 28, 0, NULL),
	GATE(none, "top_sclk_decon_ext_eclk", "dout_sclk_decon_ext_eclk", \
			EXYNOS7420_ENABLE_SCLK_TOP0_DISP, 24, 0, NULL),
	GATE(none, "top_sclk_decon_vclk", "dout_sclk_decon_vclk", \
			EXYNOS7420_ENABLE_SCLK_TOP0_DISP, 20, 0, NULL),
	GATE(none, "top_sclk_dsd", "dout_sclk_dsd", \
			EXYNOS7420_ENABLE_SCLK_TOP0_DISP, 16, 0, NULL),
	GATE(none, "top_sclk_hdmi_spdif", "dout_sclk_hdmi_spdif", \
			EXYNOS7420_ENABLE_SCLK_TOP0_DISP, 12, 0, NULL),
	GATE(none, "top_sclk_pcm1", "dout_sclk_pcm1", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC0, 8, 0, NULL),
	GATE(none, "top_sclk_i2s1", "dout_sclk_i2s1", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC0, 20, 0, NULL),
	GATE(none, "top_sclk_spdif", "dout_sclk_spdif", \
			EXYNOS7420_ENABLE_SCLK_TOP0_PERIC0, 4, 0, NULL),
	/* top1 clock */
	GATE(none, "top_aclk_fsys0_200", "dout_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_TOP13, 28, 0, NULL),
	GATE(none, "top_aclk_fsys1_200", "dout_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_TOP13, 24, 0, NULL),
	GATE(none, "top_sclk_usbdrd300", "dout_sclk_usbdrd300", \
			EXYNOS7420_ENABLE_SCLK_TOP1_FSYS0, 28, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "top_sclk_mmc2", "dout_sclk_mmc2", \
			EXYNOS7420_ENABLE_SCLK_TOP1_FSYS0, 16, 0, NULL),
	GATE(none, "top_sclk_mmc1", "dout_sclk_mmc1", \
			EXYNOS7420_ENABLE_SCLK_TOP1_FSYS11, 0, 0, NULL),
	GATE(none, "top_sclk_mmc0", "dout_sclk_mmc0", \
			EXYNOS7420_ENABLE_SCLK_TOP1_FSYS11, 12, 0, NULL),
	GATE(none, "top_sclk_phy_fsys0", "dout_sclk_phy_fsys0", \
			EXYNOS7420_ENABLE_SCLK_TOP1_FSYS0, 8, 0, NULL),
	GATE(none, "top_sclk_phy_fsys0_26m", "dout_sclk_phy_fsys0_26m", \
			EXYNOS7420_ENABLE_SCLK_TOP1_FSYS0, 0, 0, NULL),
	GATE(none, "top_sclk_phy_fsys1", "dout_sclk_phy_fsys1", \
			EXYNOS7420_ENABLE_SCLK_TOP1_FSYS1, 0, 0, NULL),
	GATE(none, "top_sclk_tlx400_wifi1", "dout_sclk_tlx400_wifi1", \
			EXYNOS7420_ENABLE_SCLK_TOP1_FSYS1, 8, 0, NULL),
	GATE(none, "top_sclk_ufsunipro20", "dout_sclk_ufsunipro20", \
			EXYNOS7420_ENABLE_SCLK_TOP1_FSYS1, 16, 0, NULL),
	GATE(top_sclk_phy_fsys1_26m, "top_sclk_phy_fsys1_26m", "dout_sclk_phy_fsys1_26m", \
			EXYNOS7420_ENABLE_SCLK_TOP1_FSYS11, 24, 0, NULL),
	/* vpp */
	GATE(aclk_lh_vpp0, "aclk_lh_vpp0", "usermux_aclk_bus0_532", \
			EXYNOS7420_ENABLE_ACLK_BUS0, 5, 0, NULL),
	GATE(aclk_lh_vpp1, "aclk_lh_vpp1", "usermux_aclk_bus0_532", \
			EXYNOS7420_ENABLE_ACLK_BUS0, 6, 0, NULL),

	GATE(aclk_vg1, "aclk_vpp_idma_vg1", "aclk_xiu_vppx1", \
			EXYNOS7420_ENABLE_ACLK_VPP, 1, 0, "aclk_vpp_idma_vg1"),
	GATE(aclk_vgr1, "aclk_vpp_idma_vgr1", "aclk_xiu_vppx1", \
			EXYNOS7420_ENABLE_ACLK_VPP, 3, 0, "aclk_vpp_idma_vgr1"),
	GATE(none, "aclk_xiu_vppx1", "aclk_axi_lh_async_si_vpp1", \
			EXYNOS7420_ENABLE_ACLK_VPP, 17, 0, "aclk_xiu_vppx1"),
	GATE(none, "aclk_axi_lh_async_si_vpp1", "usermux_aclk_vpp1_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 15, 0, "aclk_axi_lh_async_si_vpp1"),

	GATE(aclk_vg0, "aclk_vpp_idma_vg0", "aclk_xiu_vppx0", \
			EXYNOS7420_ENABLE_ACLK_VPP, 0, 0, "aclk_vpp_idma_vg0"),
	GATE(aclk_vgr0, "aclk_vpp_idma_vgr0", "aclk_xiu_vppx0", \
			EXYNOS7420_ENABLE_ACLK_VPP, 2, 0, "aclk_vpp_idma_vgr0"),
	GATE(none, "aclk_xiu_vppx0", "aclk_axi_lh_async_si_vpp0", \
			EXYNOS7420_ENABLE_ACLK_VPP, 16, 0, "aclk_xiu_vppx0"),
	GATE(none, "aclk_axi_lh_async_si_vpp0", "usermux_aclk_vpp0_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 14, 0, "aclk_axi_lh_async_si_vpp0"),

	GATE(pclk_vpp, "pclk_vpp", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 20, 0, "pclk_vpp"),

	GATE(CLK_ACLK_VPP1_SYSMMU, "aclk_smmu_vpp1", "usermux_aclk_vpp1_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 6, 0, NULL),
	GATE(aclk_smmu_vpp_sfw1, "aclk_smmu_vpp_sfw1", "usermux_aclk_vpp0_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 7, 0, NULL),
	GATE(none, "aclk_bts_idma_vg1", "usermux_aclk_vpp1_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 11, 0, "aclk_bts_idma_vg1"),
	GATE(none, "aclk_bts_idma_vgr1", "usermux_aclk_vpp1_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 13, 0, "aclk_bts_idma_vgr1"),
	GATE(CLK_ACLK_VPP0_SYSMMU, "aclk_smmu_vpp0", "usermux_aclk_vpp0_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 4, 0, NULL),
	GATE(aclk_smmu_vpp_sfw0, "aclk_smmu_vpp_sfw0", "usermux_aclk_vpp0_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 5, 0, NULL),
	GATE(none, "aclk_bts_idma_vg0", "usermux_aclk_vpp0_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 10, 0, "aclk_bts_idma_vg0"),
	GATE(none, "aclk_bts_idma_vgr0", "usermux_aclk_vpp0_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 12, 0, "aclk_bts_idma_vgr0"),
	GATE(CLK_PCLK_VPP0_SYSMMU, "pclk_smmu_vpp0", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 4, 0, NULL),
	GATE(pclk_smmu_vpp_sfw0, "pclk_smmu_vpp_sfw0", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 5, 0, NULL),
	GATE(CLK_PCLK_VPP1_SYSMMU, "pclk_smmu_vpp1", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 6, 0, NULL),
	GATE(pclk_smmu_vpp_sfw1, "pclk_smmu_vpp_sfw1", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 7, 0, NULL),
	GATE(none, "pclk_bts_idma_vg0", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 10, 0, "pclk_bts_idma_vg0"),
	GATE(none, "pclk_bts_idma_vg1", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 11, 0, "pclk_bts_idma_vg1"),
	GATE(none, "pclk_bts_idma_vgr0", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 12, 0, "pclk_bts_idma_vgr0"),
	GATE(none, "pclk_bts_idma_vgr1", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 13, 0, "pclk_bts_idma_vgr1"),
	/* bus1 */
	GATE(aclk_noc_bus1_nrt, "aclk_noc_bus1_nrt", "usermux_aclk_bus1_532", \
			EXYNOS7420_ENABLE_ACLK_BUS1, 31, CLK_GATE_ENABLE, NULL),
	GATE(none, "aclk_xiu_asyncm_fsys0", "usermux_aclk_bus1_532", \
			EXYNOS7420_ENABLE_ACLK_BUS1, 30, CLK_GATE_ENABLE, NULL),
	GATE(none, "aclk_lh_fsys1", "usermux_aclk_bus1_532", \
			EXYNOS7420_ENABLE_ACLK_BUS1, 27, CLK_GATE_ENABLE, NULL),
	GATE(pclk_gpio_bus1, "pclk_gpio_bus1", "usermux_pclk_bus1_133", \
			EXYNOS7420_ENABLE_PCLK_BUS1, 27, CLK_GATE_ENABLE, NULL),
	GATE(none, "aclk_lh_fsys0", "usermux_aclk_bus1_200", \
			EXYNOS7420_ENABLE_ACLK_BUS1, 28, CLK_GATE_ENABLE, NULL),
	GATE(aclk_lh_mfc0, "aclk_lh_mfc0", "usermux_aclk_bus1_532", \
			EXYNOS7420_ENABLE_ACLK_BUS1, 25, 0, NULL),
	GATE(aclk_lh_mfc1, "aclk_lh_mfc1", "usermux_aclk_bus1_532", \
			EXYNOS7420_ENABLE_ACLK_BUS1, 24, 0, NULL),
	GATE(none, "aclk_lh_mscl0", "usermux_aclk_bus1_532", \
			EXYNOS7420_ENABLE_ACLK_BUS1, 23, 0, NULL),
	GATE(none, "aclk_lh_mscl1", "usermux_aclk_bus1_532", \
			EXYNOS7420_ENABLE_ACLK_BUS1, 22, 0, NULL),

	/* clk_ppmu */
	GATE(none, "aclk_ppmu_vpp_1", "usermux_aclk_vpp1_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 9, 0, "aclk_ppmu_vpp_1"),
	GATE(none, "aclk_ppmu_vpp_0", "usermux_aclk_vpp0_400", \
			EXYNOS7420_ENABLE_ACLK_VPP, 8, 0, "aclk_ppmu_vpp_0"),
	GATE(none, "pclk_ppmu_vpp_0", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 8, 0, "pclk_ppmu_vpp_0"),
	GATE(none, "pclk_ppmu_vpp_1", "dout_pclk_vpp_133", \
			EXYNOS7420_ENABLE_PCLK_VPP, 9, 0, "pclk_ppmu_vpp_1"),
	GATE(none, "aclk_ppmu_mfc_0", "usermux_aclk_mfc_532", \
			EXYNOS7420_ENABLE_ACLK_MFC, 25, 0, "aclk_ppmu_mfc_0"),
	GATE(none, "aclk_ppmu_mfc_1", "usermux_aclk_mfc_532", \
			EXYNOS7420_ENABLE_ACLK_MFC, 26, 0, "aclk_ppmu_mfc_1"),
	GATE(none, "pclk_ppmu_mfc_0", "dout_pclk_mfc", \
			EXYNOS7420_ENABLE_PCLK_MFC, 26, 0, "pclk_ppmu_mfc_0"),
	GATE(none, "pclk_ppmu_mfc1", "dout_pclk_mfc", \
			EXYNOS7420_ENABLE_PCLK_MFC, 25, 0, "pclk_ppmu_mfc_1"),
	GATE(none, "aclk_ppmu_disp_ro", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP, 20, 0, "aclk_ppmu_disp_ro"),
	GATE(none, "aclk_ppmu_disp_rw", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP, 19, 0, "aclk_ppmu_disp_rw"),
	GATE(none, "pclk_ppmu_disp_ro", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 17, 0, "pclk_ppmu_disp_ro"),
	GATE(none, "pclk_ppmu_disp_rw", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 16, 0, "pclk_ppmu_disp_rw"),
	GATE(none, "aclk_ppmu_fsys0", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 7, 0, "aclk_ppmu_fsys0"),
	GATE(none, "pclk_ppmu_fsys0" ,"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_PCLK_FSYS0, 30, 0, "pclk_ppmu_fsys0"),
	GATE(none, "aclk_ppmu_fsys1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 16, 0, "aclk_ppmu_fsys1"),
	GATE(none, "pclk_ppmu_fsys1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_PCLK_FSYS1, 25, 0, "pclk_ppmu_fsys1"),
	GATE(none, "aclk_ppmu_mscl_0", "dout_pclk_mscl",
			EXYNOS7420_ENABLE_ACLK_MSCL, 22, 0, "aclk_ppmu_mscl_0"),
	GATE(none, "aclk_ppmu_mscl_1", "dout_pclk_mscl",
			EXYNOS7420_ENABLE_ACLK_MSCL, 21, 0, "aclk_ppmu_mscl_1"),
	GATE(none, "pclk_ppmu_mscl_0", "dout_pclk_mscl",
			EXYNOS7420_ENABLE_PCLK_MSCL, 25, 0, "pclk_ppmu_mscl_0"),
	GATE(none, "pclk_ppmu_mscl_1", "dout_pclk_mscl",
			EXYNOS7420_ENABLE_PCLK_MSCL, 24, 0, "pclk_ppmu_mscl_1"),
	GATE(none, "pclk_ppmu_apl", "usermux_aclk_ccore_133", \
			EXYNOS7420_ENABLE_PCLK_CCORE, 23, 0, "pclk_ppmu_apl"),
	GATE(none, "pclk_ppmu_ats", "usermux_aclk_ccore_133", \
			EXYNOS7420_ENABLE_PCLK_CCORE, 24, 0, "pclk_ppmu_ats"),

	/* g3d */
	GATE(g3d, "aclk_g3d", "mout_g3d", \
			EXYNOS7420_ENABLE_ACLK_G3D, 0, CLK_IGNORE_UNUSED, NULL),

	/* peris */
	GATE(mct, "pclk_mct", "usermux_aclk_peris_66", \
			EXYNOS7420_ENABLE_PCLK_PERIS, 5, 0, NULL),
	GATE(wdt_apl, "pclk_wdt_apl", "usermux_aclk_peris_66",\
			EXYNOS7420_ENABLE_PCLK_PERIS, 7, 0, NULL),
	GATE(cec, "pclk_hdmi_cec", "usermux_aclk_peris_66", \
			EXYNOS7420_ENABLE_PCLK_PERIS, 4, 0, NULL),
	GATE(none, "pclk_wdt_atlas", "usermux_aclk_peris_66", \
			EXYNOS7420_ENABLE_PCLK_PERIS, 6, 0, NULL),
	GATE(none, "pclk_tmu_apbif", "usermux_aclk_peris_66", \
			EXYNOS7420_ENABLE_PCLK_PERIS, 10, CLK_GATE_ENABLE, NULL),
	GATE(none, "sclk_tmu", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_PERIS, 10, CLK_GATE_ENABLE, NULL),
	/* peric0 */
	GATE(puart0, "pclk_uart0", "usermux_aclk_peric0_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC0, 16, 0, "console-pclk0"),
	GATE(suart0, "sclk_uart0", "usermux_sclk_uart0", \
			EXYNOS7420_ENABLE_SCLK_PERIC0, 16, 0, "console-sclk0"),
	GATE(hsi2c0, "pclk_hsi2c0", "usermux_aclk_peric0_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC0, 8, 0, NULL),
	GATE(hsi2c1, "pclk_hsi2c1", "usermux_aclk_peric0_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC0, 9, 0, NULL),
	GATE(hsi2c4, "pclk_hsi2c4", "usermux_aclk_peric0_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC0, 10, 0, NULL),
	GATE(hsi2c5, "pclk_hsi2c5", "usermux_aclk_peric0_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC0, 11, 0, NULL),
	GATE(hsi2c9, "pclk_hsi2c9", "usermux_aclk_peric0_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC0, 12, 0, NULL),
	GATE(hsi2c10, "pclk_hsi2c10", "usermux_aclk_peric0_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC0, 13, 0, NULL),
	GATE(hsi2c11, "pclk_hsi2c11", "usermux_aclk_peric0_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC0, 14, 0, NULL),
	GATE(pclk_pwm, "pwm_clock", "usermux_aclk_peric0_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC0, 21, 0, NULL),
	GATE(none, "sclk_pwm", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_PERIC0, 21, 0, NULL),
	GATE(pclk_adcif, "pclk_adcif", "usermux_aclk_peric0_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC0, 20, 0, NULL),
	/* peric1 */
	GATE(puart1, "pclk_uart1", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 9, 0, "console-pclk1"),
	GATE(puart2, "pclk_uart2", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 10, 0, "console-pclk2"),
	GATE(puart3, "pclk_uart3", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 11, 0, "console-pclk3"),
	GATE(suart1, "sclk_uart1", "usermux_sclk_uart1", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 9, 0, "console-sclk1"),
	GATE(suart2, "sclk_uart2", "usermux_sclk_uart2", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 10, 0, "console-sclk2"),
	GATE(suart3, "sclk_uart3", "usermux_sclk_uart3", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 11, 0, "console-sclk3"),
	GATE(hsi2c2, "pclk_hsi2c2", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 4, 0, NULL),
	GATE(hsi2c3, "pclk_hsi2c3", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 5, 0, NULL),
	GATE(hsi2c6, "pclk_hsi2c6", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 6, 0, NULL),
	GATE(hsi2c7, "pclk_hsi2c7", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 7, 0, NULL),
	GATE(hsi2c8, "pclk_hsi2c8", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 8, 0, NULL),
	GATE(pclk_spi0, "pclk_spi0", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 12, 0, NULL),
	GATE(pclk_spi1, "pclk_spi1", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 13, 0, NULL),
	GATE(pclk_spi2, "pclk_spi2", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 14, 0, NULL),
	GATE(pclk_spi3, "pclk_spi3", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 15, 0, NULL),
	GATE(pclk_spi4, "pclk_spi4", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 16, 0, NULL),
	GATE(pclk_spi5, "pclk_spi5", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 20, 0, NULL),
	GATE(pclk_i2s1, "pclk_i2s1", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 17, 0, NULL),
	GATE(pclk_pcm1, "pclk_pcm1", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 18, 0, NULL),
	GATE(pclk_spdif, "pclk_spdif", "usermux_aclk_peric1_66", \
			EXYNOS7420_ENABLE_PCLK_PERIC1, 19, 0, NULL),
	GATE(none, "sclk_pcm1", "top_sclk_pcm1", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 18, 0, NULL),
	GATE(sclk_i2s1, "sclk_i2s1", "top_sclk_i2s1", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 17, 0, NULL),
	GATE(sclk_spi0, "sclk_spi0", "usermux_sclk_spi0", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 12, CLK_SET_RATE_PARENT, NULL),
	GATE(sclk_spi1, "sclk_spi1", "usermux_sclk_spi1", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 13, CLK_SET_RATE_PARENT, NULL),
	GATE(sclk_spi2, "sclk_spi2", "usermux_sclk_spi2", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 14, CLK_SET_RATE_PARENT, NULL),
	GATE(sclk_spi3, "sclk_spi3", "usermux_sclk_spi3", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 15, CLK_SET_RATE_PARENT, NULL),
	GATE(sclk_spi4, "sclk_spi4", "usermux_sclk_spi4", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 16, CLK_SET_RATE_PARENT, NULL),
	GATE(sclk_spi5, "sclk_spi5", "usermux_sclk_spi5", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 20, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "sclk_spdif", "top_sclk_spdif", \
			EXYNOS7420_ENABLE_SCLK_PERIC10, 19, 0, NULL),
	GATE(sclk_i2s1_bclk, "gate_ioclk_i2s1_bclk", "ioclk_i2s1_bclk", \
			EXYNOS7420_ENABLE_SCLK_PERIC11, 17, 0, NULL),
	GATE(none, "gate_ioclk_spi0_clk", "ioclk_spi0_clk", \
			EXYNOS7420_ENABLE_SCLK_PERIC11, 12, 0, NULL),
	GATE(none, "gate_ioclk_spi1_clk", "ioclk_spi1_clk", \
			EXYNOS7420_ENABLE_SCLK_PERIC11, 13, 0, NULL),
	GATE(none, "gate_ioclk_spi2_clk", "ioclk_spi2_clk", \
			EXYNOS7420_ENABLE_SCLK_PERIC11, 14, 0, NULL),
	GATE(none, "gate_ioclk_spi3_clk", "ioclk_spi3_clk", \
			EXYNOS7420_ENABLE_SCLK_PERIC11, 15, 0, NULL),
	GATE(none, "gate_ioclk_spi4_clk", "ioclk_spi4_clk", \
			EXYNOS7420_ENABLE_SCLK_PERIC11, 16, 0, NULL),
	GATE(none, "gate_ioclk_spi5_clk", "ioclk_spi5_clk", \
			EXYNOS7420_ENABLE_SCLK_PERIC11, 20, 0, NULL),
	/* mfc */
	GATE(aclk_lh_s_mfc_0, "aclk_lh_s_mfc_0", "usermux_aclk_mfc_532", \
			EXYNOS7420_ENABLE_ACLK_MFC, 31, 0, NULL),
	GATE(aclk_lh_s_mfc_1, "aclk_lh_s_mfc_1", "usermux_aclk_mfc_532", \
			EXYNOS7420_ENABLE_ACLK_MFC, 30, 0, NULL),
	GATE(aclk_mfc, "aclk_mfc", "usermux_aclk_mfc_532", \
			EXYNOS7420_ENABLE_ACLK_MFC, 19, 0, NULL),
	GATE(none, "aclk_bts_mfc_0", "usermux_aclk_mfc_532", \
			EXYNOS7420_ENABLE_ACLK_MFC, 16, 0, "aclk_bts_mfc_0"),
	GATE(none, "aclk_bts_mfc_1", "usermux_aclk_mfc_532", \
			EXYNOS7420_ENABLE_ACLK_MFC, 15, 0, "aclk_bts_mfc_1"),
	GATE(pclk_mfc, "pclk_mfc", "dout_pclk_mfc", \
			EXYNOS7420_ENABLE_PCLK_MFC, 31, 0, NULL),
	GATE(none, "pclk_bts_mfc0", "dout_pclk_mfc", \
			EXYNOS7420_ENABLE_PCLK_MFC, 28, 0, "pclk_bts_mfc0"),
	GATE(none, "pclk_bts_mfc1", "dout_pclk_mfc", \
			EXYNOS7420_ENABLE_PCLK_MFC, 27, 0, "pclk_bts_mfc1"),
	/* disp */
	/* aclk_lh_disp0(decon-int): longhop master clock between main bus to disp block*/
	GATE(aclk_lh_disp0, "aclk_lh_disp0", "usermux_aclk_bus0_532", \
			EXYNOS7420_ENABLE_ACLK_BUS0, 3, 0, NULL),
	/* aclk_lh_disp1(decon-int/ext) */
	GATE(aclk_lh_disp1, "aclk_lh_disp1", "usermux_aclk_bus0_532", \
			EXYNOS7420_ENABLE_ACLK_BUS0, 4, 0, NULL),

	/* aclk_decon_int */
	GATE(none, "aclk_lh_async_si_r_top_disp", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP, 29, 0, NULL),
	GATE(none, "aclk_xiu_disp_ro", "aclk_lh_async_si_r_top_disp", \
			EXYNOS7420_ENABLE_ACLK_DISP, 27, 0, NULL),
	GATE(aclk_decon0, "aclk_decon_int", "aclk_xiu_disp_ro", \
			EXYNOS7420_ENABLE_ACLK_DISP, 31, 0, NULL),

	/* aclk_decon_ext */
	GATE(aclk_decon1, "aclk_decon_ext", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP, 30, 0, NULL),
	/* aclk_bts */
	GATE(none, "aclk_bts_axi_disp_ro_0", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP, 25, 0, "aclk_bts_axi_disp_ro_0"),
	GATE(none, "aclk_bts_axi_disp_ro_1", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP, 24, 0, "aclk_bts_axi_disp_ro_1"),
	GATE(none, "aclk_bts_axi_disp_rw_0", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP, 23, 0, "aclk_bts_axi_disp_rw_0"),
	GATE(none, "aclk_bts_axi_disp_rw_1", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP, 22, 0, "aclk_bts_axi_disp_rw_1"),

	GATE(none, "aclk_bts_mscl0", "usermux_aclk_mscl_532", \
			EXYNOS7420_ENABLE_ACLK_MSCL, 25, 0, "aclk_bts_mscl0"),
	GATE(none, "aclk_bts_mscl1", "usermux_aclk_mscl_532", \
			EXYNOS7420_ENABLE_ACLK_MSCL, 24, 0, "aclk_bts_mscl1"),
	GATE(none, "aclk_bts_jpeg", "usermux_aclk_mscl_532", \
			EXYNOS7420_ENABLE_ACLK_MSCL, 23, 0, "aclk_bts_jpeg"),

	/* aclk_disp(decon-int/ext) */
	GATE(none, "aclk_lh_async_si_top_disp", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP, 28, 0, NULL),
	GATE(aclk_disp, "aclk_xiu_disp_rw", "aclk_lh_async_si_top_disp", \
			EXYNOS7420_ENABLE_ACLK_DISP, 26, 0, NULL),

	GATE(pclk_decon0, "pclk_decon_int", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 31, 0, NULL),
	GATE(pclk_decon1, "pclk_decon_ext", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 30, 0, NULL),
	GATE(pclk_dsim0, "pclk_dsim0", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 29, 0, NULL),
	GATE(pclk_dsim1, "pclk_dsim1", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 28, 0, NULL),
	GATE(pclk_hdmi, "pclk_hdmi", "pclk_ahb2apb_disp0p", \
			EXYNOS7420_ENABLE_PCLK_DISP, 27, 0, NULL),
	GATE(pclk_hdmiphy, "pclk_hdmiphy", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 26, 0, NULL),
	/* pclk_bts */
	GATE(none, "pclk_bts_axi_disp_ro_0", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 22, 0, "pclk_bts_axi_disp_ro_0"),
	GATE(none, "pclk_bts_axi_disp_ro_1", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 21, 0, "pclk_bts_axi_disp_ro_1"),
	GATE(none, "pclk_bts_axi_disp_rw_0", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 20, 0, "pclk_bts_axi_disp_rw_0"),
	GATE(none, "pclk_bts_axi_disp_rw_1", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 19, 0, "pclk_bts_axi_disp_rw_1"),

	GATE(none, "pclk_bts_mscl0", "dout_pclk_mscl", \
			EXYNOS7420_ENABLE_PCLK_MSCL, 28, 0, "pclk_bts_mscl0"),
	GATE(none, "pclk_bts_mscl1", "dout_pclk_mscl", \
			EXYNOS7420_ENABLE_PCLK_MSCL, 27, 0, "pclk_bts_mscl1"),
	GATE(none, "pclk_bts_jpeg", "dout_pclk_mscl", \
			EXYNOS7420_ENABLE_PCLK_MSCL, 26, 0, "pclk_bts_jpeg"),

	/* pclk_disp(decon-int/ext) */
	GATE(pclk_disp, "pclk_ahb2apb_disp0p", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP, 25, 0, NULL),

	GATE(sclk_dsd, "user_sclk_dsd", "usermux_sclk_dsd", \
			EXYNOS7420_ENABLE_SCLK_DISP1, 16, 0, NULL),
	GATE(none, "sclk_hdmi_spdif_gated", "top_sclk_hdmi_spdif", \
			EXYNOS7420_ENABLE_SCLK_DISP1, 12, 0, NULL),
	GATE(decon0_eclk, "decon0_eclk", "dout_sub_sclk_decon_int_eclk", \
			EXYNOS7420_ENABLE_SCLK_DISP1, 28, 0, NULL),
	GATE(decon0_vclk, "decon0_vclk", "dout_sub_sclk_decon_int_vclk", \
			EXYNOS7420_ENABLE_SCLK_DISP1, 8, 0, NULL),
	GATE(mipi0_rx, "mipidphy0_rx", "usermux_mipidphy0_rx", \
			EXYNOS7420_ENABLE_SCLK_DISP2, 12, 0, NULL),
	GATE(mipi0_bit, "mipidphy0_bit", "usermux_mipidphy0_bit", \
			EXYNOS7420_ENABLE_SCLK_DISP2, 8, 0, NULL),
	GATE(mipi1_rx, "mipidphy1_rx", "usermux_mipidphy1_rx", \
			EXYNOS7420_ENABLE_SCLK_DISP2, 4, 0, NULL),
	GATE(mipi1_bit, "mipidphy1_bit", "usermux_mipidphy1_bit", \
			EXYNOS7420_ENABLE_SCLK_DISP2, 0, 0, NULL),
	GATE(decon1_eclk, "decon1_eclk", "dout_sub_sclk_decon_ext_eclk", \
			EXYNOS7420_ENABLE_SCLK_DISP1, 24, 0, NULL),
	GATE(decon1_vclk, "decon1_vclk", "mout_decon_ext_vclk", \
			EXYNOS7420_ENABLE_SCLK_DISP1, 4, 0, NULL),
	GATE(rgb_vclk0, "rgb_vclk0", "sclk_rgb_vclk0", \
			EXYNOS7420_ENABLE_SCLK_DISP2, 28, 0, NULL),
	GATE(rgb_vclk1, "rgb_vclk1", "sclk_rgb_vclk1", \
			EXYNOS7420_ENABLE_SCLK_DISP2, 24, 0, NULL),

	GATE(none, "sclk_dp_link_24m", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_DISP4, 24, 0, NULL),
	GATE(none, "sclk_dp_phy_24m", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_DISP4, 20, 0, NULL),

	/* disp sysmmu */
	GATE(CLK_ACLK_DISP_RO_SYSMMU, "aclk_smmu_disp_ro", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP_RO_SMMU, 0, 0, NULL),
	GATE(CLK_ACLK_DISP_RW_SYSMMU, "aclk_smmu_disp_rw", "usermux_aclk_disp_400", \
			EXYNOS7420_ENABLE_ACLK_DISP_RW_SMMU, 0, 0, NULL),
	GATE(CLK_PCLK_DISP_RO_SYSMMU, "pclk_smmu_disp_ro", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP_RO_SMMU, 0, 0, NULL),
	GATE(CLK_PCLK_DISP_RW_SYSMMU, "pclk_smmu_disp_rw", "dout_pclk_disp", \
			EXYNOS7420_ENABLE_PCLK_DISP_RW_SMMU, 0, 0, NULL),


	/* fsys0 */
	GATE(aclk_xiu_modemx, "aclk_xiu_modemx", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 24, 0, NULL),
	GATE(aclk_xiu_llisfrx, "aclk_xiu_llisfrx", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 22, 0, NULL),
	GATE(aclk_axius_lli_be, "aclk_axius_lli_be", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 17, 0, NULL),
	GATE(aclk_axius_lli_ll, "aclk_axius_lli_ll", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 16, 0, NULL),
	GATE(aclk_axius_llisfrx_llill, "aclk_axius_llisfrx_llill", \
			"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 14, 0, NULL),
	GATE(aclk_axius_llisfrx_llibe, "aclk_axius_llisfrx_llibe", \
			"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 13, 0, NULL),
	GATE(pdma0, "aclk_pdma0", "aclk_axius_pdmax", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 4, 0, NULL),
	GATE(pdma1, "aclk_pdma1", "aclk_axius_pdmax", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 3, 0, NULL),
	GATE(sclk_mmc2, "sclk_mmc2", "usermux_sclk_mmc2", \
			EXYNOS7420_ENABLE_SCLK_FSYS01, 24, 0, NULL),
#ifndef CONFIG_MMC_DW_EXYNOS
	GATE(none, "aclk_mmc2", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 31, 0, NULL),
#endif
	GATE(aclk_usbdrd300, "aclk_usbdrd300", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 29, 0, NULL),
	GATE(aclk_pcie_modem_mstr_aclk, "aclk_pcie_modem_mstr_aclk", \
			"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 27, 0, NULL),
	GATE(aclk_pcie_modem_slv_aclk, "aclk_pcie_modem_slv_aclk", \
			"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 26, 0, NULL),
	GATE(aclk_pcie_modem_dbi_aclk, "aclk_pcie_modem_dbi_aclk", \
			"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 25, 0, NULL),
	GATE(aclk_lli_ll_init, "aclk_lli_ll_init", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 24, 0, NULL),
	GATE(aclk_lli_ll_targ, "aclk_lli_ll_targ", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 23, 0, NULL),
	GATE(aclk_lli_be_init, "aclk_lli_be_init", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 22, 0, NULL),
	GATE(aclk_lli_be_targ, "aclk_lli_be_targ", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 21, 0, NULL),
	GATE(aclk_lli_svc_loc, "aclk_lli_svc_loc", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 20, 0, NULL),
	GATE(aclk_lli_svc_rem, "aclk_lli_svc_rem", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 19, 0, NULL),
	GATE(aclk_combo_phy_modem_pcs_pclk, "aclk_combo_phy_modem_pcs_pclk", \
			"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 18, 0, NULL),
	GATE(sclk_phy_fsys0, "sclk_phy_fsys0", "usermux_sclk_phy_fsys0", \
			EXYNOS7420_ENABLE_SCLK_FSYS01, 16, 0, NULL),
	GATE(sclk_pcie_modem_gated, "sclk_pcie_modem_gated", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_FSYS01, 12, 0, NULL),
	GATE(user_sclk_usbdrd300, "user_sclk_usbdrd300", "usermux_sclk_usbdrd300", \
			EXYNOS7420_ENABLE_SCLK_FSYS01, 4, 0, NULL),
	GATE(user_phyclk_usbdrd300_udrd30_phyclock, \
			"user_phyclk_usbdrd300_udrd30_phyclock", \
			"usermux_phyclk_usbdrd300_udrd30_phyclock", \
			EXYNOS7420_ENABLE_SCLK_FSYS02, 28, 0, NULL),
	GATE(user_phyclk_usbdrd300_udrd30_pipe_pclk, \
			"user_phyclk_usbdrd300_udrd30_pipe_pclk", \
			"usermux_phyclk_usbdrd300_udrd30_pipe_pclk", \
			EXYNOS7420_ENABLE_SCLK_FSYS02, 24, 0, NULL),
	GATE(user_phyclk_usbhost20_phy_freeclk__hsic1, \
			"user_phyclk_usbhost20_phy_freeclk__hsic1", \
			"usermux_phyclk_usbhost20_phy_freeclk_hsic1", \
			EXYNOS7420_ENABLE_SCLK_FSYS02, 20, 0, NULL),
	GATE(user_phyclk_usbhost20_phy_phyclk__hsic1, \
			"user_phyclk_usbhost20_phy_phyclk__hsic1", \
			"usermux_phyclk_usbhost20_phy_phyclk_hsic1", \
			EXYNOS7420_ENABLE_SCLK_FSYS02, 16, 0, NULL),
	GATE(sclk_combo_phy_modem_26m, "sclk_combo_phy_modem_26m", \
			"mout_fsys0_phyclk_sel0", \
			EXYNOS7420_ENABLE_SCLK_FSYS04, 8, 0, NULL),
	GATE(aclk_ahb_usbdrd300_linkh, "aclk_ahb_usbdrd300_linkh", \
			"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 29, 0, NULL),
	GATE(aclk_ahb_usbhs, "aclk_ahb_usbhs", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 21, 0, NULL),
	GATE(aclk_ahb2axi_usbhs_modemx, "aclk_ahb2axi_usbhs_modemx", \
			"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 20, 0,NULL),
	GATE(aclk_axius_usbdrd30x_fsys0x, "aclk_axius_usbdrd30x_fsys0x", \
			"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 19, 0, "etr_clk"),
	GATE(none, "aclk_axius_pdmax", "aclk_xiu_pdmax", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 18, 0, NULL),
	GATE(none, "aclk_xiu_pdmax", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 23, 0, NULL), \
	GATE(aclk_axius_usbhs_fsys0x, "aclk_axius_usbhs_fsys0x", \
			"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 15, 0, NULL),
	GATE(none, "aclk_bts_usbdrd300", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 12, 0, "aclk_bts_usbdrd300"),
	GATE(none, "aclk_bts_sdcardx", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 11, 0, "aclk_bts_sdcardx"),
	GATE(none, "aclk_bts_modemx", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS00, 10, 0, "aclk_bts_modemx"),
	GATE(aclk_usbhost20, "aclk_usbhost20", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS01, 28, 0, NULL),
	GATE(none, "pclk_bts_usbdrd300" ,"usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_PCLK_FSYS0, 29, 0, "pclk_bts_usbdrd300"),
	GATE(none, "pclk_bts_sdcardx", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_PCLK_FSYS0, 28, 0, "pclk_bts_sdcardx"),
	GATE(none, "pclk_bts_modemx", "usermux_aclk_fsys0_200", \
			EXYNOS7420_ENABLE_PCLK_FSYS0, 27, 0, "pclk_bts_modemx"),
	GATE(pclk_async_combo_phy_modem, "pclk_async_combo_phy_modem", \
			"dout_pclk_combo_phy_modem", \
			EXYNOS7420_ENABLE_PCLK_FSYS0, 23, 0, NULL),
	GATE(sclk_usbdrd300_ref_clk, "sclk_usbdrd300_ref_clk", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_FSYS01, 8, 0, NULL),
	GATE(none, "sclk_pcie_modem", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_FSYS01, 12, 0, NULL),
	GATE(user_phyclk_lli_tx0_symbol, "user_phyclk_lli_tx0_symbol", \
			"usermux_phyclk_lli_tx0", \
			EXYNOS7420_ENABLE_SCLK_FSYS03, 20, 0, NULL),
	GATE(user_phyclk_lli_rx0_symbol, "user_phyclk_lli_rx0_symbol", \
			"usermux_phyclk_lli_rx0", \
			EXYNOS7420_ENABLE_SCLK_FSYS03, 16, 0, NULL),
	GATE(phyclk_pcie_tx0_gated, "phyclk_pcie_tx0_gated", "usermux_phyclk_pcie_tx0", \
			EXYNOS7420_ENABLE_SCLK_FSYS03, 12, 0, NULL),
	GATE(phyclk_pcie_rx0_gated, "phyclk_pcie_rx0_gated", "usermux_phyclk_pcie_rx0", \
			EXYNOS7420_ENABLE_SCLK_FSYS03, 8, 0, NULL),
	GATE(oscclk_phy_clkout_usb300_phy, "oscclk_phy_clkout_usb300_phy", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_FSYS04, 28, 0, NULL),
	GATE(oscclk_phy_clkout_usb20_hsic_phy, \
			"oscclk_phy_clkout_usb20_hsic_phy", "oscclk_phy_div2", \
			EXYNOS7420_ENABLE_SCLK_FSYS04, 24, 0, NULL),
	GATE(sclk_usb20phy_hsic_pll480mclk, \
			"sclk_usb20phy_hsic_pll480mclk", "hsic_pll", \
			EXYNOS7420_ENABLE_SCLK_FSYS04, 16, 0, NULL),
	GATE(sclk_usbhost20_clk48mohci, "sclk_usbhost20_clk48mohci", \
			"usb20_div", \
			EXYNOS7420_ENABLE_SCLK_FSYS04, 12, 0, NULL),
	/* fsys1 */
	GATE(sclk_mmc0, "sclk_mmc0", "usermux_sclk_mmc0", \
			EXYNOS7420_ENABLE_SCLK_FSYS11, 28, 0, NULL),
#ifndef CONFIG_MMC_DW_EXYNOS
	GATE(none, "aclk_mmc0", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 30, 0, NULL),
	GATE(none, "aclk_mmc1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 29, 0, NULL),
#endif
	GATE(sclk_mmc1, "sclk_mmc1", "usermux_sclk_mmc1", \
			EXYNOS7420_ENABLE_SCLK_FSYS11, 24, 0, NULL),

	GATE(aclk_xiu_wifi1x, "aclk_xiu_wifi1x", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 24, 0, NULL),
	GATE(none, "aclk_axius_tlx400_wifi1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 21, 0, NULL),
	GATE(none, "aclk_xiuasync_tlx400_axius_wifi1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 20, 0, NULL),
	GATE(none, "aclk_bts_embedded", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 18, 0, "aclk_bts_embedded"),
	GATE(none, "aclk_bts_wifi1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 17, 0, "aclk_bts_wifi1"),
	GATE(aclk_ahb2axi_pcie_wifi1, "aclk_ahb2axi_pcie_wifi1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 13, 0, NULL),
	GATE(aclk_pcie_wifi1_mstr_aclk, "aclk_pcie_wifi1_mstr_aclk", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 12, 0, NULL),
	GATE(aclk_pcie_wifi1_slv_aclk, "aclk_pcie_wifi1_slv_aclk", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 11, 0, NULL),
	GATE(aclk_pcie_wifi1_dbi_aclk, "aclk_pcie_wifi1_dbi_aclk", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 10, 0, NULL),
	GATE(aclk_combo_phy_pcs_pclk_wifi1, "aclk_combo_phy_pcs_pclk_wifi1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 9, 0, NULL),

	GATE(none, "pclk_tlx400_wifi1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_PCLK_FSYS1, 31, 0, NULL),
	GATE(none, "pclk_xiuasync_tlx400_axius_wifi1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_PCLK_FSYS1, 29, 0, NULL),
	GATE(none, "pclk_xiuasync_axisel_tlx400_wifi1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_PCLK_FSYS1, 28, 0, NULL),
	GATE(none, "pclk_bts_embedded", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_PCLK_FSYS1, 27, 0, "pclk_bts_embedded"),
	GATE(none, "pclk_bts_wifi1", "usermux_aclk_fsys1_200", \
			EXYNOS7420_ENABLE_PCLK_FSYS1, 26, 0, "pclk_bts_wifi1"),
	GATE(pclk_async_combo_phy_wifi1, "pclk_async_combo_phy_wifi1", "dout_pclk_combo_phy_wifi1", \
			EXYNOS7420_ENABLE_PCLK_FSYS1, 22, 0, NULL),
	GATE(aclk_ufs20_link, "aclk_ufs20_link", "dout_pclk_fsys1", \
			EXYNOS7420_ENABLE_ACLK_FSYS1, 31, 0, NULL),
	GATE(sclk_ufsunipro20_gated, "sclk_ufsunipro20_gated", "usermux_sclk_ufsunipro20", \
			EXYNOS7420_ENABLE_SCLK_FSYS11, 20, 0, NULL),
	GATE(sclk_combo_phy_embedded_26m_gated, "sclk_combo_phy_embedded_26m_gated", "mout_fsys1_phyclk_sel1", \
			EXYNOS7420_ENABLE_SCLK_FSYS13, 24, 0, NULL),
	GATE(sclk_phy_fsys1_gated, "sclk_phy_fsys1_gated", "usermux_sclk_phy_fsys1", \
			EXYNOS7420_ENABLE_SCLK_FSYS11, 16, 0, NULL),
	GATE(none, "sclk_tlx400_wifi1_slow", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_FSYS11, 12, 0, NULL),
	GATE(none, "sclk_tlx400_wifi1_tx", "usermux_sclk_tlx400_wifi1", \
			EXYNOS7420_ENABLE_SCLK_FSYS11, 8, 0, NULL),
	GATE(sclk_pcie_link_wifi1_gated, "sclk_pcie_link_wifi1_gated", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_FSYS11, 0, 0, NULL),
	GATE(phyclk_ufs20_tx0_symbol_gated, "phyclk_ufs20_tx0_symbol_gated", \
			"usermux_phyclk_ufs20_tx0_symbol", \
			EXYNOS7420_ENABLE_SCLK_FSYS12, 28, CLK_GATE_ENABLE, NULL),
	GATE(phyclk_ufs20_tx1_symbol_gated, "phyclk_ufs20_tx1_symbol_gated", \
			"usermux_phyclk_ufs20_tx1_symbol", \
			EXYNOS7420_ENABLE_SCLK_FSYS12, 28, CLK_GATE_ENABLE, NULL),
	GATE(phyclk_ufs20_rx0_symbol_gated, "phyclk_ufs20_rx0_symbol_gated", \
			"usermux_phyclk_ufs20_rx0_symbol", \
			EXYNOS7420_ENABLE_SCLK_FSYS12, 24, CLK_GATE_ENABLE, NULL),
	GATE(phyclk_ufs20_rx1_symbol_gated, "phyclk_ufs20_rx1_symbol_gated", \
			"usermux_phyclk_ufs20_rx1_symbol", \
			EXYNOS7420_ENABLE_SCLK_FSYS12, 16, CLK_GATE_ENABLE, NULL),
	GATE(phyclk_pcie_wifi1_tx0_gated, "phyclk_pcie_wifi1_tx0_gated", "usermux_phyclk_pcie_wifi1_tx0", \
			EXYNOS7420_ENABLE_SCLK_FSYS12, 12, 0, NULL),
	GATE(phyclk_pcie_wifi1_rx0_gated, "phyclk_pcie_wifi1_rx0_gated", "usermux_phyclk_pcie_wifi1_rx0", \
			EXYNOS7420_ENABLE_SCLK_FSYS12, 8, 0, NULL),
	GATE(oscclk_phy_clkout_embedded_combo_phy, "oscclk_phy_clkout_embedded_combo_phy", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_FSYS12, 4, 0, NULL),

	GATE(sclk_combo_phy_wifi1_26m_gated, "sclk_combo_phy_wifi1_26m_gated", "mout_fsys1_phyclk_sel0", \
			EXYNOS7420_ENABLE_SCLK_FSYS13, 28, 0, NULL),

	/* ccore */
	GATE(none, "aclk_cci", "usermux_aclk_ccore_532", \
			EXYNOS7420_ENABLE_ACLK_CCORE0, 0, CLK_GATE_ENABLE, NULL),
	GATE(rtc, "pclk_rtc", "usermux_aclk_ccore_133", \
			EXYNOS7420_ENABLE_PCLK_CCORE, 8, 0, NULL),
	GATE(none, "aclk_noc_p_ccore", "usermux_aclk_ccore_133", \
			EXYNOS7420_ENABLE_ACLK_CCORE1, 16, CLK_GATE_ENABLE, NULL),
	GATE(none, "aclk_ixiu", "dout_aclk_ccore_532", \
			EXYNOS7420_ENABLE_ACLK_CCORE0, 2, 0, NULL),
	GATE(none, "pclk_bts_apl", "usermux_aclk_ccore_133", \
			EXYNOS7420_ENABLE_PCLK_CCORE, 21, 0, "pclk_bts_apl"),
	GATE(none, "pclk_bts_ats", "usermux_aclk_ccore_133", \
			EXYNOS7420_ENABLE_PCLK_CCORE, 22, 0, "pclk_bts_ats"),

	/* mscl */
	/*
	 * Just handle aclk_xxx and pclk_xxx which are the leaf gates of the
	 * clock hierarch. All bus gates are attached to the leaf gates as the
	 * parent of the parents of the leaf gates.
	 * Since the roots of aclk and pclk are different, both gates should be
	 * handled separately.
	 * sclk_jpeg is not the leaf gate but the parent of aclk_jpeg.
	 */

	/* hierarch of ACLK_LH_MSCL0 */
	GATE(CLK_ACLK_LH_ASYNC_SI_MSCL0, "aclk_lhasync_mscl0", "aclk_lh_mscl0", EXYNOS7420_ENABLE_ACLK_MSCL, 27, 0, NULL),
	GATE(CLK_ACLK_XIU_MSCLX, "aclk_xiu_mscl0", "aclk_lhasync_mscl0", EXYNOS7420_ENABLE_ACLK_MSCL, 25, 0, NULL),
	GATE(CLK_ACLK_M2MSCALER0, "aclk_m2mscaler0", "aclk_xiu_mscl0", EXYNOS7420_ENABLE_ACLK_MSCL, 31, 0, NULL),
	GATE(CLK_SCLK_JPEG_GATED, "sclk_jpeg", "aclk_xiu_mscl0", EXYNOS7420_ENABLE_SCLK_MSCL, 28, 0, NULL),
	GATE(CLK_ACLK_JPEG, "aclk_jpeg", "sclk_jpeg", EXYNOS7420_ENABLE_ACLK_MSCL, 29, 0, NULL),
	GATE(CLK_ACLK_SYSMMU_MSCL0, "aclk_smmu_mscl0", "aclk_xiu_mscl0", EXYNOS7420_ENABLE_ACLK_MSCL_SMMU0, 0, 0, NULL),

	/* hierarch of ACLK_LH_MSCL1 */
	GATE(CLK_ACLK_LH_ASYNC_SI_MSCL1, "aclk_lhasync_mscl1", "aclk_lh_mscl1", EXYNOS7420_ENABLE_ACLK_MSCL, 26, 0, NULL),
	GATE(CLK_ACLK_ACLK2ACEL_BRDG, "aclk_axi2acel_brdg", "aclk_lhasync_mscl1", EXYNOS7420_ENABLE_ACLK_MSCL, 23, 0, NULL),
	GATE(CLK_ACLK_XIU_MSCL1, "aclk_xiu_mscl1", "aclk_axi2acel_brdg", EXYNOS7420_ENABLE_ACLK_MSCL, 24, 0, NULL),
	GATE(CLK_ACLK_M2MSCALER1, "aclk_m2mscaler1", "aclk_xiu_mscl1", EXYNOS7420_ENABLE_ACLK_MSCL, 30, 0, NULL),
	GATE(CLK_ACLK_G2D, "aclk_g2d", "aclk_xiu_mscl1", EXYNOS7420_ENABLE_ACLK_MSCL, 28, 0, NULL),
	GATE(CLK_ACLK_SYSMMU_MSCL1, "aclk_smmu_mscl1", "aclk_xiu_mscl1", EXYNOS7420_ENABLE_ACLK_MSCL_SMMU1, 0, 0, NULL),
	GATE(CLK_ACLK_G2D_SYSMMU, "aclk_smmu_g2d", "aclk_xiu_mscl1", EXYNOS7420_ENABLE_ACLK_G2D_SMMU, 0, 0, NULL),

	/* hierarch of DOUT_PCLK_MSCL */
	GATE(CLK_PCLK_M2MSCALER0, "pclk_m2mscaler0", "dout_pclk_mscl", EXYNOS7420_ENABLE_PCLK_MSCL, 31, 0, NULL),
	GATE(CLK_PCLK_M2MSCALER1, "pclk_m2mscaler1", "dout_pclk_mscl", EXYNOS7420_ENABLE_PCLK_MSCL, 30, 0, NULL),
	GATE(CLK_PCLK_JPEG, "pclk_jpeg", "dout_pclk_mscl", EXYNOS7420_ENABLE_PCLK_MSCL, 29, 0, NULL),
	GATE(CLK_PCLK_G2D, "pclk_g2d", "dout_pclk_mscl", EXYNOS7420_ENABLE_PCLK_MSCL, 28, 0, NULL),
	GATE(CLK_PCLK_SYSMMU_MSCL0, "pclk_smmu_mscl0", "dout_pclk_mscl", EXYNOS7420_ENABLE_PCLK_MSCL_SMMU0, 0, 0, NULL),
	GATE(CLK_PCLK_SYSMMU_MSCL1, "pclk_smmu_mscl1", "dout_pclk_mscl", EXYNOS7420_ENABLE_PCLK_MSCL_SMMU1, 0, 0, NULL),
	GATE(CLK_PCLK_G2D_SYSMMU, "pclk_smmu_g2d", "dout_pclk_mscl", EXYNOS7420_ENABLE_PCLK_G2D_SMMU, 0, 0, NULL),

	/* top - isp0 */
	GATE(aclk_isp0_isp0_590, "aclk_isp0_isp0_590", "mout_aclk_isp0_isp0_590", \
			EXYNOS7420_ENABLE_ACLK_TOP04, 28, 0, 0),
	GATE(aclk_isp0_tpu_590, "aclk_isp0_tpu_590", "mout_aclk_isp0_tpu_590", \
			EXYNOS7420_ENABLE_ACLK_TOP04, 24, 0, 0),
	GATE(aclk_isp0_trex_532, "aclk_isp0_trex_532", "mout_aclk_isp0_trex_532", \
			EXYNOS7420_ENABLE_ACLK_TOP04, 20, 0, 0),

	/* top - isp1 */
	GATE(aclk_isp1_isp1_468, "aclk_isp1_isp1_468", "mout_aclk_isp1_isp1_468", \
			EXYNOS7420_ENABLE_ACLK_TOP04, 16, 0, 0),
	GATE(aclk_isp1_ahb_117, "aclk_isp1_ahb_117", "mout_aclk_isp1_ahb_117", \
			EXYNOS7420_ENABLE_ACLK_TOP04, 12, 0, 0),

	/* top - cam0 */
	GATE(aclk_cam0_csis0_690, "aclk_cam0_csis0_690", "dout_aclk_cam0_csis0_690", \
			EXYNOS7420_ENABLE_ACLK_TOP05, 28, 0, 0),
	GATE(aclk_cam0_bnsa_690, "aclk_cam0_bnsa_690", "dout_aclk_cam0_bnsa_690", \
			EXYNOS7420_ENABLE_ACLK_TOP05, 24, 0, 0),
	GATE(aclk_cam0_bnsb_690, "aclk_cam0_bnsb_690", "dout_aclk_cam0_bnsb_690", \
			EXYNOS7420_ENABLE_ACLK_TOP05, 20, 0, 0),
	GATE(aclk_cam0_bnsd_690, "aclk_cam0_bnsd_690", "dout_aclk_cam0_bnsd_690", \
			EXYNOS7420_ENABLE_ACLK_TOP05, 16, 0, 0),
	GATE(aclk_cam0_csis1_174, "aclk_cam0_csis1_174", "dout_aclk_cam0_csis1_174", \
			EXYNOS7420_ENABLE_ACLK_TOP05, 12, 0, 0),
	GATE(aclk_cam0_csis3_133, "aclk_cam0_csis3_133", "dout_aclk_cam0_csis3_133", \
			EXYNOS7420_ENABLE_ACLK_TOP05, 8, 0, 0),
	GATE(aclk_cam0_3aa0_690, "aclk_cam0_3aa0_690", "dout_aclk_cam0_3aa0_690", \
			EXYNOS7420_ENABLE_ACLK_TOP06, 28, 0, 0),
	GATE(aclk_cam0_3aa1_468, "aclk_cam0_3aa1_468", "dout_aclk_cam0_3aa1_468", \
			EXYNOS7420_ENABLE_ACLK_TOP06, 24, 0, 0),
	GATE(aclk_cam0_trex_532, "aclk_cam0_trex_532", "dout_aclk_cam0_trex_532", \
			EXYNOS7420_ENABLE_ACLK_TOP06, 20, 0, 0),
	GATE(aclk_cam0_nocp_133, "aclk_cam0_nocp_133", "dout_aclk_cam0_nocp_133", \
			EXYNOS7420_ENABLE_ACLK_TOP06, 16, 0, 0),

	/* top - cam1 */
	GATE(aclk_cam1_sclvra_491, "aclk_cam1_sclvra_491", "dout_aclk_cam1_sclvra_491", \
			EXYNOS7420_ENABLE_ACLK_TOP07, 28, 0, 0),
	GATE(aclk_cam1_arm_668, "aclk_cam1_arm_668", "dout_aclk_cam1_arm_668", \
			EXYNOS7420_ENABLE_ACLK_TOP07, 24, 0, 0),
	GATE(aclk_cam1_busperi_334, "aclk_cam1_busperi_334", "dout_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_TOP07, 20, 0, 0),
	GATE(aclk_cam1_bnscsis_133, "aclk_cam1_bnscsis_133", "dout_aclk_cam1_bnscsis_133", \
			EXYNOS7420_ENABLE_ACLK_TOP07, 16, 0, 0),
	GATE(aclk_cam1_nocp_133, "aclk_cam1_nocp_133", "dout_aclk_cam1_nocp_133", \
			EXYNOS7420_ENABLE_ACLK_TOP07, 12, 0, 0),
	GATE(aclk_cam1_trex_532, "aclk_cam1_trex_532", "dout_aclk_cam1_trex_532", \
			EXYNOS7420_ENABLE_ACLK_TOP07, 8, 0, 0),
	GATE(sclk_isp_spi0, "sclk_isp_spi0", "dout_sclk_isp_spi0", \
			EXYNOS7420_ENABLE_SCLK_TOP0_CAM10, 20, 0, 0),
	GATE(sclk_isp_spi1, "sclk_isp_spi1", "dout_sclk_isp_spi1", \
			EXYNOS7420_ENABLE_SCLK_TOP0_CAM10, 8, 0, 0),
	GATE(sclk_isp_uart, "sclk_isp_uart", "dout_sclk_isp_uart", \
			EXYNOS7420_ENABLE_SCLK_TOP0_CAM10, 4, 0, 0),
	GATE(none, "sclk_isp_mtcadc", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_TOP0_CAM10, 0, 0, 0),

	/* cam0 */
	GATE(gate_aclk_csis0_i_wrap, "gate_aclk_csis0_i_wrap", "mout_user_mux_aclk_cam0_csis0_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 16, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_fimc_bns_a, "gate_aclk_fimc_bns_a", "mout_user_mux_aclk_cam0_bnsa_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 2, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_trex_a_5x1_bns_a, "gate_aclk_trex_a_5x1_bns_a", "mout_user_mux_aclk_cam0_bnsa_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 23, CLK_IGNORE_UNUSED, 0),

	GATE(gate_pclk_fimc_bns_a, "gate_pclk_fimc_bns_a", "dout_clkdiv_pclk_cam0_bnsa_345", \
			EXYNOS7420_ENABLE_PCLK_CAM00, 2, CLK_IGNORE_UNUSED, 0),
	GATE(gate_cclk_asyncapb_socp_fimc_bns_a, "gate_cclk_asyncapb_socp_fimc_bns_a", "dout_clkdiv_pclk_cam0_bnsa_345", \
			EXYNOS7420_ENABLE_PCLK_CAM01, 22, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_fimc_bns_b, "gate_aclk_fimc_bns_b", "mout_user_mux_aclk_cam0_bnsb_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 3, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_trex_a_5x1_bns_b, "gate_aclk_trex_a_5x1_bns_b", "mout_user_mux_aclk_cam0_bnsb_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 24, CLK_IGNORE_UNUSED, 0),

	GATE(gate_pclk_fimc_bns_b, "gate_pclk_fimc_bns_b", "dout_clkdiv_pclk_cam0_bnsb_345", \
			EXYNOS7420_ENABLE_PCLK_CAM00, 3, CLK_IGNORE_UNUSED, 0),
	GATE(gate_cclk_asyncapb_socp_fimc_bns_b, "gate_cclk_asyncapb_socp_fimc_bns_b", "dout_clkdiv_pclk_cam0_bnsb_345", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 23, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_fimc_bns_d, "gate_aclk_fimc_bns_d", "mout_user_mux_aclk_cam0_bnsd_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 4, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_trex_a_5x1_bns_d, "gate_aclk_trex_a_5x1_bns_d", "mout_user_mux_aclk_cam0_bnsd_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 25, CLK_IGNORE_UNUSED, 0),

	GATE(gate_pclk_fimc_bns_d, "gate_pclk_fimc_bns_d", "dout_clkdiv_pclk_cam0_bnsd_345", \
			EXYNOS7420_ENABLE_PCLK_CAM00, 4, CLK_IGNORE_UNUSED, 0),
	GATE(gate_cclk_asyncapb_socp_fimc_bns_d, "gate_cclk_asyncapb_socp_fimc_bns_d", "dout_clkdiv_pclk_cam0_bnsd_345", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 24, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_csis1_i_wrap, "gate_aclk_csis1_i_wrap", "mout_user_mux_aclk_cam0_csis1_174", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 17, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_csis3_i_wrap, "gate_aclk_csis3_i_wrap", "mout_user_mux_aclk_cam0_csis3_133", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 18, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_fimc_3aa0, "gate_aclk_fimc_3aa0", "mout_user_mux_aclk_cam0_3aa0_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 0, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_trex_a_5x1_aa0, "gate_aclk_trex_a_5x1_aa0", "mout_user_mux_aclk_cam0_3aa0_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 21, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_pxl_asbs_fimc_bns_c, "gate_aclk_pxl_asbs_fimc_bns_c", "mout_user_mux_aclk_cam0_3aa0_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 11, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_pxl_asbs_3aa0_in, "gate_aclk_pxl_asbs_3aa0_in", "mout_user_mux_aclk_cam0_3aa0_690", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 9, CLK_IGNORE_UNUSED, 0),

	GATE(gate_pclk_fimc_3aa0, "gate_pclk_fimc_3aa0", "dout_clkdiv_pclk_cam0_3aa0_345", \
			EXYNOS7420_ENABLE_PCLK_CAM00, 0, CLK_IGNORE_UNUSED, 0),
	GATE(gate_cclk_asyncapb_socp_3aa0, "gate_cclk_asyncapb_socp_3aa0", "dout_clkdiv_pclk_cam0_3aa0_345", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 20, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_fimc_3aa1, "gate_aclk_fimc_3aa1", "mout_user_mux_aclk_cam0_3aa1_468", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 1, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_trex_a_5x1_aa1, "gate_aclk_trex_a_5x1_aa1", "mout_user_mux_aclk_cam0_3aa1_468", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 22, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_pxl_asbs_3aa1_in, "gate_aclk_pxl_asbs_3aa1_in", "mout_user_mux_aclk_cam0_3aa1_468", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 10, CLK_IGNORE_UNUSED, 0),

	GATE(gate_pclk_fimc_3aa1, "gate_pclk_fimc_3aa1", "dout_clkdiv_pclk_cam0_3aa1_234", \
			EXYNOS7420_ENABLE_PCLK_CAM00, 1, CLK_IGNORE_UNUSED, 0),
	GATE(gate_cclk_asyncapb_socp_3aa1, "gate_cclk_asyncapb_socp_3aa1", "dout_clkdiv_pclk_cam0_3aa1_234", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 21, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_trex_a_5x1, "gate_aclk_trex_a_5x1", "mout_user_mux_aclk_cam0_trex_532", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 20, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_axi_lh_async_si_top_cam0, "gate_aclk_axi_lh_async_si_top_cam0", "mout_user_mux_aclk_cam0_trex_532", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 10, CLK_IGNORE_UNUSED, 0),

	GATE(gate_pclk_csis0, "gate_pclk_csis0", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_CAM00, 16, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_csis1, "gate_pclk_csis1", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_CAM00, 17, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_csis3, "gate_pclk_csis3", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_CAM00, 18, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_axi2apb_bridge_is0p, "gate_aclk_axi2apb_bridge_is0p", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 9, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_asyncapb_socp_3aa0, "gate_pclk_asyncapb_socp_3aa0", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_CAM01, 20, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_asyncapb_socp_3aa1, "gate_pclk_asyncapb_socp_3aa1", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_CAM01, 21, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_asyncapb_socp_fimc_bns_a, "gate_pclk_asyncapb_socp_fimc_bns_a", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_CAM01, 22, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_asyncapb_socp_fimc_bns_b, "gate_pclk_asyncapb_socp_fimc_bns_b", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_CAM01, 23, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_asyncapb_socp_fimc_bns_d, "gate_pclk_asyncapb_socp_fimc_bns_d", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_CAM01, 24, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_freeruncnt, "gate_pclk_freeruncnt", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_CAM01, 30, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_is0x, "gate_aclk_xiu_is0x", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 19, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_axi2ahb_is0p, "gate_aclk_axi2ahb", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 8, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclk_ahbsyncdn_cam0, "gate_hclk_ahbsyncdn_cam0", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 12, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_async_mi_cam0, "gate_aclk_xiu_async_mi_cam0", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 16, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_xiu_async_mi_cam0, "gate_pclk_xiu_async_mi_cam0", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_CAM01, 16, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_async_mi_is0x, "gate_aclk_xiu_async_mi_is0x", "dout_clkdiv_pclk_cam0_trex_266", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 17, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_xiu_async_mi_is0x, "gate_pclk_xiu_async_mi_is0x", "dout_clkdiv_pclk_cam0_trex_133", \
			EXYNOS7420_ENABLE_PCLK_CAM01, 17, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_pmu_cam0, "gate_pclk_pmu_cam0", "dout_clkdiv_pclk_cam0_trex_133", \
			EXYNOS7420_ENABLE_PCLK_CAM00, 31, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclk_ahb2apb_bridge_is0p, "gate_hclk_ahb2apb_bridge_is0p", "dout_clkdiv_pclk_cam0_trex_133", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 9, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_trex_a_5x1, "gate_pclk_trex_a_5x1", "dout_clkdiv_pclk_cam0_trex_133", \
			EXYNOS7420_ENABLE_PCLK_CAM00, 20, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_pxl_asbs_fimc_bns_c_int, "gate_aclk_pxl_asbs_fimc_bns_c_int", "fin_pll", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 12, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_pxl_asbm_fimc_bns_c_int, "gate_aclk_pxl_asbm_fimc_bns_c_int", "fin_pll", \
			EXYNOS7420_ENABLE_ACLK_CAM00, 8, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_200_cam0_noc_p_cam0, "gate_aclk_200_cam0_noc_p_cam0", "mout_user_mux_aclk_cam0_nocp_133", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 28, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_async_si_is0x, "gate_aclk_xiu_async_si_is0x", "mout_user_mux_aclk_cam0_nocp_133", \
			EXYNOS7420_ENABLE_ACLK_CAM01, 18, CLK_IGNORE_UNUSED, 0),

	GATE(gate_user_phyclk_rxbyteclkhs0_s2a, "gate_user_phyclk_rxbyteclkhs0_s2a", "mout_user_mux_phyclk_rxbyteclkhs0_s2a", \
			EXYNOS7420_ENABLE_SCLK_CAM0, 0, CLK_IGNORE_UNUSED, 0),

	GATE(gate_user_phyclk_rxbyteclkhs0_s4, "gate_user_phyclk_rxbyteclkhs0_s4", "mout_user_mux_phyclk_rxbyteclkhs0_s4", \
			EXYNOS7420_ENABLE_SCLK_CAM0, 4, CLK_IGNORE_UNUSED, 0),

	GATE(gate_user_phyclk_rxbyteclkhs1_s4, "gate_user_phyclk_rxbyteclkhs1_s4", "mout_user_mux_phyclk_rxbyteclkhs1_s4", \
			EXYNOS7420_ENABLE_SCLK_CAM0, 5, CLK_IGNORE_UNUSED, 0),

	GATE(gate_user_phyclk_rxbyteclkhs2_s4, "gate_user_phyclk_rxbyteclkhs2_s4", "mout_user_mux_phyclk_rxbyteclkhs2_s4", \
			EXYNOS7420_ENABLE_SCLK_CAM0, 6, CLK_IGNORE_UNUSED, 0),

	GATE(gate_user_phyclk_rxbyteclkhs3_s4, "gate_user_phyclk_rxbyteclkhs3_s4", "mout_user_mux_phyclk_rxbyteclkhs3_s4", \
			EXYNOS7420_ENABLE_SCLK_CAM0, 7, CLK_IGNORE_UNUSED, 0),

	/* cam1 */
	GATE(gate_aclk_fimc_scaler, "gate_aclk_fimc_scaler", "mout_user_mux_aclk_cam1_sclvra_491", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 2, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_fimc_vra, "gate_aclk_fimc_vra", "mout_user_mux_aclk_cam1_sclvra_491", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 3, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_pxl_asbs_from_blkc, "gate_aclk_pxl_asbs_from_blkc", "mout_user_mux_aclk_cam1_sclvra_491", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 19, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_fimc_scaler, "gate_clk_fimc_scaler", "mout_user_mux_aclk_cam1_sclvra_491", \
			EXYNOS7420_ENABLE_ACLK_CAM13, 2, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_fimc_vra, "gate_clk_fimc_vra", "mout_user_mux_aclk_cam1_sclvra_491", \
			EXYNOS7420_ENABLE_ACLK_CAM13, 3, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_scaler_trex_b, "gate_clk_scaler_trex_b", "mout_user_mux_aclk_cam1_sclvra_491", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 22, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_vra_trex_b, "gate_clk_vra_trex_b", "mout_user_mux_aclk_cam1_sclvra_491", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 25, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_fimc_scaler, "gate_pclk_fimc_scaler", "dout_clkdiv_pclk_cam1_sclvra_246", \
			EXYNOS7420_ENABLE_PCLK_CAM10, 2, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_fimc_vra, "gate_pclk_fimc_vra", "dout_clkdiv_pclk_cam1_sclvra_246", \
			EXYNOS7420_ENABLE_PCLK_CAM10, 2, CLK_IGNORE_UNUSED, 0),
	GATE(gate_cclk_asyncapb_socp_fimc_scaler, "gate_cclk_asyncapb_socp_fimc_scaler", "dout_clkdiv_pclk_cam1_sclvra_246", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 13, CLK_IGNORE_UNUSED, 0),
	GATE(gate_cclk_asyncapb_socp_fimc_vra_s0, "gate_cclk_asyncapb_socp_fimc_vra_s0", "dout_clkdiv_pclk_cam1_sclvra_246", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 14, CLK_IGNORE_UNUSED, 0),
	GATE(gate_cclk_asyncapb_socp_fimc_vra_s1, "gate_cclk_asyncapb_socp_fimc_vra_s1", "dout_clkdiv_pclk_cam1_sclvra_246", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 15, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_xiu_n_async_si_cortex, "gate_aclk_xiu_n_async_si_cortex", "mout_user_mux_aclk_cam1_arm_668",     \
			EXYNOS7420_ENABLE_ACLK_CAM11, 24, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_mcu_isp_400_isp_arm_sys, "gate_clk_mcu_isp_400_isp_arm_sys", "mout_user_mux_aclk_cam1_arm_668", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 18, CLK_IGNORE_UNUSED, 0),

	GATE(gate_atclks_asatbslv_cam1_cssys, "gate_atclks_asatbslv_cam1_cssys", "dout_clkdiv_pclk_cam1_arm_167", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 28, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclkdbg_asapbmst_cssys_cam1, "gate_pclkdbg_asapbmst_cssys_cam1", "dout_clkdiv_pclk_cam1_arm_167",     \
			EXYNOS7420_ENABLE_PCLK_CAM10, 29, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_csatbdownsizer_cam1, "gate_clk_csatbdownsizer_cam1", "dout_clkdiv_pclk_cam1_arm_167", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 7, CLK_IGNORE_UNUSED, 0),

	GATE(gate_aclk_axi2apb_bridge_is3p, "gate_aclk_axi2apb_bridge_is3p", "mout_user_mux_aclk_cam1_busperi_334",     \
			EXYNOS7420_ENABLE_ACLK_CAM11, 16, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_axispcx, "gate_aclk_axispcx", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 17, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_axisphx, "gate_aclk_axisphx", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 18, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_gic_isp_arm_sys, "gate_aclk_gic_isp_arm_sys", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 16, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_r_axispcx, "gate_aclk_r_axispcx", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 20, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_r_axisphx, "gate_aclk_r_axisphx", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 21, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_ispx_1x4, "gate_aclk_xiu_ispx_1x4", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 13, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_n_async_mi_cortex, "gate_aclk_xiu_n_async_mi_cortex", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 21, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_n_async_si_cam1, "gate_aclk_xiu_n_async_si_cam1", "mout_user_mux_aclk_cam1_busperi_334",     \
		EXYNOS7420_ENABLE_ACLK_CAM11, 23, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_n_async_si_to_blkc, "gate_aclk_xiu_n_async_si_to_blkc", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 25, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_n_async_si_to_blkd, "gate_aclk_xiu_n_async_si_to_blkd", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 26, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_isp_cpu_trex_b, "gate_clk_isp_cpu_trex_b", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 17, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclk_ahbsyncdn_isp_peri, "gate_hclk_ahbsyncdn_isp_peri", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 3, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclk_ahbsyncdn_isp2h, "gate_hclk_ahbsyncdn_isp2h", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 4, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclkm_asyncahb_cam1, "gate_hclkm_asyncahb_cam1", "mout_user_mux_aclk_cam1_busperi_334", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 9, CLK_IGNORE_UNUSED, 0),

	GATE(gate_pclk_asyncapb_socp_fimc_bns_c, "gate_pclk_asyncapb_socp_fimc_bns_c", "dout_clkdiv_pclk_cam1_busperi_167", \
			EXYNOS7420_ENABLE_PCLK_CAM11, 12, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_asyncapb_socp_fimc_scaler, "gate_pclk_asyncapb_socp_fimc_scaler", "dout_clkdiv_pclk_cam1_busperi_167", \
			EXYNOS7420_ENABLE_PCLK_CAM11, 13, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_asyncapb_socp_fimc_vra_s0, "gate_pclk_asyncapb_socp_fimc_vra_s0", "dout_clkdiv_pclk_cam1_busperi_167", \
			EXYNOS7420_ENABLE_PCLK_CAM11, 14, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_asyncapb_socp_fimc_vra_s1, "gate_pclk_asyncapb_socp_fimc_vra_s1", "dout_clkdiv_pclk_cam1_busperi_167", \
			EXYNOS7420_ENABLE_PCLK_CAM11, 15, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_csis2, "gate_pclk_csis2", "dout_clkdiv_pclk_cam1_busperi_167", \
			EXYNOS7420_ENABLE_PCLK_CAM10, 6, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_fimc_is_b_glue, "gate_pclk_fimc_is_b_glue", "dout_clkdiv_pclk_cam1_busperi_167", \
			EXYNOS7420_ENABLE_PCLK_CAM10, 1, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_xiu_n_async_mi_cortex ,"gate_pclk_xiu_n_async_mi_cortex", "dout_clkdiv_pclk_cam1_busperi_167", \
			EXYNOS7420_ENABLE_PCLK_CAM11, 21, CLK_IGNORE_UNUSED, 0),

	GATE(gate_hclk_ahb_sfrisp2h, "gate_hclk_ahb_sfrisp2h", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 0, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclk_ahb2apb_bridge_is3p, "gate_hclk_ahb2apb_bridge_is3p", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 1, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclk_ahb2apb_bridge_is5p, "gate_hclk_ahb2apb_bridge_is5p", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 2, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclk_asyncahbslave_to_blkc, "gate_hclk_asyncahbslave_to_blkc", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 10, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclk_asyncahbslave_to_blkd, "gate_hclk_asyncahbslave_to_blkd", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 11, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_i2c0_isp, "gate_pclk_i2c0_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 4, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_i2c1_isp, "gate_pclk_i2c1_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 5, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_i2c2_isp, "gate_pclk_i2c2_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 6, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_mcuctl_isp, "gate_pclk_mcuctl_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 8, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_mpwm_isp, "gate_pclk_mpwm_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 9, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_mtcadc_isp, "gate_pclk_mtcadc_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 10, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_pwm_isp, "gate_pclk_pwm_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 11, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_spi0_isp, "gate_pclk_spi0_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 12, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_spi1_isp, "gate_pclk_spi1_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 13, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_trex_b, "gate_pclk_trex_b", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM10, 24, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_uart_isp, "gate_pclk_uart_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 14, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_wdt_isp, "gate_pclk_wdt_isp", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 15, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_xiu_n_async_mi_from_blkd, "gate_pclk_xiu_n_async_mi_from_blkd", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM11, 22, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_pmu_cam1, "gate_pclk_pmu_cam1", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_PCLK_CAM12, 1, CLK_IGNORE_UNUSED, 0),
	GATE(gate_sclk_isp_mpwm, "gate_sclk_isp_mpwm", "dout_clkdiv_pclk_cam1_busperi_84", \
			EXYNOS7420_ENABLE_SCLK_CAM12, 9, CLK_IGNORE_UNUSED, 0),

	GATE(gate_pclk_fimc_bns_c, "gate_pclk_fimc_bns_c", "mout_user_mux_aclk_cam1_bnscsis_133", \
			EXYNOS7420_ENABLE_PCLK_CAM10, 0, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_fimc_bns_c, "gate_aclk_fimc_bns_c", "mout_user_mux_aclk_cam1_bnscsis_133", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 0, CLK_IGNORE_UNUSED, 0),
	GATE(gate_cclk_asyncapb_socp_fimc_bns_c, "gate_cclk_asyncapb_socp_fimc_bns_c", "mout_user_mux_aclk_cam1_bnscsis_133", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 12, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_bns_c_trex_b, "gate_clk_bns_c_trex_b", "mout_user_mux_aclk_cam1_bnscsis_133", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 5, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_wrap_csis2, "gate_aclk_wrap_csis2", "mout_user_mux_aclk_cam1_bnscsis_133", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 6, CLK_IGNORE_UNUSED, 0),

	GATE(gate_clk_133_cam1_noc_p_cam1, "gate_clk_133_cam1_noc_p_cam1", "mout_user_mux_aclk_cam1_nocp_133", \
			EXYNOS7420_ENABLE_ACLK_CAM12, 24, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclks_asyncahb_cam1, "gate_hclks_asyncahb_cam1", "mout_user_mux_aclk_cam1_nocp_133", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 8, CLK_IGNORE_UNUSED, 0),

	GATE(gate_clk_axlh_async_si_top_cam1, "gate_clk_axlh_async_si_top_cam1", "mout_user_mux_aclk_cam1_trex_532"    , \
			EXYNOS7420_ENABLE_ACLK_CAM11, 28, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_n_async_mi_from_blkd, "gate_aclk_aclk_xiu_n_async_mi_from_blkd", "mout_user_mux_aclk_cam1_trex_532", \
			EXYNOS7420_ENABLE_ACLK_CAM11, 22, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_b_trex_b, "gate_clk_b_trex_b", "mout_user_mux_aclk_cam1_trex_532", \
			EXYNOS7420_ENABLE_ACLK_CAM10, 4, CLK_IGNORE_UNUSED, 0),

	GATE(gate_user_sclk_isp_spi0, "gate_user_sclk_isp_spi0", "mout_user_mux_sclk_isp_spi0", \
			EXYNOS7420_ENABLE_SCLK_CAM12, 12, CLK_IGNORE_UNUSED, 0),

	GATE(gate_user_sclk_isp_spi1, "gate_user_sclk_isp_spi1", "mout_user_mux_sclk_isp_spi1", \
			EXYNOS7420_ENABLE_SCLK_CAM12, 13, CLK_IGNORE_UNUSED, 0),

	GATE(gate_user_sclk_isp_uart, "gate_user_sclk_isp_uart", "mout_user_mux_sclk_isp_uart", \
			EXYNOS7420_ENABLE_SCLK_CAM12, 14, CLK_IGNORE_UNUSED, 0),

	GATE(gate_sclk_isp_mtcadc, "gate_sclk_isp_mtcadc", "mout_user_mux_aclk_cam1_trex_532", \
			EXYNOS7420_ENABLE_SCLK_CAM12, 10, CLK_IGNORE_UNUSED, 0),

	GATE(gate_sclk_i2c0_isp, "gate_sclk_i2c0_isp", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_CAM12, 4, CLK_IGNORE_UNUSED, 0),
	GATE(gate_sclk_i2c1_isp, "gate_sclk_i2c1_isp", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_CAM12, 5, CLK_IGNORE_UNUSED, 0),
	GATE(gate_sclk_i2c2_isp, "gate_sclk_i2c2_isp", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_CAM12, 6, CLK_IGNORE_UNUSED, 0),
	GATE(gate_sclk_isp_pwm, "gate_sclk_isp_pwm", "fin_pll", \
			EXYNOS7420_ENABLE_SCLK_CAM12, 11, CLK_IGNORE_UNUSED, 0),

	GATE(gate_user_phyclk_rxbyteclkhs0_s2b, "gate_user_phyclk_rxbyteclkhs0_s2b", "mout_user_mux_phyclk_hs0_csis2_rx_byte", \
			EXYNOS7420_ENABLE_SCLK_CAM10, 6, CLK_IGNORE_UNUSED, 0),

	/* isp0 */
	GATE(gate_clk_isp0_trex_c, "gate_clk_isp0_trex_c", "mout_user_mux_aclk_isp0_isp0_590", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 2, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_isp_v4, "gate_aclk_isp_v4", "mout_user_mux_aclk_isp0_isp0_590", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 0, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_isp_v4, "gate_clk_isp_v4", "mout_user_mux_aclk_isp0_isp0_590", \
			EXYNOS7420_ENABLE_ACLK_ISP01, 0, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_pxl_asb_s_in, "gate_clk_pxl_asb_s_in", "mout_user_mux_aclk_isp0_isp0_590", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 12, CLK_IGNORE_UNUSED, 0),

	GATE(gate_cclk_asyncapb_isp, "gate_cclk_asyncapb", "dout_clkdiv_pclk_isp0_isp0_295", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 4, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_isp_v4, "gate_pclk_isp_v4", "dout_clkdiv_pclk_isp0_isp0_295", \
			EXYNOS7420_ENABLE_PCLK_ISP0, 0, CLK_IGNORE_UNUSED, 0),

	GATE(gate_clk_tpu_trex_c, "gate_clk_tpu_trex_c", "mout_user_mux_aclk_isp0_tpu_590", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 3, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_tpu_v1, "gate_aclk_tpu_v1", "mout_user_mux_aclk_isp0_tpu_590", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 1, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_tpu_v1, "gate_clk_tpu_v1", "mout_user_mux_aclk_isp0_tpu_590", \
			EXYNOS7420_ENABLE_ACLK_ISP01, 1, CLK_IGNORE_UNUSED, 0),

	GATE(gate_cclk_asyncapb_tpu, "gate_cclk_asyncapb_tpu", "dout_clkdiv_pclk_isp0_tpu_295", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 5, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_tpu_v1, "gate_pclk_tpu_v1", "dout_clkdiv_pclk_isp0_tpu_295", \
			EXYNOS7420_ENABLE_PCLK_ISP0, 1, CLK_IGNORE_UNUSED, 0),

	GATE(gate_clk_axi_lh_async_si_top_isp0, "gate_clk_axi_lh_async_si_top_isp0", "mout_user_mux_aclk_isp0_trex_532", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 13, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_c_trex_c, "gate_clk_c_trex_c", "mout_user_mux_aclk_isp0_trex_532", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 8, CLK_IGNORE_UNUSED, 0),

	GATE(gate_pclk_asyncapb_isp, "gate_pclk_asyncapb_isp", "dout_clkdiv_pclk_isp0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_ISP0, 4, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_asyncapb_tpu, "gate_pclk_asyncapb_tpu", "dout_clkdiv_pclk_isp0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_ISP0, 5, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_axi2apb_bridge, "gate_aclk_axi2apb_bridge", "dout_clkdiv_pclk_isp0_trex_266", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 14, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_async_m, "gate_aclk_xiu_async_m", "dout_clkdiv_pclk_isp0_trex_266", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 15, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_pmu_isp0, "gate_pclk_pmu_isp0", "dout_clkdiv_pclk_isp0_trex_266", \
			EXYNOS7420_ENABLE_PCLK_ISP0, 20, CLK_IGNORE_UNUSED, 0),

	GATE(gate_hclk_ahb2apb_bridge, "gate_hclk_ahb2apb_bridge", "dout_clkdiv_pclk_isp0_trex_133", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 16, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_trex_c, "gate_pclk_trex_c", "dout_clkdiv_pclk_isp0_trex_133", \
			EXYNOS7420_ENABLE_PCLK_ISP0, 8, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclkm_ahb_async_m, "gate_hclkm_ahb_async_m", "dout_clkdiv_pclk_isp0_trex_133", \
			EXYNOS7420_ENABLE_ACLK_ISP00, 17, CLK_IGNORE_UNUSED, 0),

	/* isp1 */
	GATE(gate_aclk_fimc_isp1, "gate_aclk_fimc_isp1", "mout_user_mux_aclk_isp1_isp1_468", \
			EXYNOS7420_ENABLE_ACLK_ISP1, 0, CLK_IGNORE_UNUSED, 0),
	GATE(gate_clk_fimc_isp1, "gate_clk_fimc_isp1", "mout_user_mux_aclk_isp1_isp1_468", \
			EXYNOS7420_ENABLE_ACLK_ISP1, 1, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_pxl_asbs, "gate_aclk_pxl_asbs", "mout_user_mux_aclk_isp1_isp1_468", \
			EXYNOS7420_ENABLE_ACLK_ISP1, 4, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_n_async_si, "gate_aclk_xiu_n_async_si", "mout_user_mux_aclk_isp1_isp1_468", \
			EXYNOS7420_ENABLE_ACLK_ISP1, 8, CLK_IGNORE_UNUSED, 0),

	GATE(gate_pclk_fimc_isp1, "gate_pclk_fimc_isp1", "dout_clkdiv_pclk_isp1_isp1_234", \
			EXYNOS7420_ENABLE_PCLK_ISP1, 0, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_xiu_n_async_mi, "gate_aclk_xiu_n_async_mi", "dout_clkdiv_pclk_isp1_isp1_234", \
			EXYNOS7420_ENABLE_ACLK_ISP1, 9, CLK_IGNORE_UNUSED, 0),
	GATE(gate_aclk_axi2apb_bridge_is2p, "gate_aclk_axi2apb_bridge_is2p", "dout_clkdiv_pclk_isp1_isp1_234", \
			EXYNOS7420_ENABLE_ACLK_ISP1, 10, CLK_IGNORE_UNUSED, 0),

	GATE(gate_hclk_ahb2apb_bridge_is2p, "gate_aclk_ahb2apb_bridge_is2p", "mout_user_mux_aclk_isp1_ahb_117", \
			EXYNOS7420_ENABLE_ACLK_ISP1, 11, CLK_IGNORE_UNUSED, 0),
	GATE(gate_hclkm_asyncahbmaster, "gate_hclkm_asyncahbmaster", "mout_user_mux_aclk_isp1_ahb_117", \
			EXYNOS7420_ENABLE_ACLK_ISP1, 12, CLK_IGNORE_UNUSED, 0),
	GATE(gate_pclk_pmu_isp1, "gate_pclk_pmu_isp1", "mout_user_mux_aclk_isp1_ahb_117", \
			EXYNOS7420_ENABLE_PCLK_ISP1, 16, CLK_IGNORE_UNUSED, 0),

	/* external sensor */
	/* external sensor *///mhjang 0-> CLK_IGNORE_UNUSED
	GATE(sclk_isp_sensor0, "sclk_isp_sensor0", "dout_sclk_isp_sensor0", \
			EXYNOS7420_ENABLE_SCLK_TOP0_CAM11, 16, CLK_IGNORE_UNUSED, 0),
	GATE(sclk_isp_sensor1, "sclk_isp_sensor1", "dout_sclk_isp_sensor1", \
			EXYNOS7420_ENABLE_SCLK_TOP0_CAM11, 8, CLK_IGNORE_UNUSED, 0),
	GATE(sclk_isp_sensor2, "sclk_isp_sensor2", "dout_sclk_isp_sensor2", \
			EXYNOS7420_ENABLE_SCLK_TOP0_CAM11, 0, CLK_IGNORE_UNUSED, 0),

	/* bus0 */
	GATE(gate_aclk_lh_cam0, "gate_aclk_lh_cam0", "usermux_aclk_bus0_532", \
			EXYNOS7420_ENABLE_ACLK_BUS0, 1, 0, NULL),
	GATE(gate_aclk_lh_cam1, "gate_aclk_lh_cam1", "usermux_aclk_bus0_532", \
			EXYNOS7420_ENABLE_ACLK_BUS0, 2, 0, NULL),
	GATE(gate_aclk_lh_isp, "gate_aclk_lh_isp", "usermux_aclk_bus0_532", \
			EXYNOS7420_ENABLE_ACLK_BUS0, 8, 0, NULL),
	GATE(none, "gate_aclk_noc_bus0_rt", "usermux_aclk_bus0_532", \
			EXYNOS7420_ENABLE_ACLK_BUS0, 0, CLK_GATE_ENABLE, NULL),
	GATE(gate_aclk_noc_bus0_nrt, "gate_aclk_noc_bus0_nrt", "usermux_aclk_bus0_532", \
			EXYNOS7420_ENABLE_ACLK_BUS0, 7, CLK_GATE_ENABLE, NULL),


	/*GATE(clkout, "clk_out", "fin_pll", \
			EXYNOS_PMU_PMU_DEBUG, 0, CLK_GATE_SET_TO_DISABLE, NULL),*/

	GATE(pclk_uart, "pclk_uart", "dout_aclk_aud", \
			EXYNOS7420_ENABLE_PCLK_AUD, 25, 0, NULL),
	GATE(pclk_pcm, "pclk_pcm", "dout_aclk_aud", \
			EXYNOS7420_ENABLE_PCLK_AUD, 26, 0, NULL),
	GATE(pclk_i2s, "pclk_i2s", "dout_aclk_aud", \
			EXYNOS7420_ENABLE_PCLK_AUD, 27, 0, NULL),
	GATE(pclk_timer, "pclk_timer", "dout_aclk_aud", \
			EXYNOS7420_ENABLE_PCLK_AUD, 28, 0, NULL),

	GATE(aclk_sramc, "aclk_sramc", "dout_aclk_aud", \
			EXYNOS7420_ENABLE_ACLK_AUD, 30, 0, NULL),
	GATE(aclk_dmac, "aclk_dmac", "dout_aclk_aud", \
			EXYNOS7420_ENABLE_ACLK_AUD, 31, 0, NULL),

	GATE(sclk_ca5, "sclk_ca5", "dout_aud_ca5", \
			EXYNOS7420_ENABLE_SCLK_AUD, 31, 0, NULL),
	GATE(sclk_uart, "sclk_uart", "dout_sclk_i2s", \
			EXYNOS7420_ENABLE_SCLK_AUD, 29, 0, NULL),
	GATE(sclk_i2s, "sclk_i2s", "dout_sclk_i2s", \
			EXYNOS7420_ENABLE_SCLK_AUD, 28, 0, NULL),

	/* imem */
	GATE(none, "gate_aclk_slimsss", "mout_user_mux_aclk_imem_266", \
			EXYNOS7420_ENABLE_ACLK_IMEM_SLIMSSS, 0, 0, NULL),
	GATE(none, "gate_pclk_slimsss", "mout_user_mux_aclk_imem_200", \
			EXYNOS7420_ENABLE_PCLK_IMEM_SLIMSSS, 0, 0, NULL),
};

void __init exynos7420_clk_init(struct device_node *np)
{
	if (!np)
		panic("%s: unable to determine SoC\n", __func__);

	/*
	 * Register clocks for exynos7420 series.
	 * Gate clocks should be registered at last because of some gate clocks.
	 * Some gate clocks should be enabled at initial time.
	 */
	samsung_clk_init(np, 0, nr_clks, (unsigned long *)exynos7420_clk_regs,
			ARRAY_SIZE(exynos7420_clk_regs), NULL, 0);
	samsung_register_of_fixed_ext(exynos7420_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos7420_fixed_rate_ext_clks),
			ext_clk_match);
	samsung_register_comp_pll(exynos7420_pll_clks,
			ARRAY_SIZE(exynos7420_pll_clks));
	samsung_register_fixed_rate(exynos7420_fixed_rate_clks,
			ARRAY_SIZE(exynos7420_fixed_rate_clks));
	samsung_register_fixed_factor(exynos7420_fixed_factor_clks,
			ARRAY_SIZE(exynos7420_fixed_factor_clks));
	samsung_register_comp_mux(exynos7420_mux_clks,
			ARRAY_SIZE(exynos7420_mux_clks));
	samsung_register_comp_divider(exynos7420_div_clks,
			ARRAY_SIZE(exynos7420_div_clks));
	samsung_register_usermux(exynos7420_usermux_clks,
			ARRAY_SIZE(exynos7420_usermux_clks));
	samsung_register_gate(exynos7420_gate_clks,
			ARRAY_SIZE(exynos7420_gate_clks));

	pr_info("EXYNOS7420: Clock setup completed\n");
}
CLK_OF_DECLARE(exynos7420_clks, "samsung,exynos7420-clock", exynos7420_clk_init);
