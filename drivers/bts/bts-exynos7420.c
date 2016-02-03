/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/clk-provider.h>

#include <mach/exynos-pm.h>
#include <mach/bts.h>
#include "cal_bts.h"
#include "regs-bts.h"

#define VPP_MAX 		4
#define MIF_BLK_NUM		4
#define MAX_MIF_IDX 		13

#define update_ud_scen(a)	(pr_state.ud_scen = a)
#define BTS_ALL			(BTS_DISP_RO_0 | BTS_DISP_RO_1 | BTS_DISP_RW_0 | BTS_DISP_RW_1 | \
				BTS_VPP0 | BTS_VPP1 | BTS_VPP2 | BTS_VPP3 \
				BTS_TREX_FIMC_BNS_A | BTS_TREX_FIMC_BNS_B | BTS_TREX_FIMC_BNS_C | \
				BTS_TREX_FIMC_BNS_D | BTS_TREX_3AA0 \
				BTS_MFC_0 | BTS_MFC_1)

#define BTS_DISP		(BTS_DISP_RO_0 | BTS_DISP_RO_1 | BTS_DISP_RW_0 | BTS_DISP_RW_1)
#define BTS_VPP			(BTS_VPP0 | BTS_VPP1 | BTS_VPP2 | BTS_VPP3)
#define BTS_CAM			(BTS_TREX_FIMC_BNS_A | BTS_TREX_FIMC_BNS_B | BTS_TREX_FIMC_BNS_C | \
				BTS_TREX_FIMC_BNS_D | BTS_TREX_3AA0 | BTS_TREX_3AA1 | BTS_TREX_ISPCPU | \
				BTS_TREX_VRA | BTS_TREX_SCALER | BTS_TREX_ISP1 | BTS_TREX_TPU | BTS_TREX_ISP0)
#define BTS_MFC			(BTS_MFC_0 | BTS_MFC_1)

#define is_bts_scen_ip(a) 	(a & BTS_MFC)
#ifdef BTS_DBGGEN
#define BTS_DBG(x...) 		pr_err(x)
#else
#define BTS_DBG(x...) 		do {} while (0)
#endif

#define BTS_DBGGEN1
#ifdef BTS_DBGGEN1
#define BTS_DBG1(x...) 		pr_err(x)
#else
#define BTS_DBG1(x...) 		do {} while (0)
#endif

enum bts_mif_param {
	BTS_MIF_DISP,
	BTS_MIF_VPP,
	BTS_MIF_CAM,
	BTS_MIF_BRB,
	BTS_MIF_BRB_VPP,
	BTS_MIF_NSP,
	BTS_MIF_NSP_VPP,
	BTS_MIF_NSP_SPDMA,
	BTS_MIF_MAX,
};

enum bts_index {
	BTS_IDX_USBDRD30,
	BTS_IDX_MODEMX,
	BTS_IDX_SDCARDX,
	BTS_IDX_WIFI1,
	BTS_IDX_EMBEDDED,
	BTS_IDX_M2M1,
	BTS_IDX_M2M0,
	BTS_IDX_JPEG0,
	BTS_IDX_G2D,
	BTS_IDX_G3D0,
	BTS_IDX_G3D1,
	BTS_IDX_SLIMSSS,
	BTS_IDX_SSS,
	BTS_IDX_SMDMA,
	BTS_IDX_MCOMP,
	BTS_IDX_APOLLO,
	BTS_IDX_ATLAS,
	BTS_IDX_DISP_RO_0,
	BTS_IDX_DISP_RO_1,
	BTS_IDX_DISP_RW_0,
	BTS_IDX_DISP_RW_1,
	BTS_IDX_VPP0,
	BTS_IDX_VPP1,
	BTS_IDX_VPP2,
	BTS_IDX_VPP3,
	BTS_IDX_TREX_FIMC_BNS_A,
	BTS_IDX_TREX_FIMC_BNS_B,
	BTS_IDX_TREX_FIMC_BNS_C,
	BTS_IDX_TREX_FIMC_BNS_D,
	BTS_IDX_TREX_3AA0,
	BTS_IDX_TREX_3AA1,
	BTS_IDX_TREX_ISPCPU,
	BTS_IDX_TREX_VRA,
	BTS_IDX_TREX_SCALER,
	BTS_IDX_TREX_ISP1,
	BTS_IDX_TREX_TPU,
	BTS_IDX_TREX_ISP0,
	BTS_IDX_MFC_0,
	BTS_IDX_MFC_1,
	BTS_MAX,
};

#define BTS_USBDRD30		((u64)1 << (u64)BTS_IDX_USBDRD30)
#define BTS_MODEMX		((u64)1 << (u64)BTS_IDX_MODEMX)
#define BTS_SDCARDX		((u64)1 << (u64)BTS_IDX_SDCARDX)
#define BTS_WIFI1		((u64)1 << (u64)BTS_IDX_WIFI1)
#define BTS_EMBEDDED		((u64)1 << (u64)BTS_IDX_EMBEDDED)
#define BTS_M2M1		((u64)1 << (u64)BTS_IDX_M2M1)
#define BTS_M2M0		((u64)1 << (u64)BTS_IDX_M2M0)
#define BTS_JPEG0		((u64)1 << (u64)BTS_IDX_JPEG0)
#define BTS_G2D			((u64)1 << (u64)BTS_IDX_G2D)
#define BTS_G3D0		((u64)1 << (u64)BTS_IDX_G3D0)
#define BTS_G3D1		((u64)1 << (u64)BTS_IDX_G3D1)
#define BTS_SLIMSSS		((u64)1 << (u64)BTS_IDX_SLIMSSS)
#define BTS_SSS			((u64)1 << (u64)BTS_IDX_SSS)
#define BTS_SMDMA		((u64)1 << (u64)BTS_IDX_SMDMA)
#define BTS_MCOMP		((u64)1 << (u64)BTS_IDX_MCOMP)
#define BTS_APOLLO		((u64)1 << (u64)BTS_IDX_APOLLO)
#define BTS_ATLAS		((u64)1 << (u64)BTS_IDX_ATLAS)
#define BTS_DISP_RO_0		((u64)1 << (u64)BTS_IDX_DISP_RO_0)
#define BTS_DISP_RO_1		((u64)1 << (u64)BTS_IDX_DISP_RO_1)
#define BTS_DISP_RW_0		((u64)1 << (u64)BTS_IDX_DISP_RW_0)
#define BTS_DISP_RW_1		((u64)1 << (u64)BTS_IDX_DISP_RW_1)
#define BTS_VPP0		((u64)1 << (u64)BTS_IDX_VPP0)
#define BTS_VPP1		((u64)1 << (u64)BTS_IDX_VPP1)
#define BTS_VPP2		((u64)1 << (u64)BTS_IDX_VPP2)
#define BTS_VPP3		((u64)1 << (u64)BTS_IDX_VPP3)
#define BTS_TREX_FIMC_BNS_A	((u64)1 << (u64)BTS_IDX_TREX_FIMC_BNS_A)
#define BTS_TREX_FIMC_BNS_B	((u64)1 << (u64)BTS_IDX_TREX_FIMC_BNS_B)
#define BTS_TREX_FIMC_BNS_C	((u64)1 << (u64)BTS_IDX_TREX_FIMC_BNS_C)
#define BTS_TREX_FIMC_BNS_D	((u64)1 << (u64)BTS_IDX_TREX_FIMC_BNS_D)
#define BTS_TREX_3AA0		((u64)1 << (u64)BTS_IDX_TREX_3AA0)
#define BTS_TREX_3AA1		((u64)1 << (u64)BTS_IDX_TREX_3AA1)
#define BTS_TREX_ISPCPU		((u64)1 << (u64)BTS_IDX_TREX_ISPCPU)
#define BTS_TREX_VRA		((u64)1 << (u64)BTS_IDX_TREX_VRA)
#define BTS_TREX_SCALER		((u64)1 << (u64)BTS_IDX_TREX_SCALER)
#define BTS_TREX_ISP1		((u64)1 << (u64)BTS_IDX_TREX_ISP1)
#define BTS_TREX_TPU		((u64)1 << (u64)BTS_IDX_TREX_TPU)
#define BTS_TREX_ISP0		((u64)1 << (u64)BTS_IDX_TREX_ISP0)
#define BTS_MFC_0		((u64)1 << (u64)BTS_IDX_MFC_0)
#define BTS_MFC_1		((u64)1 << (u64)BTS_IDX_MFC_1)

