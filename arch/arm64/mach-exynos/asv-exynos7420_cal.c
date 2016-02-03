/* linux/arch/arm64/mach-exynos/include/mach/asv-exynos7420_cal.c
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* EXYNOS7420 - Adoptive Support Voltage Header file
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <mach/asv-exynos_cal.h>
#include <mach/asv-exynos7420.h>
#define EXYNOS_MAILBOX_RCC_MIN_MAX(x)		(S5P_VA_APM_SRAM + (0x3700) + (x * 0x4))
#define EXYNOS_MAILBOX_ATLAS_RCC(x)		(S5P_VA_APM_SRAM + (0x3730) + (x * 0x4))
#define EXYNOS_MAILBOX_APOLLO_RCC(x)		(S5P_VA_APM_SRAM + (0x3798) + (x * 0x4))
#define EXYNOS_MAILBOX_G3D_RCC(x)		(S5P_VA_APM_SRAM + (0x37EC) + (x * 0x4))
#define EXYNOS_MAILBOX_MIF_RCC(x)		(S5P_VA_APM_SRAM + (0x381C) + (x * 0x4))
#define EXYNOS_MAILBOX_ASV_TABLE		(S5P_VA_APM_SRAM + (0x3860))
#include <linux/smc.h>
#define SYSREG_BASE EXYNOS7420_VA_SYSREG

#define ASV_TBL_ADDR_BASE	(0x101E0160)
#define ASV_TBL_ADDR_CNT	(5)
#define ASV_RCC_ADDR_CNT	(8)
#define ASV_EMA_ADDR_CNT	(3)

static volatile bool use_dynimic_abb[SYSC_DVFS_NUM];

struct _asv_tbl_info /*little endian*/
{
	unsigned bigcpu_asv_group:12;			//(ASV_TBL_BASE+0x00)[11:0]
	unsigned bigcpu_ssa0:4;					//(ASV_TBL_BASE+0x00)[15:12]
	unsigned littlecpu_asv_group:12;		//(ASV_TBL_BASE+0x00)[27:16]
	unsigned littlecpu_ssa0:4;				//(ASV_TBL_BASE+0x00)[31:28]

	unsigned g3d_asv_group:12;				//(ASV_TBL_BASE+0x04)[11:0]
	unsigned g3d_ssa0:4;					//(ASV_TBL_BASE+0x04)[15:12]
	unsigned mif_asv_group:12;				//(ASV_TBL_BASE+0x04)[27:16]
	unsigned mif_ssa0:4;					//(ASV_TBL_BASE+0x04)[31:28]

	unsigned int_asv_group:12;				//(ASV_TBL_BASE+0x08)[11:0]
	unsigned int_ssa0:4;					//(ASV_TBL_BASE+0x08)[15:12]
	unsigned cam_disp_asv_group:12;			//(ASV_TBL_BASE+0x08)[27:16]
	unsigned cam_disp_ssa0:4;				//(ASV_TBL_BASE+0x08)[31:28]

	unsigned dvfs_asv_table_version:4;		//(ASV_TBL_BASE+0x0C)[3:0]
	unsigned asv_group_type:1;				//(ASV_TBL_BASE+0x0C)[4]
	unsigned reserved01:3;					//(ASV_TBL_BASE+0x0C)[7:5]
	unsigned shift_type:1;					//(ASV_TBL_BASE+0x0C)[8]
	unsigned ssa1_enable:1;					//(ASV_TBL_BASE+0x0C)[9]
	unsigned ssa0_enable:1;				//(ASV_TBL_BASE+0x0C)[10]
	unsigned reserved02:5;					//(ASV_TBL_BASE+0x0C)[15:11]
	unsigned asv_method:1;					//(ASV_TBL_BASE+0x0C)[16]
	unsigned reserved03:15;					//(ASV_TBL_BASE+0x0C)[31:17]

	unsigned main_asv_group:4;				//(ASV_TBL_BASE+0x10)[3:0]
	unsigned main_asv_ssa:3;				//(ASV_TBL_BASE+0x10)[6:4]
	unsigned main_asv_ssa_minus_sign:1;		//(ASV_TBL_BASE+0x10)[7]
	unsigned bigcpu_ssa1:4;					//(ASV_TBL_BASE+0x10)[11:8]
	unsigned littlecpu_ssa1:4;				//(ASV_TBL_BASE+0x10)[15:12]
	unsigned g3d_ssa1:4;					//(ASV_TBL_BASE+0x10)[19:16]
	unsigned mif_ssa1:4;					//(ASV_TBL_BASE+0x10)[23:20]
	unsigned int_ssa1:4;					//(ASV_TBL_BASE+0x10)[27:24]
	unsigned cam_disp_ssa1:4;				//(ASV_TBL_BASE+0x10)[31:28]

	unsigned reserved04:9;				//(ASV_TBL_BASE+0x14)[8:0]//160
	unsigned apollo_ema_vddpe_1_0V:3;	//(ASV_TBL_BASE+0x14)[11:9]
	unsigned apollo_ema_vddpe_0_9V:3;	//(ASV_TBL_BASE+0x14)[14:12]
	unsigned apollo_ema_vddpe_0_8V:3;	//(ASV_TBL_BASE+0x14)[17:15]
	unsigned apollo_ema_vddpe_0_7V:3;	//(ASV_TBL_BASE+0x14)[20:18]
	unsigned g3d_mcs_0_9V:2;			//(ASV_TBL_BASE+0x14)[22:21]
	unsigned g3d_mcs_0_8V:2;			//(ASV_TBL_BASE+0x14)[24:23]
	unsigned g3d_mcs_0_7V:2;			//(ASV_TBL_BASE+0x14)[26:25]
	unsigned g3d_mcs_0_59V:2;			//(ASV_TBL_BASE+0x14)[28:27]
	unsigned reserved05:3;				//(ASV_TBL_BASE+0x14)[31:29] //191

