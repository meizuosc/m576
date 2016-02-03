#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sort.h>
#include <linux/reboot.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

#include <mach/cpufreq.h>
#include <linux/suspend.h>
#include <linux/cpufreq.h>

#define DEFAULT_CLUSTER1_HOTPLUG_IN_LOAD_THRSHD 240
#define DEFAULT_CLUSTER1_HOTPLUG_OUT_LOAD_THRSHD 240

struct cpu_load_info {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_iowait;
	unsigned int cur_freq;
	spinlock_t cpu_load_lock;
};

static DEFINE_PER_CPU(struct cpu_load_info, cpuload);

static DEFINE_MUTEX(march_hotplug_lock);
static DEFINE_MUTEX(march_thread_lock);
static DEFINE_MUTEX(cluster1_hotplug_lock);
static DEFINE_MUTEX(cluster0_hotplug_lock);
static spinlock_t march_lock;

static struct task_struct *march_hotplug_task;
static wait_queue_head_t march_poll_wait;

enum current_status {
	CURR_NORMAL = 0x0,
	CURR_LOWPOWER = 0x1,
	CURR_END = 0x2,
};

#define NORMAL_MODE_POLLING_MSEC 100
#define LOWPOWER_MODE_POLLING_MSEC 500

static unsigned int polling_time[CURR_END] = {
	NORMAL_MODE_POLLING_MSEC,
	LOWPOWER_MODE_POLLING_MSEC,
};

#define DEFAULT_CLUSTER1_STAY_CNT_NORMAL 20 // 2000ms
#define DEFAULT_CLUSTER1_STAY_CNT_LOWPOWER 3 //  300ms
#define DEFAULT_CLUSTER1_STAY_CNT_HMP_MIGRAION 20 //2000ms

enum current_status curr_status = CURR_NORMAL;

static unsigned int cluster1_hotplug_in_load_threshold = DEFAULT_CLUSTER1_HOTPLUG_IN_LOAD_THRSHD;
static unsigned int cluster1_hotplug_out_load_threshold = DEFAULT_CLUSTER1_HOTPLUG_OUT_LOAD_THRSHD;
static unsigned int cluster1_stay_cnt_normal = DEFAULT_CLUSTER1_STAY_CNT_NORMAL;
static unsigned int cluster1_stay_cnt_lowpower = DEFAULT_CLUSTER1_STAY_CNT_LOWPOWER;

unsigned int cluster1_hotplug_in_threshold_by_hmp = 840;

bool lcd_is_on = true;
static bool in_suspend_prepared = false;

static unsigned int cluster1_stay_count_by_hmp = false;
static unsigned int cluster1_init_stay_count_by_hmp =DEFAULT_CLUSTER1_STAY_CNT_HMP_MIGRAION;

#if defined(CONFIG_SCHED_HMP)
#define DEFAULT_LOW_POWER_TRRESHOLD_THRESHD	8
#define DEFAULT_LOW_POWER_STAY_COUNT_THRESHOLD	2
static unsigned int low_power_thread_threshold = DEFAULT_LOW_POWER_TRRESHOLD_THRESHD;
static unsigned int low_power_stay_count_threshold = DEFAULT_LOW_POWER_STAY_COUNT_THRESHOLD;
static unsigned long low_power_current_thread;
static unsigned int cluster0_load_at_maxfreq = 0;
static unsigned int cluster1_load_at_maxfreq = 0;
static unsigned int cur_cluster1_onoff = 0;
static unsigned int cur_cluster0_core_num = 4;
#endif

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
static unsigned int cluster1_min_freq;
#endif

static bool march_pthread_started = false;
static bool march_poll_waiting = false;

struct k_data {
	enum current_status	k_curr_status;
	bool			k_lcd_is_on;
	unsigned int		k_cluster_load;
	int			k_cluster_load_invalid;
	unsigned int		k_cluster1_hotplug_in_load_threshold;
	unsigned int		k_cluster1_hotplug_out_load_threshold;
	unsigned int		k_cluster1_stay_cnt_normal;
	unsigned int		k_cluster1_stay_cnt_lowpower;
	unsigned int		k_low_power_thread_threshold;
	unsigned int		k_low_power_stay_count_threshold;
	unsigned long		k_low_power_running_thread_nr;
	unsigned int		k_log_onoff;
};

struct p_data {
	unsigned int		p_cluster1_on_off;
	unsigned int		p_target_cluster0;
};

static struct k_data march_kernel_data;
static struct p_data hotplug_data;

static struct pm_qos_request cluster1_num_max_qos;
static struct pm_qos_request cluster1_num_min_qos;
static struct pm_qos_request cluster0_num_max_qos;
static struct pm_qos_request cluster0_num_min_qos;

static struct pm_qos_request cluster1_num_booster_min_qos;
static struct pm_qos_request cluster1_num_boot_min_qos;
static struct pm_qos_request cluster0_num_boot_min_qos;

static struct pm_qos_request sys_cluster1_num_max_qos;
static struct pm_qos_request sys_cluster1_num_min_qos;
static struct pm_qos_request sys_cluster0_num_max_qos;
static struct pm_qos_request sys_cluster0_num_min_qos;

static int on_run(void *data);
static int calc_load_cluster(int cluster_num, unsigned int* load);

#if defined(CONFIG_SCHED_HMP)
static struct workqueue_struct *hotplug_cluster0_wq;
static struct workqueue_struct *hotplug_cluster1_wq;
#endif

struct notifier_block cpufreq_freq_transition;

extern void set_up_migration_for_hotplug(bool temp);

static int march_hotplug_disable = 0;
static int log_onoff = 0;

static void set_curr_status(enum current_status temp)
{
	curr_status = temp;
}

static enum current_status get_curr_status(void)
{
	return curr_status;
}

static unsigned int get_cluster1_init_stay_count_by_hmp(void)
{
	return cluster1_init_stay_count_by_hmp;
}
EXPORT_SYMBOL(get_cluster1_init_stay_count_by_hmp);

static unsigned int get_cur_cluster1_booster_value(void)
{
	return cluster1_stay_count_by_hmp;
}
EXPORT_SYMBOL(get_cur_cluster1_booster_value);

static void set_cluster1_stay_count_by_hmp(unsigned int temp)
{
	if (log_onoff)
		printk("[March] set_cluster1_stay_count_by_hmp=%d\n",temp);
	cluster1_stay_count_by_hmp = temp;
}
EXPORT_SYMBOL(set_cluster1_stay_count_by_hmp);

static void decrease_cluster1_booster(void)
{
	unsigned int temp = get_cur_cluster1_booster_value();
	if (temp > 0) {
		temp--;
		set_cluster1_stay_count_by_hmp(temp);
		if (temp == 0) {
			if (log_onoff)
				printk("[March] expire up migraion count\n");
			set_up_migration_for_hotplug(false);
		}
	}
}
EXPORT_SYMBOL(decrease_cluster1_booster);