enum exynos_bts_scenario {
	BS_DISABLE,
	BS_DEFAULT,
	BS_MFC_UD_ENCODING,
	BS_MFC_UD_DECODING,
	BS_DEBUG,
	BS_MAX,
};

enum exynos_bts_function {
	BF_SETQOS,
	BF_SETQOS_BW,
	BF_SETQOS_MO,
	BF_SETQOS_FBMBW,
	BF_DISABLE,
	BF_SETTREXQOS,
	BF_SETTREXQOS_MO,
	BF_SETTREXQOS_BW,
	BF_SETTREXQOS_FBMBW,
	BF_SETTREXDISABLE,
	BF_NOP,
};

enum vpp_state {
	VPP_BW,
	VPP_ROT_BW,
	VPP_STAT,
};

struct bts_table {
	enum exynos_bts_function fn;
	unsigned int priority;
	unsigned int window;
	unsigned int token;
	unsigned int mo;
	unsigned int mo_r;
	unsigned int mo_w;
	unsigned int fbm;
	unsigned int decval;
	struct bts_info *next_bts;
	int prev_scen;
	int next_scen;
};

struct bts_info {
	u64 id;
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	struct bts_table table[BS_MAX];
	const char *pd_name;
	bool on;
	struct list_head list;
	bool enable;
	struct clk_info *ct_ptr;
	enum exynos_bts_scenario cur_scen;
	enum exynos_bts_scenario top_scen;
};

struct bts_scen_status {
	bool ud_scen;
};

struct bts_scenario {
	const char *name;
	u64 ip;
	enum exynos_bts_scenario id;
	struct bts_info *head;
};

struct bts_scen_status pr_state = {
	.ud_scen = false,
};

struct clk_info {
	const char *clk_name;
	struct clk *clk;
	enum bts_index index;
};

static struct pm_qos_request exynos7_mif_bts_qos;
static struct pm_qos_request exynos7_int_bts_qos;
static struct srcu_notifier_head exynos_media_notifier;

static DEFINE_MUTEX(media_mutex);
static DEFINE_SPINLOCK(timeout_mutex);
static unsigned int decon_int_bw, decon_ext_bw, vpp_bw[VPP_STAT][VPP_MAX];
static unsigned int cam_bw, sum_rot_bw, total_bw, use_spdma;
static enum vpp_bw_type vpp_status[VPP_MAX];
static void __iomem *base_drex[MIF_BLK_NUM];
static void __iomem *base_nsp;
static void __iomem *base_sysreg;

static int cur_target_idx;
static unsigned int mif_freq, int_freq;

static int exynos7_bts_param_table[MAX_MIF_IDX][BTS_MIF_MAX] = {
/*	 DISPLAY     VPP         CAMERA      BRB         BRB_VPP     NSP         NSP_VPP     NSP_SPDMA  */
	{0X0FFF0FFF, 0X00000000, 0X0FFF0FFF, 0X00000000, 0X00000000, 0X00000018, 0X00000008, 0X00000003},/*MIF_LV0 */
	{0X0FFF0FFF, 0X00000000, 0X0FFF0FFF, 0X00000000, 0X00000000, 0X00000018, 0X00000008, 0X00000003},/*MIF_LV1 */
	{0X0FFF0FFF, 0X00000000, 0X0FFF0FFF, 0X00000000, 0X00000000, 0X00000018, 0X00000008, 0X00000003},/*MIF_LV2 */
	{0X0FFF0FFF, 0X00000000, 0X02000200, 0X00000000, 0X00000000, 0X00000018, 0X00000008, 0X00000003},/*MIF_LV3 */
	{0X01000100, 0X00000000, 0X01000100, 0X00000000, 0X00000033, 0X00000018, 0X00000008, 0X00000003},/*MIF_LV4 */
	{0X00800080, 0X00000000, 0X00400040, 0X00000000, 0X00000033, 0X00000008, 0X00000008, 0X00000003},/*MIF_LV5 */
	{0X00800080, 0X00000000, 0X00400040, 0X00000000, 0X00000033, 0X00000008, 0X00000008, 0X00000003},/*MIF_LV6 */
	{0X00000000, 0X00000000, 0X00000000, 0X00000000, 0X00000033, 0X00000008, 0X00000008, 0X00000003},/*MIF_LV7 */
	{0X00000000, 0X00000000, 0X00000000, 0X00000000, 0X00000033, 0X00000008, 0X00000003, 0X00000003},/*MIF_LV8 */
	{0X00000000, 0X00000000, 0X00000000, 0X00000000, 0X00000033, 0X00000008, 0X00000003, 0X00000003},/*MIF_LV9 */
	{0X00000000, 0X00000000, 0X00000000, 0X00000000, 0X00000033, 0X00000008, 0X00000003, 0X00000003},/*MIF_LV10*/
	{0X00000000, 0X00000000, 0X00000000, 0X00000000, 0X00000033, 0X00000008, 0X00000003, 0X00000003},/*MIF_LV11*/
	{0X00000000, 0X00000000, 0X00000000, 0X00000000, 0X00000033, 0X00000008, 0X00000003, 0X00000003},/*MIF_LV12*/
};

static struct clk_info clk_table[] = {
	{"aclk_bts_usbdrd300", NULL, BTS_IDX_USBDRD30},
	{"pclk_bts_usbdrd300", NULL, BTS_IDX_USBDRD30},

	{"aclk_bts_sdcardx", NULL, BTS_IDX_SDCARDX},
	{"pclk_bts_sdcardx", NULL, BTS_IDX_SDCARDX},

	{"aclk_bts_modemx", NULL, BTS_IDX_MODEMX},
	{"pclk_bts_modemx", NULL, BTS_IDX_MODEMX},

	{"aclk_bts_embedded", NULL, BTS_IDX_EMBEDDED},
	{"pclk_bts_embedded", NULL, BTS_IDX_EMBEDDED},

	{"aclk_bts_wifi1", NULL, BTS_IDX_WIFI1},
	{"pclk_bts_wifi1", NULL, BTS_IDX_WIFI1},

	{"aclk_bts_mscl0", NULL, BTS_IDX_M2M0},
	{"pclk_bts_mscl0", NULL, BTS_IDX_M2M0},

	{"aclk_bts_mscl1", NULL, BTS_IDX_M2M1},
	{"pclk_bts_mscl1", NULL, BTS_IDX_M2M1},

	{"aclk_bts_jpeg", NULL, BTS_IDX_JPEG0},
	{"pclk_bts_jpeg", NULL, BTS_IDX_JPEG0},

	{"aclk_bts_g3d0", NULL, BTS_IDX_G3D0},
	{"pclk_bts_g3d0", NULL, BTS_IDX_G3D0},

	{"aclk_bts_g3d1", NULL, BTS_IDX_G3D1},
	{"pclk_bts_g3d1", NULL, BTS_IDX_G3D1},

	{"aclk_bts_axi_disp_ro_0", NULL, BTS_IDX_DISP_RO_0},
	{"pclk_bts_axi_disp_ro_0", NULL, BTS_IDX_DISP_RO_0},

	{"aclk_bts_axi_disp_ro_1", NULL, BTS_IDX_DISP_RO_1},
	{"pclk_bts_axi_disp_ro_1", NULL, BTS_IDX_DISP_RO_1},

	{"aclk_bts_axi_disp_rw_0", NULL, BTS_IDX_DISP_RW_0},
	{"pclk_bts_axi_disp_rw_0", NULL, BTS_IDX_DISP_RW_0},

	{"aclk_bts_axi_disp_rw_1", NULL, BTS_IDX_DISP_RW_1},
	{"pclk_bts_axi_disp_rw_1", NULL, BTS_IDX_DISP_RW_1},

	{"aclk_bts_idma_vg0", NULL, BTS_IDX_VPP0},
	{"pclk_bts_idma_vg0", NULL, BTS_IDX_VPP0},
	{"aclk_vpp_idma_vg0", NULL, BTS_IDX_VPP0},
	{"aclk_axi_lh_async_si_vpp0", NULL, BTS_IDX_VPP0},
	{"aclk_xiu_vppx0", NULL, BTS_IDX_VPP0},
	{"pclk_vpp", NULL, BTS_IDX_VPP0},

