/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *              Sangkyu Kim(skwith.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DEVFREQ_EXYNOS_H
#define __DEVFREQ_EXYNOS_H __FILE__

#include <linux/clk.h>

#include <mach/pm_domains.h>

#define VOLT_STEP	(12500)

struct devfreq_data_int;
struct devfreq_data_mif;
struct devfreq_data_isp;
struct devfreq_data_disp;
typedef int(*devfreq_init_of_fn)(void *);
typedef int(*devfreq_deinit_of_fn)(void *);

struct devfreq_opp_table {
	unsigned int idx;
	unsigned long freq;
	unsigned long volt;
};

struct devfreq_clk_state {
	int clk_idx;
	int parent_clk_idx;
};

struct devfreq_clk_states {
	struct devfreq_clk_state *state;
	unsigned int state_count;
};

struct devfreq_clk_value {
	unsigned int reg;
	unsigned int set_value;
	unsigned int clr_value;
};

struct devfreq_clk_info {
	unsigned int idx;
	unsigned long freq;
	int pll;
	struct devfreq_clk_states *states;
};

struct devfreq_clk_list {
	const char *clk_name;
	struct clk *clk;
};

struct exynos_devfreq_platdata {
	unsigned long default_qos;
};

struct devfreq_info {
	unsigned int old;
	unsigned int new;
};

struct devfreq_pm_domain_link {
	const char *pm_domain_name;
	struct exynos_pm_domain *pm_domain;
};

struct devfreq_dynamic_clkgate {
	unsigned long	paddr;
	unsigned long	vaddr;
	unsigned int	bit;
	unsigned long	freq;
};

#if defined(CONFIG_ARM_EXYNOS5433_BUS_DEVFREQ)
struct devfreq_data_mif {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_mif;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	struct notifier_block tmu_notifier;

	bool use_dvfs;

	void __iomem *base_mif;
	void __iomem *base_sysreg_mif;
	void __iomem *base_drex0;
	void __iomem *base_drex1;
	void __iomem *base_lpddr_phy0;
	void __iomem *base_lpddr_phy1;

	int default_qos;
	int initial_freq;
	int cal_qos_max;
	int max_state;
	bool dll_status;

	unsigned int *mif_asv_abb_table;

	int (*mif_set_freq)(struct devfreq_data_mif *data, int index, int old_index);
	int (*mif_set_and_change_timing_set)(struct devfreq_data_mif *data, int index);
	int (*mif_set_timeout)(struct devfreq_data_mif *data, int index);
	int (*mif_set_dll)(struct devfreq_data_mif *data, unsigned long volt, int index);
	void  (*mif_dynamic_setting)(struct devfreq_data_mif *data, bool flag);
	int (*mif_set_volt)(struct devfreq_data_mif *data, unsigned long volt,  unsigned long volt_range);
	int (*mif_pre_process)(struct device *dev, struct devfreq_data_mif *data, int *index, int *old_index, unsigned long *freq, unsigned long *old_freq);
	int (*mif_post_process)(struct device *dev, struct devfreq_data_mif *data, int *index, int *old_index, unsigned long *freq, unsigned long *old_freq);
};

struct devfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_int;
	struct regulator *vdd_int_m;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	unsigned long initial_freq;
	unsigned long default_qos;
	int max_state;

	unsigned int *int_asv_abb_table;

	unsigned long target_volt;
	unsigned long volt_constraint_isp;
	unsigned int use_dvfs;

	struct notifier_block tmu_notifier;

	int (*int_set_freq)(struct devfreq_data_int *data, int index, int old_index);
	int (*int_set_volt)(struct devfreq_data_int *data, unsigned long volt,  unsigned long volt_range);
};

struct devfreq_data_disp {
	struct device *dev;
	struct devfreq *devfreq;

	struct mutex lock;
	unsigned int use_dvfs;
	unsigned long initial_freq;
	unsigned long default_qos;
	int max_state;

	int (*disp_set_freq)(struct devfreq_data_disp *data, int index, int old_index);
	int (*disp_set_volt)(struct devfreq_data_disp *data, unsigned long volt,  unsigned long volt_range);
};

struct devfreq_data_isp {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_isp;
	unsigned long old_volt;
	unsigned long volt_offset;
	int max_state;

	struct mutex lock;
	unsigned int use_dvfs;
	unsigned long initial_freq;
	unsigned long default_qos;
	struct notifier_block tmu_notifier;

	int (*isp_set_freq)(struct devfreq_data_isp *data, int index, int old_index);
	int (*isp_set_volt)(struct devfreq_data_isp *data, unsigned long volt,  unsigned long volt_range, bool tolower);
};

