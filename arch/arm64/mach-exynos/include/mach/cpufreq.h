/* linux/arch/arm64/mach-exynos/include/mach/cpufreq.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - CPUFreq support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ARCH_CPUFREQ_H
#define __ARCH_CPUFREQ_H __FILE__

#include <linux/notifier.h>

/*
 * Common definitions and structures
 */
#define APLL_FREQ(f, a0, a1, a2, a3, a4, a5, a6, a7, b0, b1, b2, m, p, s) \
	{ \
		.freq = (f) * 1000, \
		.clk_div_cpu0 = ((a0) | (a1) << 4 | (a2) << 8 | (a3) << 12 | \
			(a4) << 16 | (a5) << 20 | (a6) << 24 | (a7) << 28), \
		.clk_div_cpu1 = (b0 << 0 | b1 << 4 | b2 << 8), \
		.mps = ((m) << 16 | (p) << 8 | (s)), \
	}

/* APLL Macro for Atlas Frequency in ISTOR */
#define APLL_ATLAS_FREQ(f, a0, a1, a2, a3, a4, a5, b0, b1, b2, m, p, s) \
	{ \
		.freq = (f) * 1000, \
		.clk_div_cpu0 = ((a0) | (a1) << 4 | (a2) << 8 | (a3) << 12 | \
			(a4) << 20 | (a5) << 26), \
		.clk_div_cpu1 = (b0 << 0 | b1 << 4 | b2 << 8), \
		.mps = ((m) << 16 | (p) << 8 | (s)), \
	}

enum cpufreq_level_index {
	L0, L1, L2, L3, L4,
	L5, L6, L7, L8, L9,
	L10, L11, L12, L13, L14,
	L15, L16, L17, L18, L19,
	L20, L21, L22, L23, L24,
};

struct apll_freq {
	unsigned int freq;
	u32 clk_div_cpu0;
	u32 clk_div_cpu1;
	u32 mps;
};

struct exynos_dvfs_info {
	unsigned long	mpll_freq_khz;
	unsigned int	pll_safe_idx;
	unsigned int	max_support_idx;
	unsigned int	min_support_idx;
	unsigned int	cluster_num;
	unsigned int	reboot_limit_freq;
	unsigned int	boost_freq;	/* use only KFC when enable HMP */
	unsigned int	boot_freq;
	unsigned int	boot_cpu_min_qos;
	unsigned int	boot_cpu_max_qos;
	unsigned int	boot_cpu_min_qos_timeout;
	unsigned int	boot_cpu_max_qos_timeout;
	unsigned int    resume_freq;
	int		boot_freq_idx;
	int		*bus_table;
	bool		blocked;
	unsigned int	cur_volt;
	struct clk	*cpu_clk;
	unsigned int	*volt_table;
	unsigned int	*abb_table;
	const unsigned int	*max_op_freqs;
	struct cpufreq_frequency_table	*freq_table;
	struct regulator *regulator;
	void (*set_freq)(unsigned int, unsigned int);
	void (*set_ema)(unsigned int);
	bool (*need_apll_change)(unsigned int, unsigned int);
	bool (*is_alive)(void);
	void (*set_int_skew)(int);
	int (*check_smpl)(void);
};

struct cpufreq_clkdiv {
	unsigned int	index;
	unsigned int	clkdiv0;
	unsigned int	clkdiv1;
};

/*
 * common interfaces for IPA
 */
/* interfaces for IPA */
#if defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ) || defined(CONFIG_ARM_EXYNOS_CPUFREQ)
void exynos_set_max_freq(int max_freq, unsigned int cpu);
void ipa_set_clamp(int cpu, unsigned int clamp_freq, unsigned int gov_target);
#else
static inline void exynos_set_max_freq(int max_freq, unsigned int cpu) {}
static inline void ipa_set_clamp(int cpu, unsigned int clamp_freq, unsigned int gov_target) {}
#endif

/* interface for THERMAL */
extern void exynos_thermal_throttle(void);
extern void exynos_thermal_unthrottle(void);

/*
 * CPUFREQ init events and notifiers
 */
#define CPUFREQ_INIT_COMPLETE	0x0001