	{"aclk_bts_idma_vg1", NULL, BTS_IDX_VPP1},
	{"pclk_bts_idma_vg1", NULL, BTS_IDX_VPP1},
	{"aclk_vpp_idma_vg1", NULL, BTS_IDX_VPP1},
	{"aclk_axi_lh_async_si_vpp1", NULL, BTS_IDX_VPP1},
	{"aclk_xiu_vppx1", NULL, BTS_IDX_VPP1},
	{"pclk_vpp", NULL, BTS_IDX_VPP1},

	{"aclk_bts_idma_vgr0", NULL, BTS_IDX_VPP2},
	{"pclk_bts_idma_vgr0", NULL, BTS_IDX_VPP2},
	{"aclk_vpp_idma_vgr0", NULL, BTS_IDX_VPP2},
	{"aclk_xiu_vppx0", NULL, BTS_IDX_VPP2},
	{"aclk_axi_lh_async_si_vpp0", NULL, BTS_IDX_VPP2},
	{"pclk_vpp", NULL, BTS_IDX_VPP2},

	{"aclk_bts_idma_vgr1", NULL, BTS_IDX_VPP3},
	{"pclk_bts_idma_vgr1", NULL, BTS_IDX_VPP3},
	{"aclk_vpp_idma_vgr1", NULL, BTS_IDX_VPP3},
	{"aclk_axi_lh_async_si_vpp1", NULL, BTS_IDX_VPP3},
	{"aclk_xiu_vppx1", NULL, BTS_IDX_VPP3},
	{"pclk_vpp", NULL, BTS_IDX_VPP3},

	{"aclk_bts_mfc_0", NULL, BTS_IDX_MFC_0},
	{"pclk_bts_mfc0", NULL, BTS_IDX_MFC_0},

	{"aclk_bts_mfc_1", NULL, BTS_IDX_MFC_1},
	{"pclk_bts_mfc1", NULL, BTS_IDX_MFC_1},

	{"pclk_bts_apl", NULL, BTS_IDX_APOLLO},

	{"pclk_bts_ats", NULL, BTS_IDX_ATLAS},
};

static struct bts_info exynos7_bts[] = {
	[BTS_IDX_USBDRD30] = {
		.id = BTS_USBDRD30,
		.name = "usbdrd30",
		.pa_base = EXYNOS7420_PA_BTS_USBDRD30,
		.pd_name = "fsys0",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_MODEMX] = {
		.id = BTS_MODEMX,
		.name = "modemx",
		.pa_base = EXYNOS7420_PA_BTS_MODEMX,
		.pd_name = "fsys0",
		.table[BS_DEFAULT].fn = BF_SETQOS_MO,
		.table[BS_DEFAULT].priority = 0x44444444,
		.table[BS_DEFAULT].mo_w = 1,
		.table[BS_DEFAULT].mo_r = MAX_MO_LIMIT,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_SDCARDX] = {
		.id = BTS_SDCARDX,
		.name = "sdcardx",
		.pa_base = EXYNOS7420_PA_BTS_SDCARDX,
		.pd_name = "fsys0",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_WIFI1] = {
		.id = BTS_WIFI1,
		.name = "wifi1",
		.pa_base = EXYNOS7420_PA_BTS_WIFI1,
		.pd_name = "fsys1",
		.table[BS_DEFAULT].fn = BF_SETQOS_MO,
		.table[BS_DEFAULT].priority = 0x44444444,
		.table[BS_DEFAULT].mo_w = 1,
		.table[BS_DEFAULT].mo_r = MAX_MO_LIMIT,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_EMBEDDED] = {
		.id = BTS_EMBEDDED,
		.name = "embedded",
		.pa_base = EXYNOS7420_PA_BTS_EMBEDDED,
		.pd_name = "fsys1",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_M2M1] = {
		.id = BTS_M2M1,
		.name = "m2m1",
		.pa_base = EXYNOS7420_PA_BTS_M2M1,
		.pd_name = "spd-mscl1",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_M2M0] = {
		.id = BTS_M2M0,
		.name = "m2m0",
		.pa_base = EXYNOS7420_PA_BTS_M2M0,
		.pd_name = "spd-mscl0",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_JPEG0] = {
		.id = BTS_JPEG0,
		.name = "jpeg0",
		.pa_base = EXYNOS7420_PA_BTS_JPEG0,
		.pd_name = "spd-jpeg",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_G2D] = {
		.id = BTS_G2D,
		.name = "g2d",
		.pa_base = EXYNOS7420_PA_BTS_G2D,
		.pd_name = "spd-g2d",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_G3D0] = {
		.id = BTS_G3D0,
		.name = "g3d0",
		.pa_base = EXYNOS7420_PA_BTS_G3D0,
		.pd_name = "pd-g3d",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_G3D1] = {
		.id = BTS_G3D1,
		.name = "g3d1",
		.pa_base = EXYNOS7420_PA_BTS_G3D1,
		.pd_name = "pd-g3d",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_SLIMSSS] = {
		.id = BTS_SLIMSSS,
		.name = "slimsss",
		.pa_base = EXYNOS7420_PA_BTS_SLIMSSS,
		.pd_name = "imem",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_SSS] = {
		.id = BTS_SSS,
		.name = "sss",
		.pa_base = EXYNOS7420_PA_BTS_SSS,
		.pd_name = "imem",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_SMDMA] = {
		.id = BTS_SMDMA,
		.name = "smdma",
		.pa_base = EXYNOS7420_PA_BTS_SMDMA,
		.pd_name = "imem",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_MCOMP] = {
		.id = BTS_MCOMP,
		.name = "mcomp",
		.pa_base = EXYNOS7420_PA_BTS_MCOMP,
		.pd_name = "imem",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_APOLLO] = {
		.id = BTS_APOLLO,
		.name = "apollo",
		.pa_base = EXYNOS7420_PA_BTS_LITTLE,
		.pd_name = "cpu",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_ATLAS] = {
		.id = BTS_ATLAS,
		.name = "atlas",
		.pa_base = EXYNOS7420_PA_BTS_BIG,
		.pd_name = "cpu",
		.table[BS_DEFAULT].fn = BF_NOP,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = false,
	},
	[BTS_IDX_DISP_RO_0] = {
		.id = BTS_DISP_RO_0,
		.name = "disp_ro_0",
		.pa_base = EXYNOS7420_PA_BTS_DISP_RO_0,
		.pd_name = "spd-decon0",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xAAAAAAAA,
		.table[BS_DISABLE].fn = BF_DISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DISP_RO_1] = {
		.id = BTS_DISP_RO_1,
		.name = "disp_ro_1",
		.pa_base = EXYNOS7420_PA_BTS_DISP_RO_1,
		.pd_name = "spd-decon0",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xAAAAAAAA,
		.table[BS_DISABLE].fn = BF_DISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DISP_RW_0] = {
		.id = BTS_DISP_RW_0,
		.name = "disp_rw_0",
		.pa_base = EXYNOS7420_PA_BTS_DISP_RW_0,
		.pd_name = "spd-decon0",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xAAAAAAAA,
		.table[BS_DISABLE].fn = BF_DISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DISP_RW_1] = {
		.id = BTS_DISP_RW_1,
		.name = "disp_rw_1",
		.pa_base = EXYNOS7420_PA_BTS_DISP_RW_1,
		.pd_name = "spd-decon1",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xAAAAAAAA,
		.table[BS_DISABLE].fn = BF_DISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_VPP0] = {
		.id = BTS_VPP0,
		.name = "vpp_0",
		.pa_base = EXYNOS7420_PA_BTS_VPP0,
		.pd_name = "spd-vg0",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xBBBBBBBB,
		.table[BS_DISABLE].fn = BF_DISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_VPP1] = {
		.id = BTS_VPP1,
		.name = "vpp_1",
		.pa_base = EXYNOS7420_PA_BTS_VPP1,
		.pd_name = "spd-vg1",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xBBBBBBBB,
		.table[BS_DISABLE].fn = BF_DISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_VPP2] = {
		.id = BTS_VPP2,
		.name = "vpp_2",
		.pa_base = EXYNOS7420_PA_BTS_VPP2,
		.pd_name = "spd-vgr0",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xBBBBBBBB,
		.table[BS_DISABLE].fn = BF_DISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_VPP3] = {
		.id = BTS_VPP3,
		.name = "vpp_3",
		.pa_base = EXYNOS7420_PA_BTS_VPP3,
		.pd_name = "spd-vgr1",
		.table[BS_DEFAULT].fn = BF_SETQOS,
		.table[BS_DEFAULT].priority = 0xBBBBBBBB,
		.table[BS_DISABLE].fn = BF_DISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_FIMC_BNS_A] = {
		.id = BTS_TREX_FIMC_BNS_A,
		.name = "fimc_bns_a",
		.pa_base = EXYNOS7420_PA_BTS_TREX_FIMC_BNS_A,
		.pd_name = "pd-cam0",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0xC,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_FIMC_BNS_B] = {
		.id = BTS_TREX_FIMC_BNS_B,
		.name = "fimc_bns_b",
		.pa_base = EXYNOS7420_PA_BTS_TREX_FIMC_BNS_B,
		.pd_name = "pd-cam0",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0xC,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_FIMC_BNS_C] = {
		.id = BTS_TREX_FIMC_BNS_C,
		.name = "fimc_bns_c",
		.pa_base = EXYNOS7420_PA_BTS_TREX_FIMC_BNS_C,
		.pd_name = "pd-cam1",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_FIMC_BNS_D] = {
		.id = BTS_TREX_FIMC_BNS_D,
		.name = "fimc_bns_d",
		.pa_base = EXYNOS7420_PA_BTS_TREX_FIMC_BNS_D,
		.pd_name = "pd-cam0",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0xC,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_3AA0] = {
		.id = BTS_TREX_3AA0,
		.name = "3aa0",
		.pa_base = EXYNOS7420_PA_BTS_TREX_3AA0,
		.pd_name = "pd-cam0",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0xC,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_3AA1] = {
		.id = BTS_TREX_3AA1,
		.name = "3aa1",
		.pa_base = EXYNOS7420_PA_BTS_TREX_3AA1,
		.pd_name = "pd-cam0",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_ISPCPU] = {
		.id = BTS_TREX_ISPCPU,
		.name = "ispcpu",
		.pa_base = EXYNOS7420_PA_BTS_TREX_ISPCPU,
		.pd_name = "pd-cam1",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_VRA] = {
		.id = BTS_TREX_VRA,
		.name = "vra",
		.pa_base = EXYNOS7420_PA_BTS_TREX_VRA,
		.pd_name = "pd-cam1",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_SCALER] = {
		.id = BTS_TREX_SCALER,
		.name = "scaler",
		.pa_base = EXYNOS7420_PA_BTS_TREX_SCALER,
		.pd_name = "pd-cam1",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_ISP1] = {
		.id = BTS_TREX_ISP1,
		.name = "isp1",
		.pa_base = EXYNOS7420_PA_BTS_TREX_ISP1,
		.pd_name = "pd-cam1",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_TPU] = {
		.id = BTS_TREX_TPU,
		.name = "tpu",
		.pa_base = EXYNOS7420_PA_BTS_TREX_TPU,
		.pd_name = "pd-isp0",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_TREX_ISP0] = {
		.id = BTS_TREX_ISP0,
		.name = "isp0",
		.pa_base = EXYNOS7420_PA_BTS_TREX_ISP0,
		.pd_name = "pd-isp0",
		.table[BS_DEFAULT].fn = BF_SETTREXQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.table[BS_DISABLE].fn = BF_SETTREXDISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_MFC_0] = {
		.id = BTS_MFC_0,
		.name = "mfc_0",
		.pa_base = EXYNOS7420_PA_BTS_MFC_0,
		.pd_name = "pd-mfc",
		.table[BS_DEFAULT].fn = BF_DISABLE,
		.table[BS_MFC_UD_ENCODING].fn = BF_SETQOS,
		.table[BS_MFC_UD_ENCODING].priority = 0x55554444,
		.table[BS_MFC_UD_DECODING].fn = BF_SETQOS,
		.table[BS_MFC_UD_DECODING].priority = 0x55554444,
		.table[BS_DISABLE].fn = BF_DISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_MFC_1] = {
		.id = BTS_MFC_1,
		.name = "mfc_1",
		.pa_base = EXYNOS7420_PA_BTS_MFC_1,
		.pd_name = "pd-mfc",
		.table[BS_DEFAULT].fn = BF_DISABLE,
		.table[BS_MFC_UD_ENCODING].fn = BF_SETQOS,
		.table[BS_MFC_UD_ENCODING].priority = 0x55554444,
		.table[BS_MFC_UD_DECODING].fn = BF_SETQOS,
		.table[BS_MFC_UD_DECODING].priority = 0x55554444,
		.table[BS_DISABLE].fn = BF_DISABLE,
		.cur_scen = BS_DISABLE,
		.on = false,
		.enable = true,
	},
};

