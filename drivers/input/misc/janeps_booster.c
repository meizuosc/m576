#include <linux/pm_qos.h>
#include <linux/cpufreq.h>

#undef JANUARY_DEBUG
#ifdef JANUARY_DEBUG
#include <linux/ktime.h>
#endif

#ifdef CONFIG_KEY_BOOSTER
#include <linux/workqueue.h>
#endif

#ifdef CONFIG_FIRST_STAGE_BOOSTER
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>
#endif

#include "../../cpufreq/cpu_load_metric.h"

#include <linux/input/janeps_booster.h>


#define BOOSTER_NAME	"january"
#ifdef CONFIG_OF
static const struct of_device_id janeps_device_table[] = {
	{ .compatible = "samsung,january_booster" },
	{},
};
MODULE_DEVICE_TABLE(of, janeps_device_table);
#endif

MODULE_AUTHOR("Johnlay Park");
MODULE_DESCRIPTION("January Booster EPS driver");
MODULE_LICENSE("GPL");

#define JANUARY_MINOR	267	/* Major 10, Minor 267, /dev/janeps */

struct booster_req {
	struct pm_qos_request cpu_pmreq[CL_END];
	struct pm_qos_request bus_pmreq;
	struct pm_qos_request dev_pmreq;
#ifdef CONFIG_EXYNOS_GPU_PM_QOS
	struct pm_qos_request graphic_pmreq;
#endif
#ifdef CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG
	struct pm_qos_request nrcpu_pmreq[CL_END];
#endif
};

static bool	touched = false;
static wait_queue_head_t waitq_input;
static bool booster_enabled = false;

static DEFINE_MUTEX(janeps_mtx); // Critical Section for Process.
static spinlock_t janeps_lock;

static struct k_data kern_data;
static struct p_data boosted_data;
static janeps_seq    activity_seq = JAN_IDLED;

static struct booster_req booster_qos_req;

#ifdef JANUARY_DEBUG
static ktime_t Itime, Ptime, Rtime, Wtime;
#endif

#define set_qos(req, pm_qos_class, value, timeout) do { \
	if (pm_qos_request_active(req)) {\
		if((timeout)>0) pm_qos_update_request_timeout((req), (value), 1000*(timeout)); \
		else pm_qos_update_request((req), (value)); \
	} else {\
		pm_qos_add_request((req), (pm_qos_class), (value)); \
	} \
} while(0)

#define remove_qos(req) do { \
	if (pm_qos_request_active(req)) \
		pm_qos_remove_request(req); \
} while(0)

static char *jan_strcat(char *dest, const char *src)
{
	char *tmp = dest;

	while (*dest != '\0' && *dest != '\n' && *dest)
		dest++;
	*dest++ = ' ';
	while ((*dest++ = *src++) != '\0')
		;
	return tmp;
}

