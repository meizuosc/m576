/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Common Header for EXYNOS machines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_EXYNOS_COMMON_H
#define __ARCH_ARM_MACH_EXYNOS_COMMON_H

#include <linux/of.h>

void mct_init(void __iomem *base, int irq_g0, int irq_l0, int irq_l1);
extern unsigned long xxti_f, xusbxti_f;

struct map_desc;
void exynos_init_io(struct map_desc *mach_desc, int size);
void exynos_restart(char mode, const char *cmd);
void exynos_init_irq(void);

void exynos_firmware_init(void);

void exynos_set_timer_source(u8 channels);

#ifdef CONFIG_PM_GENERIC_DOMAINS
int exynos_pm_late_initcall(void);
#else
static inline int exynos_pm_late_initcall(void) { return 0; }
#endif

int exynos_pmu_init(void);

struct device_node;
void combiner_init(void __iomem *combiner_base, struct device_node *np,
			unsigned int max_nr, int irq_base);

extern struct smp_operations exynos_smp_ops;
extern void exynos_cpu_die(unsigned int cpu);

extern void set_boot_flag(unsigned int cpu, unsigned int mode);
extern void clear_boot_flag(unsigned int cpu, unsigned int mode);
extern void cci_snoop_disable(unsigned int sif);

#endif /* __ARCH_ARM_MACH_EXYNOS_COMMON_H */