static struct bts_scenario bts_scen[] = {
	[BS_DISABLE] = {
		.name = "bts_disable",
		.id = BS_DISABLE,
	},
	[BS_DEFAULT] = {
		.name = "bts_default",
		.id = BS_DEFAULT,
	},
	[BS_MFC_UD_ENCODING] = {
		.name = "bts_mfc_uhd_encoding_enabled",
		.ip = BTS_MFC,
		.id = BS_MFC_UD_ENCODING,
	},
	[BS_MFC_UD_DECODING] = {
		.name = "bts_mfc_uhd_decoding_enabled",
		.ip = BTS_MFC,
		.id = BS_MFC_UD_DECODING,
	},
	[BS_DEBUG] = {
		.name = "bts_dubugging_ip",
		.id = BS_DEBUG,
	},
	[BS_MAX] = {
		.name = "undefined"
	}
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);

static void is_bts_clk_enabled(struct bts_info *bts)
{
	struct clk_info *ptr;
	enum bts_index btstable_index;

	ptr = bts->ct_ptr;

	if (ptr) {
		btstable_index = ptr->index;
		do {
			if(!__clk_is_enabled(ptr->clk))
				pr_err("[BTS] CLK is not enabled : %s in %s\n",
						ptr->clk_name,
						bts->name);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->index == btstable_index);
	}
}

static void bts_clk_on(struct bts_info *bts)
{
	struct clk_info *ptr;
	enum bts_index btstable_index;

	ptr = bts->ct_ptr;

	if (ptr) {
		btstable_index = ptr->index;
		do {
			clk_enable(ptr->clk);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->index == btstable_index);
	}
}

static void bts_clk_off(struct bts_info *bts)
{
	struct clk_info *ptr;
	enum bts_index btstable_index;

	ptr = bts->ct_ptr;
	if (ptr) {
		btstable_index = ptr->index;
		do {
			clk_disable(ptr->clk);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->index == btstable_index);
	}
}

static void bts_set_ip_table(enum exynos_bts_scenario scen,
		struct bts_info *bts)
{
	enum exynos_bts_function fn = bts->table[scen].fn;

	is_bts_clk_enabled(bts);
	BTS_DBG("[BTS] %s on:%d bts scen: [%s]->[%s]\n", bts->name, bts->on,
			bts_scen[bts->cur_scen].name, bts_scen[scen].name);

	switch (fn) {
	case BF_SETTREXQOS:
		bts_settrexqos(bts->va_base, bts->table[scen].priority);
		break;
	case BF_SETTREXQOS_MO:
		bts_settrexqos_mo(bts->va_base, bts->table[scen].priority, bts->table[scen].mo);
		break;
	case BF_SETTREXQOS_BW:
		bts_settrexqos_bw(bts->va_base, bts->table[scen].priority,
				bts->table[scen].decval);
		break;
	case BF_SETTREXQOS_FBMBW:
		bts_settrexqos_fbmbw(bts->va_base, bts->table[scen].priority);
		break;
	case BF_SETTREXDISABLE:
		bts_trexdisable(bts->va_base);
		break;
	case BF_SETQOS:
		bts_setqos(bts->va_base, bts->table[scen].priority);
		break;
	case BF_SETQOS_BW:
		bts_setqos_bw(bts->va_base, bts->table[scen].priority,
				bts->table[scen].window, bts->table[scen].token);
		break;
	case BF_SETQOS_MO:
		bts_setqos_mo(bts->va_base, bts->table[scen].priority, bts->table[scen].mo_r, bts->table[scen].mo_w);
		break;
	case BF_SETQOS_FBMBW:
		bts_setqos_fbmbw(bts->va_base, bts->table[scen].priority,
				bts->table[scen].window, bts->table[scen].token, bts->table[scen].fbm);
		break;
	case BF_DISABLE:
		bts_disable(bts->va_base);
		break;
	case BF_NOP:
		break;
	}

