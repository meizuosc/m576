/* linux/arch/arm/plat-s5p/include/plat/fimc_is.h
 *
 * Copyright (C) 2011 Samsung Electronics, Co. Ltd
 *
 * Exynos 4 series FIMC-IS slave device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef EXYNOS_FIMC_IS_H_
#define EXYNOS_FIMC_IS_H_

#include <linux/platform_device.h>
#include <linux/clk-private.h>

#define CLK_NUM 104
extern char *clk_g_list[CLK_NUM];
extern struct clk *clk_target_list[CLK_NUM];

/* FIMC-IS DVFS LEVEL enum (INT, MIF, I2C) */
enum FIMC_IS_INT_LV {
	FIMC_IS_INT_L0,
	FIMC_IS_INT_L1,
	FIMC_IS_INT_L1_1,
	FIMC_IS_INT_L1_2,
	FIMC_IS_INT_L1_3,
};

enum FIMC_IS_MIF_LV {
	FIMC_IS_MIF_L0,
	FIMC_IS_MIF_L1,
	FIMC_IS_MIF_L2,
	FIMC_IS_MIF_L3,
	FIMC_IS_MIF_L4,
};

/*
 * On some soc, It needs to notify change of INT clock to F/W.
 * Because I2C clock can be take affect from other clock change(like INT)
 */
enum FIMC_IS_I2C_LV {
	FIMC_IS_I2C_L0,
	FIMC_IS_I2C_L1,
	FIMC_IS_I2C_L1_1,
	FIMC_IS_I2C_L1_3,
};

/* dvfs table idx ex.different dvfa table  pure bayer or dynamic bayer */
#define FIMC_IS_DVFS_TABLE_IDX_MAX 2

/* FIMC-IS DVFS SCENARIO enum */
enum FIMC_IS_SCENARIO_ID {
	FIMC_IS_SN_DEFAULT,
	FIMC_IS_SN_FRONT_PREVIEW,
	FIMC_IS_SN_FRONT_CAPTURE,
	FIMC_IS_SN_FRONT_CAMCORDING,
	FIMC_IS_SN_FRONT_CAMCORDING_WHD,
	FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE,
	FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE,
	FIMC_IS_SN_FRONT_VT1,
	FIMC_IS_SN_FRONT_VT2,
	FIMC_IS_SN_REAR_PREVIEW_FHD,
	FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF,
	FIMC_IS_SN_REAR_PREVIEW_WHD,
	FIMC_IS_SN_REAR_PREVIEW_UHD,
	FIMC_IS_SN_REAR_CAPTURE,
	FIMC_IS_SN_REAR_CAMCORDING_FHD,
	FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF,
	FIMC_IS_SN_REAR_CAMCORDING_WHD,
	FIMC_IS_SN_REAR_CAMCORDING_UHD,
	FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE,
	FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE,
	FIMC_IS_SN_DUAL_PREVIEW,
	FIMC_IS_SN_DUAL_CAPTURE,
	FIMC_IS_SN_DUAL_CAMCORDING,
	FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE,
#if defined(CONFIG_SOC_EXYNOS7420)
	FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS,
	FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS,
	FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS,
	FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS,
	FIMC_IS_SN_FRONT_COMPANION_PREVIEW,
	FIMC_IS_SN_FRONT_COMPANION_CAPTURE,
	FIMC_IS_SN_FRONT_COMPANION_CAMCORDING,
	FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD,
	FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE,
	FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE,
	#if defined(CONFIG_CAMERA_CUSTOM_SUPPORT)
	FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS,
	#endif
#else
	FIMC_IS_SN_HIGH_SPEED_FPS,
#endif
	FIMC_IS_SN_DIS_ENABLE,
	FIMC_IS_SN_MAX,
	FIMC_IS_SN_END,
};

enum FIMC_IS_DVFS_QOS_TYPE {
	FIMC_IS_DVFS_CPU_MIN,
	FIMC_IS_DVFS_CPU_MAX,
	FIMC_IS_DVFS_INT,
	FIMC_IS_DVFS_MIF,
	FIMC_IS_DVFS_I2C,
	FIMC_IS_DVFS_CAM,
	FIMC_IS_DVFS_DISP,
	FIMC_IS_DVFS_PWM,
	FIMC_IS_DVFS_END,
};

