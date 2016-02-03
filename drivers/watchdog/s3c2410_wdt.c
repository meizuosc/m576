/* linux/drivers/char/watchdog/s3c2410_wdt.c
 *
 * Copyright (c) 2004 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2410 Watchdog Timer Support
 *
 * Based on, softdog.c by Alan Cox,
 *     (c) Copyright 1996 Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/miscdevice.h> /* for MODULE_ALIAS_MISCDEV */
#include <linux/watchdog.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/exynos-ss.h>
#include <linux/delay.h>
#include <linux/sysrq.h>
#include <linux/reboot.h>

#include <mach/map.h>
#include <mach/pmu.h>
#include <mach/regs-pmu.h>
#include <linux/sched/rt.h>
#include <linux/rtc.h>
#include <linux/kthread.h>
#include <linux/cpu.h>

#undef S3C_VA_WATCHDOG
#define S3C_VA_WATCHDOG (0)

#include <linux/s3c2410_wdt.h>

static bool nowayout	= WATCHDOG_NOWAYOUT;
static int tmr_margin	= CONFIG_S3C2410_WATCHDOG_DEFAULT_TIME;
static int tmr_atboot	= CONFIG_S3C2410_WATCHDOG_ATBOOT;
static int soft_noboot	= CONFIG_S3C2410_WATCHDOG_IRQ ? 1 : 0;
static int debug = 1;


/************************* MEIZU BSP *******************************/
/*---WDK TEST---*/
#include <linux/proc_fs.h>

static struct proc_dir_entry *aed_proc_dir;
static int test_case;
static int test_cpu;
static struct task_struct *wdt_task[NR_CPUS];
static ssize_t proc_generate_wdt_write(struct file *file,
				       const char __user *buf, size_t size, loff_t *ppos);
static ssize_t proc_generate_wdt_read(struct file *file,
				      char __user *buf, size_t size, loff_t *ppos);

