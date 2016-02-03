/* linux/arch/arm64/mach-exynos/include/mach/asv-exynos5433_cal.c
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* EXYNOS5433 - Adoptive Support Voltage Header file
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <mach/asv-exynos_cal.h>
#include <mach/asv-exynos5433.h>

static volatile u64 *abb_base_arr[6] = {
	[SYSC_DVFS_BIG] = CHIPID_ABBG_BASE + 0x0780,
	[SYSC_DVFS_LIT] = CHIPID_ABBG_BASE + 0x0784,
	[SYSC_DVFS_MIF] = CHIPID_ABBG_BASE + 0x078C,
	[SYSC_DVFS_INT] = CHIPID_ABBG_BASE + 0x0788,
	[SYSC_DVFS_G3D] = CHIPID_ABBG_BASE + 0x0790,
	[SYSC_DVFS_CAM] = CHIPID_ABBG_BASE + 0x0788,
};

static const u64 base_add_arr[SYSC_DVFS_NUM][5] = {
	[SYSC_DVFS_BIG] = { (const u64) CHIPID_ASV_TBL_BASE + 0x0000, 0, 4, 8, 12},
	[SYSC_DVFS_LIT] = { (const u64) CHIPID_ASV_TBL_BASE + 0x0000, 16, 20, 24, 28},
	[SYSC_DVFS_G3D] = { (const u64) CHIPID_ASV_TBL_BASE + 0x0004, 0, 4, 8, 12},
	[SYSC_DVFS_MIF] = { (const u64) CHIPID_ASV_TBL_BASE + 0x0004, 16, 20, 24, 28},
	[SYSC_DVFS_INT] = { (const u64) CHIPID_ASV_TBL_BASE + 0x0008, 0, 4, 8, 12},
	[SYSC_DVFS_CAM] = { (const u64) CHIPID_ASV_TBL_BASE + 0x0008, 16, 20, 24, 28},

};

static volatile u32 big_ids;
static volatile bool use_dynimic_abb[SYSC_DVFS_NUM];
static volatile u32 dynimic_ema_big1;
static volatile u32 dynimic_ema_big2;
static volatile u32 pop_type;
static volatile u32 group_fused;

#ifdef CONFIG_SOC_EXYNOS5433
u32 re_err(void)
{
	pr_err("ASV: CAL is working wrong. \n");
	return 0;
}
#endif

u32 CHIPID_Power(u32 input, u32 Power)
{
	int i;
	u32 result;

	if (Power == 0)
		return 1;

	result = input;

	for (i = 1; i < Power; i++) {
		result *= input;
	}

	return result;
}

u32 CHIPID_IsFused_KEY3_IdsATLAS(void)
{
	u32 ErrorCnt;
	u32 fused_data = 0;
	u8 Data[32] = {0,};
	u32 ulSize = 32;
	u32 i;

	if (otp_read((BANK_OFFSET * 3), Data, ulSize, &ErrorCnt)) {
		for (i = 0; i < 16; i = i + 2) {
			if (Data[i] != 0)
				fused_data += Data[i] * CHIPID_Power(2, i / 2);
		}
	}

	if (fused_data == 0)
		return 0;

	return fused_data;
}

bool cal_is_fused_speed_grp(void)
{
	return group_fused;
}

u32 cal_get_dram_size(void)
{
	return pop_type ? 2 : 3;
}

u32 cal_get_table_ver(void)
{
	u32 ver = __raw_readl(CHIPID_ASV_TBL_BASE + 0x000C) & 0xF;

	return ver;
}

u32 cal_get_fs_abb(void)
{
	u32 master = GetBits(CHIPID_ASV_TBL_BASE + 0x0010, 20, 0x1);

	return master;
}

u32 cal_get_ids(void)
{
	u32 fused_data;
	fused_data = CHIPID_IsFused_KEY3_IdsATLAS();

	if (fused_data != 0)
		return fused_data;
	else if(GetBits(CHIPID_ASV_TBL_BASE + 0x003C, 28, 0xF) == 0)
		return __raw_readl(CHIPID_ABB_TBL_BASE) & 0xff;
	else
		return __raw_readl(CHIPID_ASV_TBL_BASE + 0x0030) & 0xff;
}

void cal_init(void)
{
	big_ids = cal_get_ids();
	dynimic_ema_big1 = GetBits(CHIPID_ASV_TBL_BASE + 0x000C, 24, 0x7);
	dynimic_ema_big2 = GetBits(CHIPID_ASV_TBL_BASE + 0x000C, 28, 0x7);
	pop_type = GetBits(PKG_ID, 4, 3);
	group_fused = GetBits(CHIPID_ASV_TBL_BASE+0x000C, 16, 0x1);

	use_dynimic_abb[SYSC_DVFS_BIG] = true;
	use_dynimic_abb[SYSC_DVFS_LIT] = true;
	use_dynimic_abb[SYSC_DVFS_G3D] = true;
	use_dynimic_abb[SYSC_DVFS_INT] = true;
	use_dynimic_abb[SYSC_DVFS_MIF] = true;
	use_dynimic_abb[SYSC_DVFS_CAM] = false;

	otp_tmu_read();
	otp_tmu_print_info();
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
	u32 i;
	u32 group = 0;

	if (big_ids >= ids_table_v0[MAX_ASV_GROUP-1])
		group = MAX_ASV_GROUP - 1;
	else if (big_ids <= ids_table_v0[0])
		group = 0;
	else {
		for (i = MAX_ASV_GROUP; i > 0; i--) {
			if (big_ids > ids_table_v0[i-1]) {
				group = i;
				break;
			}
		}
	}

	if (cal_get_table_ver() == 0)
		group = 0;	/* Temporary Code for Margin Issue */
	else if (cal_get_table_ver() == 1) {
		if((__raw_readl(CHIPID_ASV_TBL_BASE)&0x1)==1) {
			if(group < 4)    /*Group < 4 -> Group 1 */
				group = 1;
			else
				group -= 3; /* Group shift -3 for margin */
		}
	}

	return group;
}

