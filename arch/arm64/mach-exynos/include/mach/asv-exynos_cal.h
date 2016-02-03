/*
 * linux/arch/arm64/mach-exynos/include/mach/asv-exynos_cal.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - support ASV drvier to interact with cal.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_NEW_EXYNOS_ASV_CAL_H
#define __ASM_ARCH_NEW_EXYNOS_ASV_CAL_H __FILE__

/* Use by ASV drvier */
#include <linux/io.h>
#include<mach/asv-exynos.h>

u32 re_err(void);

#define Assert(b) (!(b) ? re_err() : 0)

#define SetBits(uAddr, uBaseBit, uMaskValue, uSetValue) \
	do { \
		__raw_writel((__raw_readl((const volatile void __iomem *)uAddr) & \
		 ~((uMaskValue) << (uBaseBit))) | \
		 (((uMaskValue) & (uSetValue)) << (uBaseBit)), uAddr) \
	} while (0);

#define GetBits(uAddr, uBaseBit, uMaskValue) \
	((__raw_readl((const volatile void __iomem *) uAddr) >> (uBaseBit)) & (uMaskValue))

/* COMMON code */
#define ENABLE		(1)
#define DISABLE		(0)

enum SYSC_DVFS_SEL {
	SYSC_DVFS_BIG,
	SYSC_DVFS_LIT,
	SYSC_DVFS_INT,
	SYSC_DVFS_MIF,
	SYSC_DVFS_G3D,
	SYSC_DVFS_CAM,
	SYSC_DVFS_NUM
};

enum SYSC_DVFS_LVL {
	SYSC_DVFS_L0 = 0,
	SYSC_DVFS_L1,
	SYSC_DVFS_L2,
	SYSC_DVFS_L3,
	SYSC_DVFS_L4,
	SYSC_DVFS_L5,
	SYSC_DVFS_L6,
	SYSC_DVFS_L7,
	SYSC_DVFS_L8,
	SYSC_DVFS_L9,
	SYSC_DVFS_L10,
	SYSC_DVFS_L11,
	SYSC_DVFS_L12,
	SYSC_DVFS_L13,
	SYSC_DVFS_L14,
	SYSC_DVFS_L15,
	SYSC_DVFS_L16,
	SYSC_DVFS_L17,
	SYSC_DVFS_L18,
	SYSC_DVFS_L19,
	SYSC_DVFS_L20,
	SYSC_DVFS_L21,
	SYSC_DVFS_L22,
	SYSC_DVFS_L23,
	SYSC_DVFS_L24,
};


enum SYSC_ASV_GROUP {
	SYSC_ASV_0 = 0,
	SYSC_ASV_1,
	SYSC_ASV_2,
	SYSC_ASV_3,
	SYSC_ASV_4,
	SYSC_ASV_5 = 5,
	SYSC_ASV_6,
	SYSC_ASV_7,
	SYSC_ASV_8,
	SYSC_ASV_9,
	SYSC_ASV_10,
	SYSC_ASV_11 = 11,
	SYSC_ASV_12 = 12,
	SYSC_ASV_13,
	SYSC_ASV_14,
	SYSC_ASV_15 = 15,
	SYSC_ASV_MAX /* ASV limit */
};

void cal_init(void);
u32 cal_get_max_volt(u32 id);
s32 cal_get_min_lv(u32 id);
u32 cal_get_volt(u32 id, s32 level);
u32 cal_get_freq(u32 id, s32 level);

u32 cal_get_abb(u32 id, s32 level);
bool cal_use_dynimic_abb(u32 id);
void cal_set_abb(u32 id, u32 abb);

void cal_set_ema(u32 id , u32 setvolt);
bool cal_use_dynimic_ema(u32 id);
u32 cal_get_dram_size(void);

u32 cal_get_match_subgrp(u32 id, s32 level);
u32 cal_get_ids_hpm_group(void);
u32 cal_get_ids(void);
u32 cal_get_hpm(void);

u32 cal_get_asv_grp(u32 id, s32 level);
bool cal_is_fused_speed_grp(void);

u32 cal_get_table_ver(void);
u32 is_max_limit_sample(void);

#ifdef CONFIG_SOC_EXYNOS7420
u32 cal_get_asv_info(int id);
#else
extern inline u32 cal_get_asv_info(int id){return 0;};
#endif

#ifdef CONFIG_SOC_EXYNOS5433
u32 cal_get_fs_abb(void);
#endif

#ifdef CONFIG_EXYNOS_ASV_SUPPORT_RCC
u32 cal_get_rcc(u32 id, s32 level);
u32 cal_set_rcc(u32 id, s32 level, u32 rcc);
void cal_set_rcc_limit(void);
#endif

#endif