static inline void boost_handler(struct booster_req *qos_req, struct p_data *pdata)
{
	struct cpufreq_policy *policy;

#ifdef CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG
	if(pdata->nrcpu[CL_BIG].perf > 0) {
		set_qos(&(qos_req->nrcpu_pmreq[CL_BIG]),PM_QOS_CLUSTER1_NUM_MIN,
			pdata->nrcpu[CL_BIG].perf, pdata->nrcpu[CL_BIG].pulse_timeout);

		if(pdata->big_boost.perf == 2) {
			set_hmp_boostpulse(pdata->big_boost.pulse_timeout);
		} else if(pdata->big_boost.perf == 1) {
			set_hmp_semiboostpulse(pdata->big_boost.pulse_timeout);
		}

		set_qos(&(qos_req->nrcpu_pmreq[CL_LIT]),PM_QOS_CLUSTER0_NUM_MIN,
			pdata->nrcpu[CL_LIT].perf, pdata->nrcpu[CL_LIT].pulse_timeout);
	}
#endif

	policy = cpufreq_cpu_get(0);
	if (policy) {
		if (policy->cur < pdata->cpu[CL_LIT].perf) {
			cpufreq_driver_target(policy, pdata->cpu[CL_LIT].perf, CPUFREQ_RELATION_L);
		}
		cpufreq_cpu_put(policy);
	}
	set_qos(&(qos_req->cpu_pmreq[CL_LIT]),PM_QOS_CLUSTER0_FREQ_MIN,
		pdata->cpu[CL_LIT].perf, pdata->cpu[CL_LIT].pulse_timeout);

	if (cpu_online(4)) {
		policy = cpufreq_cpu_get(4);
		if (policy) {
			if (policy->cur < pdata->cpu[CL_BIG].perf) {
				cpufreq_driver_target(policy, pdata->cpu[CL_BIG].perf, CPUFREQ_RELATION_L);
			}
			cpufreq_cpu_put(policy);
		}
	}
	set_qos(&(qos_req->cpu_pmreq[CL_BIG]),PM_QOS_CLUSTER1_FREQ_MIN,
		pdata->cpu[CL_BIG].perf, pdata->cpu[CL_BIG].pulse_timeout);

	set_qos(&qos_req->bus_pmreq,PM_QOS_BUS_THROUGHPUT,
		pdata->bus.perf, pdata->bus.pulse_timeout);
	set_qos(&qos_req->dev_pmreq,PM_QOS_DEVICE_THROUGHPUT,
		pdata->device.perf, pdata->device.pulse_timeout);

#ifdef CONFIG_EXYNOS_GPU_PM_QOS
	set_qos(&qos_req->graphic_pmreq, PM_QOS_GPU_FREQ_MIN,
		pdata->graphic.perf, pdata->graphic.pulse_timeout);
#endif

#ifdef CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG
	if(pdata->nrcpu[CL_BIG].perf == 0) {
		set_qos(&(qos_req->nrcpu_pmreq[CL_BIG]),PM_QOS_CLUSTER1_NUM_MIN,
			pdata->nrcpu[CL_BIG].perf, pdata->nrcpu[CL_BIG].pulse_timeout);

		if(pdata->big_boost.perf == 2) {
	 		set_hmp_boostpulse(pdata->big_boost.pulse_timeout);
		} else if(pdata->big_boost.perf == 1) {
	 		set_hmp_semiboostpulse(pdata->big_boost.pulse_timeout);
		}

		set_qos(&(qos_req->nrcpu_pmreq[CL_LIT]),PM_QOS_CLUSTER0_NUM_MIN,
			pdata->nrcpu[CL_LIT].perf, pdata->nrcpu[CL_LIT].pulse_timeout);
	}
#endif
}

#ifdef CONFIG_KEY_BOOSTER
struct key_info_data {
	unsigned int enable;
};

static struct booster_req   key_pmreq;
static struct p_data        key_p_data;
static struct key_info_data key_info;

static struct workqueue_struct *key_wq;
static struct work_struct       key_work;

static void key_boost_worker(struct work_struct *data)
{
	boost_handler(&key_pmreq, &key_p_data);
}

static void key_booster_handle(void) {
	if (key_info.enable == 1) {
		queue_work(key_wq, &key_work);
	}
}

