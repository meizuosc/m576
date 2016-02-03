/* linux/arch/arm64/mach-exynos/asv-exynos_cal.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5 - ASV(Adoptive Support Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>
#include <linux/of.h>

#include <mach/asv-exynos.h>
#include <mach/asv-exynos_cal.h>

#include <mach/exynos-powermode.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>

#ifdef CONFIG_EXYNOS_ASV_MARGIN_TEST
static int set_cl1_volt;
static int set_cl0_volt;
static int set_int_volt;
static int set_mif_volt;
static int set_g3d_volt;
static int set_isp_volt;

static int __init get_cl1_volt(char *str)
{
	get_option(&str, &set_cl1_volt);
	return 0;
}
early_param("cl1", get_cl1_volt);

static int __init get_cl0_volt(char *str)
{
	get_option(&str, &set_cl0_volt);
	return 0;
}
early_param("cl0", get_cl0_volt);

static int __init get_int_volt(char *str)
{
	get_option(&str, &set_int_volt);
	return 0;
}
early_param("int", get_int_volt);

static int __init get_mif_volt(char *str)
{
	get_option(&str, &set_mif_volt);
	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_g3d_volt(char *str)
{
	get_option(&str, &set_g3d_volt);
	return 0;
}
early_param("g3d", get_g3d_volt);

static int __init get_isp_volt(char *str)
{
	get_option(&str, &set_isp_volt);
	return 0;
}
early_param("isp", get_isp_volt);

static int exynos_get_margin_test_param(enum asv_type_id target_type)
{
	int add_volt = 0;

	switch (target_type){
	case ID_CL1:
		add_volt = set_cl1_volt;
		break;
	case ID_CL0:
		add_volt = set_cl0_volt;
		break;
	case ID_G3D:
		add_volt = set_g3d_volt;
		break;
	case ID_MIF:
		add_volt = set_mif_volt;
		break;
	case ID_INT:
		add_volt = set_int_volt;
		break;
	case ID_ISP:
		add_volt = set_isp_volt;
		break;
	default:
		return add_volt;
	}
	return add_volt;
}

#endif

static void exynos_set_abb(struct asv_info *asv_inform)
{
	unsigned int target_value;
	target_value = asv_inform->abb_info->target_abb;

	if (asv_inform->ops_cal->set_abb == NULL) {
		pr_err("%s: ASV : Fail set_abb from CAL\n", __func__);
		return;
	}

	asv_inform->ops_cal->set_abb(asv_inform->asv_type, target_value);
}

static struct abb_common exynos_abb_common_cl1 = {
	.set_target_abb = exynos_set_abb,
};

static struct abb_common exynos_abb_common_cl0 = {
	.set_target_abb = exynos_set_abb,
};

static struct abb_common exynos_abb_common_int = {
	.set_target_abb = exynos_set_abb,
};

static struct abb_common exynos_abb_common_g3d = {
	.set_target_abb = exynos_set_abb,
};

static struct abb_common exynos_abb_common_mif = {
	.set_target_abb = exynos_set_abb,
};

static unsigned int exynos_get_asv_group(struct asv_common *asv_comm)
{
	int asv_group = 0;
	if (asv_comm->ops_cal->is_fused_sp_gr())
		pr_info("use fused group\n");

	if (asv_comm->ops_cal->get_asv_gr == NULL)
		return asv_group = 0;

	asv_group = (int) asv_comm->ops_cal->get_asv_gr();

	if (asv_group < 0) {
		pr_err("%s: Faile get ASV group from CAL\n", __func__);
		return 0;
	}
	return asv_group;
}

static void exynos_show_asv_info(struct asv_info *asv_inform, bool use_abb)
{
	int i;

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
#ifdef CONFIG_EXYNOS_ASV_SUPPORT_RCC
		if (use_abb)
			pr_info("%s LV%d freq : %d volt : %d abb : %d, rcc: %d, group: %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_abb[i].asv_value,
					asv_inform->asv_rcc[i].asv_value,
					asv_inform->asv_sub_grp[i].asv_grp);
		else
			pr_info("%s LV%d freq : %d volt : %d, rcc : %d, group : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_rcc[i].asv_value,
					asv_inform->asv_sub_grp[i].asv_grp);
#else
		if (use_abb)
			pr_info("%s LV%d freq : %d volt : %d abb : %d, group: %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_abb[i].asv_value,
					asv_inform->asv_sub_grp[i].asv_grp);
		else
			pr_info("%s LV%d freq : %d volt : %d, group : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_sub_grp[i].asv_grp);
#endif
	}

}

static void exynos_set_asv_info(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	bool useABB;

#ifdef CONFIG_EXYNOS_ASV_DYNAMIC_ABB
	if (asv_inform->ops_cal->get_use_abb == NULL)
		useABB = false;
	else
		useABB = asv_inform->ops_cal->get_use_abb(asv_inform->asv_type);
#else
	useABB = false;
#endif

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table)
		* asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

#ifdef CONFIG_EXYNOS_ASV_DYNAMIC_ABB
	if (useABB) {
		asv_inform->asv_abb  = kmalloc((sizeof(struct asv_freq_table)
			* asv_inform->dvfs_level_nr), GFP_KERNEL);
		if (!asv_inform->asv_abb) {
			pr_err("%s: Memory allocation failed for asv abb\n", __func__);
			kfree(asv_inform->asv_volt);
			return;
		}
	}
#endif

#ifdef CONFIG_EXYNOS_ASV_SUPPORT_RCC
	asv_inform->asv_rcc  = kmalloc((sizeof(struct asv_freq_table)
				* asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_rcc) {
		pr_err("%s: Memory allocation failed for asv abb\n", __func__);
		kfree(asv_inform->asv_rcc);
		return;
	}
#endif
	asv_inform->asv_sub_grp = kmalloc((sizeof(struct asv_grp_table)
		* asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_sub_grp) {
		pr_err("%s: Memory allocation failed for asv sub_grp\n", __func__);
		kfree(asv_inform->asv_volt);
		kfree(asv_inform->asv_abb);
		return;
	}

	if (asv_inform->ops_cal->get_freq == NULL || asv_inform->ops_cal->get_vol == NULL) {
		pr_err("%s: ASV : Fail get call back function for CAL\n", __func__);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = asv_inform->ops_cal->get_freq(asv_inform->asv_type, i) * 1000;
		if (asv_inform->asv_volt[i].asv_freq == 0)
			pr_err("ASV : Fail get Freq from CAL!\n");
#ifdef CONFIG_EXYNOS_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value = asv_inform->ops_cal->get_vol(asv_inform->asv_type, i);
		if (asv_inform->asv_volt[i].asv_value == 0)
			asv_inform->asv_volt[i].asv_value = asv_inform->max_volt_value;
		else
			asv_inform->asv_volt[i].asv_value += exynos_get_margin_test_param(asv_inform->asv_type);
#else
		asv_inform->asv_volt[i].asv_value = asv_inform->ops_cal->get_vol(asv_inform->asv_type, i);
		if (asv_inform->asv_volt[i].asv_value == 0)
			asv_inform->asv_volt[i].asv_value = asv_inform->max_volt_value;
#endif

#ifdef CONFIG_EXYNOS_ASV_DYNAMIC_ABB
		if (useABB) {
			asv_inform->asv_abb[i].asv_freq = asv_inform->asv_volt[i].asv_freq;
			asv_inform->asv_abb[i].asv_value = asv_inform->ops_cal->get_abb(asv_inform->asv_type, i);
		}
#endif

#ifdef CONFIG_EXYNOS_ASV_SUPPORT_RCC
		asv_inform->asv_rcc[i].asv_freq = asv_inform->asv_volt[i].asv_freq;
		asv_inform->asv_rcc[i].asv_value = asv_inform->ops_cal->get_rcc(asv_inform->asv_type, i);
#endif
		/* get the asv group information */
		asv_inform->asv_sub_grp[i].asv_sub_idx = asv_inform->ops_cal->get_sub_grp_idx(asv_inform->asv_type, i);
		asv_inform->asv_sub_grp[i].asv_grp = asv_inform->ops_cal->get_group(asv_inform->asv_type, i);
	}

	if (show_value)
		exynos_show_asv_info(asv_inform, useABB);

}