static unsigned int get_cur_cluster1_onoff_status(void)
{
	return cur_cluster1_onoff;
}
EXPORT_SYMBOL(get_cur_cluster1_onoff_status);

static void set_cur_cluster1_onoff_status(unsigned int temp)
{
	cur_cluster1_onoff = temp;
}
EXPORT_SYMBOL(set_cur_cluster1_onoff_status);

static unsigned int get_cur_cluster0_core_num(void)
{
	return cur_cluster0_core_num;
}
EXPORT_SYMBOL(get_cur_cluster0_core_num);

static void set_cur_cluster0_core_num(unsigned int temp)
{
	cur_cluster0_core_num = temp;
}
EXPORT_SYMBOL(set_cur_cluster0_core_num);

static int get_hotplug_running_status(void)
{
	return march_hotplug_disable;
}
EXPORT_SYMBOL(get_hotplug_running_status);

static void exynos_march_hotplug_enable(void)
{
	mutex_lock(&march_hotplug_lock);
	march_hotplug_disable = 0;
	mutex_unlock(&march_hotplug_lock);
}

static void exynos_march_hotplug_disable(void)
{
	mutex_lock(&march_hotplug_lock);
	march_hotplug_disable = 1;
	mutex_unlock(&march_hotplug_lock);
}

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL) {
		return get_cpu_idle_time_jiffy(cpu, wall);
	} else {
		idle_time += get_cpu_iowait_time_us(cpu, wall);
	}

	return idle_time;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static unsigned int NonDeferedTime = 40;
struct timer_list	hotplug_timer;
struct timer_list	hotplug_timer_delay;

static void march_hotplug_monitor(unsigned long data)
{
	unsigned long expires = jiffies + msecs_to_jiffies(polling_time[get_curr_status()]);

	if (march_hotplug_task != NULL) {
		wake_up_process(march_hotplug_task);

		if(get_cur_cluster1_onoff_status() == 0) {
			if (!timer_pending(&hotplug_timer))
				mod_timer_pinned(&hotplug_timer, expires);
		}
		else {
			if (!timer_pending(&hotplug_timer_delay))
				mod_timer_pinned(&hotplug_timer_delay, expires);
		}
	} else {
		del_timer(&hotplug_timer);
		del_timer(&hotplug_timer_delay);
	}
}

void wifi_cl1_get(void)
{
	if (pm_qos_request_active(&cluster1_num_max_qos))
		pm_qos_update_request(&cluster1_num_max_qos, NR_CLUST1_CPUS);
}
void wifi_cl1_release(void)
{
	if (pm_qos_request_active(&cluster1_num_max_qos))
		pm_qos_update_request(&cluster1_num_max_qos, lcd_is_on ? NR_CLUST1_CPUS : 0);
}
#ifdef CONFIG_BCMDHD_PCIE
extern int current_level;
#endif

static int fb_state_change(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	struct fb_info *info = evdata->info;
	unsigned int blank;

	if (val != FB_EVENT_BLANK &&
		val != FB_R_EARLY_EVENT_BLANK)
		return 0;
	/*
	 * If FBNODE is not zero, it is not primary display(LCD)
	 * and don't need to process these scheduling.
	 */
	if (info->node)
		return NOTIFY_OK;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		lcd_is_on = false;
#ifdef CONFIG_BCMDHD_PCIE
		if(current_level >= 3){
#endif
		if (pm_qos_request_active(&cluster1_num_max_qos))
			pm_qos_update_request(&cluster1_num_max_qos, 0);
#ifdef CONFIG_BCMDHD_PCIE
		}
#endif
		set_curr_status(CURR_LOWPOWER);
		pr_info("LCD is off %d\n",get_curr_status());
		break;

	case FB_BLANK_UNBLANK:
		lcd_is_on = true;
		if (pm_qos_request_active(&cluster1_num_max_qos))
			pm_qos_update_request(&cluster1_num_max_qos, NR_CLUST1_CPUS);
		set_curr_status(CURR_NORMAL);
		pr_info("LCD is on %d\n",get_curr_status());
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block fb_block = {
	.notifier_call = fb_state_change,
};

extern struct cpumask hmp_fast_cpu_mask;
extern struct cpumask hmp_slow_cpu_mask;

static int get_online_cpu_num_on_cluster(int cluster)
{
	struct cpumask dst_cpumask;

	cpumask_and(&dst_cpumask, (cluster == CL_ONE) ? &hmp_fast_cpu_mask : &hmp_slow_cpu_mask, cpu_online_mask);

	return cpumask_weight(&dst_cpumask);
}

void reset_calc_load_core(unsigned int cpu)
{
	struct cpu_load_info *pcpu = &per_cpu(cpuload, cpu);
	cputime64_t cur_wall_time, cur_idle_time;
	unsigned int idle_time, wall_time;

	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time);

	wall_time = (unsigned int)(cur_wall_time - pcpu->prev_cpu_wall);
	pcpu->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int)(cur_idle_time - pcpu->prev_cpu_idle);
	pcpu->prev_cpu_idle = cur_idle_time;
}
EXPORT_SYMBOL(reset_calc_load_core);

#if defined(CONFIG_SCHED_HMP)
bool is_cluster1_hotplugged(void)
{
	struct cpumask dst_cpumask;

	cpumask_and(&dst_cpumask, &hmp_fast_cpu_mask, cpu_online_mask);
	if (cpumask_weight(&dst_cpumask) == 0)
		return true;
	return false;
}
#endif

extern void set_cpu_state_for_dynamic_hotplug(unsigned int);
extern void clear_cpu_state_for_dynamic_hotplug(unsigned int);

#if defined(CONFIG_SCHED_HMP)
static struct  cpumask store_cpu;

static int __ref ready_for_sleep(void)
{
	int ret  = 0;
	int i = 0;
	cpumask_clear(&store_cpu);
	for (i = setup_max_cpus - 1; i > 0; i--) {
		if (cpu_online(i)) {
				cpumask_set_cpu(i, &store_cpu);
				ret = cpu_down(i);
				if (ret) {
					pr_err("[%s]: down %d failed\n", __func__, i);
					return ret;
			}
		}
	}
	return ret;
}

static int __ref ready_for_wakeup(void)
{
	int ret  = 0;
	int cpu = 0;
	for_each_cpu_and(cpu, &store_cpu, cpu_possible_mask) {
		if (!cpu_online(cpu)) {
			ret = cpu_up(cpu);
			if (ret) {
				pr_err("[%s]: up %d failed\n",__func__, cpu);
			}
		}
	}
	return ret;
}