#if defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ) || defined(CONFIG_ARM_EXYNOS_CPUFREQ)
extern int exynos_cpufreq_init_register_notifier(struct notifier_block *nb);
extern int exynos_cpufreq_init_unregister_notifier(struct notifier_block *nb);
#else
static inline int exynos_cpufreq_init_register_notifier(struct notifier_block *nb)
{return 0;}
static inline int exynos_cpufreq_init_unregister_notifier(struct notifier_block *nb)
{return 0;}
#endif

#if defined(CONFIG_ARM_EXYNOS5433_CPUFREQ) || defined(CONFIG_ARM_EXYNOS7420_CPUFREQ)
extern int exynos_cpufreq_smpl_warn_notify_call_chain(void);
#else
static inline int exynos_cpufreq_smpl_warn_notify_call_chain(void)
{
	return 0;
}
#endif

/*
 * Soc specific definitions
 */
#if defined(CONFIG_SOC_EXYNOS5422)
#define EMA_VAL_0 0x4
#define EMA_VAL_1 0x3
#define EMA_VAL_2 0x1
#define EMA_VAL_3 0x1
#define EMA_VOLT_LEV_0 900000
#define EMA_VOLT_LEV_1 950000
#define EMA_VOLT_LEV_2 1045000
#define EMA_VOLT_LEV_3 1155000
#define EMA_ON_CHANGE 0x11
#endif

#if defined(CONFIG_CPU_FREQ)
#if defined(CONFIG_ARM_EXYNOS_CPUFREQ)
extern int exynos4210_cpufreq_init(struct exynos_dvfs_info *);
extern int exynos4x12_cpufreq_init(struct exynos_dvfs_info *);
static inline int exynos5250_cpufreq_init(struct exynos_dvfs_info *info)
{return 0;}
#elif defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ)
extern int exynos_cpufreq_cluster0_init(struct exynos_dvfs_info *);
extern int exynos_cpufreq_cluster1_init(struct exynos_dvfs_info *);
typedef enum {
	CL_ZERO,
	CL_ONE,
	CL_END,
} cluster_type;

#define COLD_VOLT_OFFSET	37500
#define LIMIT_COLD_VOLTAGE	1350000
#define MIN_COLD_VOLTAGE	950000
#define NR_CLUST0_CPUS		4
#define NR_CLUST1_CPUS		4
#if defined(CONFIG_ARM_EXYNOS5422_CPUFREQ)
#define ENABLE_MIN_COLD		1
#else
#define ENABLE_MIN_COLD		0
#endif

enum op_state {
	NORMAL,		/* Operation : Normal */
	SUSPEND,	/* Direct API will be blocked in this state */
	RESUME,		/* Re-enabling DVFS using direct API after resume */
};

/*
 * Keep frequency value for counterpart cluster DVFS
 * cur, min, max : Frequency (KHz),
 * c_id : Counter cluster with booting cluster, if booting cluster is
 * A15, c_id will be A7.
 */
struct cpu_info_alter {
	unsigned int cur;
	unsigned int min;
	unsigned int max;
	cluster_type boot_cluster;
	cluster_type c_id;
};

extern cluster_type exynos_boot_cluster;
extern void (*disable_c3_idle)(bool disable);
#ifdef CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG
extern void force_dynamic_hotplug(bool out_flag, int delay_msec);
#endif
#if defined(CONFIG_SCHED_HMP) 
#if defined(CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG)
int cluster1_cores_hotplug(bool out_flag);
int cluster0_core1_hotplug_in(bool in_flag);
void event_hotplug_in(void);
bool is_cluster1_hotplugged(void);
#elif defined(CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG)
bool is_cluster1_hotplugged(void);
#endif
#else
static inline int cluster1_cores_hotplug(bool out_flag) {return 0;}
static inline int cluster0_core1_hotplug_in(bool in_flag) {return 0;}
static inline void event_hotplug_in(void) {}
static inline bool is_cluster1_hotplugged(void) {return 0;}
#endif
#elif defined(CONFIG_SOC_EXYNOS7580)
#else
	#warning "Should define CONFIG_ARM_EXYNOS_(MP_)CPUFREQ\n"
#endif
#endif
static inline int cluster1_cores_hotplug(bool out_flag) {return 0;}
static inline int cluster0_core1_hotplug_in(bool in_flag) {return 0;}
static inline void event_hotplug_in(void) {}
#endif /* __ARCH_CPUFREQ_H */