	bts->cur_scen = scen;
}

static enum exynos_bts_scenario bts_get_scen(struct bts_info *bts)
{
	enum exynos_bts_scenario scen;

	scen = BS_DEFAULT;

	return scen;
}


static void bts_add_scen(enum exynos_bts_scenario scen, struct bts_info *bts)
{
	struct bts_info *first = bts;
	int next = 0;
	int prev = 0;

	if (!bts)
		return;

	BTS_DBG("[bts %s] scen %s off\n",
			bts->name, bts_scen[scen].name);

	do {
		if (bts->enable) {
			if (bts->table[scen].next_scen == 0) {
				if (scen >= bts->top_scen) {
					bts->table[scen].prev_scen = bts->top_scen;
					bts->table[bts->top_scen].next_scen = scen;
					bts->top_scen = scen;
					bts->table[scen].next_scen = -1;

					if(bts->on)
						bts_set_ip_table(bts->top_scen, bts);

				} else {
					for (prev = bts->top_scen; prev > scen; prev = bts->table[prev].prev_scen)
						next = prev;

					bts->table[scen].prev_scen = bts->table[next].prev_scen;
					bts->table[scen].next_scen = bts->table[prev].next_scen;
					bts->table[next].prev_scen = scen;
					bts->table[prev].next_scen = scen;
				}
			}
		}

		bts = bts->table[scen].next_bts;

	} while (bts && bts != first);
}

static void bts_del_scen(enum exynos_bts_scenario scen, struct bts_info *bts)
{
	struct bts_info *first = bts;
	int next = 0;
	int prev = 0;

	if (!bts)
		return;

	BTS_DBG("[bts %s] scen %s off\n",
			bts->name, bts_scen[scen].name);

	do {
		if (bts->enable) {
			if (bts->table[scen].next_scen != 0) {
				if (scen == bts->top_scen) {
					prev = bts->table[scen].prev_scen;
					bts->top_scen = prev;
					bts->table[prev].next_scen = -1;
					bts->table[scen].next_scen = 0;
					bts->table[scen].prev_scen = 0;

					if (bts->on)
						bts_set_ip_table(prev, bts);
				} else if (scen < bts->top_scen) {
					prev = bts->table[scen].prev_scen;
					next = bts->table[scen].next_scen;

					bts->table[next].prev_scen = bts->table[scen].prev_scen;
					bts->table[prev].next_scen = bts->table[scen].next_scen;

					bts->table[scen].prev_scen = 0;
					bts->table[scen].next_scen = 0;

				} else {
					BTS_DBG("%s scenario couldn't exist above top_scen\n", bts_scen[scen].name);
				}
			}

		}

		bts = bts->table[scen].next_bts;

	} while (bts && bts != first);
}

void bts_scen_update(enum bts_scen_type type, unsigned int val)
{
	enum exynos_bts_scenario scen = BS_DEFAULT;
	struct bts_info *bts = NULL;
	bool on;
	spin_lock(&bts_lock);

	switch (type) {
	case TYPE_MFC_UD_ENCODING:
		on = val ? true : false;
		scen = BS_MFC_UD_ENCODING;
		bts = &exynos7_bts[BTS_IDX_MFC_0];
		BTS_DBG("[BTS] MFC_UD_ENCODING: %s\n", bts_scen[scen].name);
		update_ud_scen(val);
		break;
	case TYPE_MFC_UD_DECODING:
		on = val ? true : false;
		scen = BS_MFC_UD_DECODING;
		bts = &exynos7_bts[BTS_IDX_MFC_0];
		BTS_DBG("[BTS] MFC_UD_DECODING: %s\n", bts_scen[scen].name);
		update_ud_scen(val);
		break;
	default:
		spin_unlock(&bts_lock);
		return;
		break;
	}

	if (on)
		bts_add_scen(scen, bts);
	else
		bts_del_scen(scen, bts);

	spin_unlock(&bts_lock);
}

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;
	enum exynos_bts_scenario scen = BS_DISABLE;

	spin_lock(&bts_lock);

	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strncmp(bts->pd_name, pd_name, strlen(pd_name))) {
			BTS_DBG("[BTS] %s on/off:%d->%d\n", bts->name, bts->on, on);

			if (!bts->enable) {
				bts->on = on;
				continue;
			}

			scen = bts_get_scen(bts);
			if (on) {
				bts_add_scen(scen, bts);
				if (!bts->on) {
					bts->on = true;
					bts_clk_on(bts);
					bts_set_ip_table(bts->top_scen, bts);
				}
			} else {
				if (bts->on) {
					bts->on = false;
					bts_clk_off(bts);
				}
				bts_del_scen(scen, bts);
			}
		}

	spin_unlock(&bts_lock);
}

static void scen_chaining(enum exynos_bts_scenario scen)
{
	struct bts_info *prev = NULL;
	struct bts_info *first = NULL;
	struct bts_info *bts;

	if (bts_scen[scen].ip) {
		list_for_each_entry(bts, &bts_list, list) {
			if (bts_scen[scen].ip & bts->id) {
				if (!first)
					first = bts;
				if (prev)
					prev->table[scen].next_bts = bts;

				prev = bts;
			}
		}

		if (prev)
			prev->table[scen].next_bts = first;

		bts_scen[scen].head = first;
	}
}

void exynos7_bts_show_mo_status(void)
{
	unsigned long i;
	unsigned int r_ctrl, w_ctrl;
	unsigned int r_mo, w_mo;

	spin_lock(&bts_lock);
	for(i = 0; i < (unsigned int)ARRAY_SIZE(exynos7_bts); i++) {
		if (exynos7_bts[i].enable && exynos7_bts[i].on &&
				!(exynos7_bts[i].id & BTS_CAM)) {
			is_bts_clk_enabled(&exynos7_bts[i]);
			r_ctrl = __raw_readl(exynos7_bts[i].va_base + READ_QOS_CONTROL);
			w_ctrl = __raw_readl(exynos7_bts[i].va_base + WRITE_QOS_CONTROL);
			r_mo = __raw_readl(exynos7_bts[i].va_base + READ_MO);
			w_mo = __raw_readl(exynos7_bts[i].va_base + WRITE_MO);
			pr_info("BTS[%s] R_MO: %d, W_MO: %d, R_CTRL: %#x, W_CTRL: %#x\n",
					exynos7_bts[i].name, r_mo, w_mo, r_ctrl, w_ctrl);
		}
	}
	spin_unlock(&bts_lock);
}


static int exynos7_qos_status_open_show(struct seq_file *buf, void *d)
{
	unsigned long i;
	unsigned int val_r, val_w;

	spin_lock(&bts_lock);
	for (i = 0; i < (unsigned int)ARRAY_SIZE(exynos7_bts); i++) {
		seq_printf(buf, "bts[%ld] %s : ", i, exynos7_bts[i].name);
		if (exynos7_bts[i].on) {
			if (exynos7_bts[i].ct_ptr)
				bts_clk_on(exynos7_bts + i);

			if (exynos7_bts[i].id & BTS_CAM) {
				/* trex control */
				seq_printf(buf, "TREX: ");
				val_r = __raw_readl(exynos7_bts[i].va_base + TREX_QOS_CONTROL);
				seq_printf(buf, "%s on, priority ch:0x%x, scen:%s\n",
						exynos7_bts[i].name, val_r,
						bts_scen[exynos7_bts[i].cur_scen].name);
			} else {
				seq_printf(buf, "axi: ");
				/* axi qoscontrol */
				val_r = __raw_readl(exynos7_bts[i].va_base + READ_QOS_CONTROL);
				val_w = __raw_readl(exynos7_bts[i].va_base + WRITE_QOS_CONTROL);
				if (val_r && val_w) {
					val_r = __raw_readl(exynos7_bts[i].va_base + READ_CHANNEL_PRIORITY);
					val_w = __raw_readl(exynos7_bts[i].va_base + WRITE_CHANNEL_PRIORITY);
					seq_printf(buf, "%s on, priority ch_r:0x%x,ch_w:0x%x, scen:%s\n",
							exynos7_bts[i].name, val_r, val_w,
							bts_scen[exynos7_bts[i].cur_scen].name);
				} else {
					seq_printf(buf, "%s control disable, scen:%s\n", exynos7_bts[i].name,
							bts_scen[exynos7_bts[i].cur_scen].name);
				}
			}

			if (exynos7_bts[i].ct_ptr)
				bts_clk_off(exynos7_bts + i);
		} else {
			seq_printf(buf, "bts: %s off\n", exynos7_bts[i].name);
		}
	}
	spin_unlock(&bts_lock);
	exynos7_bts_show_mo_status();

	return 0;
}