static void key_booster_init(void) {
	pm_qos_add_request(&key_pmreq.cpu_pmreq[CL_BIG], PM_QOS_CLUSTER1_FREQ_MIN, 0);
	pm_qos_add_request(&key_pmreq.cpu_pmreq[CL_LIT], PM_QOS_CLUSTER0_FREQ_MIN, 0);
	pm_qos_add_request(&key_pmreq.bus_pmreq, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&key_pmreq.dev_pmreq, PM_QOS_DEVICE_THROUGHPUT, 0);
#ifdef CONFIG_EXYNOS_GPU_PM_QOS
	pm_qos_add_request(&key_pmreq.graphic_pmreq, PM_QOS_GPU_FREQ_MIN, 0);
#endif
#ifdef CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG
	pm_qos_add_request(&key_pmreq.nrcpu_pmreq[CL_BIG], PM_QOS_CLUSTER1_NUM_MIN, 0);
	pm_qos_add_request(&key_pmreq.nrcpu_pmreq[CL_LIT], PM_QOS_CLUSTER0_NUM_MIN, 0);
#endif

	key_p_data.cpu[CL_BIG].perf   = 800000;
	key_p_data.cpu[CL_LIT].perf   = 1500000;
	key_p_data.bus.perf           = 1026000;
	key_p_data.device.perf        = 200000;
	key_p_data.graphic.perf       = 350;
	key_p_data.nrcpu[CL_BIG].perf = 0;
	key_p_data.nrcpu[CL_LIT].perf = 4;
	key_p_data.big_boost.perf     = 1;

	key_p_data.cpu[CL_BIG].pulse_timeout   = 200;
	key_p_data.cpu[CL_LIT].pulse_timeout   = 200;
	key_p_data.bus.pulse_timeout           = 200;
	key_p_data.device.pulse_timeout        = 200;
	key_p_data.graphic.pulse_timeout       = 200;
	key_p_data.nrcpu[CL_BIG].pulse_timeout = 200;
	key_p_data.nrcpu[CL_LIT].pulse_timeout = 200;
	key_p_data.big_boost.pulse_timeout     = 200;

	key_info.enable = 1;

	key_wq = alloc_workqueue("key_booster_wq", 0, 1);
	if (key_wq == NULL) {
		printk("[ERROR] KEY_BOOSTER: failed to alloc work queue!\r\n");
		goto out;
	}

	INIT_WORK(&key_work, key_boost_worker);
	return ;

out:
	pm_qos_remove_request(&key_pmreq.cpu_pmreq[CL_BIG]);
	pm_qos_remove_request(&key_pmreq.cpu_pmreq[CL_LIT]);
	pm_qos_remove_request(&key_pmreq.bus_pmreq);
	pm_qos_remove_request(&key_pmreq.dev_pmreq);
#ifdef CONFIG_EXYNOS_GPU_PM_QOS
	pm_qos_remove_request(&key_pmreq.graphic_pmreq);
#endif
#ifdef CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG
	pm_qos_remove_request(&key_pmreq.nrcpu_pmreq[CL_BIG]);
	pm_qos_remove_request(&key_pmreq.nrcpu_pmreq[CL_LIT]);
#endif
}
#endif // end of CONFIG_KEY_BOOSTER


#ifdef CONFIG_FIRST_STAGE_BOOSTER
struct first_stage_info_data {
	struct task_struct *task;
	wait_queue_head_t wait_queue;
	int status;

	unsigned int enable;
};

enum {
	FIRST_STAGE_BOOSTE_IDLE,
	FIRST_STAGE_BOOSTE_START,
	FIRST_STAGE_BOOSTE_HANDLE
};

static struct booster_req           first_stage_pmreq;
static struct p_data                first_stage_p_data;
static struct first_stage_info_data first_stage_info;

static void start_first_stage_boost(void) {
	if ((first_stage_info.enable == 1)
		&& (first_stage_info.status == FIRST_STAGE_BOOSTE_IDLE)) {
		first_stage_info.status = FIRST_STAGE_BOOSTE_START;
		wake_up(&first_stage_info.wait_queue);
	}
}

static void stop_first_stage_boost(void) {
	first_stage_info.status = FIRST_STAGE_BOOSTE_IDLE;
}