u32 cal_get_match_subgrp(u32 id, s32 level)
{
	u32 subgrp = 0;	/*  version 0 */

	switch (id) {
	case SYSC_DVFS_BIG:
		subgrp = (level <= SYSC_DVFS_L8) ? 0 :
			(level <= SYSC_DVFS_L13) ? 1 : 2;
		break;
	case SYSC_DVFS_LIT:
		subgrp = (level <= SYSC_DVFS_L8) ? 0 :
			(level <= SYSC_DVFS_L13) ? 1 : 2;
		break;
	case SYSC_DVFS_G3D:
		subgrp = (level <= SYSC_DVFS_L5) ? 0 :
			(level <= SYSC_DVFS_L7) ? 1 : 2;
		break;
	case SYSC_DVFS_MIF:
		subgrp = (level <= SYSC_DVFS_L0) ? 0 :
			(level <= SYSC_DVFS_L2) ? 1 : 2;
		break;
	case SYSC_DVFS_INT:
		subgrp = (level <= SYSC_DVFS_L0) ? 0 : /*  L0_A, L0 */
			(level <= SYSC_DVFS_L3) ? 1 : 2;
		break;
	case SYSC_DVFS_CAM:
		subgrp = (level <= SYSC_DVFS_L3) ? 0 :
			(level <= SYSC_DVFS_L4) ? 1 : 2;
		break;
	default:
		Assert(0);
	}

	return subgrp;
}


u32 cal_get_lock_volt(u32 id)
{
	u32 volt, lockvalue;
	lockvalue = GetBits(base_add_arr[id][0], base_add_arr[id][4], 0xf);

	if (lockvalue == 0)
		volt = 0;
	else if (id == SYSC_DVFS_BIG)
		volt = 85000 + lockvalue * 25000;
	else
		volt = 70000 + lockvalue * 25000;

	return volt;
}

u32 cal_get_asv_grp(u32 id, s32 level)
{
	u32 subgrp, asv_group = 0;
	if (cal_is_fused_speed_grp())	{
		subgrp = cal_get_match_subgrp(id, level);
		asv_group = GetBits(base_add_arr[id][0], base_add_arr[id][1 + subgrp], 0xf);
	} else {
		asv_group = cal_get_ids_hpm_group();
	}

	Assert(asv_group < MAX_ASV_GROUP);

	return asv_group;
}