	unsigned rcc_hpm_min_max0:32;					//(ASV_TBL_BASE+0x18)[31:27]
	unsigned rcc_hpm_min_max1:32;					//(ASV_TBL_BASE+0x1C)[31:27]
	unsigned rcc_hpm_min_max2:32;					//(ASV_TBL_BASE+0x20)[31:27]
	unsigned rcc_hpm_min_max3:32;					//(ASV_TBL_BASE+0x24)[31:27]
	unsigned rcc_hpm_min_max4:32;					//(ASV_TBL_BASE+0x28)[31:27]
	unsigned rcc_hpm_min_max5:32;					//(ASV_TBL_BASE+0x2C)[31:27]
	unsigned rcc_hpm_min_max6:32;					//(ASV_TBL_BASE+0x30)[31:27]
	unsigned rcc_hpm_min_max7:32;					//(ASV_TBL_BASE+0x34)[31:27]

	unsigned atlas_vthr:2;						//(ASV_TBL_BASE+0x38)[1:0]//448
	unsigned atlas_delta:2;					//(ASV_TBL_BASE+0x38)[3:2]
	unsigned apollo_vthr:2;					//(ASV_TBL_BASE+0x38)[5:4]
	unsigned apollo_delta:2;					//(ASV_TBL_BASE+0x38)[7:6]
	unsigned g3d_vthr:2;						//(ASV_TBL_BASE+0x38)[9:8]
	unsigned g3d_delta:2;						//(ASV_TBL_BASE+0x38)[11:10]
	unsigned int_vthr:2;						//(ASV_TBL_BASE+0x38)[13:12]
	unsigned int_delta:2;						//(ASV_TBL_BASE+0x38)[15:14]
	unsigned atlas_l1_ema_c_vddpe_1_1V:4;	//(ASV_TBL_BASE+0x38)[19:16]
	unsigned atlas_l1_ema_c_vddpe_0_9V:4;	//(ASV_TBL_BASE+0x38)[23:20]
	unsigned atlas_l1_ema_c_vddpe_0_8V:4;	//(ASV_TBL_BASE+0x38)[27:24]
	unsigned atlas_l1_ema_c_vddpe_0_7V:4;	//(ASV_TBL_BASE+0x38)[31:28]

	unsigned atlas_l2_ema_l2d_vddpe_1_1V:4;	//(ASV_TBL_BASE+0x3C)[3:0]//480
	unsigned atlas_l2_ema_l2d_vddpe_0_9V:4;	//(ASV_TBL_BASE+0x3C)[7:4]
	unsigned atlas_l2_ema_l2d_vddpe_0_8V:4;	//(ASV_TBL_BASE+0x3C)[11:8]
	unsigned atlas_l2_ema_l2d_vddpe_0_7V:4;	//(ASV_TBL_BASE+0x3C)[15:12]
	unsigned int_ema_vddpe:3;				//(ASV_TBL_BASE+0x3C)[18:16]
	unsigned cam_ema_vddpe:3;				//(ASV_TBL_BASE+0x3C)[21:19]
	unsigned reserved16:10;					//(ASV_TBL_BASE+0x3C)[31:22]
};

static struct _asv_tbl_info gasv_table_info;