static int first_stage_boost_fn(void *data)
{
	struct first_stage_info_data *info = (struct first_stage_info_data *)data;
	int ret = 0;

	while (1) {
		ret = wait_event_interruptible(info->wait_queue, info->status == FIRST_STAGE_BOOSTE_START);
		if (!ret) {
			info->status = FIRST_STAGE_BOOSTE_HANDLE;

			boost_handler(&first_stage_pmreq, &first_stage_p_data);
		}

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static void first_stage_boost_init(void) {
	struct sched_param first_stage_task_param = { .sched_priority = MAX_RT_PRIO - 1 };
	
	pm_qos_add_request(&first_stage_pmreq.cpu_pmreq[CL_BIG], PM_QOS_CLUSTER1_FREQ_MIN, 0);
	pm_qos_add_request(&first_stage_pmreq.cpu_pmreq[CL_LIT], PM_QOS_CLUSTER0_FREQ_MIN, 0);
	pm_qos_add_request(&first_stage_pmreq.bus_pmreq, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&first_stage_pmreq.dev_pmreq, PM_QOS_DEVICE_THROUGHPUT, 0);
#ifdef CONFIG_EXYNOS_GPU_PM_QOS
	pm_qos_add_request(&first_stage_pmreq.graphic_pmreq, PM_QOS_GPU_FREQ_MIN, 0);
#endif
#ifdef CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG
	pm_qos_add_request(&first_stage_pmreq.nrcpu_pmreq[CL_BIG], PM_QOS_CLUSTER1_NUM_MIN, 0);
	pm_qos_add_request(&first_stage_pmreq.nrcpu_pmreq[CL_LIT], PM_QOS_CLUSTER0_NUM_MIN, 0);
#endif

	first_stage_p_data.cpu[CL_BIG].perf   = 800000;
	first_stage_p_data.cpu[CL_LIT].perf   = 1000000;
	first_stage_p_data.bus.perf           = 543000;
	first_stage_p_data.device.perf        = 200000;
	first_stage_p_data.graphic.perf       = 266;
	first_stage_p_data.nrcpu[CL_BIG].perf = 0;
	first_stage_p_data.nrcpu[CL_LIT].perf = 3;
	first_stage_p_data.big_boost.perf     = 1;

	first_stage_p_data.cpu[CL_BIG].pulse_timeout   = 50;
	first_stage_p_data.cpu[CL_LIT].pulse_timeout   = 50;
	first_stage_p_data.bus.pulse_timeout           = 50;
	first_stage_p_data.device.pulse_timeout        = 50;
	first_stage_p_data.graphic.pulse_timeout       = 50;
	first_stage_p_data.nrcpu[CL_BIG].pulse_timeout = 50;
	first_stage_p_data.nrcpu[CL_LIT].pulse_timeout = 50;
	first_stage_p_data.big_boost.pulse_timeout     = 50;

	first_stage_info.enable = 1;
	first_stage_info.status = FIRST_STAGE_BOOSTE_IDLE;

	init_waitqueue_head(&first_stage_info.wait_queue);
	first_stage_info.task = kthread_create(first_stage_boost_fn, &first_stage_info, "first_stage_booster");
	sched_setscheduler_nocheck(first_stage_info.task, SCHED_FIFO, &first_stage_task_param);
	get_task_struct(first_stage_info.task);
	wake_up_process(first_stage_info.task);
}
#endif // end of CONFIG_FIRST_STAGE_BOOSTER

/*
  *  Delayed job type :
  *      0 : None
  *      1 : Invalid Touch
  *      2 : Boost after release
  */
static struct delayed_work delayed_release_work;
static void delayed_finish_off(struct work_struct *work)
{
	unsigned long 		flags;
	struct cluster_stats	c_stats[2];
	
	get_cluster_stats(c_stats);

	spin_lock_irqsave(&janeps_lock,flags);
	kern_data.status = RELEASE;
	kern_data.x = 0;
	kern_data.y = 0;
	kern_data.avg_load[CL_LIT] = (c_stats[CL_LIT].util)>>2; // Divided by # of Core in a Cluster
	kern_data.avg_load[CL_BIG] = (c_stats[CL_BIG].util)>>2; // Divided by # of Core in a Cluster

	kern_data.peak_load[CL_LIT] = 0; // TBC : Peak load occupancy ratio
	kern_data.peak_load[CL_BIG] = 0; // TBC : Peak load occupancy ratio

	if(!touched) {
		touched = true;
		spin_unlock_irqrestore(&janeps_lock,flags);
		wake_up_interruptible(&waitq_input);
		spin_lock_irqsave(&janeps_lock,flags);
		activity_seq = JAN_NOTIFIED;
	}
	spin_unlock_irqrestore(&janeps_lock,flags);

	return;
}

extern struct cpumask hmp_fast_cpu_mask;
extern struct cpumask hmp_slow_cpu_mask;

static int get_online_cpu_num_on_cluster(int cluster)
{
	struct cpumask dst_cpumask;

	cpumask_and(&dst_cpumask, (cluster == CL_BIG) ? &hmp_fast_cpu_mask : &hmp_slow_cpu_mask, cpu_online_mask);

	return cpumask_weight(&dst_cpumask);
}

int janeps_input_report(event_type evt_type, Coord_t x, Coord_t y)
{
	/* TODO : wrapping the useful data */
	/* X, Y, Peak ratio, Cluster Ratio
	 */
	unsigned long 		flags;
	struct cluster_stats	c_stats[2];

#ifdef CONFIG_KEY_BOOSTER
	if (evt_type != DOWN && evt_type != UP
		&& activity_seq == JAN_IDLED && !booster_enabled)
#else
	if (activity_seq == JAN_IDLED && !booster_enabled)
#endif
		return 1;

#ifdef CONFIG_FIRST_STAGE_BOOSTER
#ifdef CONFIG_KEY_BOOSTER
	if (evt_type == PRESS || evt_type == DOWN) {
#else
	if (evt_type == PRESS) {
#endif
		start_first_stage_boost();
#ifdef CONFIG_KEY_BOOSTER
	} else if (evt_type == RELEASE || evt_type == UP) {
#else
	} else if (evt_type == RELEASE) {
#endif
		stop_first_stage_boost();
	}
#endif

#ifdef CONFIG_KEY_BOOSTER
	if (evt_type == DOWN || evt_type == UP) {
		key_booster_handle();
		return 1;
	}
#endif

	get_cluster_stats(c_stats);

	spin_lock_irqsave(&janeps_lock,flags);
	if(activity_seq >= JAN_LOAD_RPT || activity_seq <= JAN_IDLED) {
		if(delayed_work_pending(&delayed_release_work)) {
			cancel_delayed_work(&delayed_release_work);
		}

		kern_data.status = evt_type;
		kern_data.x = x;
		kern_data.y = y;
		kern_data.avg_load[CL_LIT] = (c_stats[CL_LIT].util)/4;;	// Divided by # of Core in a Cluster
		kern_data.avg_load[CL_BIG] = (c_stats[CL_BIG].util)/get_online_cpu_num_on_cluster(CL_BIG);	// Divided by # of Core in a Cluster

		kern_data.peak_load[CL_LIT] = 0; // TBC : Peak load occupancy ratio
		kern_data.peak_load[CL_BIG] = 0; // TBC : Peak load occupancy ratio
	} else {
		spin_unlock_irqrestore(&janeps_lock,flags);
#ifdef CONFIG_SCHED_HMP
		mod_delayed_work_on(0,
				system_freezable_wq,
				&delayed_release_work,
				msecs_to_jiffies(100));
#else
		queue_delayed_work(system_freezable_wq,
				&delayed_release_work,
				msecs_to_jiffies(100));
#endif
		return 1;
	}

	if(!touched) {
		touched = true;
		spin_unlock_irqrestore(&janeps_lock,flags);
#ifdef JANUARY_DEBUG
		printk("[JANUARY Trace] %d (%d %d)(%d %d)\n",
			kern_data.status, kern_data.x, kern_data.y,
			kern_data.avg_load[CL_LIT], kern_data.avg_load[CL_BIG]);
		Itime = ktime_get();
#endif
		wake_up_interruptible(&waitq_input);
		spin_lock_irqsave(&janeps_lock,flags);
		activity_seq = JAN_NOTIFIED;
	}

	spin_unlock_irqrestore(&janeps_lock,flags);

	return 1;
}
EXPORT_SYMBOL_GPL(janeps_input_report);


static int parameter_delivered = 0;

char janeps_cmd[600];
char janeps_parameter[500];

static ssize_t set_parameters(struct device *dev, struct device_attribute *attr,
                                          const char *buf, size_t count)
 	{
	unsigned long 	flags;
	spin_lock_irqsave(&janeps_lock,flags);
	strcpy(janeps_parameter, buf);
	spin_unlock_irqrestore(&janeps_lock,flags);
	return strlen(janeps_parameter);
}

static ssize_t commit_paramters(struct device *dev, struct device_attribute *attr,
                                          const char *buf, size_t count)
{
	unsigned long 	flags;
	spin_lock_irqsave(&janeps_lock,flags);
	strcpy(janeps_cmd, buf);
	if(parameter_delivered == 0) {
		parameter_delivered = 2;
		wake_up_interruptible(&waitq_input);
	}
	spin_unlock_irqrestore(&janeps_lock,flags);
	return strlen(janeps_cmd);
}

static ssize_t show_enable(struct device *dev,
									struct device_attribute *attr, char *buf)
{
	if(booster_enabled)
		return sprintf(buf, "Enabled\n");
	else
		return sprintf(buf, "Disabled\n");
}

static ssize_t store_enable(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	if (buf[0]!='0')
		booster_enabled = true;
	else
		booster_enabled = false;
	return count;
}

#ifdef CONFIG_FIRST_STAGE_BOOSTER
static ssize_t show_first_boost_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", first_stage_info.enable);
}

static ssize_t store_first_boost_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int enable = 0;

	sscanf(buf, "%u", &enable);
	first_stage_info.enable = !!enable;

	return count;
}
#endif

#ifdef CONFIG_KEY_BOOSTER
static ssize_t show_key_boost_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", key_info.enable);
}