struct devfreq_thermal_work {
	struct delayed_work devfreq_mif_thermal_work;
	int channel;
	struct workqueue_struct *work_queue;
	unsigned int thermal_level_cs0;
	unsigned int thermal_level_cs1;
	unsigned int polling_period;
	unsigned long max_freq;
};

#define CTRL_LOCK_VALUE_SHIFT	(0x8)
#define CTRL_LOCK_VALUE_MASK	(0x1FF)
#define CTRL_FORCE_SHIFT	(0x7)
#define CTRL_FORCE_MASK		(0x1FF)
#define CTRL_FORCE_OFFSET	(8)

#define MIF_VOLT_STEP		(12500)
#define COLD_VOLT_OFFSET	(37500)
#define LIMIT_COLD_VOLTAGE	(1250000)

unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset);
int exynos5_devfreq_get_idx(struct devfreq_opp_table *table, unsigned int size, unsigned long freq);

int exynos5433_devfreq_mif_init(struct devfreq_data_mif *data);
int exynos5433_devfreq_mif_deinit(struct devfreq_data_mif *data);
int exynos5433_devfreq_int_init(struct devfreq_data_int *data);
int exynos5433_devfreq_int_deinit(struct devfreq_data_int *data);
int exynos5433_devfreq_disp_init(struct devfreq_data_disp *data);
int exynos5433_devfreq_disp_deinit(struct devfreq_data_disp *data);
int exynos5433_devfreq_isp_init(struct devfreq_data_isp *data);
int exynos5433_devfreq_isp_deinit(struct devfreq_data_isp *data);
#elif defined(CONFIG_ARM_EXYNOS7420_BUS_DEVFREQ)
#define MIF_BLK_NUM	4
#define REGULATOR_MIN_MICROVOLT        500000
#define REGULATOR_MAX_MICROVOLT        1000000
struct devfreq_data_mif {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_mif;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;
	struct notifier_block tmu_notifier;
	bool use_dvfs;

	void __iomem *base_cmu_topc;
	void __iomem *base_cmu_mif[MIF_BLK_NUM];
	void __iomem *base_sysreg_mif;
	void __iomem *base_drex[MIF_BLK_NUM];
	void __iomem *base_lp4_phy[MIF_BLK_NUM];
	void __iomem *base_vt_mon_mif[MIF_BLK_NUM];
	void __iomem *base_nsp;

	int default_qos;
	int initial_freq;
	int cal_qos_max;
	int max_state;
	int volt_of_avail_max_freq;
	unsigned long cur_freq;

	uint32_t tmu_temp;

	uint32_t per_mrs_en;
	uint32_t pll_safe_idx;
	ulong  switching_pll_rate;		/* dout_sclk_bus0_pll_mif(800M) */

	int (*mif_set_freq)(struct devfreq_data_mif *data, int index, int old_index);
	int (*mif_set_timeout)(struct devfreq_data_mif *data, int index);
	int (*mif_set_volt)(struct devfreq_data_mif *data, unsigned long volt,  unsigned long volt_range);
};

struct devfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_int;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	unsigned long initial_freq;
	unsigned long default_qos;
	int max_state;
	int volt_of_avail_max_freq;
	unsigned long cur_freq;

	unsigned long target_volt;
	unsigned int use_dvfs;

	struct notifier_block tmu_notifier;

	int (*int_set_freq)(struct devfreq_data_int *data, int index, int old_index);
	int (*int_set_volt)(struct devfreq_data_int *data, unsigned long volt,  unsigned long volt_range);
	int (*int_get_volt)(struct devfreq_data_int *data);
};

struct devfreq_data_disp {
	struct device *dev;
	struct devfreq *devfreq;

	struct mutex lock;
	unsigned int use_dvfs;
	unsigned long initial_freq;
	unsigned long default_qos;
	int max_state;
	int volt_of_avail_max_freq;
	unsigned long cur_freq;

	struct regulator *vdd_disp_cam0;
	unsigned long old_volt;
	unsigned long volt_offset;
	struct notifier_block tmu_notifier;

	int (*disp_set_freq)(struct devfreq_data_disp *data, int index, int old_index);
	int (*disp_set_volt)(struct devfreq_data_disp *data, unsigned long volt,  unsigned long volt_range);
};

struct devfreq_data_isp {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_disp_cam0;
	unsigned long old_volt;
	unsigned long volt_offset;
	int max_state;
	int volt_of_avail_max_freq;
	unsigned long cur_freq;

	struct mutex lock;
	unsigned int use_dvfs;
	unsigned long initial_freq;
	unsigned long default_qos;
	struct notifier_block tmu_notifier;

	int (*isp_set_freq)(struct devfreq_data_isp *data, int index, int old_index);
	int (*isp_set_volt)(struct devfreq_data_isp *data, unsigned long volt,  unsigned long volt_range);
};