#ifdef CONFIG_SOC_EXYNOS7420
u32 re_err(void)
{
	pr_err("ASV: CAL is working wrong. \n");
	return 0;
}
#endif
static void cal_print_asv_info(void)
{
	pr_info("(ASV_TBL_BASE+0x00)[11:0]  bigcpu_asv_group         = %d\n", gasv_table_info.bigcpu_asv_group);
	pr_info("(ASV_TBL_BASE+0x00)[15:12] bigcpu_ssa0              = %d\n", gasv_table_info.bigcpu_ssa0);
	pr_info("(ASV_TBL_BASE+0x00)[27:16] littlecpu_asv_group      = %d\n", gasv_table_info.littlecpu_asv_group);
	pr_info("(ASV_TBL_BASE+0x00)[31:28] littlecpu_ssa0           = %d\n", gasv_table_info.littlecpu_ssa0);

	pr_info("(ASV_TBL_BASE+0x04)[11:0]  g3d_asv_group            = %d\n", gasv_table_info.g3d_asv_group);
	pr_info("(ASV_TBL_BASE+0x04)[15:12] g3d_ssa0                 = %d\n", gasv_table_info.g3d_ssa0);
	pr_info("(ASV_TBL_BASE+0x04)[27:16] mif_asv_group            = %d\n", gasv_table_info.mif_asv_group);
	pr_info("(ASV_TBL_BASE+0x04)[31:28] mif_ssa0                 = %d\n", gasv_table_info.mif_ssa0);

	pr_info("(ASV_TBL_BASE+0x08)[11:0]  int_asv_group            = %d\n", gasv_table_info.int_asv_group);
	pr_info("(ASV_TBL_BASE+0x08)[15:12] int_ssa0                 = %d\n", gasv_table_info.int_ssa0);
	pr_info("(ASV_TBL_BASE+0x08)[27:16] cam_disp_asv_group       = %d\n", gasv_table_info.cam_disp_asv_group);
	pr_info("(ASV_TBL_BASE+0x08)[31:28] cam_disp_ssa0            = %d\n", gasv_table_info.cam_disp_ssa0);

	pr_info("(ASV_TBL_BASE+0x0C)[3:0]   dvfs_asv_table_version   = %d\n", gasv_table_info.dvfs_asv_table_version);
	pr_info("(ASV_TBL_BASE+0x0C)[4]     asv_group_type           = %d\n", gasv_table_info.asv_group_type);
	pr_info("(ASV_TBL_BASE+0x0C)[7:5]   reserved01               = %d\n", gasv_table_info.reserved01);
	pr_info("(ASV_TBL_BASE+0x0C)[8]     shift_type               = %d\n", gasv_table_info.shift_type);
	pr_info("(ASV_TBL_BASE+0x0C)[9]     ssa1_enable	  	     = %d\n", gasv_table_info.ssa1_enable);
	pr_info("(ASV_TBL_BASE+0x0C)[10]    ssa0_enable              = %d\n", gasv_table_info.ssa0_enable);
	pr_info("(ASV_TBL_BASE+0x0C)[15:11] reserved02               = %d\n", gasv_table_info.reserved02);
	pr_info("(ASV_TBL_BASE+0x0C)[16]    asv_method               = %d\n", gasv_table_info.asv_method);
	pr_info("(ASV_TBL_BASE+0x0C)[31:17] reserved03               = %d\n", gasv_table_info.reserved03);

	pr_info("(ASV_TBL_BASE+0x10)[3:0]   main_asv_group           = %d\n", gasv_table_info.main_asv_group);
	pr_info("(ASV_TBL_BASE+0x10)[7:4]   main_asv_ssa             = %s%d\n", (gasv_table_info.main_asv_ssa_minus_sign==1)?"-":"",gasv_table_info.main_asv_ssa);
	pr_info("(ASV_TBL_BASE+0x10)[11:8]  bigcpu_ssa1              = %d\n", gasv_table_info.bigcpu_ssa1);
	pr_info("(ASV_TBL_BASE+0x10)[15:12] littlecpu_ssa1           = %d\n", gasv_table_info.littlecpu_ssa1);
	pr_info("(ASV_TBL_BASE+0x10)[19:16] g3d_ssa1                 = %d\n", gasv_table_info.g3d_ssa1);
	pr_info("(ASV_TBL_BASE+0x10)[23:20] mif_ssa1                 = %d\n", gasv_table_info.mif_ssa1);
	pr_info("(ASV_TBL_BASE+0x10)[27:24] int_ssa1                 = %d\n", gasv_table_info.int_ssa1);
	pr_info("(ASV_TBL_BASE+0x10)[31:28] cam_disp_ssa1            = %d\n", gasv_table_info.cam_disp_ssa1);

	pr_info("(ASV_TBL_BASE+0x14)[8:0] reserved04          	     = %d\n", gasv_table_info.reserved04);
	pr_info("(ASV_TBL_BASE+0x14)[11:9] apollo_ema_vddpe_1_0V     = %d\n", gasv_table_info.apollo_ema_vddpe_1_0V);
	pr_info("(ASV_TBL_BASE+0x14)[14:12]apollo_ema_vddpe_0_9V     = %d\n", gasv_table_info.apollo_ema_vddpe_0_9V);
	pr_info("(ASV_TBL_BASE+0x14)[17:15] apollo_ema_vddpe_0_8V    = %d\n", gasv_table_info.apollo_ema_vddpe_0_8V);
	pr_info("(ASV_TBL_BASE+0x14)[20:18] apollo_ema_vddpe_0_7V    = %d\n", gasv_table_info.apollo_ema_vddpe_0_7V);
	pr_info("(ASV_TBL_BASE+0x14)[22:21] g3d_mcs_0_9V             = %d\n", gasv_table_info.g3d_mcs_0_9V);
	pr_info("(ASV_TBL_BASE+0x14)[24:23] g3d_mcs_0_8V             = %d\n", gasv_table_info.g3d_mcs_0_8V);
	pr_info("(ASV_TBL_BASE+0x14)[26:25] g3d_mcs_0_7V             = %d\n", gasv_table_info.g3d_mcs_0_7V);
	pr_info("(ASV_TBL_BASE+0x14)[28:27] g3d_mcs_0_59V            = %d\n", gasv_table_info.g3d_mcs_0_59V);
	pr_info("(ASV_TBL_BASE+0x14)[31:29] reserved05               = %d\n", gasv_table_info.reserved05);

	pr_info("(ASV_TBL_BASE+0x18)[31:0] rcc_hpm_min_max0	     = %08x\n", gasv_table_info.rcc_hpm_min_max0);
	pr_info("(ASV_TBL_BASE+0x1C)[31:0] rcc_hpm_min_max1          = %08x\n", gasv_table_info.rcc_hpm_min_max1);
	pr_info("(ASV_TBL_BASE+0x20)[31:0] rcc_hpm_min_max2          = %08x\n", gasv_table_info.rcc_hpm_min_max2);
	pr_info("(ASV_TBL_BASE+0x24)[31:0] rcc_hpm_min_max3          = %08x\n", gasv_table_info.rcc_hpm_min_max3);
	pr_info("(ASV_TBL_BASE+0x28)[31:0] rcc_hpm_min_max4          = %08x\n", gasv_table_info.rcc_hpm_min_max4);
	pr_info("(ASV_TBL_BASE+0x2C)[31:0] rcc_hpm_min_max5          = %08x\n", gasv_table_info.rcc_hpm_min_max5);
	pr_info("(ASV_TBL_BASE+0x30)[31:0] rcc_hpm_min_max6          = %08x\n", gasv_table_info.rcc_hpm_min_max6);
	pr_info("(ASV_TBL_BASE+0x34)[31:0] rcc_hpm_min_max7          = %08x\n", gasv_table_info.rcc_hpm_min_max7);
	pr_info("(ASV_TBL_BASE+0x38)[1:0] atlas_vthr           	     = %d\n", gasv_table_info.atlas_vthr);
	pr_info("(ASV_TBL_BASE+0x38)[3:2] atlas_delta          	     = %d\n", gasv_table_info.atlas_delta);
	pr_info("(ASV_TBL_BASE+0x38)[5:4] apollo_vthr          	     = %d\n", gasv_table_info.apollo_vthr);
	pr_info("(ASV_TBL_BASE+0x38)[7:6] apollo_delta               = %d\n", gasv_table_info.apollo_delta);
	pr_info("(ASV_TBL_BASE+0x38)[9:8] g3d_vthr          	     = %d\n", gasv_table_info.g3d_vthr);
	pr_info("(ASV_TBL_BASE+0x38)[11:10] g3d_delta          	     = %d\n", gasv_table_info.g3d_delta);
	pr_info("(ASV_TBL_BASE+0x38)[13:12] int_vthr          	     = %d\n", gasv_table_info.int_vthr);
	pr_info("(ASV_TBL_BASE+0x38)[15:14] int_delta          	     = %d\n", gasv_table_info.int_delta);
	pr_info("(ASV_TBL_BASE+0x38)[19:16] atlas_l1_ema_c_vddpe_1_1V   = %d\n", gasv_table_info.atlas_l1_ema_c_vddpe_1_1V);
	pr_info("(ASV_TBL_BASE+0x38)[23:20] atlas_l1_ema_c_vddpe_0_9V	= %d\n", gasv_table_info.atlas_l1_ema_c_vddpe_0_9V);
	pr_info("(ASV_TBL_BASE+0x38)[27:24] atlas_l1_ema_c_vddpe_0_8V   = %d\n", gasv_table_info.atlas_l1_ema_c_vddpe_0_8V);
	pr_info("(ASV_TBL_BASE+0x38)[31:28] atlas_l1_ema_c_vddpe_0_7V	= %d\n", gasv_table_info.atlas_l1_ema_c_vddpe_0_7V);

	pr_info("(ASV_TBL_BASE+0x3C)[3:0]  atlas_l2_ema_l2d_vddpe_1_1V 	= %d\n", gasv_table_info.atlas_l2_ema_l2d_vddpe_1_1V);
	pr_info("(ASV_TBL_BASE+0x3C)[7:4]  atlas_l2_ema_l2d_vddpe_0_9V  = %d\n", gasv_table_info.atlas_l2_ema_l2d_vddpe_0_9V);
	pr_info("(ASV_TBL_BASE+0x3C)[11:8]  atlas_l2_ema_l2d_vddpe_0_8V = %d\n", gasv_table_info.atlas_l2_ema_l2d_vddpe_0_8V);
	pr_info("(ASV_TBL_BASE+0x3C)[15:12] atlas_l2_ema_l2d_vddpe_0_7V = %d\n", gasv_table_info.atlas_l2_ema_l2d_vddpe_0_7V);
	pr_info("(ASV_TBL_BASE+0x3C)[18:16]  int_ema_vddpe  		= %d\n", gasv_table_info.int_ema_vddpe);
	pr_info("(ASV_TBL_BASE+0x3C)[21:19]  cam_ema_vddpe 		= %d\n", gasv_table_info.cam_ema_vddpe);
	pr_info("(ASV_TBL_BASE+0x3C)[31:22]  reserved16              	= %d\n", gasv_table_info.reserved16);
}