static int __ref change_core_num_cluster0(int target_num, int use_target)
{
	int ret = 0;
	int cur_num, val, i = 0;
	int max_limit, min_limit;

	mutex_lock(&cluster0_hotplug_lock);

	cur_num = get_online_cpu_num_on_cluster(CL_ZERO);

	max_limit = (int)pm_qos_request(PM_QOS_CLUSTER0_NUM_MAX);
	min_limit = (int)pm_qos_request(PM_QOS_CLUSTER0_NUM_MIN);

	if (get_curr_status() == CURR_NORMAL) {
		target_num = max_limit;
	} else {
		target_num = max(target_num, min_limit);
	}

	if (target_num != cur_num) {
		if (log_onoff)
			printk("[%s]: cur:%d target:%d max:%d min:%d\n", __func__, cur_num, target_num, max_limit,min_limit);
	}

	if (target_num > cur_num) {
		val = target_num - cur_num;
		for (i = 1; i < NR_CLUST0_CPUS && val > 0; i++) {
		    if (!cpu_online(i)) {
			ret = cpu_up(i);
			if (ret) {
				pr_err("[%s]: up %d failed\n", __func__, i);
				mutex_unlock(&cluster0_hotplug_lock);
				return ret;
			}
			val--;
	    }
		}
	} else if (target_num < cur_num) {
		val = cur_num - target_num;
		for (i = NR_CLUST0_CPUS - 1; i > 0 && val > 0; i--) {
			if (cpu_online(i)) {
				ret = cpu_down(i);
				if (ret) {
					pr_err("[%s]: down %d failed\n", __func__, i);
					mutex_unlock(&cluster0_hotplug_lock);
					return ret;
				} else
					reset_calc_load_core(i);
				val--;
			}
		}
	}
	set_cur_cluster0_core_num(get_online_cpu_num_on_cluster(CL_ZERO));
	mutex_unlock(&cluster0_hotplug_lock);

	return ret;
}

static __ref int change_core_num_cluster1(int cluster_on_off, int hmp_booster)
{
	int ret = 0;
	int cur_num, val, i = 0;
	int max_limit = 0xff, min_limit = 0xff;
	int target_num = 0;

	mutex_lock(&cluster1_hotplug_lock);
	cur_num = get_online_cpu_num_on_cluster(CL_ONE);

	max_limit = (int)pm_qos_request(PM_QOS_CLUSTER1_NUM_MAX);
	min_limit = (int)pm_qos_request(PM_QOS_CLUSTER1_NUM_MIN);

	if (hmp_booster != 0 &&  max_limit > 0) {
		target_num = max_limit;
	}
	else if (cluster_on_off == 1 ) {
		target_num = max_limit;
	} else {
		target_num = min_limit;
	}

	if (target_num != cur_num) {
		if (log_onoff) {
			printk("[%s] cur:%d target:%d max:%d min:%d\n", __func__, cur_num, target_num, max_limit,min_limit);
		}
	}

	if (target_num > cur_num) {
		val = target_num - cur_num;
		for (i = NR_CLUST0_CPUS; i < setup_max_cpus && val > 0; i++) {
		    if (!cpu_online(i)) {
					set_hmp_boostpulse(300000);
					ret = cpu_up(i);
					if (ret) {
						pr_err("[%s]: up %d failed\n", __func__, i);
						mutex_unlock(&cluster1_hotplug_lock);
						return ret;
					}
				val--;
			}
		}
	} else if (target_num < cur_num) {
		val = cur_num - target_num;
		for (i = setup_max_cpus - 1; i >= NR_CLUST0_CPUS && val > 0; i--) {
			if (cpu_online(i)) {
				ret = cpu_down(i);
				if (ret) {
					pr_err("[%s]: down %d failed\n", __func__, i);
					mutex_unlock(&cluster1_hotplug_lock);
					return ret;
				} else
					reset_calc_load_core(i);
				val--;
			}
		}
	}
	if (get_online_cpu_num_on_cluster(CL_ONE) == 0)
		set_cur_cluster1_onoff_status(0);
	else
		set_cur_cluster1_onoff_status(1);
	mutex_unlock(&cluster1_hotplug_lock);
	return ret;
}

static void event_hotplug_cluster0_work(struct work_struct *work)
{
	int target_num;
	mutex_lock(&march_thread_lock);
	target_num = (int)pm_qos_request(PM_QOS_CLUSTER0_NUM_MIN);
	change_core_num_cluster0(target_num,0);
	mutex_unlock(&march_thread_lock);
}

static void event_hotplug_cluster1_work(struct work_struct *work)
{
	int target_num;
	mutex_lock(&march_thread_lock);
	target_num = (int)pm_qos_request(PM_QOS_CLUSTER1_NUM_MIN);
	if (target_num == 0)
		change_core_num_cluster1(0,0);
	else
		change_core_num_cluster1(1,0);
	mutex_unlock(&march_thread_lock);
}

static DECLARE_WORK(hotplug_cluster0_work, event_hotplug_cluster0_work);
static DECLARE_WORK(hotplug_cluster1_work, event_hotplug_cluster1_work);

void event_hotplug_cluster0(void)
{
	if (hotplug_cluster0_wq)
		queue_work(hotplug_cluster0_wq, &hotplug_cluster0_work);
}

void event_hotplug_cluster1(void)
{
	if (hotplug_cluster1_wq)
		queue_work(hotplug_cluster1_wq, &hotplug_cluster1_work);
}

void event_hmp_for_heavy_task(bool detect)
{
	unsigned int init_value = get_cluster1_init_stay_count_by_hmp();
	if(detect == true) {
		set_cluster1_stay_count_by_hmp(init_value);
	}
}
#endif

