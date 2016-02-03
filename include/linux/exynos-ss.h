/*
 * Exynos-SnapShot for Samsung's SoC's.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef EXYNOS_SNAPSHOT_H
#define EXYNOS_SNAPSHOT_H

#ifdef CONFIG_EXYNOS_SNAPSHOT
#include "exynos-ss-soc.h"

/* mandatory */
extern void exynos_ss_task(int cpu, void *v_task);
extern void exynos_ss_work(void *worker, void *work, void *fn, int en);
extern void exynos_ss_cpuidle(int index, unsigned state, int diff, int en);
extern void exynos_ss_suspend(void *fn, void *dev, int en);
extern void exynos_ss_irq(unsigned int irq, void *fn, int irqs_disabled, int en);
extern int exynos_ss_set_enable(const char *name, int en);
extern int exynos_ss_get_enable(const char *name);
extern int exynos_ss_save_context(void *regs);
extern int exynos_ss_save_reg(void *regs);
extern int exynos_ss_dump_panic(void);
extern int exynos_ss_prepare_panic(void);
extern int exynos_ss_post_panic(void);
extern int exynos_ss_post_reboot(void);
extern int exynos_ss_set_hardlockup(int);
extern int exynos_ss_get_hardlockup(void);
extern unsigned int exynos_ss_get_item_size(char*);
extern unsigned int exynos_ss_get_item_paddr(char*);
#ifdef CONFIG_EXYNOS_DRAMTEST
extern int disable_mc_powerdn(void);
#endif

/* option */
#ifdef CONFIG_EXYNOS_SNAPSHOT_MBOX
extern void exynos_ss_mailbox(void *msg, int mode, char* f_name, void *volt);
#else
#define exynos_ss_mailbox(a,b,c,d)         do { } while(0)
#endif

#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
extern void exynos_ss_clockevent(unsigned long long clc, int64_t delta, void *next_event);
extern void exynos_ss_printk(const char *fmt, ...);
extern void exynos_ss_printkl(size_t msg, size_t val);
#else
#define exynos_ss_clockevent(a,b,c)	do { } while(0)
#define exynos_ss_printk(...)		do { } while(0)
#define exynos_ss_printkl(a,b)		do { } while(0)
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
extern void exynos_ss_irqs_disabled(unsigned long flags);
#else
#define exynos_ss_irqs_disabled(a)	do { } while(0);
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
extern void exynos_ss_hrtimer(void *timer, s64 *now, void *fn, int en);
#else
#define exynos_ss_hrtimer(a,b,c,d)	do { } while(0);
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
extern void exynos_ss_reg(unsigned int read, size_t val, size_t reg, int en);
#else
#define exynos_ss_reg(a,b,c,d)		do { } while(0);
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_SPINLOCK
extern void exynos_ss_spinlock(void *lock, int en);
#else
#define exynos_ss_spinlock(a,b)		do { } while(0);
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
struct clk;
extern void exynos_ss_clk(struct clk *clk, int en);
#else
#define exynos_ss_clk(a,b)		do { } while(0);
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
void exynos_ss_freq(int type, unsigned long freq, int en);
#else
#define exynos_ss_freq(a,b,c)	do { } while(0);
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
extern void exynos_ss_irq_exit(unsigned int irq, unsigned long long start_time);
#define exynos_ss_irq_exit_var(v)	do {	v = cpu_clock(raw_smp_processor_id());	\
					} while(0)
#else
#define exynos_ss_irq_exit(a,b)		do { } while(0);
#define exynos_ss_irq_exit_var(v)	do {	v = 0; } while(0);
#endif

#else
#define exynos_ss_task(a,b)		do { } while(0)
#define exynos_ss_work(a,b,c,d)		do { } while(0)
#define exynos_ss_clockevent(a,b,c)	do { } while(0)
#define exynos_ss_cpuidle(a,b,c,d)	do { } while(0)
#define exynos_ss_suspend(a,b,c)	do { } while(0)
#define exynos_ss_mailbox(a,b,c,d)	do { } while(0)
#define exynos_ss_irq(a,b,c,d)		do { } while(0)
#define exynos_ss_irq_exit(a,b)		do { } while(0)
#define exynos_ss_irqs_disabled(a)	do { } while(0)
#define exynos_ss_spinlock(a,b)		do { } while(0)
#define exynos_ss_clk(a,b)		do { } while(0)
#define exynos_ss_freq(a,b,c)		do { } while(0)
#define exynos_ss_irq_exit_var(v)	do { v = 0; } while(0)
#define exynos_ss_reg(a,b,c,d)		do { } while(0)
#define exynos_ss_hrtimer(a,b,c,d)	do { } while(0)
#define exynos_ss_printk(...)		do { } while(0)
#define exynos_ss_printkl(a,b)		do { } while(0)
#define exynos_ss_save_context(a)	do { } while(0)
#define exynos_ss_set_enable(a,b)	do { } while(0)
#define exynos_ss_get_enable(a)		do { } while(0)
#define exynos_ss_dump_panic()		do { } while(0)
#define exynos_ss_prepare_panic()	do { } while(0)
#define exynos_ss_post_panic()		do { } while(0)
#define exynos_ss_post_reboot()		do { } while(0)
#define exynos_ss_set_hardlockup(a)	do { } while(0)
#define exynos_ss_get_hardlockup()	do { } while(0)
#define exynos_ss_get_item_size(a)	do { } while(0)
#define exynos_ss_get_item_paddr(a)	do { } while(0)
#endif /* CONFIG_EXYNOS_SNAPSHOT */

/**
 * esslog_flag - added log information supported.
 * @ESS_FLAG_IN: Generally, marking into the function
 * @ESS_FLAG_ON: Generally, marking the status not in, not out
 * @ESS_FLAG_OUT: Generally, marking come out the function
 * @ESS_FLAG_SOFTIRQ: Marking to pass the softirq function
 * @ESS_FLAG_SOFTIRQ_HI_TASKLET: Marking to pass the tasklet function
 * @ESS_FLAG_SOFTIRQ_TASKLET: Marking to pass the tasklet function
 */
enum esslog_flag {
	ESS_FLAG_IN = 1,
	ESS_FLAG_ON = 2,
	ESS_FLAG_OUT = 3,
	ESS_FLAG_SOFTIRQ = 10000,
	ESS_FLAG_SOFTIRQ_HI_TASKLET,
	ESS_FLAG_SOFTIRQ_TASKLET,
	ESS_FLAG_CALL_TIMER_FN = 20000
};

enum esslog_freq_flag {
	ESS_FLAG_APL = 0,
	ESS_FLAG_ATL,
	ESS_FLAG_INT,
	ESS_FLAG_MIF,
	ESS_FLAG_ISP,
	ESS_FLAG_DISP,
};
#endif