static void asv_table_init(void)
{
	int i;
	u32 *pasv_table;
	unsigned long tmp;

	pasv_table = (u32 *)&gasv_table_info;
	for(i = 0; i < ASV_TBL_ADDR_CNT + ASV_RCC_ADDR_CNT + ASV_EMA_ADDR_CNT; i++) {
		exynos_smc_readsfr((unsigned long) ASV_TBL_ADDR_BASE + 0x4 * i, &tmp);
		*(pasv_table +i) = (u32)tmp;
	}

	if (gasv_table_info.dvfs_asv_table_version > MAX_ASV_TABLE) {
		pr_info("(ASV_TBL_VERSION %d ---> %d\n", gasv_table_info.dvfs_asv_table_version, MAX_ASV_TABLE);
		gasv_table_info.dvfs_asv_table_version = MAX_ASV_TABLE;
	}

	if (gasv_table_info.dvfs_asv_table_version > 11 && gasv_table_info.dvfs_asv_table_version < 15) {
		pr_info("(ASV_TBL_VERSION %d ---> %d\n", gasv_table_info.dvfs_asv_table_version, 15);
		gasv_table_info.dvfs_asv_table_version = 15;
	}
}

bool cal_is_fused_speed_grp(void)
{
	return gasv_table_info.asv_method;
}

u32 cal_get_dram_size(void)
{
	return 0;
}

u32 cal_get_table_ver(void)
{
	return gasv_table_info.dvfs_asv_table_version;
}

u32 cal_get_ids(void)
{
	return __raw_readl(CHIPID_ASV_INFO + 0x01C0) & 0xff;;
}

u32 cal_get_hpm(void)
{
	return (__raw_readl(CHIPID_ASV_INFO + 0x01C4) >> 24) & 0xff;
}