static ssize_t store_key_boost_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int enable = 0;

	sscanf(buf, "%u", &enable);
	key_info.enable = !!enable;

	return count;
}
#endif

#ifdef CONFIG_FIRST_STAGE_BOOSTER
static DEVICE_ATTR(first_boost_enable, S_IRUGO|S_IWUSR, show_first_boost_enable, store_first_boost_enable);
#endif
#ifdef CONFIG_KEY_BOOSTER
static DEVICE_ATTR(key_boost_enable, S_IRUGO|S_IWUSR, show_key_boost_enable, store_key_boost_enable);
#endif
static DEVICE_ATTR(params, 0220, NULL, set_parameters);
static DEVICE_ATTR(command, 0220, NULL, commit_paramters);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR, show_enable, store_enable);

static struct attribute *janeps_attrs_entries[] = {
	&dev_attr_params.attr,
	&dev_attr_command.attr,
	&dev_attr_enable.attr,
#ifdef CONFIG_FIRST_STAGE_BOOSTER
	&dev_attr_first_boost_enable.attr,
#endif
#ifdef CONFIG_KEY_BOOSTER
	&dev_attr_key_boost_enable.attr,
#endif
	NULL,
};

struct attribute_group janeps_attr_group = {
	.name   = "tunnables",
	.attrs  = janeps_attrs_entries,
};

