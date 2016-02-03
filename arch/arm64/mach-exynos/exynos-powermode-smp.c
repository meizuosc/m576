/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpumask.h>
#include <linux/of.h>
#include <linux/tick.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#include <mach/exynos-pm.h>
#include <mach/exynos-powermode-smp.h>
#include <mach/exynos-pm.h>
#include <mach/pmu.h>
#include <mach/pmu_cal_sys.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>

static int boot_core_id __read_mostly;
static int boot_cluster_id __read_mostly;

static void store_boot_cpu_info(void)
{
	unsigned int mpidr = read_cpuid_mpidr();

	boot_core_id = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	boot_cluster_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	pr_info("A booting CPU: core %d cluster %d\n", boot_core_id,
						       boot_cluster_id);
}

static bool is_in_boot_cluster(unsigned int cpu)
{
	return boot_cluster_id == MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
}

static void exynos_idle_clock_down(bool enable)
{
	unsigned int val;
	void __iomem *boot_cluster_reg, *nonboot_cluster_reg;

	boot_cluster_reg = EXYNOS7580_CPU_PWR_CTRL3;
	nonboot_cluster_reg = EXYNOS7580_APL_PWR_CTRL3;

	val = enable ? USE_L2QACTIVE : 0;

	__raw_writel(val, boot_cluster_reg);
	__raw_writel(val, nonboot_cluster_reg);
}

static DEFINE_SPINLOCK(cpd_lock);
static struct cpumask cpd_state_mask;

static void init_cpd_state_mask(void)
{
	cpumask_clear(&cpd_state_mask);
}

static void update_cpd_state(bool down, unsigned int cpu)
{
	if (down)
		cpumask_set_cpu(cpu, &cpd_state_mask);
	else
		cpumask_clear_cpu(cpu, &cpd_state_mask);
}

static unsigned int calc_expected_residency(unsigned int cpu)
{
	struct clock_event_device *dev = per_cpu(tick_cpu_device, cpu).evtdev;

	ktime_t now = ktime_get(), next = dev->next_event;

	return (unsigned int)ktime_to_us(ktime_sub(next, now));
}

static int is_busy(unsigned int target_residency, const struct cpumask *mask)
{
	int cpu;

	for_each_cpu_and(cpu, cpu_possible_mask, mask) {
		if (!cpumask_test_cpu(cpu, &cpd_state_mask))
			return -EBUSY;

		if (calc_expected_residency(cpu) < target_residency)
			return -EBUSY;
	}

	return 0;
}

#define LPC_FLAG_ADDR			(S5P_VA_SYSRAM_NS + 0x50)

static unsigned int lpc_reg_num __read_mostly;
static struct hw_info lpc_regs[MAX_NUM_REGS] __read_mostly;

static void set_lpc_flag(void)
{
	__raw_writel(0x1, LPC_FLAG_ADDR);
}

bool is_lpc_available(unsigned int target_residency)
{
	/*
	 * This logic comes from DCH.
	 */
	if (num_online_cpus() != 1)
		return false;

	if (is_busy(target_residency, cpu_online_mask))
		return false;

	if (check_hw_status(lpc_regs, lpc_reg_num))
		return false;

	if (exynos_lpc_prepare())
		return false;

	if (pwm_check_enable_cnt())
		return false;

	return true;
}

int determine_cpd(int index, int c2_index, unsigned int cpu,
		  unsigned int target_residency)
{
	exynos_cpu.power_down(cpu);

	if (index == c2_index)
		return c2_index;

	spin_lock(&cpd_lock);

	update_cpd_state(true, cpu);

	if (is_lpc_available(target_residency)) {
		set_lpc_flag();
		s3c24xx_serial_fifo_wait();
	}

	if (is_in_boot_cluster(cpu)) {
		index = c2_index;
		goto unlock;
	}

	if (is_busy(target_residency, cpu_coregroup_mask(cpu)))
		index = c2_index;
	else
		exynos_cpu_sequencer_ctrl(true);

unlock:
	spin_unlock(&cpd_lock);

	return index;
}