struct devfreq_thermal_work {
	struct delayed_work devfreq_mif_thermal_work;
	int channel;
	struct workqueue_struct *work_queue;
	unsigned int thermal_level_ch0_cs0;
	unsigned int thermal_level_ch0_cs1;
	unsigned int thermal_level_ch1_cs0;
	unsigned int thermal_level_ch1_cs1;
	unsigned int thermal_level_ch2_cs0;
	unsigned int thermal_level_ch2_cs1;
	unsigned int thermal_level_ch3_cs0;
	unsigned int thermal_level_ch3_cs1;
	unsigned int polling_period;
	unsigned long max_freq;
};

#define CTRL_LOCK_VALUE_SHIFT	(0x8)
#define CTRL_LOCK_VALUE_MASK	(0x1FF)
#define CTRL_FORCE_SHIFT	(0x7)
#define CTRL_FORCE_MASK		(0x1FF)
#define CTRL_FORCE_OFFSET	(8)

#define COLD_VOLT_OFFSET	(37500)
#define LIMIT_COLD_VOLTAGE	(1175000)

unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset, unsigned int max_volt);
int get_volt_of_avail_max_freq(struct device *dev);
int devfreq_get_opp_idx(struct devfreq_opp_table *table, unsigned int size, unsigned long freq);

int exynos7420_devfreq_mif_init(struct devfreq_data_mif *data);
int exynos7420_devfreq_mif_deinit(struct devfreq_data_mif *data);
int exynos7420_devfreq_int_init(struct devfreq_data_int *data);
int exynos7420_devfreq_int_deinit(struct devfreq_data_int *data);
int exynos7420_devfreq_disp_init(struct devfreq_data_disp *data);
int exynos7420_devfreq_disp_deinit(struct devfreq_data_disp *data);
int exynos7420_devfreq_isp_init(struct devfreq_data_isp *data);
int exynos7420_devfreq_isp_deinit(struct devfreq_data_isp *data);

#elif defined(CONFIG_ARM_EXYNOS7580_BUS_DEVFREQ)
static const struct of_device_id __devfreq_init_of_table_sentinel
	__used __section(__devfreq_init_of_table_end);

static const struct of_device_id __devfreq_deinit_of_table_sentinel
	__used __section(__devfreq_deinit_of_table_end);

#define DEVFREQ_INIT_OF_DECLARE(name, compat, fn)			   \
	static const struct of_device_id __devfreq_init_of_table_##name     \
		__used __section(__devfreq_init_of_table)		   \
		 = { .compatible = compat,				  \
		     .data = (fn == (devfreq_init_of_fn)NULL) ? fn : fn }

#define DEVFREQ_DEINIT_OF_DECLARE(name, compat, fn)			 \
	static const struct of_device_id __devfreq_deinit_of_table_##name   \
		__used __section(__devfreq_deinit_of_table)		 \
		 = { .compatible = compat,				  \
		     .data = (fn == (devfreq_deinit_of_fn)NULL) ? fn : fn }

extern struct of_device_id __devfreq_init_of_table[];
extern struct devfreq_opp_table devfreq_int_opp_list[];

extern struct of_device_id __devfreq_init_of_table[];
extern struct of_device_id __devfreq_deinit_of_table[];
extern struct devfreq_opp_table devfreq_mif_opp_list[];

extern void exynos7580_devfreq_set_dll_lock_value(struct devfreq_data_mif *, int);
extern struct attribute_group devfreq_mif_attr_group;

extern struct of_device_id __devfreq_init_of_table[];
extern struct devfreq_opp_table devfreq_isp_opp_list[];

extern void exynos7580_devfreq_set_dll_lock_value(struct devfreq_data_mif *, int);
extern void exynos7_devfreq_init_thermal(void);
extern struct attribute_group devfreq_mif_attr_group;

#define REGULATOR_MAX_MICROVOLT	INT_MAX

/* DREX, PHY and PMU Base address */
#define DREX_BASE	0x10400000
#define PHY_BASE	0x10420000
#define CMU_MIF_BASE	0x10430000

/* DREX REGISTER: 0x1040_0000 */
#define DREX_MEMCONTROL	0x4
#define DREX_CGCONTROL	0x8
#define DREX_DIRECTCMD	0x10
#define DREX_TIMINGRFCPB	0x20
#define DREX_TIMINGAREF	0x30
#define DREX_TIMINGROW_0	0x34
#define DREX_TIMINGDATA_0	0x38
#define DREX_TIMINGPOWER_0	0x3C
#define DREX_PHYSTATUS	0x40
#define DREX_RDFETCH_0	0x4C
#define DREX_RDFETCH_1	0x50
#define DREX_MRSTATUS	0x54
#define DREX_TIMINGSETSW	0xE0
#define DREX_TIMINGROW_1	0xE4
#define DREX_TIMINGDATA_1	0xE8
#define DREX_TIMINGPOWER_1	0xEC