#define AED_FILE_OPS(entry) \
static const struct file_operations proc_##entry##_fops = { \
	.read = proc_##entry##_read, \
	.write = proc_##entry##_write, \
}
#define  AED_PROC_ENTRY(name, entry, mode)\
if (!proc_create(#name, S_IFREG | mode, aed_proc_dir, &proc_##entry##_fops)) \
	printk(KERN_ERR "proc_create %s failed\n", #name)

AED_FILE_OPS(generate_wdt);

/*---WDK TEST END---*/

#define CPU_NR (nr_cpu_ids)
struct task_struct *wk_tsk[16];	/* max cpu 16 */

static int g_kinterval = -1;
static int g_enable = 1;
static unsigned int cpus_kick_bit = 0;
static int g_kicker_init =0;
static unsigned int kick_bit = 0;
static DEFINE_SPINLOCK(wdk_lock);

/*******************************************************************/


module_param(tmr_margin,  int, 0);
module_param(tmr_atboot,  int, 0);
module_param(nowayout,   bool, 0);
module_param(soft_noboot, int, 0);
module_param(debug,	  int, 0);

MODULE_PARM_DESC(tmr_margin, "Watchdog tmr_margin in seconds. (default="
		__MODULE_STRING(CONFIG_S3C2410_WATCHDOG_DEFAULT_TIME) ")");
MODULE_PARM_DESC(tmr_atboot,
		"Watchdog is started at boot time if set to 1, default="
			__MODULE_STRING(CONFIG_S3C2410_WATCHDOG_ATBOOT));
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
			__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");
MODULE_PARM_DESC(soft_noboot, "Watchdog action, set to 1 to ignore reboots, "
			"0 to reboot (default 0)");
MODULE_PARM_DESC(debug, "Watchdog debug, set to >1 for debug (default 0)");

static struct device    *wdt_dev;	/* platform device attached to */
static struct resource	*wdt_mem;
static struct resource	*wdt_irq;
static struct clk	*rate_wdt_clock;
static struct clk	*wdt_clock;
static void __iomem	*wdt_base;
static unsigned int	 wdt_count;
static DEFINE_SPINLOCK(wdt_lock);

#ifdef CONFIG_OF
struct s3c_watchdog_platdata {
	void (*pmu_wdt_control)(bool on, unsigned int pmu_wdt_reset_type);
	unsigned int pmu_wdt_reset_type;
};
#endif

/* watchdog control routines */

#define DBG(fmt, ...)					\
do {							\
	if (debug)					\
		pr_info(fmt, ##__VA_ARGS__);		\
} while (0)

/* functions */

static int s3c2410wdt_keepalive(struct watchdog_device *wdd)
{
	unsigned long flags;

	spin_lock_irqsave(&wdt_lock, flags);
	writel(wdt_count, wdt_base + S3C2410_WTCNT);
	spin_unlock_irqrestore(&wdt_lock, flags);

	return 0;
}

static void __s3c2410wdt_stop(void)
{
	unsigned long wtcon;

	wtcon = readl(wdt_base + S3C2410_WTCON);
	wtcon &= ~(S3C2410_WTCON_ENABLE | S3C2410_WTCON_RSTEN);
	writel(wtcon, wdt_base + S3C2410_WTCON);
}

static int s3c2410wdt_stop(struct watchdog_device *wdd)
{
	unsigned long flags;

	spin_lock_irqsave(&wdt_lock, flags);
	__s3c2410wdt_stop();
	spin_unlock_irqrestore(&wdt_lock, flags);

	return 0;
}

static int s3c2410wdt_stop_intclear(struct watchdog_device *wdd)
{
	unsigned long flags;

	spin_lock_irqsave(&wdt_lock, flags);
	__s3c2410wdt_stop();
	writel(1, wdt_base + S3C2410_WTCLRINT);
	spin_unlock_irqrestore(&wdt_lock, flags);

	return 0;
}

static int s3c2410wdt_start(struct watchdog_device *wdd)
{
	unsigned long wtcon;
	unsigned long flags;

	spin_lock_irqsave(&wdt_lock, flags);

	__s3c2410wdt_stop();

	wtcon = readl(wdt_base + S3C2410_WTCON);
	wtcon |= S3C2410_WTCON_ENABLE | S3C2410_WTCON_DIV128;

	if (soft_noboot) {
		wtcon |= S3C2410_WTCON_INTEN;
		wtcon &= ~S3C2410_WTCON_RSTEN;
	} else {
		wtcon &= ~S3C2410_WTCON_INTEN;
		wtcon |= S3C2410_WTCON_RSTEN;
	}

	DBG("%s: wdt_count=0x%08x, wtcon=%08lx\n",
	    __func__, wdt_count, wtcon);

	writel(wdt_count, wdt_base + S3C2410_WTDAT);
	writel(wdt_count, wdt_base + S3C2410_WTCNT);
	writel(wtcon, wdt_base + S3C2410_WTCON);
	spin_unlock_irqrestore(&wdt_lock, flags);

	return 0;
}

static inline int s3c2410wdt_is_running(void)
{
	return readl(wdt_base + S3C2410_WTCON) & S3C2410_WTCON_ENABLE;
}

static int s3c2410wdt_set_min_max_timeout(struct watchdog_device *wdd)
{
	unsigned int freq = (unsigned int)clk_get_rate(rate_wdt_clock);

	if(freq == 0) {
		dev_err(wdd->dev, "failed to get platdata\n");
		return -EINVAL;
	}

	wdd->min_timeout = 1;
	wdd->max_timeout = S3C2410_WTCNT_MAX *
		(S3C2410_WTCON_PRESCALE_MAX + 1) * S3C2410_WTCON_DIVMAX / freq;

	return 0;
}

static int s3c2410wdt_set_heartbeat(struct watchdog_device *wdd, unsigned timeout)
{
	unsigned int freq = (unsigned int)clk_get_rate(rate_wdt_clock);
	unsigned int count;
	unsigned int divisor = 1;
	unsigned long wtcon;

	if (timeout < 1)
		return -EINVAL;

	freq /= 128;
	count = timeout * freq;

	DBG("%s: count=%d, timeout=%d, freq=%d\n",
	    __func__, count, timeout, freq);

	/* if the count is bigger than the watchdog register,
	   then work out what we need to do (and if) we can
	   actually make this value
	*/

	if (count >= 0x10000) {
		for (divisor = 1; divisor <= 0x100; divisor++) {
			if ((count / divisor) < 0x10000)
				break;
		}

		if ((count / divisor) >= 0x10000) {
			dev_err(wdt_dev, "timeout %d too big\n", timeout);
			return -EINVAL;
		}
	}

	DBG("%s: timeout=%d, divisor=%d, count=%d (%08x)\n",
	    __func__, timeout, divisor, count, count/divisor);

	count /= divisor;
	wdt_count = count;

	/* update the pre-scaler */
	wtcon = readl(wdt_base + S3C2410_WTCON);
	wtcon &= ~S3C2410_WTCON_PRESCALE_MASK;
	wtcon |= S3C2410_WTCON_PRESCALE(divisor-1);

	writel(count, wdt_base + S3C2410_WTDAT);
	writel(wtcon, wdt_base + S3C2410_WTCON);

	wdd->timeout = (count * divisor) / freq;

	return 0;
}

void s3c2410wdt_reset(void)
{
	unsigned int val = 0;

	pr_info("Do WDT reset.\n");

	val = readl(wdt_base + S3C2410_WTCON);
	val &= ~(0x1<<5);
	writel(val, wdt_base + S3C2410_WTCON);

	val = readl(EXYNOS_PMU_AUTOMATIC_DISABLE_WDT);
	val &= ~(0x1<<0);
	writel(val, EXYNOS_PMU_AUTOMATIC_DISABLE_WDT);

	val = readl(EXYNOS_PMU_MASK_WDT_RESET_REQUEST);
	val &= ~(0x1<<0);
	writel(val, EXYNOS_PMU_MASK_WDT_RESET_REQUEST);

	val = readl(wdt_base + S3C2410_WTCNT);
	val = (0x1);
	writel(val, wdt_base + S3C2410_WTCNT);

	val = readl(wdt_base + S3C2410_WTCON);
	val |= (0x1<<0 | 0x1<<5 | 0x1<<15);
	writel(val, wdt_base + S3C2410_WTCON);
}

#define OPTIONS (WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE)

static const struct watchdog_info s3c2410_wdt_ident = {
	.options          =     OPTIONS,
	.firmware_version =	0,
	.identity         =	"S3C2410 Watchdog",
};

static struct watchdog_ops s3c2410wdt_ops = {
	.owner = THIS_MODULE,
	.start = s3c2410wdt_start,
	.stop = s3c2410wdt_stop,
	.ping = s3c2410wdt_keepalive,
	.set_timeout = s3c2410wdt_set_heartbeat,
};

static struct watchdog_device s3c2410_wdd = {
	.info = &s3c2410_wdt_ident,
	.ops = &s3c2410wdt_ops,
	.timeout = CONFIG_S3C2410_WATCHDOG_DEFAULT_TIME,
};

/* interrupt handler code */

static irqreturn_t s3c2410wdt_irq(int irqno, void *param)
{
	dev_info(wdt_dev, "watchdog timer expired (irq) kick_bit=0x%08x,check_bit=0x%08x\n", kick_bit, cpus_kick_bit);

	/* Stop and clear watchdog interrupt */
	s3c2410wdt_stop_intclear(&s3c2410_wdd);

	/* Print backtrace of all cpus. */
	handle_sysrq('l');

	/* Restart for availability */
	pr_info("%s: emergency reboot\n", __func__);
	emergency_restart();

	return IRQ_HANDLED;
}


static const struct of_device_id s3c2410_wdt_match[];

static int s3c2410wdt_get_platdata(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	struct s3c_watchdog_platdata *pdata;
	struct device_node *np = pdev->dev.of_node;

	if (np) {
		const struct of_device_id *match;
		match = of_match_node(s3c2410_wdt_match, pdev->dev.of_node);
		pdev->dev.platform_data = (struct s3c_watchdog_platdata *)match->data;
		pdata = pdev->dev.platform_data;
		if (of_property_read_u32(np, "pmu_wdt_reset_type",
					&pdata->pmu_wdt_reset_type)) {
			pr_err("%s: failed to get pmu_wdt_reset_type property\n", __func__);
			return -EINVAL;
		}
	}
#else
	pdev->dev.platform_data = dev_get_platdata(&pdev->dev);
#endif
	return 0;
}

int s3c2410wdt_set_emergency_stop(void)
{
	if (!s3c2410_wdd.dev)
		return -ENODEV;

	/* stop watchdog */
	dev_emerg(wdt_dev, "watchdog is stopped\n");
	s3c2410wdt_stop(&s3c2410_wdd);
	return 0;
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_WATCHDOG_RESET
static int s3c2410wdt_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	if (!s3c2410_wdd.dev)
		return -ENODEV;

	/* We assumed that num_online_cpus() > 1 status is abnormal */
	if (exynos_ss_get_hardlockup() || num_online_cpus() > 1) {
#ifdef CONFIG_EXYNOS7420_MC
		disable_mc_powerdn();
#endif
		dev_emerg(wdt_dev, "watchdog reset is started on panic after 5secs\n");

		/* set watchdog timer is started and  set by 5 seconds*/
		s3c2410wdt_start(&s3c2410_wdd);
		s3c2410wdt_set_heartbeat(&s3c2410_wdd, 5);
		s3c2410wdt_keepalive(&s3c2410_wdd);
	}

	return 0;
}

static struct notifier_block nb_panic_block = {
	.notifier_call = s3c2410wdt_panic_handler,
};
#endif


unsigned int wk_check_kick_bit(void)
{
	return cpus_kick_bit;
}

static int kicker_thread(void *arg)
{
	struct sched_param param = {.sched_priority = MAX_USER_RT_PRIO-1 }; // set real-time policies 1 -> 99 (low -> high)
	struct rtc_time tm;
	struct timeval tv = { 0 };
	/* android time */
	struct rtc_time tm_android;
	struct timeval tv_android = { 0 };
	int cpu = 0;
	int local_bit = 0;

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {
		if (kthread_should_stop())
			break;
		spin_lock(&wdk_lock);
		cpu = smp_processor_id();
		spin_unlock(&wdk_lock);

		if(g_enable){
			if (wk_tsk[cpu]->pid == current->pid) {
				/* only process WDT info if thread-x is on cpu-x */
				spin_lock(&wdk_lock);
				local_bit = kick_bit;
				printk("[WDK] local_bit:0x%x, cpu:%d\n", local_bit, cpu);

				if ((local_bit & (1 << cpu)) == 0) {
					local_bit |= (1 << cpu);  //kick the cpu bit
				}

				printk("[WDK] local_bit:0x%x, cpu:%d, check bit:0x%x, RT[%lld]\n", local_bit, cpu, wk_check_kick_bit(), sched_clock());
				if (local_bit == wk_check_kick_bit()) {
					printk("[WDK]: kick WDT, RT[%lld]\n", sched_clock());
					//mtk_wdt_restart(WD_TYPE_NORMAL);	/* for KICK external wdt */
					s3c2410wdt_keepalive(&s3c2410_wdd);
					local_bit = 0;

					/* show rtc time */
					do_gettimeofday(&tv);
					tv_android = tv;
					rtc_time_to_tm(tv.tv_sec, &tm);
					tv_android.tv_sec -= sys_tz.tz_minuteswest * 60;
					rtc_time_to_tm(tv_android.tv_sec, &tm_android);
					printk("[WDK][thread:%d][RT:%lld] %d-%02d-%02d %02d:%02d:%02d.%u UTC; android time %d-%02d-%02d %02d:%02d:%02d.%03d\n",
					     current->pid, sched_clock(), tm.tm_year + 1900, tm.tm_mon + 1,
					     tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, (unsigned int)tv.tv_usec,
					     tm_android.tm_year + 1900, tm_android.tm_mon + 1, tm_android.tm_mday,
					     tm_android.tm_hour, tm_android.tm_min, tm_android.tm_sec,
					     (unsigned int)tv_android.tv_usec);
				}
				kick_bit = local_bit;
				spin_unlock(&wdk_lock);
			}
		} else {
			printk("[WDK] WDK stop to kick\n");
		}
		msleep((g_kinterval) * 1000);
	}
	printk(KERN_EMERG "[WDK] WDT kicker thread stop, cpu:%d, pid:%d\n", cpu, current->pid);

	return 0;
}

void kicker_cpu_bind(int cpu)
{
	if(IS_ERR(wk_tsk[cpu]))
	{
		printk("[WDK] wk_task[%d] is NULL\n",cpu);
	}
	else
	{
		//kthread_bind(wk_tsk[cpu], cpu);
		WARN_ON_ONCE(set_cpus_allowed_ptr(wk_tsk[cpu], cpumask_of(cpu)) < 0);

		printk("[WDK] bind kicker thread[%d] to cpu[%d]\n",wk_tsk[cpu]->pid, cpu);
		wake_up_process(wk_tsk[cpu]);
	}
}


void wk_cpu_update_bit_flag(int cpu, int plug_status)
{
	if (1 == plug_status)	/* plug on */
	{
		spin_lock(&wdk_lock);
		cpus_kick_bit |= (1 << cpu);
		kick_bit = 0;
		spin_unlock(&wdk_lock);
	}

	if (0 == plug_status)	/* plug off */
	{
		spin_lock(&wdk_lock);
		cpus_kick_bit &= (~(1 << cpu));
		kick_bit = 0;
		spin_unlock(&wdk_lock);
	}
}

void wk_start_kick_cpu(int cpu)
{
	if (IS_ERR(wk_tsk[cpu])) {
		printk("[WDK] wk_task[%d] is NULL\n", cpu);
	} else {
		kthread_bind(wk_tsk[cpu], cpu);
		printk("[WDK] bind thread[%d] to cpu[%d]\n", wk_tsk[cpu]->pid, cpu);
		wake_up_process(wk_tsk[cpu]);
	}
}


static int start_kicker(void)
{
	int i;

	wk_cpu_update_bit_flag(0, 1); //set cpu 0, plug on

	for (i = 0; i < CPU_NR; i++) {
		wk_tsk[i] = kthread_create(kicker_thread, (void *)(unsigned long)i, "WDT_Kicker-%d", i);
		if (IS_ERR(wk_tsk[i])) {
			int ret = PTR_ERR(wk_tsk[i]);
			wk_tsk[i] = NULL;
			return ret;
		}
		wk_start_kick_cpu(i);
	}

	g_kicker_init = 1;
	printk("[WDK] WDT start kicker done.\n");

	return 0;
}


static void start_kicker_thread_with_default_setting(void)
{
	spin_lock(&wdk_lock);

	g_kinterval = 10;	/* default interval: 10s */

	spin_unlock(&wdk_lock);

	s3c2410wdt_start(&s3c2410_wdd);
	s3c2410wdt_set_heartbeat(&s3c2410_wdd, 30);//set watchdog timeout in 30s.
	s3c2410wdt_keepalive(&s3c2410_wdd);

	start_kicker();

	printk("[WDK] start_kicker_thread_with_default_setting done.\n");
}

static int __init init_wk_check_bit(void)
{
	int i = 0;

	printk("[WDK] arch init check_bit=0x%x +++++\n", cpus_kick_bit);

	for (i = 0; i < CPU_NR; i++) {
		wk_cpu_update_bit_flag(i, 1);
	}

	printk("[WDK] arch init check_bit=0x%x -----\n", cpus_kick_bit);

	return 0;
}


static int __cpuinit wk_cpu_callback(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int hotcpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		/* watchdog_prepare_cpu(hotcpu); */
		break;
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		wk_cpu_update_bit_flag(hotcpu, 1);
		if(1 == g_kicker_init)
		{
		   kicker_cpu_bind(hotcpu);
		}
		printk("[WDK] cpu %d plug on kick wdt. cpus_kick_bit=0x%x\n", hotcpu,cpus_kick_bit);
		s3c2410wdt_keepalive(&s3c2410_wdd);
		break;
#ifdef CONFIG_HOTPLUG_CPU
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		s3c2410wdt_keepalive(&s3c2410_wdd);
		wk_cpu_update_bit_flag(hotcpu, 0);
		printk("[WDK] cpu %d plug off, kick wdt. cpus_kick_bit=0x%x\n", hotcpu,cpus_kick_bit);
		break;
#endif
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata cpu_status_nfb = {
	.notifier_call = wk_cpu_callback,
	.priority = 6
};


static int __init init_wk(void)
{
	int i=0;

	start_kicker_thread_with_default_setting();

    cpu_hotplug_disable();
	register_cpu_notifier(&cpu_status_nfb);

	for (i = 0; i < CPU_NR; i++)
	{
		if(cpu_online(i))
		{
			wk_cpu_update_bit_flag(i, 1);
			printk("[WDK] init cpu online %d\n",i);
		}
		else
		{
			wk_cpu_update_bit_flag(i, 0);
			printk("[WDK] init cpu offline %d\n",i);
		}
	}
	//mtk_wdt_restart(WD_TYPE_NORMAL);	/* for KICK external wdt */
	s3c2410wdt_keepalive(&s3c2410_wdd);

	cpu_hotplug_enable();
	printk("[WDK] init_wk done late_initcall cpus_kick_bit=0x%x -----\n", cpus_kick_bit);

	return 0;
}

/*==============================MEIZU WDK TEST==========================================*/

static int kwdt_thread_test(void *arg)
{
	struct sched_param param = {.sched_priority = 99 };
	int cpu;

	spin_lock(&wdk_lock);
	cpu = smp_processor_id();
	spin_unlock(&wdk_lock);

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);
	printk("==> kwdt_thread_test on CPU %d, test_case = %d\n", cpu, test_case);


	if (test_case == 1) {
		if (cpu == test_cpu) {
			printk("Test 1 : One CPU WDT timeout\n");
			printk("\n CPU %d : disable preemption forever.(soft lockup)\n", cpu);
			preempt_disable();
			while (1);
			printk("\n Error : You should not see this !\n");
		} else {
			printk("\n CPU %d : Do nothing and exit\n ", cpu);
			return 0;
		}
	}
	else if (test_case == 2) {
		if (cpu == test_cpu) {
			printk("Test 2 : One CPU WDT timeout\n");
			printk("\n CPU %d : disable preemption and local IRQ forever.(hard lockup)\n", cpu);
			preempt_disable();
			local_irq_disable();
			while (1);
			printk("\n Error : You should not see this !\n");
		} else {
			printk("\n CPU %d : Do nothing and exit\n ", cpu);
			return 0;
		}
	} else if (test_case == 3) {
		if (cpu == test_cpu) {
			printk("Test 3 : One CPU WDT timeout, other CPU disable irq\n");
			printk("\n CPU %d : disable preemption and local IRQ forever\n", cpu);
			preempt_disable();
			while (1);
			printk("\n Error : You should not see this !\n");
		} else {
			printk("\n CPU %d : disable irq\n ", cpu);
			local_irq_disable();
			while (1);
			printk("\n Error : You should not see this !\n");
		}
	} else if (test_case == 4) {
		if (cpu == test_cpu) {
			printk("Test 4 : test watchdog timeout time < kick dog time\n");

			g_kinterval = 9999;	/* default interval: 10s */

			s3c2410wdt_set_heartbeat(&s3c2410_wdd, 3);
			s3c2410wdt_keepalive(&s3c2410_wdd);
		} else {
			printk("\n CPU %d : Do nothing and exit\n ", cpu);
			return 0;
		}
	} else if (test_case == 5) {
		printk("Test 5 : All CPU WDT timeout (other CPU stop in the loop)\n");
		printk("\n CPU %d : disable preemption and local IRQ forever\n ", cpu);
		preempt_disable();
		local_irq_disable();
		while (1);
		printk("\n Error : You should not see this !\n");
	} else if (test_case == 6) {
		printk("Test 6 : Disable ALL CPU IRQ/FIQ (HW_reboot)\n");
		printk("\n CPU %d : disable preemption and local IRQ/FIQ forever\n ", cpu);
		local_fiq_disable();
		preempt_disable();
		local_irq_disable();
		while (1);
		printk("\n Error : You should not see this !\n");
	}

	return 0;
}


static ssize_t proc_generate_wdt_read(struct file *file,
				      char __user *buf, size_t size, loff_t *ppos)
{
	char buffer[128];
	return sprintf(buffer, "WDT test - Usage: [test case number:test cpu]\n");
}

static ssize_t proc_generate_wdt_write(struct file *file,
				       const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned int i = 0;
	char msg[4];
	unsigned char name[20] = { 0 };

	if ((size < 2) || (size > sizeof(msg))) {
		printk("\n size = %zx\n", size);
		return -EINVAL;
	}
	if (copy_from_user(msg, buf, size)) {
		printk("copy_from_user error");
		return -EFAULT;
	}
	test_case = (unsigned int)msg[0] - '0';
	test_cpu = (unsigned int)msg[2] - '0';
	printk("test_case = %d, test_cpu = %d", test_case, test_cpu);
	if ((msg[1] != ':') || (test_case < 0) || (test_case > 8)
	    || (test_cpu < 0) || (test_cpu > nr_cpu_ids)) {
		printk("WDT test - Usage for M86: [test case number(1~6):test cpu(0~%d)]\n", nr_cpu_ids);
		return -EINVAL;
	}

	if (test_case == 1) {
		printk("Test 1 : One CPU WDT timeout\n");
	} else if (test_case == 2) {
		printk("Test 2 : One CPU WDT timeout, other CPU disable irq\n");
	} else if (test_case == 3) {
		printk("Test 3 : WDT timeout and loop in panic flow\n");
	} else if (test_case == 4) {
		printk("Test 4 : All CPU WDT timeout\n");
	} else if (test_case == 5) {
		printk("Test 5 : Disable ALL CPU IRQ/FIQ (HW_reboot)\n");
	} else {
		printk("\n Unknown test_case %d\n", test_case);
	}

	/* create kernel threads and bind on every cpu */
	for (i = 0; i < nr_cpu_ids; i++) {
		sprintf(name, "wd-test-%d", i);
		printk("[WDK]thread name: %s\n", name);
		wdt_task[i] = kthread_create(kwdt_thread_test, NULL, name);
		if (IS_ERR(wdt_task[i])) {
			int ret = PTR_ERR(wdt_task[i]);
			wdt_task[i] = NULL;
			return ret;
		}
		kthread_bind(wdt_task[i], i);
	}

	for (i = 0; i < nr_cpu_ids; i++) {
		printk(" wake_up_process(wk_tsk[%d])\n", i);
		wake_up_process(wdt_task[i]);
	}

	return size;
}



int init_wdt_test(void)
{
	aed_proc_dir = proc_mkdir("exynos-wdt", NULL);
	if (aed_proc_dir == NULL) {
		printk(KERN_ERR "aed proc_mkdir failed\n");
		return -ENOMEM;
	}

	AED_PROC_ENTRY(generate-wdt, generate_wdt, S_IRUSR | S_IWUSR);

	return 0;
}


/*======================================================================================*/


static int s3c2410wdt_probe(struct platform_device *pdev)
{
	struct device *dev;
	unsigned int wtcon;
	int started = 0;
	int ret;
	struct s3c_watchdog_platdata *pdata;

	DBG("%s: probe=%p\n", __func__, pdev);

	dev = &pdev->dev;
	wdt_dev = &pdev->dev;

	if (s3c2410wdt_get_platdata(pdev)) {
		dev_err(dev, "failed to get platdata\n");
		return -EINVAL;
	}
	pdata = dev_get_platdata(&pdev->dev);

	wdt_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (wdt_mem == NULL) {
		dev_err(dev, "no memory resource specified\n");
		return -ENOENT;
	}

	wdt_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (wdt_irq == NULL) {
		dev_err(dev, "no irq resource specified\n");
		ret = -ENOENT;
		goto err;
	}

	/* get the memory region for the watchdog timer */
	wdt_base = devm_ioremap_resource(dev, wdt_mem);
	if (IS_ERR(wdt_base)) {
		ret = PTR_ERR(wdt_base);
		goto err;
	}

	DBG("probe: mapped wdt_base=%p\n", wdt_base);

	rate_wdt_clock = devm_clk_get(dev, "rate_watchdog");
	if (IS_ERR(rate_wdt_clock)) {
		dev_err(dev, "failed to find watchdog rate clock source\n");
		ret = PTR_ERR(rate_wdt_clock);
		goto err;
	}

	wdt_clock = devm_clk_get(dev, "gate_watchdog");
	if (IS_ERR(wdt_clock)) {
		dev_err(dev, "failed to find watchdog clock source\n");
		ret = PTR_ERR(wdt_clock);
		goto err;
	}

	clk_prepare_enable(wdt_clock);

	/* Enable pmu watchdog reset control */
	if (pdata != NULL && pdata->pmu_wdt_control != NULL) {
		/* Prevent watchdog reset while setting */
		s3c2410wdt_stop_intclear(&s3c2410_wdd);
		pdata->pmu_wdt_control(1, pdata->pmu_wdt_reset_type);
	}

	/* see if we can actually set the requested timer margin, and if
	 * not, try the default value */

	ret = s3c2410wdt_set_min_max_timeout(&s3c2410_wdd);
	if (ret != 0) {
		dev_err(dev, "clock rate is 0\n");
		goto err_clk;
	}

	watchdog_init_timeout(&s3c2410_wdd, tmr_margin,  &pdev->dev);
	if (s3c2410wdt_set_heartbeat(&s3c2410_wdd, s3c2410_wdd.timeout)) {
		started = s3c2410wdt_set_heartbeat(&s3c2410_wdd,
					CONFIG_S3C2410_WATCHDOG_DEFAULT_TIME);

		if (started == 0)
			dev_info(dev,
			   "tmr_margin value out of range, default %d used\n",
			       CONFIG_S3C2410_WATCHDOG_DEFAULT_TIME);
		else
			dev_info(dev, "default timer value is out of range, "
							"cannot start\n");
	}

	ret = devm_request_irq(dev, wdt_irq->start, s3c2410wdt_irq, 0,
				pdev->name, pdev);
	if (ret != 0) {
		dev_err(dev, "failed to install irq (%d)\n", ret);
		goto err_clk;
	}

	watchdog_set_nowayout(&s3c2410_wdd, nowayout);

	ret = watchdog_register_device(&s3c2410_wdd);
	if (ret) {
		dev_err(dev, "cannot register watchdog (%d)\n", ret);
		goto err_clk;
	}

	if (tmr_atboot && started == 0) {
		dev_info(dev, "starting watchdog timer\n");
		s3c2410wdt_start(&s3c2410_wdd);
	} else if (!tmr_atboot) {
		/* if we're not enabling the watchdog, then ensure it is
		 * disabled if it has been left running from the bootloader
		 * or other source */

		s3c2410wdt_stop(&s3c2410_wdd);
	}

	/* print out a statement of readiness */

	wtcon = readl(wdt_base + S3C2410_WTCON);

#ifdef CONFIG_EXYNOS_SNAPSHOT_WATCHDOG_RESET
	/* register panic handler for watchdog reset */
	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
#endif
	dev_info(dev, "watchdog %sactive, reset %sabled, irq %sabled\n",
		 (wtcon & S3C2410_WTCON_ENABLE) ?  "" : "in",
		 (wtcon & S3C2410_WTCON_RSTEN) ? "en" : "dis",
		 (wtcon & S3C2410_WTCON_INTEN) ? "en" : "dis");

	/* MEIZU BSP watchdog kicker */
	//init_wk();

	/* MEIZU BSP WDT test */
	init_wdt_test();

	return 0;

 err_clk:
	clk_disable_unprepare(wdt_clock);
	wdt_clock = NULL;
	rate_wdt_clock = NULL;

 err:
	wdt_irq = NULL;
	wdt_mem = NULL;
	return ret;
}

static int s3c2410wdt_remove(struct platform_device *dev)
{
	watchdog_unregister_device(&s3c2410_wdd);

	clk_disable_unprepare(wdt_clock);
	wdt_clock = NULL;
	rate_wdt_clock = NULL;

	wdt_irq = NULL;
	wdt_mem = NULL;
	return 0;
}

static void s3c2410wdt_shutdown(struct platform_device *dev)
{
	s3c2410wdt_stop(&s3c2410_wdd);
}

#ifdef CONFIG_PM

static unsigned long wtcon_save;
static unsigned long wtdat_save;

static int s3c2410wdt_suspend(struct platform_device *dev, pm_message_t state)
{
	struct s3c_watchdog_platdata *pdata;

	pdata = dev_get_platdata(&dev->dev);
	/* Save watchdog state, and turn it off. */
	wtcon_save = readl(wdt_base + S3C2410_WTCON);
	wtdat_save = readl(wdt_base + S3C2410_WTDAT);

	/* Note that WTCNT doesn't need to be saved. */
	s3c2410wdt_stop(&s3c2410_wdd);

	/* Disable pmu watchdog reset control */
	if (pdata != NULL && pdata->pmu_wdt_control != NULL)
		pdata->pmu_wdt_control(0, pdata->pmu_wdt_reset_type);

	return 0;
}

static int s3c2410wdt_resume(struct platform_device *dev)
{
	struct s3c_watchdog_platdata *pdata;

	pdata = dev_get_platdata(&dev->dev);
	/* Stop and clear watchdog interrupt */
	s3c2410wdt_stop_intclear(&s3c2410_wdd);

	/* Enable pmu watchdog reset control */
	if (pdata != NULL && pdata->pmu_wdt_control != NULL)
		pdata->pmu_wdt_control(1, pdata->pmu_wdt_reset_type);

	/* Restore watchdog state. */

	writel(wtdat_save, wdt_base + S3C2410_WTDAT);
	writel(wtdat_save, wdt_base + S3C2410_WTCNT); /* Reset count */
	writel(wtcon_save, wdt_base + S3C2410_WTCON);

	pr_info("watchdog %sabled\n",
		(wtcon_save & S3C2410_WTCON_ENABLE) ? "en" : "dis");

	return 0;
}

#else
#define s3c2410wdt_suspend NULL
#define s3c2410wdt_resume  NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static struct s3c_watchdog_platdata watchdog_platform_data = {
	.pmu_wdt_control = exynos_pmu_wdt_control,
};

static const struct of_device_id s3c2410_wdt_match[] = {
	{ .compatible = "samsung,s3c2410-wdt",
	  .data = &watchdog_platform_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, s3c2410_wdt_match);
#endif

static struct platform_driver s3c2410wdt_driver = {
	.probe		= s3c2410wdt_probe,
	.remove		= s3c2410wdt_remove,
	.shutdown	= s3c2410wdt_shutdown,
	.suspend	= s3c2410wdt_suspend,
	.resume		= s3c2410wdt_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c2410-wdt",
		.of_match_table	= of_match_ptr(s3c2410_wdt_match),
	},
};


late_initcall(init_wk);
arch_initcall(init_wk_check_bit);
module_platform_driver(s3c2410wdt_driver);

MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>, "
	      "Dimitry Andric <dimitry.andric@tomtom.com>");
MODULE_DESCRIPTION("S3C2410 Watchdog Device Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
MODULE_ALIAS("platform:s3c2410-wdt");