static int exynos_march_hotplug_notifier(struct notifier_block *notifier,
					unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		printk("[%s] PM_SUSEND_PREPARE %d\n",__func__,num_online_cpus());
		mutex_lock(&march_thread_lock);

		in_suspend_prepared = true;
		exynos_march_hotplug_disable();
		ready_for_sleep();
		if (march_hotplug_task) {
			mutex_unlock(&march_thread_lock);
			kthread_stop(march_hotplug_task);
			mutex_lock(&march_thread_lock);
			march_hotplug_task = NULL;
		}
		set_up_migration_for_hotplug(false);

		del_timer(&hotplug_timer);
		del_timer(&hotplug_timer_delay);

		mutex_unlock(&march_thread_lock);
		break;

	case PM_POST_SUSPEND:
		printk("[%s] PM_POST_SUSPEND \n",__func__);
		mutex_lock(&march_thread_lock);
		ready_for_wakeup();
		exynos_march_hotplug_enable();

		march_hotplug_task =
			kthread_create(on_run, NULL, "thread_hotplug");
		if (IS_ERR(march_hotplug_task)) {
			mutex_unlock(&march_thread_lock);
			pr_err("Failed in creation of thread.\n");
			return -EINVAL;
		}

		in_suspend_prepared = false;
		hotplug_timer.expires = jiffies + msecs_to_jiffies(polling_time[get_curr_status()]);
		add_timer_on(&hotplug_timer, (int)(hotplug_timer.data));
		hotplug_timer_delay.expires = jiffies + msecs_to_jiffies(polling_time[get_curr_status()]);
		add_timer_on(&hotplug_timer_delay, (int)(hotplug_timer_delay.data));

		wake_up_process(march_hotplug_task);
		mutex_unlock(&march_thread_lock);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_march_hotplug_nb = {
	.notifier_call = exynos_march_hotplug_notifier,
	.priority = 1,
};

static int exynos_march_hotplut_reboot_notifier(struct notifier_block *this,
				unsigned long code, void *_cmd)
{
	switch (code) {
	case SYSTEM_POWER_OFF:
	case SYS_RESTART:
		mutex_lock(&march_thread_lock);
		exynos_march_hotplug_disable();
		if (march_hotplug_task) {
			mutex_unlock(&march_thread_lock);
			kthread_stop(march_hotplug_task);
			mutex_lock(&march_thread_lock);
			march_hotplug_task = NULL;
		}
		mutex_unlock(&march_thread_lock);
		break;
	}
	del_timer(&hotplug_timer);
	del_timer(&hotplug_timer_delay);
	return NOTIFY_OK;
}

static struct notifier_block exynos_march_hotplug_reboot_nb = {
	.notifier_call = exynos_march_hotplut_reboot_notifier,
};

static int cpufreq_transition_notifier(struct notifier_block *nb,
				      unsigned long val, void *data)
{
	struct cpufreq_freqs *freqs = data;
	unsigned long flags;

	switch (val) {
	case CPUFREQ_POSTCHANGE:
		/* flush previous laod */
		spin_lock_irqsave(&per_cpu(cpuload, freqs->cpu).cpu_load_lock, flags);
		reset_calc_load_core(freqs->cpu);
		spin_unlock_irqrestore(&per_cpu(cpuload, freqs->cpu).cpu_load_lock, flags);
		break;
	}
	return 0;
}

static int calc_load_core(unsigned int cpu, unsigned int *core_load_at_max_freq,
						unsigned int cur_freq, unsigned int cluster_max_freq)
{
	struct cpu_load_info *pcpu = &per_cpu(cpuload, cpu);
	cputime64_t cur_wall_time, cur_idle_time;
	unsigned int idle_time, wall_time;
	unsigned int cur_load, load_at_max_freq = 0;

	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time);

	wall_time = (unsigned int)(cur_wall_time - pcpu->prev_cpu_wall);
	pcpu->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int)(cur_idle_time - pcpu->prev_cpu_idle);
	pcpu->prev_cpu_idle = cur_idle_time;

	if (unlikely(!wall_time))
		return -1;

	if (wall_time < idle_time) {
		wall_time = idle_time;
	}

	cur_load = 100 * (wall_time - idle_time) / wall_time;

	/* Calculate the scaled load across CPU */
	load_at_max_freq = (cur_load * cur_freq) / cluster_max_freq;
	*core_load_at_max_freq = load_at_max_freq;
	return 0;
}

static int calc_load_cluster(int cluster_num, unsigned int *load)
{
	struct cpufreq_policy *policy;
	unsigned int i;
	unsigned int cur_freq, core_load,cluster_max_freq = 0;
	unsigned int cluster_load = 0;

	if (cluster_num == 0)
		policy = cpufreq_cpu_get(0);
	else if (cluster_num == 1)
		policy = cpufreq_cpu_get(4);
	else {
		pr_err("Invalid parameter\n");
		return -1;
	}

	if (!policy) {
		pr_err("Invalid policy\n");
		goto bad;
	}

	cur_freq = policy->cur;
	cluster_max_freq = policy->max;

	for_each_cpu_and(i, cpu_coregroup_mask(cluster_num * NR_CLUST1_CPUS), cpu_online_mask) {
		int ret = 0;
		unsigned long flags;
		spin_lock_irqsave(&per_cpu(cpuload, i).cpu_load_lock, flags);
		ret = calc_load_core(i, &core_load, cur_freq, cluster_max_freq);
		spin_unlock_irqrestore(&per_cpu(cpuload, i).cpu_load_lock, flags);
		if (!ret)
			cluster_load += core_load;
		else {
			pr_warn("%s error:%d\n",__func__,ret);
			continue;
		}
	}

	if (cluster_num == 0)
		cluster0_load_at_maxfreq = cluster_load;
	else
		cluster1_load_at_maxfreq = cluster_load;

	cpufreq_cpu_put(policy);
	*load = cluster_load;
	return 0;
bad:
	return -1;
}

int calc_next_hotplug_num(int* cluster1_on_off, int* target_cluster0)
{
	unsigned long	flags;
	int ret = 0;

	if(march_pthread_started == false)
		goto not_ready;

	spin_lock_irqsave(&march_lock,flags);
	if(march_poll_waiting == true) {
		wake_up_interruptible(&march_poll_wait);
		march_poll_waiting = false;
	}
	spin_unlock_irqrestore(&march_lock,flags);

	*cluster1_on_off = hotplug_data.p_cluster1_on_off;
	*target_cluster0 = hotplug_data.p_target_cluster0;

	return ret;

not_ready:
	*cluster1_on_off = 1;
	*target_cluster0 = 4;

	return ret;
}

static int on_run(void *data)
{
	int on_cpu = 0;
	struct cpumask thread_cpumask;

	cpumask_clear(&thread_cpumask);
	cpumask_set_cpu(on_cpu, &thread_cpumask);
	sched_setaffinity(0, &thread_cpumask);

	mutex_lock(&march_thread_lock);
	while (!kthread_should_stop()) {
		unsigned int cluster1_core_onoff, cluster0_core_num;
		calc_next_hotplug_num(&cluster1_core_onoff,&cluster0_core_num);
		if (get_hotplug_running_status()) {
			mutex_unlock(&march_thread_lock);
			goto Sleep_Out;
		}

		if (get_cur_cluster1_booster_value() > 0) {
			change_core_num_cluster1(cluster1_core_onoff,true);
			decrease_cluster1_booster();
		} else if (cluster1_core_onoff != get_cur_cluster1_onoff_status()) {
			change_core_num_cluster1(cluster1_core_onoff,false);
		}

		if (cluster0_core_num != get_cur_cluster0_core_num()) {
			change_core_num_cluster0(cluster0_core_num,1);
		}
		mutex_unlock(&march_thread_lock);
		NonDeferedTime = cpufreq_interactive_get_down_sample_time()/USEC_PER_MSEC;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		set_current_state(TASK_RUNNING);

		if (kthread_should_stop())
			goto thread_stop;

Sleep_Out:
		set_current_state(TASK_INTERRUPTIBLE);
		if (!in_suspend_prepared) schedule_timeout_interruptible(msecs_to_jiffies(NonDeferedTime));
		set_current_state(TASK_RUNNING);
		if (kthread_should_stop())
			goto thread_stop;
		mutex_lock(&march_thread_lock);
	}
	mutex_unlock(&march_thread_lock);

thread_stop:
	pr_info("stopped %s\n", march_hotplug_task->comm);
	return 0;
}

