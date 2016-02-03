/* linux/arch/arm64/mach-exynos/include/mach/asv-exynos7580_cal.c
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* EXYNOS7580 - Adoptive Support Voltage Header file
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <mach/asv-exynos_cal.h>
#include <mach/asv-exynos7580.h>

#define CHIPID_BASE		S5P_VA_CHIPID
#define CHIPID_ASV_TBL_BASE	S5P_VA_CHIPID2
#define CHIPID_ABB_TBL_BASE	S5P_VA_CHIPID3

#ifdef CONFIG_SOC_EXYNOS7580
u32 re_err(void)
{
	pr_err("ASV: CAL is working wrong. \n");
	return 0;
}
#endif

static volatile bool use_dynimic_abb[SYSC_DVFS_NUM];
static volatile u32 group_fused;

bool cal_is_fused_speed_grp(void)
{
	return false;
}

u32 cal_get_dram_size(void)
{
	return 0;
}

u32 cal_get_table_ver(void)
{
	return 0;
}

u32 cal_get_ids(void)
{
	return 0;
}

u32 cal_get_hpm(void)
{
	return 0;
}

void cal_init(void)
{
}

u32 cal_get_max_volt(u32 id)
{
	s32 volt = 0;
	switch (id) {
	case SYSC_DVFS_BIG:
		volt = BIG_MAX_VOLT;
		break;
	case SYSC_DVFS_LIT:
		volt = LIT_MAX_VOLT;
		break;
	case SYSC_DVFS_G3D:
		volt = G3D_MAX_VOLT;
		break;
	case SYSC_DVFS_MIF:
		volt = MIF_MAX_VOLT;
		break;
	case SYSC_DVFS_INT:
		volt = INT_MAX_VOLT;
		break;
	case SYSC_DVFS_CAM:
		volt = ISP_MAX_VOLT;
		break;
	default:
		Assert(0);
	}
	return volt;
}

s32 cal_get_min_lv(u32 id)
{
	s32 level = 0;
	switch (id) {
	case SYSC_DVFS_BIG:
		level = SYSC_DVFS_END_LVL_BIG;
		break;
	case SYSC_DVFS_LIT:
		level = SYSC_DVFS_END_LVL_LIT;
		break;
	case SYSC_DVFS_G3D:
		level = SYSC_DVFS_END_LVL_G3D;
		break;
	case SYSC_DVFS_MIF:
		level = SYSC_DVFS_END_LVL_MIF;
		break;
	case SYSC_DVFS_INT:
		level = SYSC_DVFS_END_LVL_INT;
		break;
	case SYSC_DVFS_CAM:
		level = SYSC_DVFS_END_LVL_CAM;
		break;
	default:
		Assert(0);
	}
	return level;
}

u32 cal_get_ids_hpm_group(void)
{
	return 0;
}

u32 cal_get_match_subgrp(u32 id, s32 level)
{
	return 0;
}


u32 cal_get_lock_volt(u32 id)
{
	return 0;
}

u32 cal_get_asv_grp(u32 id, s32 level)
{
	u32 asv_group, uBits, i;
	uBits = (__raw_readl(CHIPID_BASE + 0x0004) >> 16) & 0xFF;
	for(i = 0; i < sizeof(ids_table) / sizeof(u32); i++)
	{
		if(ids_table[i] >= uBits)
			break;
	}
	asv_group = i;

	return asv_group;
}

u32 cal_get_volt(u32 id, s32 level)
{
	u32 volt;
	u32 asvgrp;
	u32 minlvl = cal_get_min_lv(id);
	const u32 *p_table;
	u32 idx;
	u32 table_ver;

	if (level >= minlvl)
		level = minlvl;

	idx = level;

	table_ver = cal_get_table_ver();
	if (table_ver == 0) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_v0[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_v0[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_v0[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_v0[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_v0[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_v0[idx] :
				NULL);
	} else {
		/* when unknown table number come in, return max voltage */
		return cal_get_max_volt(id);
	}

	if (p_table == NULL) {
		pr_info("%s : voltae table pointer is NULL\n", __func__);
		return 0;
	}

	asvgrp = cal_get_asv_grp(id, level);
	volt = p_table[asvgrp + 1];

	return volt;
}

u32 cal_get_freq(u32 id, s32 level)
{
	u32 freq = 0;
	switch (id) {
	case SYSC_DVFS_BIG:
		freq = volt_table_big_v0[level][0];
		break;
	case SYSC_DVFS_LIT:
		freq = volt_table_lit_v0[level][0];
		break;
	case SYSC_DVFS_G3D:
		freq = volt_table_g3d_v0[level][0];
		break;
	case SYSC_DVFS_MIF:
		freq = volt_table_mif_v0[level][0];
		break;
	case SYSC_DVFS_INT:
		freq = volt_table_int_v0[level][0];
		break;
	case SYSC_DVFS_CAM:
		freq = volt_table_cam_v0[level][0];
		break;
	default:
		freq = 0;
	}
	return freq;
}

bool cal_use_dynimic_abb(u32 id)
{
	return 0;
}

bool cal_use_dynimic_ema(u32 id)
{
	return false;
}

void cal_set_ema(u32 id, u32 setvolt)
{
}

u32 cal_get_abb(u32 id, s32 level)
{
	return 0;
}

void cal_set_abb(u32 id, u32 abb)
{
}