void cal_init(void)
{
	use_dynimic_abb[SYSC_DVFS_BIG] = false;
	use_dynimic_abb[SYSC_DVFS_LIT] = false;
	use_dynimic_abb[SYSC_DVFS_G3D] = false;
	use_dynimic_abb[SYSC_DVFS_INT] = false;
	use_dynimic_abb[SYSC_DVFS_MIF] = false;
	use_dynimic_abb[SYSC_DVFS_CAM] = false;

	asv_table_init();
	cal_print_asv_info();
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
	u32 subgrp = 0;	/*  version 0 */

	switch (id) {
	case SYSC_DVFS_BIG:
		subgrp = (level <= SYSC_DVFS_L9) ? 0 :
			(level <= SYSC_DVFS_L15) ? 1 : 2;
		break;
	case SYSC_DVFS_LIT:
		if (gasv_table_info.dvfs_asv_table_version >= 9) {
			subgrp = (level <= SYSC_DVFS_L9) ? 0 :
				(level <= SYSC_DVFS_L13) ? 1 : 2;
		} else {
			subgrp = (level <= SYSC_DVFS_L9) ? 0 :
				(level <= SYSC_DVFS_L14) ? 1 : 2;
		}
		break;
	case SYSC_DVFS_G3D:
		if(gasv_table_info.dvfs_asv_table_version <= 6){
			subgrp = (level <= SYSC_DVFS_L3) ? 0 :
				(level <= SYSC_DVFS_L6) ? 1 : 2;
		} else{
			subgrp = (level <= SYSC_DVFS_L1) ? 0 :
				(level <= SYSC_DVFS_L3) ? 1 : 2;
		}
		break;
	case SYSC_DVFS_MIF:
		if(gasv_table_info.dvfs_asv_table_version <= 6){
			subgrp = (level <= SYSC_DVFS_L3) ? 0 :
				(level <= SYSC_DVFS_L7) ? 1 : 2;
		} else{
			subgrp = (level <= SYSC_DVFS_L1) ? 0 :
				(level <= SYSC_DVFS_L4) ? 1 : 2;
		}
		break;
	case SYSC_DVFS_INT:
		if(gasv_table_info.dvfs_asv_table_version <= 2) {
			subgrp = (level <= SYSC_DVFS_L6) ? 0 :
				(level <= SYSC_DVFS_L8) ? 1 : 2;
		} else if(gasv_table_info.dvfs_asv_table_version >= 7) {
			if(level <= SYSC_DVFS_L0)
				subgrp = 0;
			else if(level >= SYSC_DVFS_L2)
				subgrp = 2;
			else
				subgrp = 1;
		} else{
			if(level <= SYSC_DVFS_L2)
				subgrp = 0;
			else if(level >= SYSC_DVFS_L9)
				subgrp = 2;
			else
				subgrp = 1;
		}
		break;
	case SYSC_DVFS_CAM:
		if (gasv_table_info.dvfs_asv_table_version <= 2) {
			subgrp = (level <= SYSC_DVFS_L5) ? 0 :
				(level <= SYSC_DVFS_L7) ? 1 : 2;
		} else if ((gasv_table_info.dvfs_asv_table_version == 7) ||
				(gasv_table_info.dvfs_asv_table_version == 8)) {
			if ((level ==SYSC_DVFS_L0) || (level == SYSC_DVFS_L5))
				subgrp = 0;
			else if ((level == SYSC_DVFS_L1) || (level == SYSC_DVFS_L6))
				subgrp = 1;
			else
				subgrp = 2;
		} else if (gasv_table_info.dvfs_asv_table_version >= 9) {
			if ((level ==SYSC_DVFS_L0) || (level == SYSC_DVFS_L5))
				subgrp = 0;
			else if((level == SYSC_DVFS_L1) || (level == SYSC_DVFS_L6))
				subgrp = 1;
			else
				subgrp = 2;
		} else {
			if ((level == SYSC_DVFS_L0) || (level == SYSC_DVFS_L5))
				subgrp = 0;
			else if (level >= SYSC_DVFS_L8)
				subgrp = 2;
			else
				subgrp = 1;
		}
		break;
	default:
		Assert(0);
	}

	return subgrp;
}

u32 cal_get_ssa0_volt(u32 id)
{
	u32 ssa0_volt, ssa0_value;

	ssa0_value = gasv_table_info.ssa0_enable;
	if (ssa0_value == 0)
		return 0;

	if (id == SYSC_DVFS_BIG)
		ssa0_value = gasv_table_info.bigcpu_ssa0;
	else if (id == SYSC_DVFS_LIT)
		ssa0_value = gasv_table_info.littlecpu_ssa0;
	else if (id == SYSC_DVFS_G3D)
		ssa0_value = gasv_table_info.g3d_ssa0;
	else if (id == SYSC_DVFS_MIF)
		ssa0_value = gasv_table_info.mif_ssa0;
	else if (id == SYSC_DVFS_INT)
		ssa0_value = gasv_table_info.int_ssa0;
	else if (id == SYSC_DVFS_CAM)
		ssa0_value = gasv_table_info.cam_disp_ssa0;
	else
		return 0;

	if (ssa0_value == 0)
		ssa0_volt = 0;
	else
		ssa0_volt = 550000 + ssa0_value * 25000;

	return ssa0_volt;
}
u32 cal_get_ssa1_volt(u32 id, u32 subgrp,u32 volt)
{
	u32 ssa1_volt = 0, ssa1_value = 0;

	ssa1_value = gasv_table_info.ssa1_enable;
	if (ssa1_value == 0)
		return 0;

	if (id == SYSC_DVFS_BIG)
		ssa1_value = gasv_table_info.bigcpu_ssa1;
	else if (id == SYSC_DVFS_LIT)
		ssa1_value = gasv_table_info.littlecpu_ssa1;
	else if (id == SYSC_DVFS_G3D)
		ssa1_value = gasv_table_info.g3d_ssa1;
	else if (id == SYSC_DVFS_MIF)
		ssa1_value = gasv_table_info.mif_ssa1;
	else if (id == SYSC_DVFS_INT)
		ssa1_value = gasv_table_info.int_ssa1;
	else if (id == SYSC_DVFS_CAM)
		ssa1_value = gasv_table_info.cam_disp_ssa1;
	else
		return 0;

	if(subgrp == 0) {
		ssa1_volt = 12500 + (ssa1_value & 0x3) * 12500;
	} else if(subgrp == 1) {
		ssa1_volt = 12500 + ((ssa1_value & 0xc) >> 2 ) * 12500;
	}

	ssa1_volt = volt + ssa1_volt;

	return ssa1_volt;
}