#define MAGIC_CODE		0xDADBABE
static bool tunables_delivered = false;

struct tunables_janeps {
	unsigned int big_perf;
	unsigned int little_perf;
	unsigned int bus_perf;
	unsigned int device_perf;
	unsigned int graphics_perf;
	unsigned int big_core_count;
	unsigned int little_core_count;

	unsigned int repeativity;
	unsigned int renew_period;
	unsigned int tail_period;
	unsigned int upper_threshold;
	unsigned int default_threshold;
	unsigned int bottom_threshold;
	unsigned int upper_cnt[CL_END];
	unsigned int down_differential;

	unsigned int big_margin;
	unsigned int little_margin;
};

static struct tunables_janeps default_param;

static ssize_t janeps_read(struct file *file, char __user *buffer,
			size_t count, loff_t *ppos)
{
	ssize_t 		retval;
	unsigned long 	flags;

#ifdef JANUARY_DEBUG
	Rtime = ktime_get();
#endif

	if (tunables_delivered) {
		retval = sizeof(struct tunables_janeps);
		spin_lock_irqsave(&janeps_lock,flags);
		tunables_delivered = false;
		if (copy_to_user(buffer, &default_param, retval)) {
			spin_unlock_irqrestore(&janeps_lock,flags);
			printk(KERN_ERR "Tunnables copy_to_user() failed!\n");

			return -EFAULT;
		}
		spin_unlock_irqrestore(&janeps_lock,flags);
	} else if (parameter_delivered == 1) {
		char * concat_str;
		spin_lock_irqsave(&janeps_lock,flags);
		parameter_delivered = 0;
		concat_str = jan_strcat(janeps_cmd, janeps_parameter);
		retval = strlen(concat_str);
		if (copy_to_user(buffer, concat_str, retval)) {
			spin_unlock_irqrestore(&janeps_lock,flags);
			printk(KERN_ERR "Parameters copy_to_user() failed!\n");

			return -EFAULT;
		}

		janeps_cmd[0] = '\0';
		janeps_parameter[0] = '\0';
		spin_unlock_irqrestore(&janeps_lock,flags);
	} else {
		/* read the cluster usage data from the cpufreq */
		/* report utils_data (status, x, y) and cluster usage*/
		retval = sizeof(struct k_data);

		spin_lock_irqsave(&janeps_lock,flags);

		if (copy_to_user(buffer, &kern_data, retval)) {
			spin_unlock_irqrestore(&janeps_lock,flags);
			printk(KERN_ERR "System data copy_to_user() failed!\n");

			return -EFAULT;
		}

		activity_seq = JAN_LOAD_RPT;
		spin_unlock_irqrestore(&janeps_lock,flags);
	}
	return retval;
}