static int exynos_cluster0_core_num_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	event_hotplug_cluster0();
	return NOTIFY_OK;
}

static int exynos_cluster1_core_num_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	event_hotplug_cluster1();
	return NOTIFY_OK;
}

static struct notifier_block exynos_cluster0_core_num_qos_notifier = {
	.notifier_call = exynos_cluster0_core_num_qos_handler,
	.priority = INT_MAX,
};

static struct notifier_block exynos_cluster1_core_num_qos_notifier = {
	.notifier_call = exynos_cluster1_core_num_qos_handler,
	.priority = INT_MAX,
};

static ssize_t show_enable_march_thread_hotplug(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	int disabled = get_hotplug_running_status();;

	return snprintf(buf, 10, "%s\n", disabled ? "enabled" : "disabled");
}

static ssize_t store_enable_march_thread_hotplug(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input_threshold;

	if (!sscanf(buf, "%8d", &input_threshold))
		return -EINVAL;

	if (input_threshold < 0) {
		pr_err("%s: invalid value (%d)\n", __func__, input_threshold);
		return -EINVAL;
	}

	if (input_threshold == 0)
		exynos_march_hotplug_enable();
	else
		exynos_march_hotplug_disable();

	return count;
}

#if defined(CONFIG_SCHED_HMP)
static ssize_t show_lowpower_parameter(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "current_nr_running = %lu, "
			"low_power_thread_threshold = %u, low_power_stay_count_threshold = %u\n",
			low_power_current_thread, low_power_thread_threshold, low_power_stay_count_threshold);
}

static ssize_t store_lowpower_parameter(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input_nr_running_thrshd;
	int input_low_power_stay_count_threshold;

	if (!sscanf(buf, "%5d %5d", &input_nr_running_thrshd, &input_low_power_stay_count_threshold))
		return -EINVAL;

	if (input_nr_running_thrshd <= 1 || input_low_power_stay_count_threshold < 1) {
		pr_err("%s: invalid values (thrshd = %d, range = %d)\n",
			__func__, input_nr_running_thrshd, input_low_power_stay_count_threshold);
		pr_err("%s: thrshd is should be over than 1,"
			" and range is should be over than 0\n", __func__);
		return -EINVAL;
	}

	low_power_thread_threshold = (unsigned int)input_nr_running_thrshd;
	low_power_stay_count_threshold = (unsigned int)input_low_power_stay_count_threshold;

	pr_info("%s: low_power_thread_threshold = %u, low_power_stay_count_threshold = %u\n",
		__func__, low_power_thread_threshold, low_power_stay_count_threshold);

	return count;
}
#endif

static ssize_t show_cl1_hotplug_in_load_threshold(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", cluster1_hotplug_in_load_threshold);
}

static ssize_t store_cl1_hotplug_in_load_threshold(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input_in_threshold;

	if (!sscanf(buf, "%8d", &input_in_threshold))
		return -EINVAL;

	if (input_in_threshold < 0) {
		pr_err("%s: invalid value (%d)\n", __func__, input_in_threshold);
		return -EINVAL;
	}

	cluster1_hotplug_in_load_threshold = (unsigned int)input_in_threshold;

	return count;
}

static ssize_t show_cl1_hotplug_out_load_threshold(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", cluster1_hotplug_out_load_threshold);
}

static ssize_t store_cl1_hotplug_out_load_threshold(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input_out_threshold;

	if (!sscanf(buf, "%8d", &input_out_threshold))
		return -EINVAL;

	if (input_out_threshold < 0) {
		pr_err("%s: invalid value (%d)\n", __func__, input_out_threshold);
		return -EINVAL;
	}

	cluster1_hotplug_out_load_threshold = (unsigned int)input_out_threshold;

	return count;
}

static ssize_t show_cl1_stay_cnt_normal(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", cluster1_stay_cnt_normal);
}

static ssize_t store_cl1_stay_cnt_normal(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input_threshold;

	if (!sscanf(buf, "%8d", &input_threshold))
		return -EINVAL;

	if (input_threshold < 0) {
		pr_err("%s: invalid value (%d)\n", __func__, input_threshold);
		return -EINVAL;
	}

	cluster1_stay_cnt_normal = (unsigned int)input_threshold;

	return count;
}

static ssize_t show_cl1_stay_cnt_lowpower(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", cluster1_stay_cnt_lowpower);
}

static ssize_t store_cl1_stay_cnt_lowpower(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input_threshold;

	if (!sscanf(buf, "%8d", &input_threshold))
		return -EINVAL;

	if (input_threshold < 0) {
		pr_err("%s: invalid value (%d)\n", __func__, input_threshold);
		return -EINVAL;
	}

	cluster1_stay_cnt_lowpower = (unsigned int)input_threshold;

	return count;
}
static ssize_t show_cl1_stay_cnt_hmp_migration(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", cluster1_init_stay_count_by_hmp);
}

static ssize_t store_cl1_stay_cnt_hmp_migration(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input_threshold;

	if (!sscanf(buf, "%8d", &input_threshold))
		return -EINVAL;

	if (input_threshold < 0) {
		pr_err("%s: invalid value (%d)\n", __func__, input_threshold);
		return -EINVAL;
	}

	cluster1_init_stay_count_by_hmp = (unsigned int)input_threshold;

	return count;
}

static ssize_t show_cl0_max_num(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "cl0 max num: %d\n",
				(int)pm_qos_request(PM_QOS_CLUSTER0_NUM_MAX));
}

static ssize_t store_cl0_max_num(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int max_num;

	if (!sscanf(buf, "%8d", &max_num))
		return -EINVAL;

	if (max_num < 0) {
		pr_err("%s: invalid value (%d)\n",
			__func__, max_num);
		return -EINVAL;
	}

	if (pm_qos_request_active(&sys_cluster0_num_max_qos))
		pm_qos_update_request(&sys_cluster0_num_max_qos, max_num);

	return count;
}

static ssize_t show_cl0_min_num(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "cl0 min num : %d\n",
				(int)pm_qos_request(PM_QOS_CLUSTER0_NUM_MIN));
}

static ssize_t store_cl0_min_num(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int min_num;

	if (!sscanf(buf, "%8d", &min_num))
		return -EINVAL;

	if (min_num < 0) {
		pr_err("%s: invalid value (%d)\n",
			__func__, min_num);
		return -EINVAL;
	}

	if (pm_qos_request_active(&sys_cluster0_num_min_qos))
		pm_qos_update_request(&sys_cluster0_num_min_qos, min_num);

	return count;
}