static unsigned int exynos_get_asv_sub_group(struct asv_info *asv_inform, unsigned int idx)
{
	int i;

	if (idx > MAX_ASV_SUB_GRP_NR) {
		pr_info("ASV: index %d is invalidated index\n", idx);
		return -EINVAL;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		if (asv_inform->asv_sub_grp[i].asv_sub_idx == idx)
			return asv_inform->asv_sub_grp[i].asv_grp;
	}

	pr_info("ASV: faile to get asv_sub_group\n");
	return 0;
}

#ifdef CONFIG_EXYNOS_ASV_SUPPORT_RCC
static void exynos_set_rcc_info(struct asv_info *asv_inform)
{
	int i;
	unsigned int rcc_value, domain_id;

	domain_id = asv_inform->asv_type;

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		rcc_value = asv_inform->asv_rcc[i].asv_value;
		asv_inform->ops_cal->set_rcc(domain_id, i, rcc_value);
	}
}
#endif

static struct asv_ops exynos_asv_ops = {
	.get_asv_group  = exynos_get_asv_group,
	.set_asv_info   = exynos_set_asv_info,
	.get_asv_sub_group = exynos_get_asv_sub_group,
#ifdef CONFIG_EXYNOS_ASV_SUPPORT_RCC
	.set_rcc_info   = exynos_set_rcc_info,
#endif
};