void wakeup_from_c2(unsigned int cpu)
{
	exynos_cpu.power_up(cpu);

	spin_lock(&cpd_lock);

	update_cpd_state(false, cpu);

	if (is_in_boot_cluster(cpu))
		goto unlock;

	exynos_cpu_sequencer_ctrl(false);

unlock:
	spin_unlock(&cpd_lock);
}

static unsigned int lpm_reg_num __read_mostly;
static struct hw_info lpm_regs[MAX_NUM_REGS] __read_mostly;

int determine_lpm(void)
{
	if (check_hw_status(lpm_regs, lpm_reg_num))
		return SYS_AFTR;

	if (exynos_lpa_prepare())
		return SYS_AFTR;

	return SYS_LPA;
}

static void parse_dt_reg_list(struct device_node *np, const char *reg,
			      const char *val, struct hw_info *regs,
			      unsigned int *reg_num)
{
	unsigned int i, reg_len, val_len;
	const __be32 *reg_list, *val_list;

	reg_list = of_get_property(np, reg, &reg_len);
	val_list = of_get_property(np, val, &val_len);

	if (!reg_list) {
		pr_warn("%s property does not exist\n", reg);
		return;
	}

	if (!val_list) {
		pr_warn("%s property does not exist\n", val);
		return;
	}

	BUG_ON(reg_len != val_len);

	*reg_num = reg_len / sizeof(unsigned int);

	for (i = 0; i < *reg_num; i++) {
		regs[i].addr = ioremap(be32_to_cpup(reg_list++), SZ_32);
		BUG_ON(!regs[i].addr);
		regs[i].mask = be32_to_cpup(val_list++);
	}
}

static void exynos_lpm_dt_init(void)
{
	struct device_node *np = of_find_node_by_name(NULL, "low-power-mode");

	parse_dt_reg_list(np, "lpc-reg", "lpc-val", lpc_regs, &lpc_reg_num);
	parse_dt_reg_list(np, "lpm-reg", "lpm-val", lpm_regs, &lpm_reg_num);
}

static void exynos_sys_powerdown_set_clk(enum sys_powerdown mode)
{
}

static void exynos_sys_powerdown_restore_clk(enum sys_powerdown mode)
{
}

void exynos_prepare_sys_powerdown(enum sys_powerdown mode)
{
	exynos_set_wakeupmask(mode);

	exynos_pmu_cal_sys_prepare(mode);

	exynos_idle_clock_down(false);

	switch (mode) {
	case SYS_LPA:
		exynos_lpa_enter();
		break;
	default:
		break;
	}

	exynos_sys_powerdown_set_clk(mode);
}

void exynos_wakeup_sys_powerdown(enum sys_powerdown mode, bool early_wakeup)
{
	if (early_wakeup)
		exynos_pmu_cal_sys_earlywake(mode);
	else
		exynos_pmu_cal_sys_post(mode);

	exynos_clear_wakeupmask();

	exynos_idle_clock_down(true);

	exynos_sys_powerdown_restore_clk(mode);

	switch (mode) {
	case SYS_LPA:
		exynos_lpa_exit();
		break;
	default:
		break;
	}
}

bool exynos_sys_powerdown_enabled(void)
{
	/*
	 * When system enters low power mode,
	 * this bit changes automatically to high
	 */
	return !((__raw_readl(EXYNOS_PMU_CENTRAL_SEQ_CONFIGURATION) &
			CENTRALSEQ_PWR_CFG));
}

int __init exynos_powermode_init(void)
{
	store_boot_cpu_info();

	if (IS_ENABLED(CONFIG_CPU_IDLE_EXYNOS))
		exynos_idle_clock_down(true);

	init_cpd_state_mask();

	exynos_lpm_dt_init();

	exynos_pmu_cal_sys_init();

	return 0;
}
arch_initcall(exynos_powermode_init);
