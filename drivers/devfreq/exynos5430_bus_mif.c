/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Sangkyu Kim(skwith.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#include <mach/tmu.h>
#include <mach/devfreq.h>
#include <mach/asv-exynos.h>
#include <mach/regs-clock-exynos5430.h>

#include "exynos5430_ppmu.h"
#include "exynos_ppmu2.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(825000)
#define DEVFREQ_POLLING_PERIOD	(0)

#define MIF_VOLT_STEP		(12500)
#define COLD_VOLT_OFFSET	(37500)
#define LIMIT_COLD_VOLTAGE	(1250000)

#define TIMING_RFCPB_MASK	(0x3F)

#define DLL_ON_BASE_VOLT	(900000)
#define PERBANK_CONTROL_BASE	(543000)

#define MRSTATUS_THERMAL_BIT_SHIFT	(7)
#define MRSTATUS_THERMAL_BIT_MASK	(1)
#define MRSTATUS_THERMAL_LV_MASK	(0x7)

#define CTRL_FORCE_SHIFT	(0x7)
#define CTRL_FORCE_MASK		(0x1FF)
#define CTRL_LOCK_VALUE_SHIFT	(0x8)
#define CTRL_LOCK_VALUE_MASK	(0x1FF)

#define TRAFFIC_BYTES_HD_32BIT_60FPS	(1280*720*4*60)
#define TRAFFIC_BYTES_FHD_32BIT_60FPS	(1920*1080*4*60)
#define TRAFFIC_BYTES_WQHD_32BIT_60FPS	(2560*1440*4*60)
#define TRAFFIC_BYTES_WQXGA_32BIT_60FPS	(2560*1600*4*60)

#define TRAFFIC_BYTES_32BIT_60FPS	(4*60)

enum devfreq_mif_idx {
	LV0,
	LV1,
	LV2,
	LV3,
	LV4,
	LV5,
	LV6,
	LV7,
	LV8,
	LV_COUNT,
};

enum devfreq_mif_clk {
	FOUT_MEM0_PLL,
	FOUT_MEM1_PLL,
	FOUT_MFC_PLL,
	FOUT_BUS_PLL,
	MOUT_MEM0_PLL,
	MOUT_MEM0_PLL_DIV2,
	MOUT_MEM1_PLL,
	MOUT_MEM1_PLL_DIV2,
	MOUT_MFC_PLL,
	MOUT_MFC_PLL_DIV2,
	MOUT_BUS_PLL,
	MOUT_BUS_PLL_DIV2,
	MOUT_ACLK_MIF_400,
	DOUT_ACLK_MIF_400,
	DOUT_ACLK_MIF_200,
	DOUT_MIF_PRE,
	MOUT_ACLK_MIFNM_200,
	DOUT_ACLK_MIFNM_200,
	DOUT_ACLK_MIFND_133,
	DOUT_ACLK_MIF_133,
	DOUT_ACLK_CPIF_200,
	DOUT_CLK2X_PHY,
	DOUT_ACLK_DREX0,
	DOUT_ACLK_DREX1,
	DOUT_SCLK_HPM_MIF,
	CLK_COUNT,
};

enum devfreq_mif_thermal_autorate {
	RATE_ONE = 0x000B005D,
	RATE_HALF = 0x0005002E,
	RATE_QUARTER = 0x00030017,
};

enum devfreq_mif_thermal_channel {
	THERMAL_CHANNEL0,
	THERMAL_CHANNEL1,
};

enum devfreq_memorysize {
	LP3_2GB,
	LP3_3GB,
	LP3_4GB,
	LP3_UNKNOWN,
};

struct devfreq_data_mif {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_mif;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	struct notifier_block tmu_notifier;

	bool use_dvfs;
	bool dll_status;
	bool dll_gated;

	void __iomem *base_mif;
	void __iomem *base_sysreg_mif;
	void __iomem *base_drex0;
	void __iomem *base_drex1;
	void __iomem *base_lpddr_phy0;
	void __iomem *base_lpddr_phy1;
};

struct devfreq_mif_timing_parameter {
	unsigned int timing_row;
	unsigned int timing_data;
	unsigned int timing_power;
	unsigned int rd_fetch;
	unsigned int timing_rfcpb;
	unsigned int dvfs_con1;
	unsigned int dvfs_offset;
};

struct devfreq_thermal_work {
	struct delayed_work devfreq_mif_thermal_work;
	enum devfreq_mif_thermal_channel channel;
	struct workqueue_struct *work_queue;
	unsigned int thermal_level_cs0;
	unsigned int thermal_level_cs1;
	unsigned int polling_period;
	unsigned long max_freq;
};

struct devfreq_clk_list devfreq_mif_clk[CLK_COUNT] = {
	{"fout_mem0_pll",},
	{"fout_mem1_pll",},
	{"fout_mfc_pll",},
	{"fout_bus_pll",},
	{"mout_mem0_pll",},
	{"mout_mem0_pll_div2",},
	{"mout_mem1_pll",},
	{"mout_mem1_pll_div2",},
	{"mout_mfc_pll",},
	{"mout_mfc_pll_div2",},
	{"mout_bus_pll",},
	{"mout_bus_pll_div2",},
	{"mout_aclk_mif_400",},
	{"dout_aclk_mif_400",},
	{"dout_aclk_mif_200",},
	{"dout_mif_pre",},
	{"mout_aclk_mifnm_200",},
	{"dout_aclk_mifnm_200",},
	{"dout_aclk_mifnd_133",},
	{"dout_aclk_mif_133",},
	{"dout_aclk_cpif_200",},
	{"dout_clk2x_phy",},
	{"dout_aclk_drex0",},
	{"dout_aclk_drex1",},
	{"dout_sclk_hpm_mif",},
};

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_opp_table devfreq_mif_opp_list[] = {
	{LV0,	825000, 1050000},
	{LV1,	633000,	 962500},
	{LV2,	413000,	 937500},
	{LV3,	317000,	 862500},
	{LV4,	211000,  862500},
	{LV5,	158000,  862500},
	{LV6,	127000,  862500},
	{LV7,	106000,  862500},
	{LV8,	 79000,	 862500},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_opp_table devfreq_mif_opp_list[] = {
	{LV0,	825000,	1050000},
	{LV1,	633000,	1000000},
	{LV2,	543000,	 975000},
	{LV3,	413000,	 950000},
	{LV4,	272000,  950000},
	{LV5,	211000,  950000},
	{LV6,	158000,  925000},
	{LV7,	136000,  900000},
	{LV8,	109000,	 875000},
};
#endif

static unsigned int devfreq_mif_asv_abb[LV_COUNT];

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_value aclk_clk2x_825[] = {
	{0x1000, (0x1 << 20), ((0xF << 28) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_633[] = {
	{0x1000, ((0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_413[] = {
	{0x1000, ((0x1 << 28) | (0x1 << 20)), ((0xF << 28) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_317[] = {
	{0x1000, ((0x1 << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_211[] = {
	{0x1000, ((0x2 << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_158[] = {
	{0x1000, ((0x3 << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_127[] = {
	{0x1000, ((0x4 << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_106[] = {
	{0x1000, ((0x5 << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_79[] = {
	{0x1000, ((0x7 << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_value aclk_clk2x_825[] = {
	{0x1000, (0x1 << 20), ((0xF << 28) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_633[] = {
	{0x1000, ((0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_543[] = {
	{0x1000, 0, ((0xF << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_413[] = {
	{0x1000, ((0x1 << 28) | (0x1 << 20)), ((0xF << 28) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_272[] = {
	{0x1000, (0x1 << 28), ((0xF << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_211[] = {
	{0x1000, ((0x2 << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_158[] = {
	{0x1000, ((0x3 << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_136[] = {
	{0x1000, (0x3 << 28), ((0xF << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_109[] = {
	{0x1000, (0x4 << 28), ((0xF << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12))},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_value *aclk_clk2x_list[] = {
	aclk_clk2x_825,
	aclk_clk2x_633,
	aclk_clk2x_413,
	aclk_clk2x_317,
	aclk_clk2x_211,
	aclk_clk2x_158,
	aclk_clk2x_127,
	aclk_clk2x_106,
	aclk_clk2x_79,
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_value *aclk_clk2x_list[] = {
	aclk_clk2x_825,
	aclk_clk2x_633,
	aclk_clk2x_543,
	aclk_clk2x_413,
	aclk_clk2x_272,
	aclk_clk2x_211,
	aclk_clk2x_158,
	aclk_clk2x_136,
	aclk_clk2x_109,
};
#endif

struct devfreq_clk_state aclk_mif_400_mem1_pll[] = {
	{MOUT_ACLK_MIF_400,     MOUT_MEM1_PLL_DIV2},
};

struct devfreq_clk_state aclk_mifnm_200_bus_pll[] = {
	{DOUT_MIF_PRE,		MOUT_ACLK_MIFNM_200},
};

struct devfreq_clk_states aclk_mif_400_mem1_pll_list = {
	.state = aclk_mif_400_mem1_pll,
	.state_count = ARRAY_SIZE(aclk_mif_400_mem1_pll),
};

struct devfreq_clk_states aclk_mifnm_200_bus_pll_list = {
	.state = aclk_mifnm_200_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mifnm_200_bus_pll),
};

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_mif_400[] = {
	{LV0,   413000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV1,   275000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV2,   275000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV3,   207000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV4,   207000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV5,   165000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV6,   165000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV7,   138000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV8,   138000000,      0,      &aclk_mif_400_mem1_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_mif_400[] = {
	{LV0,   413000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV1,   275000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV2,   275000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV3,   275000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV4,   275000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV5,   207000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV6,   165000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV7,   165000000,      0,      &aclk_mif_400_mem1_pll_list},
	{LV8,   138000000,      0,      &aclk_mif_400_mem1_pll_list},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_mif_200[] = {
	{LV0,	207000000,      0,      NULL},
	{LV1,	138000000,      0,      NULL},
	{LV2,	138000000,      0,      NULL},
	{LV3,	104000000,      0,      NULL},
	{LV4,	104000000,      0,      NULL},
	{LV5,	 83000000,      0,      NULL},
	{LV6,	 83000000,      0,      NULL},
	{LV7,	 69000000,      0,      NULL},
	{LV8,	 69000000,      0,      NULL},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_mif_200[] = {
	{LV0,	207000000,      0,      NULL},
	{LV1,	138000000,      0,      NULL},
	{LV2,	138000000,      0,      NULL},
	{LV3,	138000000,      0,      NULL},
	{LV4,	138000000,      0,      NULL},
	{LV5,	104000000,      0,      NULL},
	{LV6,	 83000000,      0,      NULL},
	{LV7,	 83000000,      0,      NULL},
	{LV8,	 69000000,      0,      NULL},
};
#endif

struct devfreq_clk_info mif_pre[] = {
	{LV0,   400000000,      0,      NULL},
	{LV1,   400000000,      0,      NULL},
	{LV2,   400000000,      0,      NULL},
	{LV3,   400000000,      0,      NULL},
	{LV4,   400000000,      0,      NULL},
	{LV5,   400000000,      0,      NULL},
	{LV6,   400000000,      0,      NULL},
	{LV7,   400000000,      0,      NULL},
	{LV8,   400000000,      0,      NULL},
};

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_mifnm[] = {
	{LV0,   134000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV1,   134000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV2,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV3,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV4,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV5,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV6,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV7,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV8,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_mifnm[] = {
	{LV0,   134000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV1,   134000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV2,   134000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV3,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV4,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV5,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV6,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV7,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{LV8,   100000000,      0,      &aclk_mifnm_200_bus_pll_list},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_mifnd[] = {
	{LV0,	 80000000,      0,      NULL},
	{LV1,	 80000000,      0,      NULL},
	{LV2,	 67000000,      0,      NULL},
	{LV3,	 67000000,      0,      NULL},
	{LV4,	 67000000,      0,      NULL},
	{LV5,	 67000000,      0,      NULL},
	{LV6,	 67000000,      0,      NULL},
	{LV7,	 67000000,      0,      NULL},
	{LV8,	 67000000,      0,      NULL},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_mifnd[] = {
	{LV0,	 80000000,      0,      NULL},
	{LV1,	 80000000,      0,      NULL},
	{LV2,	 80000000,      0,      NULL},
	{LV3,	 67000000,      0,      NULL},
	{LV4,	 67000000,      0,      NULL},
	{LV5,	 67000000,      0,      NULL},
	{LV6,	 67000000,      0,      NULL},
	{LV7,	 67000000,      0,      NULL},
	{LV8,	 67000000,      0,      NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_mif_133[] = {
	{LV0,	 80000000,      0,      NULL},
	{LV1,	 67000000,      0,      NULL},
	{LV2,	 67000000,      0,      NULL},
	{LV3,	 50000000,      0,      NULL},
	{LV4,	 50000000,      0,      NULL},
	{LV5,	 50000000,      0,      NULL},
	{LV6,	 50000000,      0,      NULL},
	{LV7,	 50000000,      0,      NULL},
	{LV8,	 50000000,      0,      NULL},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_mif_133[] = {
	{LV0,	 80000000,      0,      NULL},
	{LV1,	 67000000,      0,      NULL},
	{LV2,	 67000000,      0,      NULL},
	{LV3,	 67000000,      0,      NULL},
	{LV4,	 67000000,      0,      NULL},
	{LV5,	 50000000,      0,      NULL},
	{LV6,	 50000000,      0,      NULL},
	{LV7,	 50000000,      0,      NULL},
	{LV8,	 50000000,      0,      NULL},
};
#endif

struct devfreq_clk_info aclk_cpif_200[] = {
	{LV0,   100000000,      0,      NULL},
	{LV1,   100000000,      0,      NULL},
	{LV2,   100000000,      0,      NULL},
	{LV3,   100000000,      0,      NULL},
	{LV4,   100000000,      0,      NULL},
	{LV5,   100000000,      0,      NULL},
	{LV6,   100000000,      0,      NULL},
	{LV7,   100000000,      0,      NULL},
	{LV8,   100000000,      0,      NULL},
};

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info sclk_hpm_mif[] = {
	{LV0,	207000000,      0,      NULL},
	{LV1,	159000000,      0,      NULL},
	{LV2,	104000000,      0,      NULL},
	{LV3,	 80000000,      0,      NULL},
	{LV4,	 53000000,      0,      NULL},
	{LV5,	 40000000,      0,      NULL},
	{LV6,	 32000000,      0,      NULL},
	{LV7,	 27000000,      0,      NULL},
	{LV8,	 20000000,      0,      NULL},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info sclk_hpm_mif[] = {
	{LV0,	207000000,      0,      NULL},
	{LV1,	159000000,      0,      NULL},
	{LV2,	136000000,      0,      NULL},
	{LV3,	104000000,      0,      NULL},
	{LV4,	 69000000,      0,      NULL},
	{LV5,	 53000000,      0,      NULL},
	{LV6,	 42000000,      0,      NULL},
	{LV7,	 35000000,      0,      NULL},
	{LV8,	 26000000,      0,      NULL},
};
#endif

struct devfreq_clk_info *devfreq_clk_mif_info_list[] = {
	aclk_mif_400,
	aclk_mif_200,
	mif_pre,
	aclk_mifnm,
	aclk_mifnd,
	aclk_mif_133,
	aclk_cpif_200,
	sclk_hpm_mif,
};

enum devfreq_mif_clk devfreq_clk_mif_info_idx[] = {
	DOUT_ACLK_MIF_400,
	DOUT_ACLK_MIF_200,
	DOUT_MIF_PRE,
	DOUT_ACLK_MIFNM_200,
	DOUT_ACLK_MIFND_133,
	DOUT_ACLK_MIF_133,
	DOUT_ACLK_CPIF_200,
	DOUT_SCLK_HPM_MIF,
};

static struct devfreq_simple_exynos_data exynos5_devfreq_mif_governor_data = {
	.pm_qos_class		= PM_QOS_BUS_THROUGHPUT,
	.urgentthreshold	= 80,
	.upthreshold		= 70,
	.downthreshold		= 60,
	.idlethreshold		= 50,
	.cal_qos_max		= 825000,
};

static struct exynos_devfreq_platdata exynos5430_qos_mif = {
#if defined(CONFIG_EXYNOS5430_HD)
	.default_qos		=  79000,
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
	.default_qos		= 109000,
#endif
};

static struct ppmu_info ppmu_mif[] = {
	{
		.base = (void __iomem *)PPMU_D0_CPU_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D0_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D0_GEN_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_CPU_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_GEN_ADDR,
	},
};

static struct devfreq_exynos devfreq_mif_exynos = {
	.type = MIF,
	.ppmu_list = ppmu_mif,
	.ppmu_count = ARRAY_SIZE(ppmu_mif),
};

struct devfreq_mif_timing_parameter *dmc_timing_parameter;

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_mif_timing_parameter dmc_timing_parameter_2gb[] = {
	{	/* 825Mhz */
		.timing_row	= 0x36588652,
		.timing_data	= 0x4740085E,
		.timing_power	= 0x4C3A0446,
		.rd_fetch	= 0x00000003,
		.timing_rfcpb	= 0x00001919,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 0,
	}, {	/* 633Mhz */
		.timing_row	= 0x2A4674CE,
		.timing_data	= 0x3530084E,
		.timing_power	= 0x3C2D0335,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001313,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 0,
	}, {	/* 413Mhz */
		.timing_row	= 0x1B34534A,
		.timing_data	= 0x2420083E,
		.timing_power	= 0x281F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000D0D,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 15,
	}, {	/* 317Mhz */
		.timing_row	= 0x19234288,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x201F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000A0A,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 30,
	}, {	/* 211Mhz */
		.timing_row	= 0x192231C5,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x141F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000707,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 30,
	}, {	/* 158Mhz */
		.timing_row	= 0x19222144,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x101F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000505,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 40,
	}, {	/* 127Mhz */
		.timing_row	= 0x19222104,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x101F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000404,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 40,
	}, {	/* 106Mhz */
		.timing_row	= 0x19222103,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x101F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000404,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 40,
	}, {	/* 79Mhz */
		.timing_row	= 0x192220C3,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x101F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000303,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 40,
	},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_mif_timing_parameter dmc_timing_parameter_2gb[] = {
	{	/* 825Mhz */
		.timing_row	= 0x365A9713,
		.timing_data	= 0x4740085E,
		.timing_power	= 0x543A0446,
		.rd_fetch	= 0x00000003,
		.timing_rfcpb	= 0x00001919,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 0,
	}, {	/* 633Mhz */
		.timing_row	= 0x2A48758F,
		.timing_data	= 0x3530084E,
		.timing_power	= 0x402D0335,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001313,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 0,
	}, {	/* 543Mhz */
		.timing_row	= 0x244764CD,
		.timing_data	= 0x3530084E,
		.timing_power	= 0x38270335,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001111,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 0,
	}, {	/* 413Mhz */
		.timing_row	= 0x1B35538A,
		.timing_data	= 0x2420083E,
		.timing_power	= 0x2C1F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000D0D,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 15,
	}, {	/* 275Mhz */
		.timing_row	= 0x19244287,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x1C1F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000A0A,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 30,
	}, {	/* 206Mhz */
		.timing_row	= 0x19233206,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x181F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000707,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 30,
	}, {	/* 165Mhz */
		.timing_row	= 0x19223184,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x101F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000505,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 40,
	}, {	/* 138Mhz */
		.timing_row	= 0x19222144,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x101F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000404,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 40,
	}, {	/* 103Mhz */
		.timing_row	= 0x19222103,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x101F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000404,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 40,
	},
};
#endif

struct devfreq_mif_timing_parameter dmc_timing_parameter_3gb[] = {
	{	/* 825Mhz */
		.timing_row	= 0x575A9713,
		.timing_data	= 0x4740085E,
		.timing_power	= 0x545B0446,
		.rd_fetch	= 0x00000003,
		.timing_rfcpb	= 0x00002626,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 0,
	}, {	/* 633Mhz */
		.timing_row	= 0x4348758F,
		.timing_data	= 0x3530084E,
		.timing_power	= 0x40460335,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001D1D,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 0,
	}, {	/* 543Mhz */
		.timing_row	= 0x3A4764CD,
		.timing_data	= 0x3530084E,
		.timing_power	= 0x383C0335,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001919,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 0,
	}, {	/* 413Mhz */
		.timing_row	= 0x2C35538A,
		.timing_data	= 0x2420083E,
		.timing_power	= 0x2C2E0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001313,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 15,
	}, {	/* 272Mhz */
		.timing_row	= 0x1D244287,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x1C1F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000D0D,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 30,
	}, {	/* 211Mhz */
		.timing_row	= 0x17233206,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x181F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000A0A,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 30,
	}, {	/* 158Mhz */
		.timing_row	= 0x12223185,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x141F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000808,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 40,
	}, {	/* 136Mhz */
		.timing_row	= 0x11222144,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x101F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000707,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 40,
	}, {	/* 109Mhz */
		.timing_row	= 0x11222103,
		.timing_data	= 0x2320082E,
		.timing_power	= 0x101F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000505,
		.dvfs_con1      = 0x0E0E2121,
		.dvfs_offset    = 40,
	},
};

static struct workqueue_struct *devfreq_mif_thermal_wq_ch0;
static struct workqueue_struct *devfreq_mif_thermal_wq_ch1;
static struct devfreq_thermal_work devfreq_mif_ch0_work = {
	.channel = THERMAL_CHANNEL0,
	.polling_period = 1000,
};
static struct devfreq_thermal_work devfreq_mif_ch1_work = {
	.channel = THERMAL_CHANNEL1,
	.polling_period = 1000,
};
struct devfreq_data_mif *data_mif;

static struct pm_qos_request exynos5_mif_qos;
static struct pm_qos_request boot_mif_qos;
static struct pm_qos_request min_mif_thermal_qos;
static struct pm_qos_request exynos5_mif_bts_qos;

static bool use_mif_timing_set_0;

static DEFINE_MUTEX(media_mutex);
static unsigned int media_enabled_fimc_lite;
static unsigned int media_enabled_gscl_local;
static unsigned int media_enabled_tv;
static unsigned int media_num_mixer_layer;
static unsigned int media_num_decon_layer;
static enum devfreq_media_resolution media_resolution;
static unsigned int media_resolution_bandwidth;

static unsigned int (*timeout_table)[2];
static unsigned int wqhd_tv_window5;
static unsigned int hd_normal_window5;

struct devfreq_distriction_level {
	int mif_level;
	int disp_level;
};

struct devfreq_distriction_level distriction_hd[] = {
	{LV8,	LV3},
	{LV8,	LV3},
	{LV8,	LV3},
	{LV7,	LV3},
	{LV7,	LV3},
	{LV7,	LV3},
	{LV0,	LV0},
};

unsigned int timeout_hd[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

struct devfreq_distriction_level distriction_hd_gscl[] = {
	{LV6,	LV3},
	{LV5,	LV3},
	{LV5,	LV3},
	{LV4,	LV3},
	{LV4,	LV3},
	{LV0,	LV0},
	{LV0,	LV0},
};

unsigned int timeout_hd_gscl[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

struct devfreq_distriction_level distriction_hd_tv[] = {
	{LV4,	LV1},
	{LV4,	LV1},
	{LV4,	LV1},
	{LV4,	LV1},
	{LV4,	LV1},
	{LV3,	LV1},
	{LV0,	LV0},
};

unsigned int timeout_hd_tv[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

struct devfreq_distriction_level distriction_hd_camera[] = {
	{LV3,	LV3},
	{LV3,	LV3},
	{LV3,	LV3},
	{LV3,	LV3},
	{LV2,	LV3},
	{LV2,	LV3},
	{LV0,	LV0},
};

unsigned int timeout_hd_camera[][2] = {
	{0x00800080,	0x00000000},
	{0x00800080,	0x00000000},
	{0x00800080,	0x00000000},
	{0x00800080,	0x00000000},
	{0x00800080,	0x00000000},
	{0x00800080,	0x00000000},
	{0x00800080,	0x00000000},
	{0x00800080,	0x00000000},
	{0x00800080,	0x00000000},
};

struct devfreq_distriction_level distriction_fullhd[] = {
	{LV8,	LV3},			/* 103000 */
	{LV8,	LV3},			/* 103000 */
	{LV7,	LV3},			/* 138000 */
	{LV6,	LV2},			/* 165000 */
	{LV5,	LV2},			/* 206000 */
	{LV4,	LV2},			/* 275000 */
	{LV0,	LV0},			/* 825000 */
};

unsigned int timeout_fullhd[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

struct devfreq_distriction_level distriction_fullhd_gscl[] = {
	{LV6,	LV2},
	{LV5,	LV2},
	{LV5,	LV2},
	{LV4,	LV2},
	{LV4,	LV2},
	{LV8,	LV3},
	{LV0,	LV0},
};

unsigned int timeout_fullhd_gscl[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

struct devfreq_distriction_level distriction_fullhd_tv[] = {
	{LV4,	LV1},
	{LV4,	LV1},
	{LV4,	LV1},
	{LV4,	LV1},
	{LV4,	LV1},
	{LV3,	LV1},
	{LV0,	LV0},
};

unsigned int timeout_fullhd_tv[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};


struct devfreq_distriction_level distriction_fullhd_camera[] = {
	{LV3,	LV3},
	{LV3,	LV3},
	{LV3,	LV3},
	{LV3,	LV2},
	{LV3,	LV2},
	{LV3,	LV2},
	{LV0,	LV0},
};

unsigned int timeout_fullhd_camera[][2] = {
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

struct devfreq_distriction_level distriction_wqhd[] = {
	{LV8,   LV2},
	{LV7,	LV2},
	{LV5,	LV1},
	{LV4,	LV0},
	{LV3,   LV0},
	{LV2,	LV0},
	{LV0,	LV0},
};

unsigned int timeout_wqhd[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x02000200,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
};

struct devfreq_distriction_level distriction_wqhd_gscl[] = {
	{LV7,	LV2},
	{LV6,	LV2},
	{LV4,	LV1},
	{LV3,   LV0},
	{LV2,	LV0},
	{LV2,	LV0},
	{LV0,	LV0},
};

unsigned int timeout_wqhd_gscl[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x02000200,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
};

struct devfreq_distriction_level distriction_wqhd_tv[] = {
	{LV8,	LV1},
	{LV5,	LV1},
	{LV4,	LV1},
	{LV3,	LV0},
	{LV2,	LV0},
	{LV0,	LV0},
	{LV0,	LV0},
};

unsigned int timeout_wqhd_tv[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x02000200,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
};

struct devfreq_distriction_level distriction_wqhd_camera[] = {
	{LV3,	LV1},
	{LV3,	LV1},
	{LV3,	LV1},
	{LV3,	LV0},
	{LV3,	LV0},
	{LV3,	LV0},
	{LV0,	LV0},
};

unsigned int timeout_wqhd_camera[][2] = {
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

extern void exynos5_update_district_disp_level(unsigned int idx);

static int exynos5_devfreq_mif_set_timeout(struct devfreq_data_mif *data,
					int target_idx)
{
	int level_up_condition;

#if defined(CONFIG_EXYNOS5430_HD)
	level_up_condition = LV6;
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
	level_up_condition = LV7;
#else
#error resolution type should be selected at least one.
#endif

	if (timeout_table == NULL) {
		pr_err("DEVFREQ(MIF) : can't setting timeout value\n");
		return -EINVAL;
	}
	if (media_enabled_fimc_lite) {
		__raw_writel(timeout_table[target_idx][0], data->base_drex0 + 0xD0);
		__raw_writel(timeout_table[target_idx][0], data->base_drex0 + 0xC0);
		__raw_writel(timeout_table[target_idx][1], data->base_drex0 + 0x100);

		__raw_writel(timeout_table[target_idx][0], data->base_drex1 + 0xD0);
		__raw_writel(timeout_table[target_idx][0], data->base_drex1 + 0xC0);
		__raw_writel(timeout_table[target_idx][1], data->base_drex1 + 0x100);

		__raw_writel(0x0fff0fff, data->base_drex0 + 0xC8);
		__raw_writel(0x0fff0fff, data->base_drex1 + 0xC8);
	} else {
		if (wqhd_tv_window5 &&
				target_idx == LV1) {
			__raw_writel(timeout_table[LV2][0], data->base_drex0 + 0xD0);
			__raw_writel(timeout_table[LV2][0], data->base_drex0 + 0xC8);
			__raw_writel(timeout_table[LV2][0], data->base_drex0 + 0xC0);
			__raw_writel(timeout_table[LV2][1], data->base_drex0 + 0x100);
		} else if (hd_normal_window5 &&
				target_idx == level_up_condition) {
			__raw_writel(timeout_table[level_up_condition - 1][0], data->base_drex0 + 0xD0);
			__raw_writel(timeout_table[level_up_condition - 1][0], data->base_drex0 + 0xC8);
			__raw_writel(timeout_table[level_up_condition - 1][0], data->base_drex0 + 0xC0);
			__raw_writel(timeout_table[level_up_condition - 1][1], data->base_drex0 + 0x100);
		} else {
			__raw_writel(timeout_table[target_idx][0], data->base_drex0 + 0xD0);
			__raw_writel(timeout_table[target_idx][0], data->base_drex0 + 0xC8);
			__raw_writel(timeout_table[target_idx][0], data->base_drex0 + 0xC0);
			__raw_writel(timeout_table[target_idx][1], data->base_drex0 + 0x100);
		}

		if (wqhd_tv_window5 &&
				target_idx == LV1) {
			__raw_writel(timeout_table[LV2][0], data->base_drex1 + 0xD0);
			__raw_writel(timeout_table[LV2][0], data->base_drex1 + 0xC8);
			__raw_writel(timeout_table[LV2][0], data->base_drex1 + 0xC0);
			__raw_writel(timeout_table[LV2][1], data->base_drex1 + 0x100);
		} else if (hd_normal_window5 &&
				target_idx == level_up_condition) {
			__raw_writel(timeout_table[level_up_condition - 1][0], data->base_drex0 + 0xD0);
			__raw_writel(timeout_table[level_up_condition - 1][0], data->base_drex0 + 0xC8);
			__raw_writel(timeout_table[level_up_condition - 1][0], data->base_drex0 + 0xC0);
			__raw_writel(timeout_table[level_up_condition - 1][1], data->base_drex0 + 0x100);
		} else {
			__raw_writel(timeout_table[target_idx][0], data->base_drex1 + 0xD0);
			__raw_writel(timeout_table[target_idx][0], data->base_drex1 + 0xC8);
			__raw_writel(timeout_table[target_idx][0], data->base_drex1 + 0xC0);
			__raw_writel(timeout_table[target_idx][1], data->base_drex1 + 0x100);
		}
	}

	return 0;
}

static int exynos5_set_timeout_table_hd(unsigned int total_layer_count, int *mif_qos, int *disp_qos)
{
	if (media_enabled_fimc_lite) {
		if (*mif_qos > distriction_hd_camera[total_layer_count].mif_level)
			*mif_qos = distriction_hd_camera[total_layer_count].mif_level;
		timeout_table = timeout_hd_camera;
	}
	if (media_enabled_gscl_local) {
		if (total_layer_count == NUM_LAYER_5) {
			pr_err("DEVFREQ(MIF) : can't support mif and disp distriction. using gscl local with 5 windows.\n");
			return false;
		}
		if (*mif_qos > distriction_hd_gscl[total_layer_count].mif_level)
			*mif_qos = distriction_hd_gscl[total_layer_count].mif_level;
		if (*disp_qos > distriction_hd_gscl[total_layer_count].disp_level)
			*disp_qos = distriction_hd_gscl[total_layer_count].disp_level;
		timeout_table = timeout_hd_gscl;
	}
	if (media_enabled_tv) {
		if (*mif_qos > distriction_hd_tv[total_layer_count].mif_level)
			*mif_qos = distriction_hd_tv[total_layer_count].mif_level;
		if (*disp_qos > distriction_hd_tv[total_layer_count].disp_level)
			*disp_qos = distriction_hd_tv[total_layer_count].disp_level;
		timeout_table = timeout_hd_tv;
	}
	if (!media_enabled_fimc_lite && !media_enabled_gscl_local && !media_enabled_tv) {
		timeout_table = timeout_hd;
		hd_normal_window5 = (total_layer_count == NUM_LAYER_5);
	} else {
		hd_normal_window5 = false;
	}
	if (*mif_qos > distriction_hd[total_layer_count].mif_level)
		*mif_qos = distriction_hd[total_layer_count].mif_level;
	if (*disp_qos > distriction_hd[total_layer_count].disp_level)
		*disp_qos = distriction_hd[total_layer_count].disp_level;

	return true;
}

static int exynos5_set_timeout_table_fullhd(unsigned int total_layer_count, int *mif_qos, int *disp_qos)
{
	if (media_enabled_fimc_lite) {
		if (*mif_qos > distriction_fullhd_camera[total_layer_count].mif_level)
			*mif_qos = distriction_fullhd_camera[total_layer_count].mif_level;
		timeout_table = timeout_fullhd_camera;
	}
	if (media_enabled_gscl_local) {
		if (total_layer_count == NUM_LAYER_5) {
			pr_err("DEVFREQ(MIF) : can't support mif and disp distriction. using gscl local with 5 windows.\n");
			return false;
		}
		if (*mif_qos > distriction_fullhd_gscl[total_layer_count].mif_level)
			*mif_qos = distriction_fullhd_gscl[total_layer_count].mif_level;
		if (*disp_qos > distriction_fullhd_gscl[total_layer_count].disp_level)
			*disp_qos = distriction_fullhd_gscl[total_layer_count].disp_level;
		timeout_table = timeout_fullhd_gscl;
	}
	if (media_enabled_tv) {
		if (*mif_qos > distriction_fullhd_tv[total_layer_count].mif_level)
			*mif_qos = distriction_fullhd_tv[total_layer_count].mif_level;
		if (*disp_qos > distriction_fullhd_tv[total_layer_count].disp_level)
			*disp_qos = distriction_fullhd_tv[total_layer_count].disp_level;
		timeout_table = timeout_fullhd_tv;
	}
	if (!media_enabled_fimc_lite && !media_enabled_gscl_local && !media_enabled_tv)
		timeout_table = timeout_fullhd;
	if (*mif_qos > distriction_fullhd[total_layer_count].mif_level)
		*mif_qos = distriction_fullhd[total_layer_count].mif_level;
	if (*disp_qos > distriction_fullhd[total_layer_count].disp_level)
		*disp_qos = distriction_fullhd[total_layer_count].disp_level;

	return true;
}

static int exynos5_set_timeout_table_wqhd(unsigned int total_layer_count, int *mif_qos, int *disp_qos)
{
	if (media_enabled_fimc_lite) {
		if (*mif_qos > distriction_wqhd_camera[total_layer_count + media_enabled_gscl_local].mif_level)
			*mif_qos = distriction_wqhd_camera[total_layer_count + media_enabled_gscl_local].mif_level;
		if (*disp_qos > distriction_wqhd_camera[total_layer_count].disp_level)
			*disp_qos = distriction_wqhd_camera[total_layer_count].disp_level;
		timeout_table = timeout_wqhd_camera;
	}
	if (media_enabled_tv) {
		if (*mif_qos > distriction_wqhd_tv[total_layer_count + media_enabled_gscl_local].mif_level)
			*mif_qos = distriction_wqhd_tv[total_layer_count + media_enabled_gscl_local].mif_level;
		if (*disp_qos > distriction_wqhd_tv[total_layer_count].disp_level)
			*disp_qos = distriction_wqhd_tv[total_layer_count].disp_level;
		timeout_table = timeout_wqhd_tv;
			wqhd_tv_window5 = (total_layer_count == NUM_LAYER_5);
	} else {
		wqhd_tv_window5 = false;
	}
	if (!media_enabled_fimc_lite && !media_enabled_gscl_local && !media_enabled_tv)
		timeout_table = timeout_wqhd;
	if (*mif_qos > distriction_wqhd[total_layer_count + media_enabled_gscl_local].mif_level)
		*mif_qos = distriction_wqhd[total_layer_count + media_enabled_gscl_local].mif_level;
	if (*disp_qos > distriction_wqhd[total_layer_count].disp_level)
		*disp_qos = distriction_wqhd[total_layer_count].disp_level;

	return true;
}

void exynos5_update_media_layers(enum devfreq_media_type media_type, unsigned int value)
{
	unsigned int total_layer_count = 0;
	int disp_qos = LV3;
	int mif_qos = LV8;
	int ret = false;
	int tv_layer_value;

	mutex_lock(&media_mutex);

	switch (media_type) {
	case TYPE_FIMC_LITE:
		media_enabled_fimc_lite = value;
		break;
	case TYPE_MIXER:
		media_num_mixer_layer = value;
		break;
	case TYPE_DECON:
		media_num_decon_layer = value;
		break;
	case TYPE_GSCL_LOCAL:
		media_enabled_gscl_local = value;
		break;
	case TYPE_TV:
		media_enabled_tv = !!value;
		tv_layer_value = value;
		if (tv_layer_value == 0)
			break;
		switch (media_resolution) {
		case RESOLUTION_HD:
			tv_layer_value = (tv_layer_value - (TRAFFIC_BYTES_FHD_32BIT_60FPS * 2)
					+ (TRAFFIC_BYTES_FHD_32BIT_60FPS - 1));
			if (tv_layer_value < 0)
				tv_layer_value = 0;
			media_num_mixer_layer = tv_layer_value / TRAFFIC_BYTES_FHD_32BIT_60FPS;
			break;
		case RESOLUTION_FULLHD:
		case RESOLUTION_WQHD:
		case RESOLUTION_WQXGA:
			tv_layer_value = (tv_layer_value - (TRAFFIC_BYTES_FHD_32BIT_60FPS * 2)
					+ (media_resolution_bandwidth - 1));
			if (tv_layer_value < 0)
				tv_layer_value = 0;
			media_num_mixer_layer = tv_layer_value / media_resolution_bandwidth;
			break;
		default:
			pr_err("DEVFREQ(MIF) : can't calculate mixer layer by traffic(%u)\n", media_resolution);
			break;
		}
		break;
	case TYPE_RESOLUTION:
		switch (value) {
		case TRAFFIC_BYTES_HD_32BIT_60FPS:
			media_resolution = RESOLUTION_HD;
			break;
		case TRAFFIC_BYTES_FHD_32BIT_60FPS:
			media_resolution = RESOLUTION_FULLHD;
			break;
		case TRAFFIC_BYTES_WQHD_32BIT_60FPS:
			media_resolution = RESOLUTION_WQHD;
			break;
		case TRAFFIC_BYTES_WQXGA_32BIT_60FPS:
			media_resolution = RESOLUTION_WQXGA;
			break;
		default:
			pr_err("DEVFREQ(MIF) : can't decide resolution type by traffic bytes(%u)\n", value);
			break;
		}
		media_resolution_bandwidth = value;
		mutex_unlock(&media_mutex);
		return;
	}

	total_layer_count = media_num_mixer_layer + media_num_decon_layer;

	if (total_layer_count > 6)
		total_layer_count = 6;

	if (media_resolution == RESOLUTION_HD) {
		ret = exynos5_set_timeout_table_hd(total_layer_count, &mif_qos, &disp_qos);
	} else if (media_resolution == RESOLUTION_FULLHD) {
		ret = exynos5_set_timeout_table_fullhd(total_layer_count, &mif_qos, &disp_qos);
	} else if (media_resolution == RESOLUTION_WQHD) {
		ret = exynos5_set_timeout_table_wqhd(total_layer_count, &mif_qos, &disp_qos);
	}

	if (!ret)
		goto out;

	if (pm_qos_request_active(&exynos5_mif_bts_qos)) {
		if (mif_qos != LV8)
			pm_qos_update_request(&exynos5_mif_bts_qos, devfreq_mif_opp_list[mif_qos].freq);
		else
			pm_qos_update_request(&exynos5_mif_bts_qos, exynos5430_qos_mif.default_qos);
	}

	exynos5_update_district_disp_level(disp_qos);
out:
	mutex_unlock(&media_mutex);
}

static inline int exynos5_devfreq_mif_get_idx(struct devfreq_opp_table *table,
				unsigned int size,
				unsigned long freq)
{
	int i;

	for (i = 0; i < size; ++i) {
		if (table[i].freq == freq)
			return i;
	}

	return -1;
}

static int exynos5_devfreq_mif_update_timingset(struct devfreq_data_mif *data)
{
	use_mif_timing_set_0 = ((__raw_readl(data->base_mif + 0x1004) & 0x1) == 0);

	return 0;
}

static int exynos5_devfreq_mif_change_timing_set(struct devfreq_data_mif *data)
{
	unsigned int tmp;

	if (use_mif_timing_set_0) {
		tmp = __raw_readl(data->base_mif + 0x1004);
		tmp |= 0x1;
		__raw_writel(tmp, data->base_mif + 0x1004);

		tmp = __raw_readl(data->base_lpddr_phy0 + 0xB8);
		tmp &= ~(0x3 << 24);
		tmp |= (0x2 << 24);
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xB8);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xB8);
		tmp &= ~(0x3 << 24);
		tmp |= (0x2 << 24);
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xB8);

	} else {
		tmp = __raw_readl(data->base_mif + 0x1004);
		tmp &= ~0x1;
		__raw_writel(tmp, data->base_mif + 0x1004);

		tmp = __raw_readl(data->base_lpddr_phy0 + 0xB8);
		tmp &= ~(0x3 << 24);
		tmp |= (0x1 << 24);
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xB8);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xB8);
		tmp &= ~(0x3 << 24);
		tmp |= (0x1 << 24);
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xB8);
	}

	exynos5_devfreq_mif_update_timingset(data);

	return 0;
}

static int exynos5_devfreq_mif_set_phy(struct devfreq_data_mif *data,
		int target_idx)
{
	struct devfreq_mif_timing_parameter *cur_parameter;
	unsigned int tmp;

	cur_parameter = &dmc_timing_parameter[target_idx];

	if (use_mif_timing_set_0) {
		tmp = __raw_readl(data->base_lpddr_phy0 + 0xBC);
		tmp &= ~(0x1F << 24);
		tmp |= (cur_parameter->dvfs_con1 & (0x1F << 24));
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xBC);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xBC);
		tmp &= ~(0x1F << 24);
		tmp |= (cur_parameter->dvfs_con1 & (0x1F << 24));
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xBC);
	} else {
		tmp = __raw_readl(data->base_lpddr_phy0 + 0xBC);
		tmp &= ~(0x1F << 16);
		tmp |= (cur_parameter->dvfs_con1 & (0x1F << 16));
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xBC);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xBC);
		tmp &= ~(0x1F << 16);
		tmp |= (cur_parameter->dvfs_con1 & (0x1F << 16));
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xBC);
	}

	return 0;
}

static int exynos5_devfreq_mif_set_timing_set(struct devfreq_data_mif *data,
							int target_idx)
{
	struct devfreq_mif_timing_parameter *cur_parameter;
	unsigned int tmp;

	cur_parameter = &dmc_timing_parameter[target_idx];

	if (use_mif_timing_set_0) {
		__raw_writel(cur_parameter->timing_row, data->base_drex0 + 0xE4);
		__raw_writel(cur_parameter->timing_data, data->base_drex0 + 0xE8);
		__raw_writel(cur_parameter->timing_power, data->base_drex0 + 0xEC);
		tmp = __raw_readl(data->base_drex0 + 0x20);
		tmp &= ~(TIMING_RFCPB_MASK << 8);
		tmp |= (cur_parameter->timing_rfcpb & (TIMING_RFCPB_MASK << 8));
		__raw_writel(tmp, data->base_drex0 + 0x20);
		__raw_writel(cur_parameter->rd_fetch, data->base_drex0 + 0x50);

		__raw_writel(cur_parameter->timing_row, data->base_drex1 + 0xE4);
		__raw_writel(cur_parameter->timing_data, data->base_drex1 + 0xE8);
		__raw_writel(cur_parameter->timing_power, data->base_drex1 + 0xEC);
		tmp = __raw_readl(data->base_drex1 + 0x20);
		tmp &= ~(TIMING_RFCPB_MASK << 8);
		tmp |= (cur_parameter->timing_rfcpb & (TIMING_RFCPB_MASK << 8));
		__raw_writel(tmp, data->base_drex1 + 0x20);
		__raw_writel(cur_parameter->rd_fetch, data->base_drex1 + 0x50);

		tmp = __raw_readl(data->base_lpddr_phy0 + 0xB8);
		tmp &= ~(0xFF << 8);
		tmp |= (cur_parameter->dvfs_offset << 8);
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xB8);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xB8);
		tmp &= ~(0xFF << 8);
		tmp |= (cur_parameter->dvfs_offset << 8);
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xB8);
	} else {
		__raw_writel(cur_parameter->timing_row, data->base_drex0 + 0x34);
		__raw_writel(cur_parameter->timing_data, data->base_drex0 + 0x38);
		__raw_writel(cur_parameter->timing_power, data->base_drex0 + 0x3C);
		tmp = __raw_readl(data->base_drex0 + 0x20);
		tmp &= ~(TIMING_RFCPB_MASK);
		tmp |= (cur_parameter->timing_rfcpb & TIMING_RFCPB_MASK);
		__raw_writel(tmp, data->base_drex0 + 0x20);
		__raw_writel(cur_parameter->rd_fetch, data->base_drex0 + 0x4C);

		__raw_writel(cur_parameter->timing_row, data->base_drex1 + 0x34);
		__raw_writel(cur_parameter->timing_data, data->base_drex1 + 0x38);
		__raw_writel(cur_parameter->timing_power, data->base_drex1 + 0x3C);
		tmp = __raw_readl(data->base_drex1 + 0x20);
		tmp &= ~(TIMING_RFCPB_MASK);
		tmp |= (cur_parameter->timing_rfcpb & TIMING_RFCPB_MASK);
		__raw_writel(tmp, data->base_drex1 + 0x20);
		__raw_writel(cur_parameter->rd_fetch, data->base_drex1 + 0x4C);

		tmp = __raw_readl(data->base_lpddr_phy0 + 0xB8);
		tmp &= ~(0xFF);
		tmp |= (cur_parameter->dvfs_offset);
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xB8);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xB8);
		tmp &= ~(0xFF);
		tmp |= (cur_parameter->dvfs_offset);
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xB8);
	}

	return 0;
}

static int exynos5_devfreq_calculate_dll_lock_value(struct devfreq_data_mif *data,
							long vdd_mif_l0)
{
	return  ((vdd_mif_l0 - DLL_ON_BASE_VOLT + 9999) / 10000) * 2;
}

static void exynos5_devfreq_set_dll_lock_value(struct devfreq_data_mif *data,
							long vdd_mif_l0)
{
	/* 9999 make ceiling result */
	int lock_value_offset = exynos5_devfreq_calculate_dll_lock_value(data, vdd_mif_l0);
	int ctrl_force, ctrl_force_value;

	ctrl_force = __raw_readl(data->base_lpddr_phy0 + 0xB0);
	ctrl_force_value = (ctrl_force >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
	ctrl_force_value += lock_value_offset;
	ctrl_force &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
	ctrl_force |= ((ctrl_force_value & CTRL_FORCE_MASK) << CTRL_FORCE_SHIFT);
	__raw_writel(ctrl_force, data->base_lpddr_phy0 + 0xB0);

	ctrl_force = __raw_readl(data->base_lpddr_phy1 + 0xB0);
	ctrl_force_value = (ctrl_force >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
	ctrl_force_value += lock_value_offset;
	ctrl_force &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
	ctrl_force |= ((ctrl_force_value & CTRL_FORCE_MASK) << CTRL_FORCE_SHIFT);
	__raw_writel(ctrl_force, data->base_lpddr_phy1 + 0xB0);
}

static int exynos5_devfreq_mif_set_dll(struct devfreq_data_mif *data,
					unsigned long target_volt,
					int target_idx)
{
	unsigned int tmp;
	unsigned int lock_value;
	unsigned int timeout;

	if (target_idx == LV0) {
		if (data->dll_gated) {
			/* only LV0 use DLL tacing mode(CLKM_PHY_C_ENABLE mux gating 1(enable)/0(disable)). */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			tmp |= (0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);
			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			tmp |= (0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);

			timeout = 1000;
			while ((__raw_readl(data->base_lpddr_phy0 + 0xB4) & 0x5) != 0x5) {
				if (timeout-- == 0) {
					pr_err("DEVFREQ(MIF) : Timeout to wait dll on(lpddrphy0)\n");
					return -EINVAL;
				}
				udelay(1);
			}
			timeout = 1000;
			while ((__raw_readl(data->base_lpddr_phy1 + 0xB4) & 0x5) != 0x5) {
				if (timeout-- == 0) {
					pr_err("DEVFREQ(MIF) : Timeout to wait dll on(lpddrphy1)\n");
					return -EINVAL;
				}
				udelay(1);
			}
			data->dll_gated = false;
		}
	} else {
		if (!data->dll_gated) {
			/* DLL Tracing off mode */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			tmp &= ~(0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);
			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			tmp &= ~(0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);
			data->dll_gated = true;
		}

		if (data->dll_status) {
			/* Get Current DLL lock value */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB4);
			lock_value = (tmp >> CTRL_LOCK_VALUE_SHIFT) & CTRL_LOCK_VALUE_MASK;
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			tmp |= (0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (lock_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			tmp &= ~(0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);

			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB4);
			lock_value = (tmp >> CTRL_LOCK_VALUE_SHIFT) & CTRL_LOCK_VALUE_MASK;
			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			tmp |= (0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);
			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (lock_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);
			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			tmp &= ~(0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);

			data->dll_status = false;
		}
	}

	return 0;
}

static void exynos5_devfreq_mif_dynamic_setting(struct devfreq_data_mif *data,
						bool flag)
{
	unsigned int tmp;

	if (flag) {
		tmp = __raw_readl(data->base_drex0 + 0x0004);
		tmp |= ((0x1 << 5) | (0x1 << 1));
		__raw_writel(tmp, data->base_drex0 + 0x0004);
		tmp = __raw_readl(data->base_drex1 + 0x0004);
		tmp |= ((0x1 << 5) | (0x1 << 1));
		__raw_writel(tmp, data->base_drex1 + 0x0004);

		tmp = __raw_readl(data->base_drex0 + 0x0008);
		tmp |= (0x3F);
		__raw_writel(tmp, data->base_drex0 + 0x0008);
		tmp = __raw_readl(data->base_drex1 + 0x0008);
		tmp |= (0x3F);
		__raw_writel(tmp, data->base_drex1 + 0x0008);
	} else {
		tmp = __raw_readl(data->base_drex0 + 0x0004);
		tmp &= ~((0x1 << 5) | (0x1 << 1));
		__raw_writel(tmp, data->base_drex0 + 0x0004);
		tmp = __raw_readl(data->base_drex1 + 0x0004);
		tmp &= ~((0x1 << 5) | (0x1 << 1));
		__raw_writel(tmp, data->base_drex1 + 0x0004);

		tmp = __raw_readl(data->base_drex0 + 0x0008);
		tmp &= ~(0x3F);
		__raw_writel(tmp, data->base_drex0 + 0x0008);
		tmp = __raw_readl(data->base_drex1 + 0x0008);
		tmp &= ~(0x3F);
		__raw_writel(tmp, data->base_drex1 + 0x0008);
	}
}

static void exynos5_devfreq_waiting_pause(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;

	while ((__raw_readl(data->base_mif + 0x1008) & 0x00070000) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait pause completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
}

static void exynos5_devfreq_waiting_mux(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;

	while ((__raw_readl(data->base_mif + 0x0404) & 0x04440000) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait mux completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
	timeout = 1000;
	while ((__raw_readl(data->base_mif + 0x0704) & 0x00000010) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait divider completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
}

static int exynos5_devfreq_mif_set_freq(struct devfreq_data_mif *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
	unsigned int tmp;

	if (target_idx < old_idx) {
		tmp = __raw_readl(data->base_mif + aclk_clk2x_list[target_idx]->reg);
		tmp &= ~(aclk_clk2x_list[target_idx]->clr_value);
		tmp |= aclk_clk2x_list[target_idx]->set_value;
		__raw_writel(tmp, data->base_mif + aclk_clk2x_list[target_idx]->reg);

		exynos5_devfreq_waiting_pause(data);
		exynos5_devfreq_waiting_mux(data);

		for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); ++i) {
			clk_info = &devfreq_clk_mif_info_list[i][target_idx];
			clk_states = clk_info->states;
			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
						devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); ++i) {
			clk_info = &devfreq_clk_mif_info_list[i][target_idx];
			clk_states = clk_info->states;

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
						devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);
		}

		tmp = __raw_readl(data->base_mif + aclk_clk2x_list[target_idx]->reg);
		tmp &= ~(aclk_clk2x_list[target_idx]->clr_value);
		tmp |= aclk_clk2x_list[target_idx]->set_value;
		__raw_writel(tmp, data->base_mif + aclk_clk2x_list[target_idx]->reg);

		exynos5_devfreq_waiting_pause(data);
		exynos5_devfreq_waiting_mux(data);
	}

	return 0;
}

static int exynos5_devfreq_mif_set_volt(struct devfreq_data_mif *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_mif, volt, volt_range);
	data->old_volt = volt;
out:
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
static unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset)
{
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	return voltage + volt_offset;
}
#endif

unsigned long g_mif_freq;
unsigned long exynos5_devfreq_get_mif_freq(void)
{
	return g_mif_freq;
}

static int exynos5_devfreq_mif_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_mif *mif_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_mif = mif_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	mutex_lock(&mif_data->lock);

	*target_freq = min3(*target_freq,
			devfreq_mif_ch0_work.max_freq,
			devfreq_mif_ch1_work.max_freq);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		mutex_unlock(&mif_data->lock);
		dev_err(dev, "DEVFREQ(MIF) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, mif_data->volt_offset);
#endif
	rcu_read_unlock();

	target_idx = exynos5_devfreq_mif_get_idx(devfreq_mif_opp_list,
						ARRAY_SIZE(devfreq_mif_opp_list),
						*target_freq);
	old_idx = exynos5_devfreq_mif_get_idx(devfreq_mif_opp_list,
						ARRAY_SIZE(devfreq_mif_opp_list),
						devfreq_mif->previous_freq);
	old_freq = devfreq_mif->previous_freq;

	if (target_idx < 0 ||
		old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq) {
		exynos5_devfreq_mif_set_timeout(mif_data, target_idx);
		goto out;
	}

	exynos5_devfreq_mif_dynamic_setting(mif_data, false);
	if (old_freq < *target_freq) {
		exynos5_devfreq_mif_set_volt(mif_data, target_volt, target_volt + VOLT_STEP);
		set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
		exynos5_devfreq_mif_set_dll(mif_data, target_volt, target_idx);
		exynos5_devfreq_mif_set_timing_set(mif_data, target_idx);
		exynos5_devfreq_mif_set_phy(mif_data, target_idx);
		exynos5_devfreq_mif_change_timing_set(mif_data);
		exynos5_devfreq_mif_set_freq(mif_data, target_idx, old_idx);
		exynos5_devfreq_mif_set_timeout(mif_data, target_idx);
	} else {
		exynos5_devfreq_mif_set_timeout(mif_data, target_idx);
		exynos5_devfreq_mif_set_timing_set(mif_data, target_idx);
		exynos5_devfreq_mif_set_phy(mif_data, target_idx);
		exynos5_devfreq_mif_change_timing_set(mif_data);
		exynos5_devfreq_mif_set_freq(mif_data, target_idx, old_idx);
		exynos5_devfreq_mif_set_dll(mif_data, target_volt, target_idx);
		set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
		exynos5_devfreq_mif_set_volt(mif_data, target_volt, target_volt + VOLT_STEP);
	}
	exynos5_devfreq_mif_dynamic_setting(mif_data, true);

	g_mif_freq = *target_freq;
out:
	mutex_unlock(&mif_data->lock);

	return ret;
}

static int exynos5_devfreq_mif_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_mif *data = dev_get_drvdata(dev);
	unsigned int idx = -1;
	int above_idx = 0;
	int below_idx = LV_COUNT - 1;

	if (!data->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;

	idx = exynos5_devfreq_mif_get_idx(devfreq_mif_opp_list,
						ARRAY_SIZE(devfreq_mif_opp_list),
						stat->current_frequency);

	if (idx < 0)
                return -EAGAIN;

	above_idx = idx - 1;
	below_idx = idx + 1;

	if (above_idx < 0)
		above_idx = 0;

	if (below_idx >= LV_COUNT)
		below_idx = LV_COUNT - 1;

	exynos5_devfreq_mif_governor_data.above_freq = devfreq_mif_opp_list[above_idx].freq;
	exynos5_devfreq_mif_governor_data.below_freq = devfreq_mif_opp_list[below_idx].freq;

	stat->busy_time = devfreq_mif_exynos.val_pmcnt;
	stat->total_time = devfreq_mif_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_mif_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_mif_target,
	.get_dev_status	= exynos5_devfreq_mif_get_dev_status,
	.max_state	= LV_COUNT,
};

static int exynos5_devfreq_mif_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_clk); ++i) {
		devfreq_mif_clk[i].clk = __clk_lookup(devfreq_mif_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_mif_clk[i].clk)) {
			pr_err("DEVFREQ(MIF) : %s can't get clock\n", devfreq_mif_clk[i].clk_name);
			return -EINVAL;
		}
	}

	return 0;
}

static int exynos5_init_mif_table(struct device *dev,
				struct devfreq_data_mif *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_opp_list); ++i) {
		freq = devfreq_mif_opp_list[i].freq;
		volt = get_match_volt(ID_MIF, freq);
		if (!volt)
			volt = devfreq_mif_opp_list[i].volt;

		exynos5_devfreq_mif_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(MIF) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(MIF) : %uKhz, %uV\n", freq, volt);
		}

		devfreq_mif_asv_abb[i] = get_match_abb(ID_MIF, freq);

		pr_info("DEVFREQ(MIF) : %uKhz, ABB %u\n", freq, devfreq_mif_asv_abb[i]);
	}

	return 0;
}

static int exynos5_devfreq_mif_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_notifier_block *devfreq_nb;

	devfreq_nb = container_of(nb, struct devfreq_notifier_block, nb);

	mutex_lock(&devfreq_nb->df->lock);
	update_devfreq(devfreq_nb->df);
	mutex_unlock(&devfreq_nb->df->lock);

	return NOTIFY_OK;
}

static int exynos5_devfreq_mif_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_mif_qos, exynos5_devfreq_mif_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_mif_reboot_notifier = {
	.notifier_call = exynos5_devfreq_mif_reboot_notifier,
};

#define CTRL_FORCE_OFFSET	(8)

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos5_devfreq_mif_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_mif *data = container_of(nb, struct devfreq_data_mif,
							tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;
	unsigned int tmp;
	unsigned int ctrl_force_value;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&min_mif_thermal_qos))
			pm_qos_update_request(&min_mif_thermal_qos,
					exynos5_devfreq_mif_profile.initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_mif);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			regulator_set_voltage(data->vdd_mif, set_volt, set_volt + VOLT_STEP);

			/* Update CTRL FORCE */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			ctrl_force_value = (tmp >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
			ctrl_force_value += CTRL_FORCE_OFFSET;
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (ctrl_force_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);

			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			ctrl_force_value = (tmp >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
			ctrl_force_value += CTRL_FORCE_OFFSET;
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (ctrl_force_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_mif);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			regulator_set_voltage(data->vdd_mif, set_volt, set_volt + VOLT_STEP);

			/* Update CTRL FORCE */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			ctrl_force_value = (tmp >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
			ctrl_force_value -= CTRL_FORCE_OFFSET;
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (ctrl_force_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);

			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			ctrl_force_value = (tmp >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
			ctrl_force_value -= CTRL_FORCE_OFFSET;
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (ctrl_force_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_mif_thermal_qos))
			pm_qos_update_request(&min_mif_thermal_qos,
					exynos5430_qos_mif.default_qos);
	}

	return NOTIFY_OK;
}
#endif

static enum devfreq_memorysize exynos5_devfreq_mif_memory_size(struct devfreq_data_mif *data)
{
	unsigned int mrr_status;
	unsigned int mem_type;
	unsigned int density;
	unsigned int bus_width;

	/* Issuing MRR Command */
	__raw_writel(0x09010000, data->base_drex0 + 0x10);
	mrr_status = __raw_readl(data->base_drex0 + 0x54);

	mem_type = (mrr_status & 0x3);
	density = ((mrr_status >> 2) & 0xF);
	bus_width = ((mrr_status >> 6) & 0x3);

	if (mem_type == 0x3) {
		if (bus_width == 0x0) {
			switch (density) {
				case 0x6:
					pr_info("DEVFREQ(MIF) : Memory Size : 2GB\n");
					return LP3_2GB;
				case 0xE:
					pr_info("DEVFREQ(MIF) : Memory Size : 3GB\n");
					return LP3_3GB;
				case 0x7:
					pr_info("DEVFREQ(MIF) : Memory Size : 4GB\n");
					return LP3_4GB;
				default:
					pr_err("DEVFREQ(MIF) : can't support memory size %d\n", density);
					break;
			}
		} else {
			pr_err("DEVFREQ(MIF) : can't support bus width which is not 32\n");
		}
	} else {
		pr_err("DEVFREQ(MIF) : can't support device which is not S8 SDRAM\n");
	}

	return LP3_UNKNOWN;
}

static int exynos5_devfreq_mif_init_dvfs(struct devfreq_data_mif *data)
{
	switch (exynos5_devfreq_mif_memory_size(data)) {
	case LP3_2GB:
		dmc_timing_parameter = dmc_timing_parameter_2gb;
		break;
	case LP3_3GB:
		dmc_timing_parameter = dmc_timing_parameter_3gb;
		break;
	default:
		pr_err("DEVFREQ(MIF) : can't get information of memory size!!\n");
		break;
	}

	return 0;
}

static int exynos5_devfreq_mif_init_parameter(struct devfreq_data_mif *data)
{
	data->base_mif = ioremap(0x105B0000, SZ_64K);
	data->base_sysreg_mif = ioremap(0x105E0000, SZ_64K);
	data->base_drex0 = ioremap(0x10400000, SZ_64K);
	data->base_drex1 = ioremap(0x10440000, SZ_64K);
	data->base_lpddr_phy0 = ioremap(0x10420000, SZ_64K);
	data->base_lpddr_phy1 = ioremap(0x10460000, SZ_64K);

	exynos5_devfreq_mif_update_timingset(data);
	exynos5_devfreq_mif_init_dvfs(data);

	return 0;
}

static void exynos5_devfreq_thermal_event(struct devfreq_thermal_work *work)
{
	if (work->polling_period == 0)
		return;

#ifdef CONFIG_SCHED_HMP
	mod_delayed_work_on(0,
			work->work_queue,
			&work->devfreq_mif_thermal_work,
			msecs_to_jiffies(work->polling_period));
#else
	queue_delayed_work(work->work_queue,
			&work->devfreq_mif_thermal_work,
			msecs_to_jiffies(work->polling_period));
#endif
}

static ssize_t mif_show_templvl_ch0_0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch0_work.thermal_level_cs0);
}
static ssize_t mif_show_templvl_ch0_1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch0_work.thermal_level_cs1);
}
static ssize_t mif_show_templvl_ch1_0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch1_work.thermal_level_cs0);
}
static ssize_t mif_show_templvl_ch1_1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch1_work.thermal_level_cs1);
}

#if defined(CONFIG_ARM_EXYNOS_BUS_DEVFREQ_SYSFS_MAX_TEMP)
static ssize_t mif_show_max_temp_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int thermal_level;

	thermal_level = max(
			max(devfreq_mif_ch0_work.thermal_level_cs0, devfreq_mif_ch0_work.thermal_level_cs1),
			max(devfreq_mif_ch1_work.thermal_level_cs0, devfreq_mif_ch1_work.thermal_level_cs1));

	return snprintf(buf, PAGE_SIZE, "%u\n", thermal_level);
}
#endif

static DEVICE_ATTR(mif_templvl_ch0_0, 0644, mif_show_templvl_ch0_0, NULL);
static DEVICE_ATTR(mif_templvl_ch0_1, 0644, mif_show_templvl_ch0_1, NULL);
static DEVICE_ATTR(mif_templvl_ch1_0, 0644, mif_show_templvl_ch1_0, NULL);
static DEVICE_ATTR(mif_templvl_ch1_1, 0644, mif_show_templvl_ch1_1, NULL);
#if defined(CONFIG_ARM_EXYNOS_BUS_DEVFREQ_SYSFS_MAX_TEMP)
static DEVICE_ATTR(mif_max_temp_level, 0644, mif_show_max_temp_level, NULL);
#endif

static struct attribute *devfreq_mif_sysfs_entries[] = {
	&dev_attr_mif_templvl_ch0_0.attr,
	&dev_attr_mif_templvl_ch0_1.attr,
	&dev_attr_mif_templvl_ch1_0.attr,
	&dev_attr_mif_templvl_ch1_1.attr,
#if defined(CONFIG_ARM_EXYNOS_BUS_DEVFREQ_SYSFS_MAX_TEMP)
	&dev_attr_mif_max_temp_level.attr,
#endif
	NULL,
};

static struct attribute_group devfreq_mif_attr_group = {
	.name   = "temp_level",
	.attrs  = devfreq_mif_sysfs_entries,
};

static BLOCKING_NOTIFIER_HEAD(mif_thermal_level_notifier);

int exynos5_mif_thermal_add_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&mif_thermal_level_notifier, n);
}

void exynos5_mif_thermal_call_notifier(int val, enum devfreq_mif_thermal_channel ch)
{
	blocking_notifier_call_chain(&mif_thermal_level_notifier, val, &ch);
}

static void exynos5_devfreq_swtrip(void)
{
#ifdef CONFIG_EXYNOS_SWTRIP
	char tmustate_string[20];
	char *envp[2];

	snprintf(tmustate_string, sizeof(tmustate_string), "TMUSTATE=%d", 3);
	envp[0] = tmustate_string;
	envp[1] = NULL;
	pr_err("DEVFREQ(MIF) : SW trip by MR4\n");
	kobject_uevent_env(&data_mif->dev->kobj, KOBJ_CHANGE, envp);
#endif
}

static void exynos5_devfreq_thermal_monitor(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
			container_of(d_work, struct devfreq_thermal_work, devfreq_mif_thermal_work);
	unsigned int mrstatus, tmp_thermal_level, max_thermal_level = 0, tmp;
	unsigned int timingaref_value = RATE_HALF;
	unsigned long max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;
	bool throttling = false;
	void __iomem *base_drex = NULL;

	if (thermal_work->channel == THERMAL_CHANNEL0) {
		base_drex = data_mif->base_drex0;
	} else if (thermal_work->channel == THERMAL_CHANNEL1) {
		base_drex = data_mif->base_drex1;
	}

	mutex_lock(&data_mif->lock);

	__raw_writel(0x09001000, base_drex + 0x10);
	mrstatus = __raw_readl(base_drex + 0x54);
	tmp_thermal_level = (mrstatus & MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_work->thermal_level_cs0 = tmp_thermal_level;

	__raw_writel(0x09101000, base_drex + 0x10);
	mrstatus = __raw_readl(base_drex + 0x54);
	tmp_thermal_level = (mrstatus & MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_work->thermal_level_cs1 = tmp_thermal_level;

	mutex_unlock(&data_mif->lock);

	exynos5_mif_thermal_call_notifier(thermal_work->thermal_level_cs1, thermal_work->channel);

	switch (max_thermal_level) {
	case 0:
	case 1:
	case 2:
	case 3:
		timingaref_value = RATE_HALF;
		thermal_work->polling_period = 1000;
		break;
	case 4:
		timingaref_value = RATE_HALF;
		thermal_work->polling_period = 300;
		break;
	case 5:
		throttling = true;
		timingaref_value = RATE_QUARTER;
		thermal_work->polling_period = 100;
		break;
	case 6:
		exynos5_devfreq_swtrip();
		return;
	default:
		pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");
		return;
	}

	if (throttling)
#if defined(CONFIG_EXYNOS5430_HD)
		max_freq = devfreq_mif_opp_list[LV4].freq;
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
		max_freq = devfreq_mif_opp_list[LV5].freq;
#endif
	else
		max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;

	if (thermal_work->max_freq != max_freq) {
		thermal_work->max_freq = max_freq;
		mutex_lock(&data_mif->devfreq->lock);
		update_devfreq(data_mif->devfreq);
		mutex_unlock(&data_mif->devfreq->lock);
	}

	if (max_freq != exynos5_devfreq_mif_governor_data.cal_qos_max) {
		tmp = __raw_readl(base_drex + 0x4);
		tmp &= ~(0x1 << 27);
		__raw_writel(tmp, base_drex + 0x4);
	}

	__raw_writel(timingaref_value, base_drex + 0x30);

	if (max_freq == exynos5_devfreq_mif_governor_data.cal_qos_max) {
		tmp = __raw_readl(base_drex + 0x4);
		tmp |= (0x1 << 27);
		__raw_writel(tmp, base_drex + 0x4);
	}

	exynos5_devfreq_thermal_event(thermal_work);
}

static void exynos5_devfreq_init_thermal(void)
{
	devfreq_mif_thermal_wq_ch0 = create_freezable_workqueue("devfreq_thermal_wq_ch0");
	devfreq_mif_thermal_wq_ch1 = create_freezable_workqueue("devfreq_thermal_wq_ch1");

	INIT_DELAYED_WORK(&devfreq_mif_ch0_work.devfreq_mif_thermal_work,
			exynos5_devfreq_thermal_monitor);
	INIT_DELAYED_WORK(&devfreq_mif_ch1_work.devfreq_mif_thermal_work,
			exynos5_devfreq_thermal_monitor);

	devfreq_mif_ch0_work.work_queue = devfreq_mif_thermal_wq_ch0;
	devfreq_mif_ch1_work.work_queue = devfreq_mif_thermal_wq_ch1;

	exynos5_devfreq_thermal_event(&devfreq_mif_ch0_work);
	exynos5_devfreq_thermal_event(&devfreq_mif_ch1_work);
}

static int exynos5_devfreq_mif_probe(struct platform_device *pdev)
{
	int ret = 0;
	int index;
	struct devfreq_data_mif *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;
	struct opp *target_opp;
	unsigned long freq;

	if (exynos5_devfreq_mif_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

	data = kzalloc(sizeof(struct devfreq_data_mif), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	data->use_dvfs = false;
	data_mif = data;
	mutex_init(&data->lock);

	if (exynos5_devfreq_mif_init_parameter(data)) {
		ret = -EINVAL;
		goto err_data;
	}

	data->dll_status = ((__raw_readl(data->base_lpddr_phy0 + 0xB0) & (0x1 << 5)) != 0);
	data->dll_gated = !data->dll_status;
	pr_info("DEVFREQ(MIF) : default dll satus : %s\n", (data->dll_status ? "on" : "off"));

	exynos5_devfreq_mif_profile.freq_table = kzalloc(sizeof(int) * LV_COUNT, GFP_KERNEL);
	if (exynos5_devfreq_mif_profile.freq_table == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos5_init_mif_table(&pdev->dev, data);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);

	devfreq_mif_ch0_work.max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;
	devfreq_mif_ch1_work.max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;

	data->volt_offset = 0;
	data->dev = &pdev->dev;
	data->vdd_mif = regulator_get(NULL, "vdd_mif");
	if (IS_ERR(data->vdd_mif)) {
		dev_err(data->dev, "DEVFREQ(INT) : failed to get regulator\n");
		goto err_regulator;
	}

	rcu_read_lock();
	freq = exynos5_devfreq_mif_governor_data.cal_qos_max;
	target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(data->dev, "DEVFREQ(MIF) : Invalid OPP to set voltagen");
		ret = PTR_ERR(target_opp);
		goto err_opp;
	}
	data->old_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	data->old_volt = get_limit_voltage(data->old_volt, data->volt_offset);
#endif
	rcu_read_unlock();
	regulator_set_voltage(data->vdd_mif, data->old_volt, data->old_volt + VOLT_STEP);
	exynos5_devfreq_set_dll_lock_value(data, data->old_volt);

	index = exynos5_devfreq_mif_get_idx(devfreq_mif_opp_list,
						ARRAY_SIZE(devfreq_mif_opp_list),
						freq);
	if (index < 0) {
		pr_err("DEVFREQ(MIF) : Failed to find index abb\n");
		ret = -EINVAL;
		goto err_opp;
	}
	set_match_abb(ID_MIF, devfreq_mif_asv_abb[index]);

	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_mif_profile,
						"simple_exynos",
						&exynos5_devfreq_mif_governor_data);

	exynos5_devfreq_init_thermal();

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	devfreq_nb->df = data->devfreq;
	devfreq_nb->nb.notifier_call = exynos5_devfreq_mif_notifier;

	exynos5430_devfreq_register(&devfreq_mif_exynos);
	exynos5430_ppmu_register_notifier(MIF, &devfreq_nb->nb);

	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos5_mif_reboot_notifier);

	ret = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_mif_attr_group);

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_devfreq_mif_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_opp:
	regulator_put(data->vdd_mif);
err_regulator:
	mutex_destroy(&data->lock);
err_inittable:
	kfree(exynos5_devfreq_mif_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_mif_remove(struct platform_device *pdev)
{
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);

	iounmap(data->base_mif);
	iounmap(data->base_sysreg_mif);
	iounmap(data->base_drex0);
	iounmap(data->base_drex1);
	iounmap(data->base_lpddr_phy0);
	iounmap(data->base_lpddr_phy1);

	flush_workqueue(devfreq_mif_thermal_wq_ch0);
	destroy_workqueue(devfreq_mif_thermal_wq_ch0);
	flush_workqueue(devfreq_mif_thermal_wq_ch1);
	destroy_workqueue(devfreq_mif_thermal_wq_ch1);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_mif_thermal_qos);
	pm_qos_remove_request(&exynos5_mif_qos);
	pm_qos_remove_request(&boot_mif_qos);
	pm_qos_remove_request(&exynos5_mif_bts_qos);

	regulator_put(data->vdd_mif);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_mif_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_mif_qos))
		pm_qos_update_request(&exynos5_mif_qos, exynos5_devfreq_mif_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_mif_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	data_mif->dll_status = ((__raw_readl(data_mif->base_lpddr_phy0 + 0xB0) & (0x1 << 5)) != 0);
	pr_info("DEVFREQ(MIF) : default dll satus : %s\n", (data_mif->dll_status ? "on" : "off"));

	exynos5_devfreq_mif_update_timingset(data_mif);

	if (pm_qos_request_active(&exynos5_mif_qos))
		pm_qos_update_request(&exynos5_mif_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_mif_pm = {
	.suspend	= exynos5_devfreq_mif_suspend,
	.resume		= exynos5_devfreq_mif_resume,
};

static struct platform_driver exynos5_devfreq_mif_driver = {
	.probe	= exynos5_devfreq_mif_probe,
	.remove	= exynos5_devfreq_mif_remove,
	.driver	= {
		.name	= "exynos5-devfreq-mif",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_mif_pm,
	},
};

static struct platform_device exynos5_devfreq_mif_device = {
	.name	= "exynos5-devfreq-mif",
	.id	= -1,
};

static int exynos5_devfreq_mif_qos_init(void)
{
	pm_qos_add_request(&exynos5_mif_qos, PM_QOS_BUS_THROUGHPUT, exynos5430_qos_mif.default_qos);
	pm_qos_add_request(&min_mif_thermal_qos, PM_QOS_BUS_THROUGHPUT, exynos5430_qos_mif.default_qos);
	pm_qos_add_request(&boot_mif_qos, PM_QOS_BUS_THROUGHPUT, exynos5430_qos_mif.default_qos);
	pm_qos_add_request(&exynos5_mif_bts_qos, PM_QOS_BUS_THROUGHPUT, exynos5430_qos_mif.default_qos);
	pm_qos_update_request_timeout(&boot_mif_qos,
					exynos5_devfreq_mif_profile.initial_freq, 40000 * 1000);

	return 0;
}
device_initcall(exynos5_devfreq_mif_qos_init);

static int __init exynos5_devfreq_mif_init(void)
{
	int ret;

	timeout_table = timeout_fullhd;
	media_enabled_fimc_lite = false;
	media_enabled_gscl_local = false;
	media_enabled_tv = false;
	media_num_mixer_layer = false;
	media_num_decon_layer = false;
	wqhd_tv_window5 = false;
	dmc_timing_parameter = NULL;

	exynos5_devfreq_mif_device.dev.platform_data = &exynos5430_qos_mif;

	ret = platform_device_register(&exynos5_devfreq_mif_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_mif_driver);
}
late_initcall(exynos5_devfreq_mif_init);

static void __exit exynos5_devfreq_mif_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_mif_driver);
	platform_device_unregister(&exynos5_devfreq_mif_device);
}
module_exit(exynos5_devfreq_mif_exit);