u32 cal_get_volt(u32 id, s32 level)
{
	u32 volt, lock_volt;
	u32 asvgrp;
	u32 minlvl = cal_get_min_lv(id);
	const u32 *p_table = NULL;
	u32 idx;
	Assert(level >= 0);

	if (level >= minlvl)
		level = minlvl;
	idx = level;

	if (cal_get_table_ver() == 0) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_v0[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_v0[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_v0[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_v0[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_v0[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_v0[idx] :
				NULL);
		Assert(p_table != NULL);
	} else if (cal_get_table_ver() <= 4) {	/* table ver 1, 2, 3, 4 */
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_v1[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_v1[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_v1[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_v1[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_v1[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_v1[idx] :
				NULL);
		Assert(p_table != NULL);
	} else if (cal_get_table_ver() == 5) {	/* table ver 5 */
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_v2[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_v2[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_v2[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_v2[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_v2[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_v2[idx] :
				NULL);
		Assert(p_table != NULL);
	} else if (cal_get_table_ver() <= 8) {	/* table ver 6, 7, 8 */
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_v3[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_v3[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_v3[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_v3[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_v3[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_v3[idx] :
				NULL);
		Assert(p_table != NULL);
	}

	if (p_table == NULL)
		return 0;

	asvgrp = cal_get_asv_grp(id, level);
	volt = p_table[asvgrp + 1];
	lock_volt = cal_get_lock_volt(id);

	if (lock_volt > volt)
		volt = lock_volt;

	return volt;
}

u32 cal_get_freq(u32 id, s32 level)
{
	u32 freq = 0;
	u32 idx;
	u32 minlvl = cal_get_min_lv(id);

	if (level >= minlvl)
		idx = 0;

	idx = level;

	if (cal_get_table_ver() == 0) {
		freq = ((id == SYSC_DVFS_BIG) ? volt_table_big_v0[idx][0] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_v0[idx][0] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_v0[idx][0] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_v0[idx][0] :
				(id == SYSC_DVFS_INT) ? volt_table_int_v0[idx][0] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_v0[idx][0] :
				0);
	} else if (cal_get_table_ver() <= 4) {	/* table ver 1, 2, 3, 4 */
		freq = ((id == SYSC_DVFS_BIG) ? volt_table_big_v1[idx][0] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_v1[idx][0] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_v1[idx][0] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_v1[idx][0] :
				(id == SYSC_DVFS_INT) ? volt_table_int_v1[idx][0] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_v1[idx][0] :
				0);
	} else if (cal_get_table_ver() == 5) {	/* table ver 5 */
		freq = ((id == SYSC_DVFS_BIG) ? volt_table_big_v2[idx][0] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_v2[idx][0] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_v2[idx][0] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_v2[idx][0] :
				(id == SYSC_DVFS_INT) ? volt_table_int_v2[idx][0] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_v2[idx][0] :
				0);
	} else if (cal_get_table_ver() <= 8) { /* table ver 6, 7, 8 */
		freq = ((id == SYSC_DVFS_BIG) ? volt_table_big_v3[idx][0] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_v3[idx][0] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_v3[idx][0] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_v3[idx][0] :
				(id == SYSC_DVFS_INT) ? volt_table_int_v3[idx][0] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_v3[idx][0] :
				0);
	}

	return freq;
}

u32 cal_get_abb(u32 id, s32 level)
{
	u32 match_abb;
	u32 asv_grp;
	u32 min_lvl = cal_get_min_lv(id);
	u32 is_fs_abb = cal_get_fs_abb();
	u32 fs_abb;
	const u32 *p_table = NULL;
	u32 idx;

	Assert(level >= 0);

	if (level >= min_lvl)
		level = min_lvl;

	idx = level;

	if (cal_get_table_ver() == 0) {
		p_table = ((id == SYSC_DVFS_BIG) ? abb_table_big_v0[idx] :
				(id == SYSC_DVFS_LIT) ? abb_table_lit_v0[idx] :
				(id == SYSC_DVFS_G3D) ? abb_table_g3d_v0[idx] :
				(id == SYSC_DVFS_MIF) ? abb_table_mif_v0[idx] :
				(id == SYSC_DVFS_INT) ? abb_table_int_v0[idx] :
				NULL);
	} else if (cal_get_table_ver() <= 4) {
		p_table = ((id == SYSC_DVFS_BIG) ? abb_table_big_v1[idx] :
				(id == SYSC_DVFS_LIT) ? abb_table_lit_v1[idx] :
				(id == SYSC_DVFS_G3D) ? abb_table_g3d_v1[idx] :
				(id == SYSC_DVFS_MIF) ? abb_table_mif_v1[idx] :
				(id == SYSC_DVFS_INT) ? abb_table_int_v1[idx] :
				NULL);
	} else if (cal_get_table_ver() == 5) {
		p_table = ((id == SYSC_DVFS_BIG) ? abb_table_big_v2[idx] :
				(id == SYSC_DVFS_LIT) ? abb_table_lit_v2[idx] :
				(id == SYSC_DVFS_G3D) ? abb_table_g3d_v2[idx] :
				(id == SYSC_DVFS_MIF) ? abb_table_mif_v2[idx] :
				(id == SYSC_DVFS_INT) ? abb_table_int_v2[idx] :
				NULL);
	} else if (cal_get_table_ver() <= 8) {
		p_table = ((id == SYSC_DVFS_BIG) ? abb_table_big_v3[idx] :
				(id == SYSC_DVFS_LIT) ? abb_table_lit_v3[idx] :
				(id == SYSC_DVFS_G3D) ? abb_table_g3d_v3[idx] :
				(id == SYSC_DVFS_MIF) ? abb_table_mif_v3[idx] :
				(id == SYSC_DVFS_INT) ? abb_table_int_v3[idx] :
				NULL);
	}

	Assert(p_table != NULL);
	if (p_table == NULL)
		return 0;

	if (is_fs_abb) {
		if (id == SYSC_DVFS_BIG)
			fs_abb = GetBits(CHIPID_ASV_TBL_BASE + 0x0010, 0, 0xF);
		else if (id == SYSC_DVFS_LIT)
			fs_abb = GetBits(CHIPID_ASV_TBL_BASE + 0x0010, 4, 0xF);
		else if (id == SYSC_DVFS_G3D)
			fs_abb = GetBits(CHIPID_ASV_TBL_BASE + 0x0010, 8, 0xF);
		else if (id == SYSC_DVFS_MIF)
			fs_abb = GetBits(CHIPID_ASV_TBL_BASE + 0x0010, 12, 0xF);
		else if (id == SYSC_DVFS_INT)
			fs_abb = GetBits(CHIPID_ASV_TBL_BASE + 0x0010, 16, 0xF);
		else
			fs_abb = ABB_BYPASS;

		match_abb = fs_abb;
	} else {
		asv_grp = cal_get_asv_grp(id, level);
		match_abb = p_table[asv_grp + 1];
	}

	return match_abb;
}

bool cal_use_dynimic_abb(u32 id)
{
	return use_dynimic_abb[id];
}

void cal_set_abb(u32 id, u32 abb)
{
	u32 bits;
	Assert(id < SYSC_DVFS_NUM);

	if (abb == ABB_BYPASS)	/* bypass */
		bits = 0;
	else
		bits = (1 << 31) | (1 << 7) | (abb & 0x1f);

	__raw_writel(bits, abb_base_arr[id]);
}

bool cal_use_dynimic_ema(u32 id)
{
	if (id == SYSC_DVFS_BIG)
		return dynimic_ema_big1;
	else
		return false;
}

void cal_set_ema(u32 id, u32 setvolt)
{
	if (id == SYSC_DVFS_BIG && dynimic_ema_big1) {
		u32 value = (setvolt <= 900000) ? dynimic_ema_big2 : dynimic_ema_big1;
		u32 tmp;
		if (value == 0) value = 2;
		tmp = __raw_readl(SYSREG_BIG_BASE + 0x330);
		tmp &= ~(7 << 20 | 7 << 8);
		tmp |= (value << 20 | value << 8);
		__raw_writel(tmp, SYSREG_BIG_BASE + 0x330);
	}
}

u32 cal_get_hpm(void)
{
	return 0;
}

u32 is_max_limit_sample(void)
{
	if (__raw_readl(S5P_VA_CHIPID2 + 0x3C) & 0x00200000)
		return 1;

	return 0;
}