static ssize_t show_cl1_max_num(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "cl1 max num : %d\n",
				(int)pm_qos_request(PM_QOS_CLUSTER1_NUM_MAX));
}

static ssize_t store_cl1_max_num(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int max_num;

	if (!sscanf(buf, "%8d", &max_num))
		return -EINVAL;

	if (max_num < 0) {
		pr_err("%s: invalid value (%d)\n",
			__func__, max_num);
		return -EINVAL;
	}

	if (pm_qos_request_active(&sys_cluster1_num_max_qos))
		pm_qos_update_request(&sys_cluster1_num_max_qos, max_num);

	return count;
}

static ssize_t show_cl1_min_num(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "cl1 min num : %d\n",
				(int)pm_qos_request(PM_QOS_CLUSTER1_NUM_MIN));
}

static ssize_t store_cl1_min_num(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int min_num;

	if (!sscanf(buf, "%8d", &min_num))
		return -EINVAL;

	if (min_num < 0) {
		pr_err("%s: invalid value (%d)\n",
			__func__, min_num);
		return -EINVAL;
	}

	if (pm_qos_request_active(&sys_cluster1_num_min_qos))
		pm_qos_update_request(&sys_cluster1_num_min_qos, min_num);

	return count;
}

static ssize_t show_log_onoff(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "log_onoff : %d\n",
				(int)log_onoff);
}

static ssize_t store_log_onoff(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int temp;

	if (!sscanf(buf, "%8d", &temp))
		return -EINVAL;

	if (temp < 0) {
		pr_err("%s: invalid value (%d)\n",
			__func__, temp);
		return -EINVAL;
	}

	log_onoff = temp;

	return count;
}

static ssize_t show_hotplug_polling_time(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "hotplug polling_time (normal : %umsec, lowpower : %umsec, cur : %umsec)\n",
				polling_time[CURR_NORMAL], polling_time[CURR_LOWPOWER], polling_time[curr_status]);
}

static ssize_t store_hotplug_polling_time(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input_lowpower_mode_polling_time, input_nomal_mode_polling_time;

	if (!sscanf(buf, "%8d %8d", &input_lowpower_mode_polling_time, &input_nomal_mode_polling_time))
		return -EINVAL;

	if (input_lowpower_mode_polling_time < 0 || input_nomal_mode_polling_time < 0) {
		pr_err("%s: invalid value (%d, %d)\n",
			__func__, input_lowpower_mode_polling_time, input_nomal_mode_polling_time);
		return -EINVAL;
	}

	polling_time[CURR_LOWPOWER] = (unsigned int)input_lowpower_mode_polling_time;
	polling_time[CURR_NORMAL] = (unsigned int)input_nomal_mode_polling_time;

	printk("hotplug polling_time (normal : %umsec, lowpower : %umsec\n",
				polling_time[CURR_NORMAL], polling_time[CURR_LOWPOWER]);

	return count;
}

#if defined(CONFIG_SCHED_HMP)
static ssize_t show_cl1_hotplug_in_threshold_by_hmp(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CL1_hotplug_in_threshold_by_hmp : %u\n", cluster1_hotplug_in_threshold_by_hmp);
}

static ssize_t store_cl1_hotplug_in_threshold_by_hmp(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input_cl1_hotplug_in_threshold_by_hmp;

	if (!sscanf(buf, "%8d", &input_cl1_hotplug_in_threshold_by_hmp))
		return -EINVAL;

	if (input_cl1_hotplug_in_threshold_by_hmp < 0) {
		pr_err("%s: invalid value (%d)\n",
			__func__, input_cl1_hotplug_in_threshold_by_hmp);
		return -EINVAL;
	}

	cluster1_hotplug_in_threshold_by_hmp = input_cl1_hotplug_in_threshold_by_hmp;

	printk("cluster1_hotplug_in_threshold_by_hmp : %u\n",	cluster1_hotplug_in_threshold_by_hmp);

	return count;
}
#endif

extern struct time_in_state_info cl0_time_in_state;
extern struct time_in_state_info cl1_time_in_state;


static ssize_t show_time_in_state(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;
	for (i = 0; i < cl1_time_in_state.state_num; i++) {
		len += sprintf(buf + len, "%u %llu\n", cl1_time_in_state.freq_table[i],
			(unsigned long long)
			jiffies_64_to_clock_t(cl1_time_in_state.time_in_state[i]));
	}
	for (i = 0; i < cl0_time_in_state.state_num; i++) {
		len += sprintf(buf + len, "%u %llu\n", cl0_time_in_state.freq_table[i],
			(unsigned long long)
			jiffies_64_to_clock_t(cl0_time_in_state.time_in_state[i]));
	}
	return len;
}

static ssize_t store_time_in_state(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	return count;
}

/*
 *  sysfs implementation for exynos-snapshot
 *  you can access the sysfs of march-hotplug to /sys/devices/system/march-hotplug
 *  path.
 */
static struct bus_type march_hotplug_subsys = {
	.name = "march-hotplug",
	.dev_name = "march-hotplug",
};

static struct kobj_attribute enable_march_hotplug_thread_attr =
		__ATTR(enable_march_thread_hotplug, S_IRUGO | S_IWUSR,
			show_enable_march_thread_hotplug, store_enable_march_thread_hotplug);

#if defined(CONFIG_SCHED_HMP)
static struct kobj_attribute lowpower_parameter_attr =
		__ATTR(lowpower_parameter, S_IRUGO | S_IWUSR,
			show_lowpower_parameter, store_lowpower_parameter);
#endif

static struct kobj_attribute cl1_hotplug_in_load_threshold_attr =
		__ATTR(cl1_hotplug_in_load_threshold, S_IRUGO | S_IWUSR,
			show_cl1_hotplug_in_load_threshold, store_cl1_hotplug_in_load_threshold);

static struct kobj_attribute cl1_hotplug_out_load_threshold_attr =
		__ATTR(cl1_hotplug_out_load_threshold, S_IRUGO | S_IWUSR,
			show_cl1_hotplug_out_load_threshold, store_cl1_hotplug_out_load_threshold);

static struct kobj_attribute cl1_stay_cnt_normal_attr =
		__ATTR(cl1_stay_cnt_normal, S_IRUGO | S_IWUSR,
			show_cl1_stay_cnt_normal, store_cl1_stay_cnt_normal);

static struct kobj_attribute cl1_stay_cnt_lowpower_attr =
		__ATTR(cl1_stay_cnt_lowpower, S_IRUGO | S_IWUSR,
			show_cl1_stay_cnt_lowpower, store_cl1_stay_cnt_lowpower);


