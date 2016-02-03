#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/of.h>

#include <linux/pm_qos.h>
#include <linux/cpufreq.h>

#include <linux/ktime.h>

#include <linux/workqueue.h>

#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>

#define KEY_BOOSTER_NAME "key_booster"

#ifdef CONFIG_OF
static const struct of_device_id key_booster_device_table[] = {
	{ .compatible = "meizu,key_booster" },
	{},
};
MODULE_DEVICE_TABLE(of, key_booster_device_table);
#endif

typedef enum {
	CL_LIT = 0,
	CL_BIG,
	CL_END,
} cl_type;

struct key_booster_info_data {
	struct task_struct *task;
	wait_queue_head_t wait_queue;
	int status;

	unsigned int enable;
};

struct qos_perf {
	unsigned int perf;	        // Frequency or # of CPU
	unsigned long pulse_timeout;	// If not zero, request_pm_qos_timeout
};

struct p_data {
	struct qos_perf cpu[CL_END];		// For PM_QOS_KFC_FREQ_MIN, PM_QOS_CPU_FREQ_MIN
	struct qos_perf bus;			// For PM_QOS_BUS_THROUGHPUT
	struct qos_perf device; 		// For PM_QOS_DEVICE_THROUGHT
	struct qos_perf graphic;		// For PM_QOS_GPU_THROUGHT
	struct qos_perf nrcpu[CL_END];
	struct qos_perf big_boost;		// 0: Non, 1: SemiBoost, 2: Boost
};

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

enum {
	KEY_BOOSTE_IDLE,
	KEY_BOOSTE_START,
	KEY_BOOSTE_HANDLE
};

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

static struct booster_req           key_booster_pmreq;
static struct p_data                key_booster_p_data;
static struct key_booster_info_data key_booster_info;

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

static void start_key_booster_boost(void) {
	if ((key_booster_info.enable == 1)
		&& (key_booster_info.status == KEY_BOOSTE_IDLE)) {
		key_booster_info.status = KEY_BOOSTE_START;
		wake_up(&key_booster_info.wait_queue);
	}
}

static void stop_key_booster_boost(void) {
	key_booster_info.status = KEY_BOOSTE_IDLE;
}

static int key_booster_boost_fn(void *data)
{
	struct key_booster_info_data *info = (struct key_booster_info_data *)data;
	int ret = 0;

	while (1) {
		ret = wait_event_interruptible(info->wait_queue, info->status == KEY_BOOSTE_START);
		if (!ret) {
			info->status = KEY_BOOSTE_HANDLE;

			boost_handler(&key_booster_pmreq, &key_booster_p_data);
		}

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static void key_booster_boost_init(void) {
	struct sched_param key_booster_task_param = { .sched_priority = MAX_RT_PRIO - 1 };

	pm_qos_add_request(&key_booster_pmreq.cpu_pmreq[CL_BIG], PM_QOS_CLUSTER1_FREQ_MIN, 0);
	pm_qos_add_request(&key_booster_pmreq.cpu_pmreq[CL_LIT], PM_QOS_CLUSTER0_FREQ_MIN, 0);
	pm_qos_add_request(&key_booster_pmreq.bus_pmreq, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&key_booster_pmreq.dev_pmreq, PM_QOS_DEVICE_THROUGHPUT, 0);
#ifdef CONFIG_EXYNOS_GPU_PM_QOS
	pm_qos_add_request(&key_booster_pmreq.graphic_pmreq, PM_QOS_GPU_FREQ_MIN, 0);
#endif
#ifdef CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG
	pm_qos_add_request(&key_booster_pmreq.nrcpu_pmreq[CL_BIG], PM_QOS_CLUSTER1_NUM_MIN, 0);
	pm_qos_add_request(&key_booster_pmreq.nrcpu_pmreq[CL_LIT], PM_QOS_CLUSTER0_NUM_MIN, 0);
#endif

	key_booster_p_data.cpu[CL_BIG].perf   = 800000;
	key_booster_p_data.cpu[CL_LIT].perf   = 1000000;
	key_booster_p_data.bus.perf           = 543000;
	key_booster_p_data.device.perf        = 200000;
	key_booster_p_data.graphic.perf       = 266;
	key_booster_p_data.nrcpu[CL_BIG].perf = 0;
	key_booster_p_data.nrcpu[CL_LIT].perf = 3;
	key_booster_p_data.big_boost.perf     = 1;

	key_booster_p_data.cpu[CL_BIG].pulse_timeout   = 50;
	key_booster_p_data.cpu[CL_LIT].pulse_timeout   = 50;
	key_booster_p_data.bus.pulse_timeout           = 50;
	key_booster_p_data.device.pulse_timeout        = 50;
	key_booster_p_data.graphic.pulse_timeout       = 50;
	key_booster_p_data.nrcpu[CL_BIG].pulse_timeout = 50;
	key_booster_p_data.nrcpu[CL_LIT].pulse_timeout = 50;
	key_booster_p_data.big_boost.pulse_timeout     = 50;

	key_booster_info.status = KEY_BOOSTE_IDLE;
	key_booster_info.enable = 1;

	init_waitqueue_head(&key_booster_info.wait_queue);
	key_booster_info.task = kthread_create(key_booster_boost_fn, &key_booster_info, "input_key_booster_booster");
	sched_setscheduler_nocheck(key_booster_info.task, SCHED_FIFO, &key_booster_task_param);
	get_task_struct(key_booster_info.task);
	wake_up_process(key_booster_info.task);
}

int key_input_report(int evt_type)
{
	if (evt_type == 1) {
		start_key_booster_boost();
	} else if (evt_type == 0) {
		stop_key_booster_boost();
	}

	return 0;
}

static ssize_t show_key_boost_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", key_booster_info.enable);
}

static ssize_t store_key_boost_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int enable = 0;

	sscanf(buf, "%u", &enable);
	key_booster_info.enable = !!enable;

	return count;
}

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR, show_key_boost_enable, store_key_boost_enable);

static struct attribute *key_booster_attrs_entries[] = {
	&dev_attr_enable.attr,
	NULL,
};

struct attribute_group key_booster_attr_group = {
	.name   = "tunnables",
	.attrs  = key_booster_attrs_entries,
};

static int key_booster_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_create_group(&pdev->dev.kobj, &key_booster_attr_group);
	if (ret) {
		pr_err("%s: failed to create request_vsync sysfs interface\n",
			__func__);
		return ret;
	}

	key_booster_boost_init();

	printk("[Info] Succeed to start key booster device!\r\n");

	return 0;
}

static int key_booster_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static int key_booster_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int key_booster_resume(struct platform_device *dev)
{
	return 0;
}
#else
#define key_booster_suspend NULL
#define key_booster_resume NULL
#endif

static struct platform_driver key_booster_driver = {
	.probe		= key_booster_probe,
	.remove         = key_booster_remove,
	.suspend	= key_booster_suspend,
	.resume		= key_booster_resume,
	.driver		= {
		.name	= KEY_BOOSTER_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(key_booster_device_table),
#endif
	},
};

static int __init key_booster_init(void)
{
	int ret;

	ret = platform_driver_register(&key_booster_driver);

	if (ret) {
		printk(KERN_ERR "Key Booster Platform Device Register Failed %d\n", ret);
		return -1;
	}

	return 0;
}

static void __exit key_booster_exit(void)
{
	platform_driver_unregister(&key_booster_driver);
}

module_init(key_booster_init);
module_exit(key_booster_exit);

MODULE_AUTHOR("Bobo");
MODULE_DESCRIPTION("Alone Key Booster EPS driver");
MODULE_LICENSE("GPL");