#define SET_QOS_WITH_CPU(t, s, i, m, _i, c, d, p, cmin, cmax)	\
	(t)[s][FIMC_IS_DVFS_INT]	= i;	\
	(t)[s][FIMC_IS_DVFS_MIF]	= m;	\
	(t)[s][FIMC_IS_DVFS_I2C]	= _i;	\
	(t)[s][FIMC_IS_DVFS_CAM]	= c;	\
	(t)[s][FIMC_IS_DVFS_DISP]	= d;	\
	(t)[s][FIMC_IS_DVFS_PWM]	= p;	\
	(t)[s][FIMC_IS_DVFS_CPU_MIN]	= cmin;	\
	(t)[s][FIMC_IS_DVFS_CPU_MAX]	= cmax;

#define SET_QOS(t, s, i, m, _i, c, d, p)	\
	(t)[s][FIMC_IS_DVFS_INT]	= i;	\
	(t)[s][FIMC_IS_DVFS_MIF]	= m;	\
	(t)[s][FIMC_IS_DVFS_I2C]	= _i;	\
	(t)[s][FIMC_IS_DVFS_CAM]	= c;	\
	(t)[s][FIMC_IS_DVFS_DISP]	= d;	\
	(t)[s][FIMC_IS_DVFS_PWM]	= p;	\
	(t)[s][FIMC_IS_DVFS_CPU_MIN]	= 0;	\
	(t)[s][FIMC_IS_DVFS_CPU_MAX]	= 0;

enum FIMC_IS_CLK_GATE {
	FIMC_IS_GATE_3AA1_IP,
	FIMC_IS_GATE_ISP_IP,
	FIMC_IS_GATE_DRC_IP,
	FIMC_IS_GATE_SCC_IP,
	FIMC_IS_GATE_ODC_IP,
	FIMC_IS_GATE_DIS_IP,
	FIMC_IS_GATE_3DNR_IP,
	FIMC_IS_GATE_SCP_IP,
	FIMC_IS_GATE_FD_IP,
	FIMC_IS_GATE_3AA0_IP,
	FIMC_IS_GATE_ISP1_IP,
	FIMC_IS_GATE_TPU_IP,
	FIMC_IS_GATE_VRA_IP,
	FIMC_IS_CLK_GATE_MAX,
};

enum FIMC_IS_GRP {
	FIMC_IS_GRP_3A0,
	FIMC_IS_GRP_3A1,
	FIMC_IS_GRP_ISP,
	FIMC_IS_GRP_DIS,
	FIMC_IS_GRP_MAX,
};

enum FIMC_IS_CLK_GATE_USR_SCENARIO {
	CLK_GATE_NOT_FULL_BYPASS_SN = 1,
	CLK_GATE_FULL_BYPASS_SN,
	CLK_GATE_DIS_SN,
};

#if !defined(CONFIG_SOC_EXYNOS7420)
/*
 * struct exynos_fimc_is_subip_info
 *   This info describes what and how many subip fimc-is has.
 *   In machine data, this info should be defined.
 */
struct exynos_fimc_is_subip_ext {
	bool valid;
	bool full_bypass;
	u32 version;
	u32 base_addr;
};

struct exynos_fimc_is_subip_info {
	/* You can add more new subip */
	struct exynos_fimc_is_subip_ext _mcuctl;
	struct exynos_fimc_is_subip_ext _3a0;
	struct exynos_fimc_is_subip_ext _3a1;
	struct exynos_fimc_is_subip_ext _isp;
	struct exynos_fimc_is_subip_ext _drc;
	struct exynos_fimc_is_subip_ext _scc;
	struct exynos_fimc_is_subip_ext _odc;
	struct exynos_fimc_is_subip_ext _dis;
	struct exynos_fimc_is_subip_ext _dnr;
	struct exynos_fimc_is_subip_ext _scp;
	struct exynos_fimc_is_subip_ext _fd;
	struct exynos_fimc_is_subip_ext _pwm;
};
#endif

/*
 * struct exynos_fimc_is_clk_gate_group
 *	This struct is for host clock gating.
 * 	It decsribes register, masking bit info and other control for each group.
 *	If you uses host clock gating, You must define this struct in exynos_fimc_is_clk_gate_info.
 */
struct exynos_fimc_is_clk_gate_group {
	u32	mask_clk_on_org;	/* masking value in clk on */
	u32	mask_clk_on_mod;	/* masking value in clk on */
	u32	mask_clk_off_self_org;	/* masking value in clk off(not depend) original */
	u32	mask_clk_off_self_mod;	/* masking value in clk off(not depend) final */
	u32	mask_clk_off_depend;	/* masking value in clk off(depend on other grp) */
	u32	mask_cond_for_depend;	/* masking group having dependancy for other */
};