u32 cal_get_ssa_volt(u32 id, s32 level,u32 volt)
{
	u32 subgrp, ssa_volt = 0;
	if(gasv_table_info.asv_group_type == 1)
		return 0;
	if (cal_is_fused_speed_grp())	{
		subgrp = cal_get_match_subgrp(id, level);
		if(subgrp < 2)	{
			ssa_volt = cal_get_ssa1_volt(id,subgrp,volt);
		}
		else	{
			ssa_volt = cal_get_ssa0_volt(id);
		}
	}
	return ssa_volt;
}

u32 cal_get_asv_grp(u32 id, s32 level)
{
	u32 subgrp;
	int asv_group = 0;

	if (cal_is_fused_speed_grp()) {
		if (gasv_table_info.asv_group_type == 1) {
			asv_group = gasv_table_info.main_asv_group;

			/* ASV group shift */
			if (gasv_table_info.main_asv_ssa_minus_sign == 1) {
				asv_group -= (int)gasv_table_info.main_asv_ssa;
			} else {
				asv_group += gasv_table_info.main_asv_ssa;
			}

			if(asv_group < 0)
				asv_group = 0;
			if(asv_group >= MAX_ASV_GROUP)
				asv_group = MAX_ASV_GROUP - 1;

		} else {
			subgrp = cal_get_match_subgrp(id, level);
			subgrp = subgrp * 4;

			if (id == SYSC_DVFS_BIG)
				asv_group = (gasv_table_info.bigcpu_asv_group >> subgrp) & 0xF;
			else if (id == SYSC_DVFS_LIT)
				asv_group = (gasv_table_info.littlecpu_asv_group >> subgrp) & 0xF;
			else if (id == SYSC_DVFS_G3D)
				asv_group = (gasv_table_info.g3d_asv_group >> subgrp) & 0xF;
			else if (id == SYSC_DVFS_MIF)
				asv_group = (gasv_table_info.mif_asv_group >> subgrp) & 0xF;
			else if (id == SYSC_DVFS_INT)
				asv_group = (gasv_table_info.int_asv_group >> subgrp) & 0xF;
			else if (id == SYSC_DVFS_CAM)
				asv_group = (gasv_table_info.cam_disp_asv_group >> subgrp) & 0xF;
			else
				asv_group = 0;
		}
	} else {
		asv_group = cal_get_ids_hpm_group();
	}

	if (asv_group > MAX_ASV_GROUP)
		return 0;

	return asv_group;
}