/* DREX MASK */
#define DREX_TIMINGRFCPB_SET1_MASK	0x3F00
#define DREX_TIMINGRFCPB_SET0_MASK	0x3F
#define DREX_MRSTATUS_THERMAL_LV_MASK	0x7

/* DREX SHIFT */
#define CG_EN_SHIFT	4
#define TIMING_SET_SW_SHIFT	31

/* DREX SET */
#define PB_REF_EN	BIT(27)

/* PHY Control Register: 0x1042_0000 */
#define PHY_MDLL_CON0	0xB0
#define PHY_MDLL_CON1	0xB4
#define PHY_DVFS_CON0	0xB8
#define PHY_DVFS_CON2	0xBC
#define PHY_DVFS_CON3	0xC0
#define PHY_STATUS	0x40

/* PHY MASK */
#define PHY_CTRL_LOCK_NEW_MASK	0x1FF
#define PHY_CTRL_FORCE_MASK	0x1FF

/* PHY SHIFT */
#define CTRL_LOCK_RDEN_SHIFT	23
#define CTRL_LOCK_NEW_SHIFT	20
#define CTRL_FORCE_SHIFT	7
#define CTRL_DLL_ON_SHIFT	5

/* CMU_MIF Register: 0x10430000 */
#define CLK_DIV_STAT_MIF0	0x0700
#define CLK_MUX_STAT_MIF4	0x0410
#define CLK_MUX_STAT_MIF5	0x0414
#define CLK_PAUSE			0x1008

/* CMU_MIF MASK */
#define CLK_DIV_STAT_MIF0_MASK	0x11
#define CLK_MUX_STAT_MIF4_MASK	0x400040
#define CLK_PAUSE_MASK			0x00070000

/* The OPP table level (DREX_CLK 416MHz) */
#define DLL_LOCK_LV	5

struct devfreq_data_mif {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_mif;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;
	struct notifier_block tmu_notifier;
	bool use_dvfs;

	void __iomem *base_mif;
	void __iomem *base_drex;
	void __iomem *base_lpddr_phy;

	int default_qos;
	int initial_freq;
	int cal_qos_max;
	int max_state;

	uint32_t dll_lock_value;

	uint32_t pll_safe_idx;
	ulong  switching_pll_rate;

	int (*mif_set_freq)(struct devfreq_data_mif *data,
					int index, int old_index);
	int (*mif_set_timeout)(struct devfreq_data_mif *data, int index);
	int (*mif_set_volt)(struct devfreq_data_mif *data,
			unsigned long volt,  unsigned long volt_range);
};

struct devfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_int;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	unsigned long initial_freq;
	unsigned long default_qos;
	int max_state;

	unsigned long target_volt;
	unsigned int use_dvfs;

	struct notifier_block tmu_notifier;

	int (*int_set_freq)(struct devfreq_data_int *data,
					int index, int old_index);
	int (*int_set_volt)(struct devfreq_data_int *data,
				unsigned long volt,  unsigned long volt_range);
};

struct devfreq_data_isp {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_isp_cam0;
	unsigned long old_volt;
	unsigned long volt_offset;
	int max_state;

	struct mutex lock;
	unsigned int use_dvfs;
	unsigned long initial_freq;
	unsigned long default_qos;
	struct notifier_block tmu_notifier;

	int (*isp_set_freq)(struct devfreq_data_isp *data,
						int index, int old_index);
	int (*isp_set_volt)(struct devfreq_data_isp *data,
			unsigned long volt,  unsigned long volt_range);
};

struct devfreq_thermal_work {
	struct delayed_work devfreq_mif_thermal_work;
	struct workqueue_struct *work_queue;
	unsigned int thermal_level_cs0;
	unsigned int thermal_level_cs1;
	unsigned int polling_period;
	unsigned long max_freq;
};

#define COLD_VOLT_OFFSET	(25000)
#define LIMIT_COLD_VOLTAGE	(1200000)

unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset);
int devfreq_get_opp_idx(struct devfreq_opp_table *table,
			unsigned int size, unsigned long freq);

int exynos7580_devfreq_mif_init(void *data);
int exynos7580_devfreq_mif_deinit(void *data);
int exynos7580_devfreq_int_init(void *data);
int exynos7580_devfreq_int_deinit(void *data);
int exynos7580_devfreq_isp_init(void *data);
int exynos7580_devfreq_isp_deinit(void *data);

#endif

#endif /* __DEVFREQ_EXYNOS_H */