#if defined(CONFIG_SCHED_HMP)
static struct kobj_attribute cl1_stay_cnt_hmp_migration_attr =
		__ATTR(cl1_stay_cnt_hmp_migration, S_IRUGO | S_IWUSR,
			show_cl1_stay_cnt_hmp_migration, store_cl1_stay_cnt_hmp_migration);
#endif

static struct kobj_attribute log_onoff_attr =
		__ATTR(log_onoff, S_IRUGO | S_IWUSR,
			show_log_onoff, store_log_onoff);

static struct kobj_attribute polling_time_attr =
		__ATTR(hotplug_polling_time, S_IRUGO | S_IWUSR,
			show_hotplug_polling_time, store_hotplug_polling_time);

#if defined(CONFIG_SCHED_HMP)
static struct kobj_attribute cl1_hotplug_in_threshold_by_hmp_attr =
		__ATTR(cl1_hotplug_in_threshold_by_hmp, S_IRUGO | S_IWUSR,
			show_cl1_hotplug_in_threshold_by_hmp, store_cl1_hotplug_in_threshold_by_hmp);
#endif

static struct kobj_attribute cl0_max_num_attr =
		__ATTR(cl0_max_num, S_IRUGO | S_IWUSR,
			show_cl0_max_num, store_cl0_max_num);

static struct kobj_attribute cl0_min_num_attr =
		__ATTR(cl0_min_num, S_IRUGO | S_IWUSR,
			show_cl0_min_num, store_cl0_min_num);

static struct kobj_attribute cl1_max_num_attr =
		__ATTR(cl1_max_num, S_IRUGO | S_IWUSR,
			show_cl1_max_num, store_cl1_max_num);

static struct kobj_attribute cl1_min_num_attr =
		__ATTR(cl1_min_num, S_IRUGO | S_IWUSR,
			show_cl1_min_num, store_cl1_min_num);

static struct kobj_attribute time_in_state_attr =
		__ATTR(time_in_state, S_IRUGO | S_IWUSR,
			show_time_in_state, store_time_in_state);

static struct attribute *march_hotplug_sysfs_attrs[] = {
	&enable_march_hotplug_thread_attr.attr,
#if defined(CONFIG_SCHED_HMP)
	&lowpower_parameter_attr.attr,
#endif
	&cl1_hotplug_in_load_threshold_attr.attr,
	&cl1_hotplug_out_load_threshold_attr.attr,
	&cl1_stay_cnt_normal_attr.attr,
	&cl1_stay_cnt_lowpower_attr.attr,
#if defined(CONFIG_SCHED_HMP)
	&cl1_stay_cnt_hmp_migration_attr.attr,
#endif
	&log_onoff_attr.attr,
	&polling_time_attr.attr,
#if defined(CONFIG_SCHED_HMP)
	&cl1_hotplug_in_threshold_by_hmp_attr.attr,
#endif
	&cl0_max_num_attr.attr,
	&cl0_min_num_attr.attr,
	&cl1_max_num_attr.attr,
	&cl1_min_num_attr.attr,
	&time_in_state_attr.attr,
	NULL,
};

static struct attribute_group march_hotplug_sysfs_group = {
	.attrs = march_hotplug_sysfs_attrs,
};

static const struct attribute_group *march_hotplug_sysfs_groups[] = {
	&march_hotplug_sysfs_group,
	NULL,
};

static struct proc_dir_entry *proc_march_dir;

unsigned int init_march_kernel_data(void)
{
	unsigned long	flags;

	spin_lock_irqsave(&march_lock,flags);
	march_kernel_data.k_curr_status = CURR_NORMAL;
	march_kernel_data.k_lcd_is_on = true;
	march_kernel_data.k_cluster1_hotplug_in_load_threshold = DEFAULT_CLUSTER1_HOTPLUG_IN_LOAD_THRSHD;
	march_kernel_data.k_cluster1_hotplug_out_load_threshold = DEFAULT_CLUSTER1_HOTPLUG_OUT_LOAD_THRSHD;
	march_kernel_data.k_cluster1_stay_cnt_normal = DEFAULT_CLUSTER1_STAY_CNT_NORMAL;
	march_kernel_data.k_cluster1_stay_cnt_lowpower = DEFAULT_CLUSTER1_STAY_CNT_LOWPOWER;
	march_kernel_data.k_low_power_thread_threshold = DEFAULT_LOW_POWER_TRRESHOLD_THRESHD;
	march_kernel_data.k_low_power_stay_count_threshold = DEFAULT_LOW_POWER_STAY_COUNT_THRESHOLD;
	march_kernel_data.k_low_power_running_thread_nr = 0;
	march_kernel_data.k_log_onoff = 0;
	spin_unlock_irqrestore(&march_lock,flags);

	return 0;
}

static ssize_t march_read(struct file *file, char __user *buf,
			size_t size, loff_t *ppos)
{
	unsigned long	flags;
	unsigned int	load;
	ssize_t 	retval;

	retval = sizeof(struct k_data);

	spin_lock_irqsave(&march_lock,flags);
	if (calc_load_cluster(0, &load) == 0)
		march_kernel_data.k_cluster_load = load;
	else
		march_kernel_data.k_cluster_load_invalid = 1;

	march_kernel_data.k_curr_status = get_curr_status();
	march_kernel_data.k_cluster1_hotplug_in_load_threshold = cluster1_hotplug_in_load_threshold;
	march_kernel_data.k_cluster1_hotplug_out_load_threshold = cluster1_hotplug_out_load_threshold;
	march_kernel_data.k_cluster1_stay_cnt_normal = cluster1_stay_cnt_normal;
	march_kernel_data.k_cluster1_stay_cnt_lowpower = cluster1_stay_cnt_lowpower;
	march_kernel_data.k_lcd_is_on = lcd_is_on;
	march_kernel_data.k_low_power_thread_threshold = low_power_thread_threshold;
	march_kernel_data.k_low_power_stay_count_threshold = low_power_stay_count_threshold;
	march_kernel_data.k_low_power_running_thread_nr = nr_running();
	march_kernel_data.k_log_onoff = log_onoff;

	if (copy_to_user(buf, &march_kernel_data, retval)) {
		spin_unlock_irqrestore(&march_lock,flags);
		printk(KERN_ERR " copy_to_user() failed!\n");

		return -EFAULT;
	}
	spin_unlock_irqrestore(&march_lock,flags);

	return retval;
}

static ssize_t march_write(struct file *file, const char __user *buf,
			size_t size, loff_t *ppos)
{
	unsigned long flags;

	if (size != sizeof(struct p_data))
		return -1;

	memset(&hotplug_data, 0, sizeof(struct p_data));

	spin_lock_irqsave(&march_lock,flags);
	if (copy_from_user((void *)(&hotplug_data), buf, size)) {
		spin_unlock_irqrestore(&march_lock,flags);
		printk(KERN_ERR " copy_from_user() failed!\n");

		return -EFAULT;
	}
	spin_unlock_irqrestore(&march_lock,flags);

	return size;
}