static struct asv_ops_cal exynos_asv_ops_cal = {
	.get_vol = cal_get_volt,
	.get_freq = cal_get_freq,
	.get_abb = cal_get_abb,
	.get_use_abb = cal_use_dynimic_abb,
	.set_abb = cal_set_abb,
	.get_group = cal_get_asv_grp,
	.get_sub_grp_idx = cal_get_match_subgrp,
#ifdef CONFIG_EXYNOS_ASV_SUPPORT_RCC
	.get_rcc = cal_get_rcc,
	.set_rcc = cal_set_rcc,
#endif
};

struct asv_info exynos_asv_member[] = {
	{
		.asv_type	= ID_CL1,
		.name		= "VDD_CL1",
		.ops		= &exynos_asv_ops,
		.ops_cal	= &exynos_asv_ops_cal,
		.abb_info	= &exynos_abb_common_cl1,
		.asv_group_nr	= MAX_ASV_GRP_NR,
	}, {
		.asv_type	= ID_CL0,
		.name		= "VDD_CL0",
		.ops		= &exynos_asv_ops,
		.ops_cal	= &exynos_asv_ops_cal,
		.abb_info	= &exynos_abb_common_cl0,
		.asv_group_nr	= MAX_ASV_GRP_NR,
	}, {
		.asv_type	= ID_INT,
		.name		= "VDD_INT",
		.ops		= &exynos_asv_ops,
		.ops_cal	= &exynos_asv_ops_cal,
		.abb_info	= &exynos_abb_common_int,
		.asv_group_nr	= MAX_ASV_GRP_NR,
	}, {
		.asv_type	= ID_MIF,
		.name		= "VDD_MIF",
		.ops		= &exynos_asv_ops,
		.ops_cal	= &exynos_asv_ops_cal,
		.abb_info	= &exynos_abb_common_mif,
		.asv_group_nr	= MAX_ASV_GRP_NR,
	}, {
		.asv_type	= ID_G3D,
		.name		= "VDD_G3D",
		.ops		= &exynos_asv_ops,
		.ops_cal	= &exynos_asv_ops_cal,
		.abb_info	= &exynos_abb_common_g3d,
		.asv_group_nr	= MAX_ASV_GRP_NR,
	}, {
		.asv_type	= ID_ISP,
		.name		= "VDD_ISP",
		.ops		= &exynos_asv_ops,
		.ops_cal	= &exynos_asv_ops_cal,
		.asv_group_nr	= MAX_ASV_GRP_NR,
	},
};

unsigned int exynos_regist_asv_member(void)
{
	unsigned int i;

	/* Regist asv member into list */
	for (i = 0; i < ARRAY_SIZE(exynos_asv_member); i++)
		add_asv_member(&exynos_asv_member[i]);

	return 0;
}