static int exynos7_qos_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos7_qos_status_open_show, inode->i_private);
}

static const struct file_operations debug_qos_status_fops = {
	.open		= exynos7_qos_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int exynos7_spdma_status_open_show(struct seq_file *buf, void *d)
{
	spin_lock(&timeout_mutex);
	pr_info("[BTS] spdma call count : %d\n", use_spdma);
	spin_unlock(&timeout_mutex);
	return 0;
}

static int exynos7_spdma_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos7_spdma_status_open_show, inode->i_private);
}

static const struct file_operations debug_spdma_fops = {
	.open		= exynos7_spdma_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
static int exynos7_bw_status_open_show(struct seq_file *buf, void *d)
{
	mutex_lock(&media_mutex);

	seq_printf(buf, "bts bandwidth (total %d) : decon int %d, decon ext %d, cam %d\n"
			"vpp0 %d, vpp1 %d, vpp2 %d, vpp3 %d\n"
			"rotation bandwidth %d, mif_freq %d, intfreq %d\n",
			total_bw, decon_int_bw, decon_ext_bw, cam_bw,
			vpp_bw[VPP_BW][0], vpp_bw[VPP_BW][1], vpp_bw[VPP_BW][2], vpp_bw[VPP_BW][3],
			sum_rot_bw, mif_freq, int_freq);

	mutex_unlock(&media_mutex);

	return 0;
}

static int exynos7_bw_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos7_bw_status_open_show, inode->i_private);
}

static const struct file_operations debug_bw_status_fops = {
	.open		= exynos7_bw_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int debug_enable_get(void *data, unsigned long long *val)
{
	struct bts_info *first = bts_scen[BS_DEBUG].head;
	struct bts_info *bts = bts_scen[BS_DEBUG].head;
	int cnt = 0;

	if (first) {
		do {
			pr_info("%s, ", bts->name);
			cnt++;
			bts = bts->table[BS_DEBUG].next_bts;
		} while (bts && bts != first);
	}
	if (first && first->top_scen == BS_DEBUG)
		pr_info("is on\n");
	else
		pr_info("is off\n");
	*val = cnt;

	return 0;
}

static int debug_enable_set(void *data, unsigned long long val)
{
	struct bts_info *first = bts_scen[BS_DEBUG].head;
	struct bts_info *bts = bts_scen[BS_DEBUG].head;

	if (first) {
		do {
			pr_info("%s, ", bts->name);

			bts = bts->table[BS_DEBUG].next_bts;
		} while (bts && bts != first);
	}

	spin_lock(&bts_lock);

	if (val) {
		bts_add_scen(BS_DEBUG, bts_scen[BS_DEBUG].head);
		pr_info("is on\n");
	} else {
		bts_del_scen(BS_DEBUG, bts_scen[BS_DEBUG].head);
		pr_info("is off\n");
	}

	spin_unlock(&bts_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_enable_fops, debug_enable_get, debug_enable_set, "%llx\n");

static void bts_status_print(void)
{
	pr_info("0 : disable debug ip\n");
	pr_info("1 : BF_SETQOS\n");
	pr_info("2 : BF_SETQOS_BW\n");
	pr_info("3 : BF_SETQOS_MO\n");
}

static int debug_ip_enable_get(void *data, unsigned long long *val)
{
	struct bts_info *bts = data;

	if (bts->table[BS_DEBUG].next_scen) {
		switch (bts->table[BS_DEBUG].fn) {
		case BF_SETQOS:
			*val = 1;
			break;
		case BF_SETQOS_BW:
			*val = 2;
			break;
		case BF_SETQOS_MO:
			*val = 3;
			break;
		case BF_SETTREXQOS:
			*val = 1;
			break;
		case BF_SETTREXQOS_BW:
			*val = 2;
			break;
		case BF_SETTREXQOS_MO:
			*val = 3;
			break;
		default:
			*val = 4;
			break;
		}
	} else {
		*val = 0;
	}

	bts_status_print();

	return 0;
}

static int debug_ip_enable_set(void *data, unsigned long long val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	if (val) {
		bts_scen[BS_DEBUG].ip |= bts->id;

		scen_chaining(BS_DEBUG);

		if (bts->id & BTS_CAM) {
			switch (val) {
			case 1:
				bts->table[BS_DEBUG].fn = BF_SETTREXQOS;
				break;
			case 2:
				bts->table[BS_DEBUG].fn = BF_SETTREXQOS_BW;
				break;
			case 3:
				bts->table[BS_DEBUG].fn = BF_SETTREXQOS_MO;
				break;
			default:
				break;
			}
		} else {
			switch (val) {
			case 1:
				bts->table[BS_DEBUG].fn = BF_SETQOS;
				break;
			case 2:
				bts->table[BS_DEBUG].fn = BF_SETQOS_BW;
				break;
			case 3:
				bts->table[BS_DEBUG].fn = BF_SETQOS_MO;
				break;
			default:
				break;
			}
		}

		bts_add_scen(BS_DEBUG, bts);

		pr_info ("%s on %#llx\n", bts->name, bts_scen[BS_DEBUG].ip);
	} else {
		bts->table[BS_DEBUG].next_bts = NULL;
		bts_del_scen(BS_DEBUG, bts);

		bts_scen[BS_DEBUG].ip &= ~bts->id;
		scen_chaining(BS_DEBUG);

		pr_info ("%s off %#llx\n", bts->name, bts_scen[BS_DEBUG].ip);
	}

	spin_unlock(&bts_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ip_enable_fops, debug_ip_enable_get, debug_ip_enable_set, "%llx\n");

static int debug_ip_mo_get(void *data, unsigned long long *val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	*val = bts->table[BS_DEBUG].mo;

	spin_unlock(&bts_lock);
	return 0;
}

static int debug_ip_mo_set(void *data, unsigned long long val)
{
	struct bts_info *bts = data;
	spin_lock(&bts_lock);

	bts->table[BS_DEBUG].mo = val;
	if (bts->top_scen == BS_DEBUG) {
		if (bts->on)
			bts_set_ip_table(BS_DEBUG, bts);
	}
	pr_info ("Debug mo set %s :priority %#x mo %d\n", bts->name, bts->table[BS_DEBUG].priority, bts->table[BS_DEBUG].mo);

	spin_unlock(&bts_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ip_mo_fops, debug_ip_mo_get, debug_ip_mo_set, "%llx\n");

static int debug_ip_token_get(void *data, unsigned long long *val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	*val = bts->table[BS_DEBUG].token;

	spin_unlock(&bts_lock);
	return 0;
}

static int debug_ip_token_set(void *data, unsigned long long val)
{
	struct bts_info *bts = data;
	spin_lock(&bts_lock);

	bts->table[BS_DEBUG].token = val;
	if (bts->top_scen == BS_DEBUG) {
		if (bts->on && bts->table[BS_DEBUG].window)
			bts_set_ip_table(BS_DEBUG, bts);
	}
	pr_info ("Debug bw set %s :priority %#x window %d token %d\n", bts->name, bts->table[BS_DEBUG].priority, bts->table[BS_DEBUG].window, bts->table[BS_DEBUG].token);

	spin_unlock(&bts_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ip_token_fops, debug_ip_token_get, debug_ip_token_set, "%llx\n");

static int debug_ip_window_get(void *data, unsigned long long *val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	*val = bts->table[BS_DEBUG].window;

	spin_unlock(&bts_lock);
	return 0;
}

static int debug_ip_window_set(void *data, unsigned long long val)
{
	struct bts_info *bts = data;
	spin_lock(&bts_lock);

	bts->table[BS_DEBUG].window = val;
	if (bts->top_scen == BS_DEBUG) {
		if (bts->on && bts->table[BS_DEBUG].token)
			bts_set_ip_table(BS_DEBUG, bts);
	}
	pr_info ("Debug bw set %s :priority %#x window %d token %d\n", bts->name, bts->table[BS_DEBUG].priority, bts->table[BS_DEBUG].window, bts->table[BS_DEBUG].token);

	spin_unlock(&bts_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ip_window_fops, debug_ip_window_get, debug_ip_window_set, "%llx\n");

static int debug_ip_qos_get(void *data, unsigned long long *val)
{
	struct bts_info *bts = data;

	spin_lock(&bts_lock);

	*val = bts->table[BS_DEBUG].priority;

	spin_unlock(&bts_lock);
	return 0;
}

static int debug_ip_qos_set(void *data, unsigned long long val)
{
	struct bts_info *bts = data;
	spin_lock(&bts_lock);

	bts->table[BS_DEBUG].priority = val;
	if (bts->top_scen == BS_DEBUG) {
		if (bts->on)
			bts_setqos(bts->va_base, bts->table[BS_DEBUG].priority);
	}
	pr_info ("Debug qos set %s : %#x\n", bts->name, bts->table[BS_DEBUG].priority);

	spin_unlock(&bts_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ip_qos_fops, debug_ip_qos_get, debug_ip_qos_set, "%llx\n");

void bts_debugfs(void)
{
	struct bts_info *bts;
	struct dentry *den;
	struct dentry *subden;

	den = debugfs_create_dir("bts_dbg", NULL);
	debugfs_create_file("spdma_call_count", 0440,	den, NULL, &debug_spdma_fops);
	debugfs_create_file("qos_status", 0440,	den, NULL, &debug_qos_status_fops);
	debugfs_create_file("bw_status", 0440,	den, NULL, &debug_bw_status_fops);
	debugfs_create_file("enable", 0440,	den, NULL, &debug_enable_fops);

	den = debugfs_create_dir("bts", den);
	list_for_each_entry(bts, &bts_list, list) {
		subden = debugfs_create_dir(bts->name, den);
		debugfs_create_file("qos", 0644, subden, bts, &debug_ip_qos_fops);
		debugfs_create_file("token", 0644, subden, bts, &debug_ip_token_fops);
		debugfs_create_file("window", 0644, subden, bts, &debug_ip_window_fops);
		debugfs_create_file("mo", 0644, subden, bts, &debug_ip_mo_fops);
		debugfs_create_file("enable", 0644, subden, bts, &debug_ip_enable_fops);
	}
}

static void bts_drex_init(void __iomem *base)
{

	BTS_DBG("[BTS][%s] bts drex init\n", __func__);

	__raw_writel(0x00000000, base + QOS_TIMEOUT_0xF);
	__raw_writel(0x00200020, base + QOS_TIMEOUT_0xE);
	__raw_writel(0x0FFF0FFF, base + QOS_TIMEOUT_0xD);
	__raw_writel(0x0FFF0FFF, base + QOS_TIMEOUT_0xC);
	__raw_writel(0x00000FFF, base + QOS_TIMEOUT_0xB);
	__raw_writel(0x02000200, base + QOS_TIMEOUT_0xA);
	__raw_writel(0x025E0294, base + QOS_TIMEOUT_0x8);
	__raw_writel(0x00200020, base + QOS_TIMEOUT_0x5);
	__raw_writel(0x00000FFF, base + QOS_TIMEOUT_0x4);
	__raw_writel(0x00000000, base + BRB_CON);
	__raw_writel(0x8858CC9C, base + BRB_THRESHOLD);
}


static void bts_noc_init(void __iomem *base)
{
	bts_setnsp(base + 0x0008, 0x18);
	bts_setnsp(base + 0x0408, 0x18);
	bts_setnsp(base + 0x0808, 0x18);
	bts_setnsp(base + 0x0C08, 0x18);
}

static int exynos_bts_notifier_event(struct notifier_block *this,
		unsigned long event,
		void *ptr)
{
	unsigned long i;

	switch ((unsigned int)event) {
	case PM_POST_SUSPEND:

		for (i = 0; i < (unsigned int)ARRAY_SIZE(base_drex); i++)
			bts_drex_init(base_drex[i]);

		bts_noc_init(base_nsp);
		__raw_writel(0x00004455, base_sysreg + FSYS0_QOS_VAL0);
		bts_initialize("cpu", true);
		bts_initialize("fsys0", true);
		bts_initialize("fsys1", true);
		bts_initialize("imem", true);

		return NOTIFY_OK;
		break;
	case PM_SUSPEND_PREPARE:
		bts_initialize("cpu", false);
		bts_initialize("fsys0", false);
		bts_initialize("fsys1", false);
		bts_initialize("imem", false);

		return NOTIFY_OK;
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};

void exynos7_rot_param(int target_idx)
{
	int rot = sum_rot_bw ? 1 : 0;
	int spdma = use_spdma ? 2 : rot;

	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_DISP + rot], base_drex[0] + QOS_TIMEOUT_0xB);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_DISP + rot], base_drex[1] + QOS_TIMEOUT_0xB);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_DISP + rot], base_drex[2] + QOS_TIMEOUT_0xB);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_DISP + rot], base_drex[3] + QOS_TIMEOUT_0xB);

	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_BRB + rot], base_drex[0] + BRB_CON);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_BRB + rot], base_drex[1] + BRB_CON);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_BRB + rot], base_drex[2] + BRB_CON);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_BRB + rot], base_drex[3] + BRB_CON);

	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_NSP + spdma], base_nsp + NSP_CH0);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_NSP + spdma], base_nsp + NSP_CH1);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_NSP + spdma], base_nsp + NSP_CH2);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_NSP + spdma], base_nsp + NSP_CH3);
}