/*
 * struct exynos_fimc_is_clk_gate_info
 * 	This struct is for host clock gating.
 * 	It has exynos_fimc_is_clk_gate_group to control each group's clk gating.
 * 	And it has function pointer to include user scenario masking
 */
struct exynos_fimc_is_clk_gate_info {
	const char *gate_str[FIMC_IS_CLK_GATE_MAX];			/* register adr for gating */
	struct exynos_fimc_is_clk_gate_group groups[FIMC_IS_GRP_MAX];
	/* You must set this function pointer (on/off) */
	int (*clk_on_off)(u32 clk_gate_id, bool is_on);
	/*
	 * if there are specific scenarios for clock gating,
	 * You can define user function.
	 * user_scenario_id will be in
	 */
	int (*user_clk_gate)(u32 group_id,
			bool is_on,
			u32 user_scenario_id,
			unsigned long msk_state,
			struct exynos_fimc_is_clk_gate_info *gate_info);
};

/**
* struct exynos_platform_fimc_is - camera host interface platform data
*
* @isp_info: properties of camera sensor required for host interface setup
*/
struct exynos_platform_fimc_is {
	int hw_ver;
	bool clock_on;
#if !defined(CONFIG_SOC_EXYNOS7420)
	struct exynos_fimc_is_subip_info *subip_info;
#endif
	int (*clk_cfg)(struct platform_device *pdev);
	int (*clk_on)(struct platform_device *pdev);
	int (*clk_off)(struct platform_device *pdev);
	int (*print_clk)(struct platform_device *pdev);
	int (*print_cfg)(struct platform_device *pdev, u32 channel);
#if !defined(CONFIG_SOC_EXYNOS7420)
	int (*print_pwr)(struct platform_device *pdev);
#endif
	/* These fields are to return qos value for dvfs scenario */
	u32 *int_qos_table;
	u32 *mif_qos_table;
	u32 *i2c_qos_table;
	int (*get_int_qos)(int scenario_id);
	int (*get_mif_qos)(int scenario_id);
	int (*get_i2c_qos)(int scenario_id);
	u32 dvfs_data[FIMC_IS_DVFS_TABLE_IDX_MAX][FIMC_IS_SN_END][FIMC_IS_DVFS_END];
	u32 dvfs_hal_1_0_table_idx;
	u32 dvfs_hal_3_2_table_idx;

	/* For host clock gating */
	struct exynos_fimc_is_clk_gate_info *gate_info;
	u32 rear_sensor_id;
	u32 front_sensor_id;
	bool check_sensor_vendor;
	bool skip_cal_loading;
#ifdef CONFIG_OIS_USE
	bool use_ois;
#endif /* CONFIG_OIS_USE */
	bool use_ois_hsi2c;
	bool use_module_check;
};

extern struct device *fimc_is_dev;

int fimc_is_set_parent_dt(struct platform_device *pdev, const char *child, const char *parent);
int fimc_is_set_rate_dt(struct platform_device *pdev, const char *conid, unsigned int rate);
int fimc_is_get_rate_dt(struct platform_device *pdev, const char *conid);
int fimc_is_enable_dt(struct platform_device *pdev, const char *conid);
int fimc_is_disable_dt(struct platform_device *pdev, const char *conid);

int fimc_is_set_rate(struct platform_device *pdev, const char *conid, unsigned int rate);
int fimc_is_get_rate(struct platform_device *pdev, const char *conid);
int fimc_is_enable(struct platform_device *pdev, const char *conid);
int fimc_is_disable(struct platform_device *pdev, const char *conid);

/* platform specific clock functions */
extern int exynos_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos_fimc_is_clk_off(struct platform_device *pdev);
extern int exynos_fimc_is_print_clk(struct platform_device *pdev);
extern int exynos_fimc_is_clk_gate(u32 clk_gate_id, bool is_on);

#if !defined(CONFIG_SOC_EXYNOS7420)
extern int exynos_fimc_is_set_user_clk_gate(u32 group_id, bool is_on, u32 user_scenario_id,
											unsigned long msk_state,
											struct exynos_fimc_is_clk_gate_info *gate_info);
extern int exynos_fimc_is_print_cfg(struct platform_device *pdev, u32 channel);
extern int exynos_fimc_is_print_pwr(struct platform_device *pdev);
#endif
#endif /* EXYNOS_FIMC_IS_H_ */