void exynos_set_asv_member_info(struct asv_common *asv_inform)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(exynos_asv_member); i++)
		if (exynos_asv_member[i].asv_type == ID_CL1) {
			exynos_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_CL1);
			exynos_asv_member[i].max_volt_value =
				asv_inform->ops_cal->get_max_volt(ID_CL1);
		} else if (exynos_asv_member[i].asv_type == ID_CL0) {
			exynos_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_CL0);
			exynos_asv_member[i].max_volt_value =
				asv_inform->ops_cal->get_max_volt(ID_CL0);
		} else if (exynos_asv_member[i].asv_type == ID_MIF) {
			exynos_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_MIF);
			exynos_asv_member[i].max_volt_value =
				asv_inform->ops_cal->get_max_volt(ID_MIF);
		} else if (exynos_asv_member[i].asv_type == ID_INT) {
			exynos_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_INT);
			exynos_asv_member[i].max_volt_value =
				asv_inform->ops_cal->get_max_volt(ID_INT);
		} else if (exynos_asv_member[i].asv_type == ID_G3D) {
			exynos_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_G3D);
			exynos_asv_member[i].max_volt_value =
				asv_inform->ops_cal->get_max_volt(ID_G3D);
		} else if (exynos_asv_member[i].asv_type == ID_ISP) {
			exynos_asv_member[i].dvfs_level_nr =
				asv_inform->ops_cal->get_min_lv(ID_ISP);
			exynos_asv_member[i].max_volt_value =
				asv_inform->ops_cal->get_max_volt(ID_ISP);
		}
}

#if defined (CONFIG_PM) && defined (CONFIG_EXYNOS_ASV_DYNAMIC_ABB)
static struct sfr_save exynos_abb_save[] = {
	SFR_SAVE(EXYNOS_PMU_BODY_BIAS_CON0),
	SFR_SAVE(EXYNOS_PMU_BODY_BIAS_CON1),
	SFR_SAVE(EXYNOS_PMU_BODY_BIAS_CON2),
	SFR_SAVE(EXYNOS_PMU_BODY_BIAS_CON3),
	SFR_SAVE(EXYNOS_PMU_BODY_BIAS_CON4),
};

static int exynos_asv_suspend(void)
{
	struct asv_info *exynos_asv_info;
	int i;

	exynos_save_sfr(exynos_abb_save,
			ARRAY_SIZE(exynos_abb_save));

	for (i = 0; i < ARRAY_SIZE(exynos_asv_member); i++) {
		exynos_asv_info = &exynos_asv_member[i];
		exynos_asv_info->ops_cal
			->set_abb(exynos_asv_info->asv_type, ABB_BYPASS);
	}

	return 0;
}

static void exynos_asv_resume(void)
{
	exynos_restore_sfr(exynos_abb_save,
			ARRAY_SIZE(exynos_abb_save));
}
#else
#define exynos_asv_suspend NULL
#define exynos_asv_resume NULL
#endif

static struct syscore_ops exynos_asv_syscore_ops = {
	.suspend	= exynos_asv_suspend,
	.resume		= exynos_asv_resume,
};

static struct asv_common_ops_cal exynos_asv_ops_common_cal = {
	.get_max_volt		= cal_get_max_volt,
	.get_min_lv		= cal_get_min_lv,
	.init			= cal_init,
	.get_table_ver		= cal_get_table_ver,
	.is_fused_sp_gr		= cal_is_fused_speed_grp,
	.get_asv_gr		= cal_get_ids_hpm_group,
	.get_ids		= cal_get_ids,
	.get_hpm		= cal_get_hpm,
#ifdef CONFIG_EXYNOS_ASV_SUPPORT_RCC
	.set_rcc_limit_info = cal_set_rcc_limit,
#endif
};

void exynos_set_ema(enum asv_type_id type, unsigned int volt)
{
	cal_set_ema(type, volt);
}

unsigned int exynos_get_asv_info(int id)
{
	return cal_get_asv_info(id);
}

unsigned int exynos_get_table_ver(void)
{
	return cal_get_table_ver();
}

int exynos_init_asv(struct asv_common *asv_info)
{
	asv_info->ops_cal = &exynos_asv_ops_common_cal;

	asv_info->ops_cal->init();	/* CAL initiallize */

	if (asv_info->ops_cal->get_table_ver != NULL)
		pr_info("ASV: ASV Table Ver : %d \n",
			asv_info->ops_cal->get_table_ver());

	if (asv_info->ops_cal->is_fused_sp_gr())
		pr_info("ASV: Use Speed Group\n");
	else {
		pr_info("ASV: Use not Speed Group\n");
		if (asv_info->ops_cal->get_ids != NULL)
			pr_info("ASV: IDS: %d \n",
					asv_info->ops_cal->get_ids());

		if (asv_info->ops_cal->get_hpm != NULL)
			pr_info("ASV: HPM: %d \n",
					asv_info->ops_cal->get_hpm());

	}

	register_syscore_ops(&exynos_asv_syscore_ops);

	asv_info->regist_asv_member = exynos_regist_asv_member;
	exynos_set_asv_member_info(asv_info);

	return 0;
}