static ssize_t janeps_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *ppos)
{
	unsigned long 	flags;

#ifdef JANUARY_DEBUG
	Wtime = ktime_get();

	printk("[JANUARY Trace] %ld %ld %ld \n",
		(long int)ktime_to_us(ktime_sub(Ptime, Itime)),
		(long int)ktime_to_us(ktime_sub(Rtime, Ptime)),
		(long int)ktime_to_us(ktime_sub(Wtime, Rtime)));
#endif

	if (count != sizeof(struct p_data))
		return -1;

	memset(&boosted_data, 0, sizeof(struct p_data));

	if (copy_from_user((void *)(&boosted_data), buffer, count)) {
		printk(KERN_ERR " copy_from_user() failed!\n");
		return -EFAULT;
	}

	if (boosted_data.cpu[CL_BIG].perf == MAGIC_CODE) {
		tunables_delivered = true;
		return count;
	}

	spin_lock_irqsave(&janeps_lock,flags);
	activity_seq = JAN_PERF_DLV;
	spin_unlock_irqrestore(&janeps_lock,flags);

	boost_handler(&booster_qos_req, &boosted_data);

	spin_lock_irqsave(&janeps_lock, flags);
	activity_seq = JAN_PERF_REQ;
	spin_unlock_irqrestore(&janeps_lock, flags);

#ifdef JANUARY_DEBUG
	printk("B:%d.%d ", boosted_data.cpu[CL_BIG].perf/1000, (unsigned int)boosted_data.cpu[CL_BIG].pulse_timeout);
	printk("L:%d.%d ", boosted_data.cpu[CL_LIT].perf/1000, (unsigned int)boosted_data.cpu[CL_LIT].pulse_timeout);
	printk("M:%d.%d ", boosted_data.bus.perf/1000, (unsigned int)boosted_data.bus.pulse_timeout);
	printk("I:%d.%d ", boosted_data.device.perf/1000, (unsigned int)boosted_data.device.pulse_timeout);
	printk("G:%d.%d ", boosted_data.graphic.perf/1000, (unsigned int)boosted_data.graphic.pulse_timeout);
#ifdef CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG
	printk("C:%d.%d/%d.%d\n", boosted_data.nrcpu[CL_BIG].perf, (unsigned int)boosted_data.nrcpu[CL_BIG].pulse_timeout,
		boosted_data.nrcpu[CL_LIT].perf, (unsigned int)boosted_data.nrcpu[CL_LIT].pulse_timeout);
#endif
#endif // JANUARY_DEBUG
	spin_lock_irqsave(&janeps_lock, flags);
	activity_seq = JAN_IDLED;
	spin_unlock_irqrestore(&janeps_lock, flags);
	return count;
}

static int janeps_open(struct inode *inode, struct file *file)
{
	int ret=0;

	mutex_lock(&janeps_mtx);
	/* TODO : janeps initialization */
	booster_enabled = true;


	mutex_unlock(&janeps_mtx);
	return ret;
}