static unsigned int march_poll(struct file *file, poll_table *wait)
{
	unsigned long flags;
	int mask = 0;

	poll_wait(file, &march_poll_wait, wait);

	spin_lock_irqsave(&march_lock,flags);
	if(!march_poll_waiting) {
		mask |= POLLPRI;
		march_poll_waiting = true;
	}
	spin_unlock_irqrestore(&march_lock,flags);

	return mask;
}

static int march_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	march_pthread_started = true;

	return ret;
}

static const struct file_operations march_fileops = {
	.owner		= THIS_MODULE,
	.open		= march_open,
	.read		= march_read,
	.write		= march_write,
	.poll		= march_poll,
};

int setup_hotplug_sysfs(void)
{
	int ret = 0;

	ret = subsys_system_register(&march_hotplug_subsys, march_hotplug_sysfs_groups);
	if (ret)
		pr_err("fail to register march-hotplug subsys\n");

	return ret;
}

static int __init march_cpu_hotplug_init(void)
{
	int ret, i = 0;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	struct cpufreq_policy *policy;
#endif
	struct proc_dir_entry *entry;

	unsigned long expires = jiffies + msecs_to_jiffies(polling_time[get_curr_status()]);
	init_timer_deferrable(&hotplug_timer);
	hotplug_timer.function = march_hotplug_monitor;
	hotplug_timer.data = 0;	// Target Core on which the timer function run.

	init_timer(&hotplug_timer_delay);
	hotplug_timer_delay.function = march_hotplug_monitor;
	hotplug_timer_delay.data = 0;	// Target Core on which the timer function run.

	march_hotplug_task =
		kthread_create(on_run, NULL, "thread_hotplug");
	if (IS_ERR(march_hotplug_task)) {
		pr_err("Failed in creation of thread.\n");
		return -EINVAL;
	}

	for_each_possible_cpu(i) {
		struct cpu_load_info *pcpu = &per_cpu(cpuload, i);
		spin_lock_init(&pcpu->cpu_load_lock);
	}
	spin_lock_init(&march_lock);

	fb_register_client(&fb_block);

	pm_qos_add_notifier(PM_QOS_CLUSTER1_NUM_MAX, &exynos_cluster1_core_num_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CLUSTER1_NUM_MIN, &exynos_cluster1_core_num_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CLUSTER0_NUM_MAX, &exynos_cluster0_core_num_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CLUSTER0_NUM_MIN, &exynos_cluster0_core_num_qos_notifier);

	pm_qos_add_request(&cluster1_num_max_qos, PM_QOS_CLUSTER1_NUM_MAX, NR_CLUST1_CPUS);
	pm_qos_add_request(&cluster1_num_min_qos, PM_QOS_CLUSTER1_NUM_MIN, 0);
	pm_qos_add_request(&cluster0_num_max_qos, PM_QOS_CLUSTER0_NUM_MAX, NR_CLUST0_CPUS);
	pm_qos_add_request(&cluster0_num_min_qos, PM_QOS_CLUSTER0_NUM_MIN, 0);

	pm_qos_add_request(&cluster1_num_booster_min_qos, PM_QOS_CLUSTER1_NUM_MIN, 0);
	pm_qos_add_request(&cluster1_num_boot_min_qos, PM_QOS_CLUSTER1_NUM_MIN, 0);
	pm_qos_add_request(&cluster0_num_boot_min_qos, PM_QOS_CLUSTER0_NUM_MIN, 0);
	pm_qos_update_request_timeout(&cluster1_num_boot_min_qos, NR_CLUST1_CPUS, PM_BOOT_TIME_LEN * USEC_PER_SEC);
	pm_qos_update_request_timeout(&cluster0_num_boot_min_qos, NR_CLUST0_CPUS, PM_BOOT_TIME_LEN * USEC_PER_SEC);

	pm_qos_add_request(&sys_cluster1_num_max_qos, PM_QOS_CLUSTER1_NUM_MAX, NR_CLUST1_CPUS);
	pm_qos_add_request(&sys_cluster1_num_min_qos, PM_QOS_CLUSTER1_NUM_MIN, 0);
	pm_qos_add_request(&sys_cluster0_num_max_qos, PM_QOS_CLUSTER0_NUM_MAX, NR_CLUST0_CPUS);
	pm_qos_add_request(&sys_cluster0_num_min_qos, PM_QOS_CLUSTER0_NUM_MIN, 0);

	setup_hotplug_sysfs();

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	policy = cpufreq_cpu_get(NR_CLUST0_CPUS);
	if (!policy) {
		pr_err("%s: invaled policy cpu%d\n", __func__, NR_CLUST0_CPUS);
		ret = -ENODEV;
		goto err_policy;
	}
	cluster1_min_freq = policy->min;
	cpufreq_cpu_put(policy);
#endif

#if defined(CONFIG_SCHED_HMP)
	hotplug_cluster0_wq = create_singlethread_workqueue("event-hotplug-cluster0");
	if (!hotplug_cluster0_wq) {
		ret = -ENOMEM;
		goto err_wq;
	}

	hotplug_cluster1_wq = create_singlethread_workqueue("event-hotplug-cluster1");
	if (!hotplug_cluster1_wq) {
		ret = -ENOMEM;
		goto err_wq;
	}

#endif

	proc_march_dir = proc_mkdir("march", NULL);
	if (!proc_march_dir)
		return -ENOMEM;

	entry = proc_create("kernel_data", S_IRUGO|S_IWUGO, proc_march_dir,
			    &march_fileops);
	if (!entry)
		goto err_remove;

	init_march_kernel_data();
	init_waitqueue_head(&march_poll_wait);

	register_pm_notifier(&exynos_march_hotplug_nb);
	register_reboot_notifier(&exynos_march_hotplug_reboot_nb);

	hotplug_timer.expires =	expires;
	add_timer_on(&hotplug_timer, (int)(hotplug_timer.data));

	hotplug_timer_delay.expires =	expires;
	add_timer_on(&hotplug_timer_delay, (int)(hotplug_timer_delay.data));

	cpufreq_freq_transition.notifier_call = cpufreq_transition_notifier;
	cpufreq_register_notifier(&cpufreq_freq_transition,
				  CPUFREQ_TRANSITION_NOTIFIER);

	wake_up_process(march_hotplug_task);

	return ret;

err_remove:
	remove_proc_entry("kernel_data", proc_march_dir);

#if defined(CONFIG_SCHED_HMP)
err_wq:
	destroy_workqueue(hotplug_cluster0_wq);
	destroy_workqueue(hotplug_cluster1_wq);
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
err_policy:
#endif
	fb_unregister_client(&fb_block);
	kthread_stop(march_hotplug_task);

	return ret;
}

late_initcall(march_cpu_hotplug_init);