int exynos7_update_bts_param(int target_idx, int work)
{
	if (!work) {
		return 0;
	}

	spin_lock(&timeout_mutex);
	cur_target_idx = target_idx;
	exynos7_rot_param(target_idx);
	spin_unlock(&timeout_mutex);

	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_DISP], base_drex[0] + QOS_TIMEOUT_0xA);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_DISP], base_drex[1] + QOS_TIMEOUT_0xA);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_DISP], base_drex[2] + QOS_TIMEOUT_0xA);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_DISP], base_drex[3] + QOS_TIMEOUT_0xA);

	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_CAM], base_drex[0] + QOS_TIMEOUT_0xC);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_CAM], base_drex[1] + QOS_TIMEOUT_0xC);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_CAM], base_drex[2] + QOS_TIMEOUT_0xC);
	__raw_writel(exynos7_bts_param_table[target_idx][BTS_MIF_CAM], base_drex[3] + QOS_TIMEOUT_0xC);

	return 0;
}

static int exynos7_bts_notify(unsigned long freq)
{
	BUG_ON(irqs_disabled());

	return srcu_notifier_call_chain(&exynos_media_notifier, freq, NULL);
}

int exynos7_bts_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&exynos_media_notifier, nb);
}

int exynos7_bts_unregister_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&exynos_media_notifier, nb);
}