u32 cal_get_volt(u32 id, s32 level)
{
	u32 volt, ssa_volt;
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
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v0[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v0[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v0[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v0[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v0[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v0[idx] :
				NULL);
	} else if(table_ver == 1) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v1[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v1[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v1[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v1[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v1[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v1[idx] :
				NULL);
	}  else if(table_ver == 2) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v2[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v2[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v2[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v2[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v2[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v2[idx] :
				NULL);
	} else if(table_ver == 3) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v3[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v3[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v3[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v3[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v3[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v3[idx] :
				NULL);
	} else if(table_ver == 4) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v4[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v4[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v4[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v4[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v4[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v4[idx] :
				NULL);
	} else if(table_ver == 5) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v5[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v5[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v5[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v5[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v5[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v5[idx] :
				NULL);
	} else if(table_ver == 6) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v6[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v6[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v6[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v6[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v6[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v6[idx] :
				NULL);
	} else if(table_ver == 7) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v7[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v7[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v7[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v7[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v7[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v7[idx] :
				NULL);
	} else if(table_ver == 8) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v8[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v8[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v8[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v8[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v8[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v8[idx] :
				NULL);
	} else if(table_ver == 9) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v9[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v9[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v9[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v9[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v9[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v9[idx] :
				NULL);
	} else if(table_ver == 10) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v10[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v10[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v10[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v10[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v10[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v10[idx] :
				NULL);
	} else if(table_ver == 11) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v11[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v11[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v11[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v11[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v11[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v11[idx] :
				NULL);
	} else if(table_ver == 15) {
		p_table = ((id == SYSC_DVFS_BIG) ? volt_table_big_asv_v15[idx] :
				(id == SYSC_DVFS_LIT) ? volt_table_lit_asv_v15[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_asv_v15[idx] :
				(id == SYSC_DVFS_MIF) ? volt_table_mif_asv_v15[idx] :
				(id == SYSC_DVFS_INT) ? volt_table_int_asv_v15[idx] :
				(id == SYSC_DVFS_CAM) ? volt_table_cam_asv_v15[idx] :
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

	ssa_volt = cal_get_ssa_volt(id,level,volt);

	if (ssa_volt > volt)
		volt = ssa_volt;

	return volt;
}

u32 cal_get_freq(u32 id, s32 level)
{
	u32 freq = 0;
	switch (id) {
	case SYSC_DVFS_BIG:
		freq = volt_table_big_asv_v0[level][0];
		break;
	case SYSC_DVFS_LIT:
		freq = volt_table_lit_asv_v0[level][0];
		break;
	case SYSC_DVFS_G3D:
		freq = volt_table_g3d_asv_v0[level][0];
		break;
	case SYSC_DVFS_MIF:
		freq = volt_table_mif_asv_v0[level][0];
		break;
	case SYSC_DVFS_INT:
		freq = volt_table_int_asv_v0[level][0];
		break;
	case SYSC_DVFS_CAM:
		freq = volt_table_cam_asv_v0[level][0];
		break;
	default:
		freq = 0;
	}
	return freq;
}

void cal_set_rcc_limit(void)
{
	u32 table_ver = cal_get_table_ver();

	__raw_writel(gasv_table_info.rcc_hpm_min_max0, EXYNOS_MAILBOX_RCC_MIN_MAX(0));
	__raw_writel(gasv_table_info.rcc_hpm_min_max1, EXYNOS_MAILBOX_RCC_MIN_MAX(1));
	__raw_writel(gasv_table_info.rcc_hpm_min_max2, EXYNOS_MAILBOX_RCC_MIN_MAX(2));
	__raw_writel(gasv_table_info.rcc_hpm_min_max3, EXYNOS_MAILBOX_RCC_MIN_MAX(3));
	__raw_writel(gasv_table_info.rcc_hpm_min_max4, EXYNOS_MAILBOX_RCC_MIN_MAX(4));
	__raw_writel(gasv_table_info.rcc_hpm_min_max5, EXYNOS_MAILBOX_RCC_MIN_MAX(5));
	__raw_writel(gasv_table_info.rcc_hpm_min_max6, EXYNOS_MAILBOX_RCC_MIN_MAX(6));
	__raw_writel(gasv_table_info.rcc_hpm_min_max7, EXYNOS_MAILBOX_RCC_MIN_MAX(7));
	__raw_writel(table_ver, EXYNOS_MAILBOX_ASV_TABLE);
}

u32 cal_set_rcc(u32 id, s32 level, u32 rcc)
{
	switch (id) {
	case SYSC_DVFS_BIG:
		__raw_writel(rcc, EXYNOS_MAILBOX_ATLAS_RCC(level));
		break;
	case SYSC_DVFS_LIT:
		__raw_writel(rcc, EXYNOS_MAILBOX_APOLLO_RCC(level));
		break;
	case SYSC_DVFS_G3D:
		__raw_writel(rcc, EXYNOS_MAILBOX_G3D_RCC(level));
		break;
	case SYSC_DVFS_MIF:
		__raw_writel(rcc, EXYNOS_MAILBOX_MIF_RCC(level));
		break;
	case SYSC_DVFS_INT:
	case SYSC_DVFS_CAM:
		break;
	default:
		return -1;
	}
	return 0;
}

u32 cal_get_rcc(u32 id, s32 level)
{
	u32 rcc;
	u32 asvgrp;
	u32 minlvl = cal_get_min_lv(id);
	const u32 *p_table;
	u32 idx;
	u32 table_ver;

	if (id == SYSC_DVFS_INT || id == SYSC_DVFS_CAM)
		return 0;

	if (level >= minlvl)
		level = minlvl;

	idx = level;

	table_ver = cal_get_table_ver();
	if (table_ver == 0) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v00[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v00[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v00[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v00[idx] :
				NULL);
	} else if (table_ver == 1) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v01[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v01[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v01[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v01[idx] :
				NULL);
	} else if (table_ver == 2) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v02[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v02[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v02[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v02[idx] :
				NULL);
	} else if (table_ver == 3) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v03[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v03[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v03[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v03[idx] :
				NULL);
	} else if (table_ver == 4) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v04[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v04[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v04[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v04[idx] :
				NULL);
	} else if (table_ver == 5) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v05[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v05[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v05[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v05[idx] :
				NULL);
	} else if (table_ver == 6) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v06[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v06[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v06[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v06[idx] :
				NULL);
	} else if (table_ver == 7) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v07[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v07[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v07[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v07[idx] :
				NULL);
	} else if (table_ver == 8) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v08[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v08[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v08[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v08[idx] :
				NULL);
	} else if (table_ver == 9) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v09[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v09[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v09[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v09[idx] :
				NULL);
	} else if (table_ver == 10) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v10[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v10[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v10[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v10[idx] :
				NULL);
	} else if (table_ver == 11) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v11[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v11[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v11[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v11[idx] :
				NULL);
	} else if (table_ver == 15) {
		p_table = ((id == SYSC_DVFS_BIG) ? rcc_table_big_asv_v15[idx] :
				(id == SYSC_DVFS_LIT) ? rcc_table_lit_asv_v15[idx] :
				(id == SYSC_DVFS_G3D) ? rcc_table_g3d_asv_v15[idx] :
				(id == SYSC_DVFS_MIF) ? rcc_table_mif_asv_v15[idx] :
				NULL);
	} else {
		return 0;
	}

	if (p_table == NULL) {
		pr_info("%s : RCC table pointer is NULL\n", __func__);
		return 0;
	}

	asvgrp = cal_get_asv_grp(id, level);
	rcc = p_table[asvgrp + 1];

	return rcc;
}

bool cal_use_dynimic_abb(u32 id)
{
	return use_dynimic_abb[id];
}

bool cal_use_dynimic_ema(u32 id)
{
	return false;
}

void cal_set_ema(u32 id, u32 setvolt)
{
	u32 tmp, value, value1;
	if (id == SYSC_DVFS_BIG)
	{
		if(setvolt >= 1000000)
		{
			if(gasv_table_info.atlas_l1_ema_c_vddpe_1_1V == 0)
				value = 6;		// 0x62
			else
				value = gasv_table_info.atlas_l1_ema_c_vddpe_1_1V;

			if(gasv_table_info.atlas_l2_ema_l2d_vddpe_1_1V == 0)
				value1 = 5;	// 0x530
			else
				value1 = gasv_table_info.atlas_l2_ema_l2d_vddpe_1_1V;
		}
		else if((setvolt >= 900000) && (setvolt < 1000000))
		{
			if(gasv_table_info.atlas_l1_ema_c_vddpe_0_9V == 0)
				value = 6;		// 0x62
			else
				value = gasv_table_info.atlas_l1_ema_c_vddpe_0_9V;

			if(gasv_table_info.atlas_l2_ema_l2d_vddpe_0_9V == 0)
				value1 = 5;	// 0x530
			else
				value1 = gasv_table_info.atlas_l2_ema_l2d_vddpe_0_9V;
		}
		else if((setvolt >= 800000) && (setvolt < 900000))
		{
			if(gasv_table_info.atlas_l1_ema_c_vddpe_0_8V == 0)
				value = 8;			// 0x82
			else
				value = gasv_table_info.atlas_l1_ema_c_vddpe_0_8V;

			if(gasv_table_info.atlas_l2_ema_l2d_vddpe_0_8V == 0)
				value1 = 0xb;		//0xb30
			else
				value1 = gasv_table_info.atlas_l2_ema_l2d_vddpe_0_8V;
		}
		else if(setvolt < 800000)
		{
			if(gasv_table_info.atlas_l1_ema_c_vddpe_0_7V == 0)
				value = 7;			// 0x72
			else
				value = gasv_table_info.atlas_l1_ema_c_vddpe_0_7V;

			if(gasv_table_info.atlas_l2_ema_l2d_vddpe_0_7V == 0)
				value1 = 0x9;		//0x930
			else
				value1 = gasv_table_info.atlas_l2_ema_l2d_vddpe_0_7V;
		}
		 tmp = 0x2;
		 tmp &= ~(0xf << 4);
		 tmp |= (value << 4);
		__raw_writel(tmp, SYSREG_BASE + 0x138);

		 tmp = 0x30;
		 tmp &= ~(0xf << 8);
		 tmp |= (value1 << 8);
		__raw_writel(tmp, SYSREG_BASE + 0x2908);
	}
	else if (id == SYSC_DVFS_LIT)
	{
		if(setvolt >= 1000000)
		{
			if(gasv_table_info.apollo_ema_vddpe_1_0V == 0)
				value = 2;			// 0x210211
			else
				value = gasv_table_info.apollo_ema_vddpe_1_0V;
		}
		else if((setvolt >= 900000) && (setvolt < 1000000))
		{
			if(gasv_table_info.apollo_ema_vddpe_0_9V == 0)
				value = 2;			// 0x210211
			else
				value = gasv_table_info.apollo_ema_vddpe_0_9V;
		}
		else if((setvolt >= 800000) && (setvolt < 900000))
		{
			if(gasv_table_info.apollo_ema_vddpe_0_8V == 0)
				value = 2;			// 0x210211
			else
				value = gasv_table_info.apollo_ema_vddpe_0_8V;
		}
		else if(setvolt < 800000)
		{
			if(gasv_table_info.apollo_ema_vddpe_0_7V == 0)
				value = 2;			// 0x210211
			else
				value = gasv_table_info.apollo_ema_vddpe_0_7V;
		}
		tmp = 0x210211;			//default value
		tmp &= ~( (0x7 << 20) |(0x7 << 8) );
		tmp |= ((value << 20) |(value << 8));
		__raw_writel(tmp, SYSREG_BASE + 0x38);
	}
	else if (id == SYSC_DVFS_G3D)
	{
		if(setvolt >= 900000)
		{
			if(gasv_table_info.g3d_mcs_0_9V == 0)
				value = 1;			// 0x11
			else
				value = gasv_table_info.g3d_mcs_0_9V;
		}
		else if((setvolt >= 800000) && (setvolt<900000))
		{
			if(gasv_table_info.g3d_mcs_0_8V == 0)
				value = 1;			// 0x11
			else
				value = gasv_table_info.g3d_mcs_0_8V;
		}
		else if((setvolt >= 700000) && (setvolt < 800000))
		{
			if(gasv_table_info.g3d_mcs_0_7V == 0)
				value = 1;			// 0x11
			else
				value = gasv_table_info.g3d_mcs_0_7V;
		}
		else if(setvolt < 700000)
		{
			if(gasv_table_info.g3d_mcs_0_59V == 0)
				value = 3;			// 0x11
			else
				value = gasv_table_info.g3d_mcs_0_59V;
		}
		tmp = 0x11;			//default value
		tmp &= ~(0x3 << 4);
		tmp |= (value << 4);
		__raw_writel(tmp, SYSREG_BASE + 0x1224);
	}
	else if (id == SYSC_DVFS_INT)
	{
		if(gasv_table_info.int_ema_vddpe == 0)
			value = 2;			// 0x12
		else
			value = gasv_table_info.int_ema_vddpe;

		tmp = 0x12;			//default value
		tmp &= ~(0x7 << 0);
		tmp |= (value << 0);
		__raw_writel(tmp, SYSREG_BASE + 0x2758);
	}
	else if (id == SYSC_DVFS_CAM)
	{
		if(gasv_table_info.cam_ema_vddpe == 0)
			value = 2;			// 0x12
		else
			value = gasv_table_info.cam_ema_vddpe;

		tmp = 0x12;			//default value
		tmp &= ~(0x7 << 0);
		tmp |= (value << 0);
		__raw_writel(tmp, SYSREG_BASE + 0x2718);
	}
	else
		return;
}

u32 cal_get_abb(u32 id, s32 level)
{
	return 0;
}

void cal_set_abb(u32 id, u32 abb)
{
}

u32 cal_get_asv_info(int id)
{
	u32 temp = 0x0;
	temp = gasv_table_info.atlas_vthr |
		gasv_table_info.atlas_delta << 2 |
		gasv_table_info.apollo_vthr << 4 |
		gasv_table_info.apollo_delta << 6 |
		gasv_table_info.g3d_vthr << 8 |
		gasv_table_info.g3d_delta << 10 |
		gasv_table_info.int_vthr << 12 |
		gasv_table_info.int_delta << 14;

		return temp;
}