static int janeps_release(struct inode *inode, struct file *file)
{

	remove_qos(&(booster_qos_req.cpu_pmreq[CL_BIG]));
	remove_qos(&(booster_qos_req.cpu_pmreq[CL_LIT]));

	remove_qos(&booster_qos_req.bus_pmreq);
	remove_qos(&booster_qos_req.dev_pmreq);

#ifdef CONFIG_EXYNOS_GPU_PM_QOS
	remove_qos(&booster_qos_req.graphic_pmreq);
#endif

#ifdef CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG
	remove_qos(&(booster_qos_req.nrcpu_pmreq[CL_BIG]));
	remove_qos(&(booster_qos_req.nrcpu_pmreq[CL_LIT]));
#endif

	return 0;
}

static long janeps_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	return 0;
}

static u32 janeps_poll(struct file *file, poll_table *wait)
{
	int mask = 0;
	unsigned long 	flags;

	poll_wait(file, &waitq_input, wait);

	spin_lock_irqsave(&janeps_lock,flags);
	if (parameter_delivered == 2) {
		parameter_delivered--;
		mask |= POLLPRI;
	}

	if(touched) {
		touched = false;
		spin_unlock_irqrestore(&janeps_lock,flags);
		mask |= POLLIN;
#ifdef JANUARY_DEBUG
		Ptime = ktime_get();
#endif
	} else {
		spin_unlock_irqrestore(&janeps_lock,flags);
	}

	return mask;
}

static const struct file_operations janeps_fops = {
	.owner   = THIS_MODULE,
	.open    = janeps_open,
	.release = janeps_release,
	.read    = janeps_read,
	.write   = janeps_write,
	.unlocked_ioctl = janeps_ioctl,
	.poll    = janeps_poll,
};

static struct miscdevice janeps_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "january",
	.fops  = &janeps_fops,
};

static int janeps_probe(struct platform_device *pdev)
{
	int ret = 0;
	if (misc_register(&janeps_misc_device)) {
		printk(KERN_WARNING " Couldn't register device 10, %d.\n",
			JANUARY_MINOR);

		return -EBUSY;
	}

	spin_lock_init(&janeps_lock);

	touched = false;
	init_waitqueue_head(&waitq_input);

	INIT_DELAYED_WORK(&delayed_release_work, delayed_finish_off);

	ret = sysfs_create_group(&pdev->dev.kobj, &janeps_attr_group);
	if (ret) {
		pr_err("%s: failed to create request_vsync sysfs interface\n",
			__func__);
		return ret;
	}

	/* Default parameter settings */
	default_param.big_perf  = 800000;
	default_param.little_perf = 1000000;
	default_param.bus_perf = 500000;
	default_param.device_perf = 100000;
	default_param.graphics_perf = 133;
	default_param.big_core_count = 0;
	default_param.little_core_count = 4;

	default_param.repeativity = 100;
	default_param.renew_period = 20;
	default_param.tail_period = 100;
	default_param.upper_threshold = 99;
	default_param.default_threshold = 90;
	default_param.bottom_threshold = 70;
	default_param.upper_cnt[CL_BIG] = 100;
	default_param.upper_cnt[CL_LIT] = 10;
	default_param.down_differential = 90;

	default_param.big_margin = 400000;
	default_param.little_margin = 200000;

#ifdef CONFIG_KEY_BOOSTER
	key_booster_init();
#endif

#ifdef CONFIG_FIRST_STAGE_BOOSTER
	first_stage_boost_init();
#endif

	return 0;
}

static int janeps_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static int janeps_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int janeps_resume(struct platform_device *dev)
{
	return 0;
}
#else
#define janeps_suspend NULL
#define janeps_resume NULL
#endif

static struct platform_driver janeps_driver = {
	.probe		= janeps_probe,
	.remove         = janeps_remove,
	.suspend	= janeps_suspend,
	.resume		= janeps_resume,
	.driver		= {
		.name	= "janeps_booster",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(janeps_device_table),
	},
};

static int __init janeps_init(void)
{
	int ret;

	ret = platform_driver_register(&janeps_driver);

	if (ret) {
		printk(KERN_ERR "Platform Device Register Failed %d\n", ret);
		return -1;
	}

	return 0;
}

static void __exit janeps_exit(void)
{
	platform_driver_unregister(&janeps_driver);
}

module_init(janeps_init);
module_exit(janeps_exit);

