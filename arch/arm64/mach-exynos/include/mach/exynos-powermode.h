/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __EXYNOS_POWERMODE_H
#define __EXYNOS_POWERMODE_H __FILE__

#include <asm/io.h>

/* system power down configuration */
enum sys_powerdown {
	SYS_AFTR,
	SYS_STOP,
	SYS_DSTOP,
	SYS_DSTOP_PSR,
	SYS_LPD,
	SYS_LPA,
	SYS_ALPA,
	SYS_SLEEP,
	NUM_SYS_POWERDOWN,
};

struct exynos_pmu_conf {
	void __iomem *reg;
	unsigned int val[NUM_SYS_POWERDOWN];
};

/* SFR bit control to enter system power down */
struct sfr_bit_ctrl {
	void __iomem	*reg;
	unsigned int	mask;
	unsigned int	val;
};

struct sfr_save {
	void __iomem	*reg;
	unsigned int	val;
};

#define SFR_CTRL(_reg, _mask, _val)	\
	{				\
		.reg	= _reg,		\
		.mask	= _mask,	\
		.val	= _val,		\
	}

#define SFR_SAVE(_reg)			\
	{				\
		.reg	= _reg,		\
	}

/* setting SFR helper function */
static inline void exynos_set_sfr(struct sfr_bit_ctrl *ptr, int count)
{
	unsigned int tmp;

	for (; count > 0; count--, ptr++) {
		tmp = __raw_readl(ptr->reg);
		__raw_writel((tmp & ~ptr->mask) | ptr->val, ptr->reg);
	}
}

static inline void exynos_save_sfr(struct sfr_save *ptr, int count)
{
	for (; count > 0; count--, ptr++)
		ptr->val = __raw_readl(ptr->reg);
}

static inline void exynos_restore_sfr(struct sfr_save *ptr, int count)
{
	for (; count > 0; count--, ptr++)
		__raw_writel(ptr->val, ptr->reg);
}

/* power mode check flag */
#define EXYNOS_CHECK_DIDLE	0xBAD00000
#define EXYNOS_CHECK_LPA	0xABAD0000
#define EXYNOS_CHECK_DSTOP	0xABAE0000
#define EXYNOS_CHECK_SLEEP	0x00000BAD

/* system power down function */
extern void exynos_prepare_sys_powerdown(enum sys_powerdown mode);
extern void exynos_wakeup_sys_powerdown(enum sys_powerdown mode, bool early_wakeup);
extern bool exynos_sys_powerdown_enabled(void);

/* functions for cpuidle */
extern int determine_lpm(void);
extern int enter_c2(unsigned int cpu, int index, int *sub_state);
extern void wakeup_from_c2(unsigned int cpu);

/* cpuidle profile state */
#define C2_STATE		0x1
#define LPM_STATE		0x2

#define LPM_SUB_STATE_OFFSET	24
#define LPM_SUB_STATE		(0xF << LPM_SUB_STATE_OFFSET)
#define C2_SUB_STATE_OFFSET	16
#define C2_SUB_STATE		(0xF << C2_SUB_STATE_OFFSET)
#define MAJOR_STATE		(0xF)

#define SUB_STATE_OFFSET	16
#define SUB_STATE		(0xF << SUB_STATE_OFFSET)
#define CPD_STATE		(1 << SUB_STATE_OFFSET)
#define LPC_STATE		(2 << SUB_STATE_OFFSET)

/* cpuidle profile helper function */
#ifdef CONFIG_CPU_IDLE_EXYNOS
extern void lpa_blocking_counter(int);
#else
static inline void lpa_blocking_counter(int blocker)
{
	return;
}
#endif

/* Max length of check register list */
#define LIST_MAX_LENGTH	20

/* register check */
struct check_reg {
	void __iomem	*reg;
	unsigned int	check_bit;
};

static inline int check_reg_status(struct check_reg *list, unsigned int count)
{
	unsigned int i, tmp;

	for (i = 0; i < count; i++) {
		if (!list[i].reg)
			return 0;

		tmp = __raw_readl(list[i].reg);
		if (tmp & list[i].check_bit)
			return -EBUSY;
	}

	return 0;
}

/* functions for LPC */
#if defined(CONFIG_PWM_SAMSUNG)
extern int pwm_check_enable_cnt(void);
#else
static inline int pwm_check_enable_cnt(void)
{
	return 0;
}
#endif

#ifdef CONFIG_SERIAL_SAMSUNG
extern void s3c24xx_serial_fifo_wait(void);
#else
static inline void s3c24xx_serial_fifo_wait(void) { }
#endif

#ifdef CONFIG_PM_DEVFREQ
extern int is_dll_on(void);
#else
static inline int is_dll_on(void)
{
	return -EINVAL;
}
#endif

#define EXYNOS_SS_LPC_INDEX		('L' + 'P' + 'C')		/* 223 */

#endif /* __EXYNOS_POWERMODE_H */