void exynos7_init_bts_ioremap(void)
{
	base_drex[0] = ioremap(EXYNOS7_PA_DREX0, SZ_4K);
	base_drex[1] = ioremap(EXYNOS7_PA_DREX1, SZ_4K);
	base_drex[2] = ioremap(EXYNOS7_PA_DREX2, SZ_4K);
	base_drex[3] = ioremap(EXYNOS7_PA_DREX3, SZ_4K);
	base_nsp = ioremap(EXYNOS7_PA_NSP + 0x2000, SZ_4K);

	base_sysreg = ioremap(EXYNOS7_PA_SYSREG, SZ_16K);

	pm_qos_add_request(&exynos7_mif_bts_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&exynos7_int_bts_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
}

void exynos7_update_media_scenario(enum bts_media_type media_type,
		unsigned int bw, int bw_type)
{
	unsigned int vpp_total_bw;
	unsigned int vpp_bus_bw[2];
	int utilization;
	int i;
	int max_status = 0;
	static unsigned int is_yuv;
	static unsigned int prev_sum_rot_bw;
	static unsigned int mif_ud_encoding, mif_ud_decoding;

	mutex_lock(&media_mutex);

	switch (media_type) {
	case TYPE_DECON_INT:
		decon_int_bw = bw;
		break;
	case TYPE_DECON_EXT:
		decon_ext_bw = bw;
		break;
	case TYPE_VPP0:
	case TYPE_VPP1:
	case TYPE_VPP2:
	case TYPE_VPP3:
		vpp_bw[VPP_BW][media_type - TYPE_VPP0] = bw;
		vpp_status[media_type - TYPE_VPP0] = bw_type;
		if (bw_type)
			vpp_bw[VPP_ROT_BW][media_type - TYPE_VPP0] = bw;
		else
			vpp_bw[VPP_ROT_BW][media_type - TYPE_VPP0] = 0;

		vpp_bus_bw[0] = vpp_bw[VPP_BW][0] + vpp_bw[VPP_BW][2];
		vpp_bus_bw[1] = vpp_bw[VPP_BW][1] + vpp_bw[VPP_BW][3];

		sum_rot_bw = vpp_bw[VPP_ROT_BW][0] + vpp_bw[VPP_ROT_BW][1] +
			vpp_bw[VPP_ROT_BW][2] + vpp_bw[VPP_ROT_BW][3];
		int_freq = vpp_bus_bw[vpp_bus_bw[0] < vpp_bus_bw[1]] *
			100 / (INT_UTIL * BUS_WIDTH);
		if (int_freq > 400000)
			int_freq = 560000;

		if (pm_qos_request_active(&exynos7_int_bts_qos))
			pm_qos_update_request(&exynos7_int_bts_qos, int_freq);

		if (!!sum_rot_bw ^ !!prev_sum_rot_bw) {
			spin_lock(&timeout_mutex);
			exynos7_rot_param(cur_target_idx);
			spin_unlock(&timeout_mutex);
		}
		prev_sum_rot_bw = sum_rot_bw;
		break;
	case TYPE_CAM:
		cam_bw = bw;
		break;
	case TYPE_YUV:
		is_yuv = bw;
		break;
	case TYPE_UD_ENC:
		mif_ud_encoding = bw;
		break;
	case TYPE_UD_DEC:
		mif_ud_decoding = bw;
		break;
	case TYPE_SPDMA:
		spin_lock(&timeout_mutex);
		if (!use_spdma && !bw) {
			pr_err("[BTS] do not call spdma stop function previous start\n");
		} else {
			use_spdma += bw ? 1 : -1;
			exynos7_rot_param(cur_target_idx);
		}
		spin_unlock(&timeout_mutex);
		mutex_unlock(&media_mutex);
		return;
		break;
	default:
		pr_err("DEVFREQ(MIF) : unsupportd media_type - %u", media_type);
		break;
	}

	vpp_total_bw = vpp_bw[VPP_BW][0] + vpp_bw[VPP_BW][1]
		+ vpp_bw[VPP_BW][2] + vpp_bw[VPP_BW][3];
	total_bw = decon_int_bw + decon_ext_bw + cam_bw + vpp_total_bw;

	for (i = 0; i < VPP_MAX; i++) {
		if (max_status < vpp_status[i])
			max_status = vpp_status[i];
	}
	switch (max_status) {
	case BW_FULLHD_ROT:
		SIZE_FACTOR(total_bw);
	case BW_ROT:
		if (total_bw < 200000)
			utilization = 7;
		else if (total_bw < 500000)
			utilization = 10;
		else if (total_bw < 1600000)
			utilization = 29;
		else if (total_bw < 3000000)
			utilization = 48;
		else if (total_bw < 3500000)
			utilization = 55;
		else if (total_bw < 4000000)
			utilization = 60;
		else if (total_bw < 5000000)
			utilization = 70;
		else if (total_bw < 7000000)
			utilization = 76;
		else
			utilization = 83;
		break;
	default:
		if (total_bw < 133000 * 8 * 4)
			utilization = MIF_UTIL;
		else
			utilization = MIF_UTIL2;
		break;
	}

	mif_freq = (total_bw * 100) / (utilization * BUS_WIDTH);

	if (mif_ud_encoding && mif_freq < MIF_ENCODING)
		mif_freq = MIF_ENCODING;
	if (mif_ud_decoding && mif_freq < MIF_DECODING)
		mif_freq = MIF_DECODING;

	exynos7_bts_notify(mif_freq);

	BTS_DBG("[BTS BW] total: %d, vpp %d, vpp_rot: %d, decon_int %d, decon_ext %d, cam %d\n",
			total_bw, vpp_total_bw, sum_rot_bw,
			decon_int_bw, decon_ext_bw,
			cam_bw);
	BTS_DBG("[BTS VPP] vpp0 %d, vpp1 %d, vpp2 %d, vpp3 %d\n",
			vpp_bw[VPP_BW][0], vpp_bw[VPP_BW][1],
			vpp_bw[VPP_BW][2], vpp_bw[VPP_BW][3]);
	BTS_DBG("[BTS FREQ] freq: %d, util: %d vpp int_freq: %d\n",
			mif_freq, utilization, int_freq);

	if (pm_qos_request_active(&exynos7_mif_bts_qos))
		pm_qos_update_request(&exynos7_mif_bts_qos, mif_freq);

	mutex_unlock(&media_mutex);
}

#ifdef CONFIG_CPU_IDLE
static int exynos7_bts_lpa_event(struct notifier_block *nb,
				unsigned long event, void *data)
{
	switch (event) {
	case LPA_EXIT:
		exynos7_rot_param(cur_target_idx);
		__raw_writel(0x00004455, base_sysreg + FSYS0_QOS_VAL0);
		bts_initialize("cpu", true);
		bts_initialize("fsys0", true);
		bts_initialize("fsys1", true);
		bts_initialize("imem", true);
		break;
	case LPA_ENTER:
		bts_initialize("cpu", false);
		bts_initialize("fsys0", false);
		bts_initialize("fsys1", false);
		bts_initialize("imem", false);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block bts_lpa_nb = {
	.notifier_call = exynos7_bts_lpa_event,
};
#endif

static int __init exynos7_bts_init(void)
{
	long i;
	int ret;
	enum bts_index btstable_index = BTS_MAX - 1;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	for (i = 0; i < ARRAY_SIZE(clk_table); i++) {
		if (btstable_index != clk_table[i].index) {
			btstable_index = clk_table[i].index;
			exynos7_bts[btstable_index].ct_ptr = clk_table + i;
		}
		clk_table[i].clk = clk_get(NULL, clk_table[i].clk_name);

		if (IS_ERR(clk_table[i].clk)){
			BTS_DBG("failed to get bts clk %s\n",
					clk_table[i].clk_name);
			exynos7_bts[btstable_index].ct_ptr = NULL;
		}
		else {
			ret = clk_prepare(clk_table[i].clk);
			if (ret) {
				pr_err("[BTS] failed to prepare bts clk %s\n",
						clk_table[i].clk_name);
				for (; i >= 0; i--)
					clk_put(clk_table[i].clk);
				return ret;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(exynos7_bts); i++) {
		exynos7_bts[i].va_base = ioremap(exynos7_bts[i].pa_base, SZ_4K);

		list_add(&exynos7_bts[i].list, &bts_list);
	}

	for (i = BS_DEFAULT + 1; i < BS_MAX; i++)
		scen_chaining(i);

	exynos7_init_bts_ioremap();
	for (i = 0; i < ARRAY_SIZE(base_drex); i++)
		bts_drex_init(base_drex[i]);

	bts_noc_init(base_nsp);

	__raw_writel(0x00004455, base_sysreg + FSYS0_QOS_VAL0);

	bts_initialize("cpu", true);
	bts_initialize("fsys0", true);
	bts_initialize("fsys1", true);
	bts_initialize("imem", true);

	bts_clk_on(&exynos7_bts[BTS_IDX_MODEMX]);
	bts_clk_on(&exynos7_bts[BTS_IDX_WIFI1]);

	register_pm_notifier(&exynos_bts_notifier);

	bts_debugfs();

	srcu_init_notifier_head(&exynos_media_notifier);
#ifdef CONFIG_CPU_IDLE
	exynos_pm_register_notifier(&bts_lpa_nb);
#endif

	return 0;
}
arch_initcall(exynos7_bts_init);
