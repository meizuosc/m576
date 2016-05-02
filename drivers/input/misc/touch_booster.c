#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/cpufreq.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>
#include <linux/wait.h>

#include <plat/cpu.h>

#include <linux/perf_mode.h>

#include <linux/input/touch_booster.h>


#define DEFAULT_BOOST_TIME_MS     (1 * MSEC_PER_SEC)
#define DEFAULT_BOOST_HMP_TIME_MS (DEFAULT_BOOST_TIME_MS * 3)
#define MAX_BOOST_DEVICE		2

static unsigned int boost_device_count = 0;

static unsigned int multi_timeout = 1;  /* for screen off entend delay timeout*/
static DEFINE_MUTEX(touch_lock);

enum {
	EVENT_FLICK = 0,
	EVENT_DOWN  = 1,
	EVENT_UP    = 2,
	EVENT_NONE  = 3,
};

enum {
	KEYS_BOOST = 0,
	TOUCH_BOOST,
};

static int boost_time_multi[] = {1, 2, 2};
static int boost_perf_mode[] = {PERF_MODE_LOW, PERF_MODE_NORMAL, PERF_MODE_NORMAL};
static int boost_qos_mode[] = {PM_QOS_EVENT_FLICK, PM_QOS_EVENT_DOWN, PM_QOS_EVENT_UP};
static int hmp_boost_type[] = {HMP_BOOST_SEMI, HMP_BOOST_SEMI, HMP_BOOST_SEMI};

static unsigned int isBoosterEnable = 1;

extern void hps_touch_boost(int big_cores);

static void start_touch_boost(struct tb_private_info *info)
{
	if (!info->task_started) {
		//hps_touch_boost(1);

		atomic_set(&info->is_start, 1);
		if (info->boost_type == TOUCH_BOOST && info->event_type == EVENT_UP)
			atomic_set(&info->is_ondemand, 1);
		wake_up(&info->wait_queue);

		info->task_started = 1;
	}
}

static void stop_touch_boost(struct tb_private_info *info)
{
	if (info->task_started) {
		//hps_touch_boost(0);

		if (info->boost_type == TOUCH_BOOST)
			atomic_set(&info->is_ondemand, 0);

		atomic_set(&info->is_start, 0);
		info->task_started = 0;
	}
}

static void start_boost(struct tb_private_info *info)
{
	unsigned long boost_time;	/* us */
	unsigned long boost_hmp_time;	/* us */
	int event_type;

	if (isBoosterEnable != 1)
		return ;

	mutex_lock(&touch_lock);
	stop_touch_boost(info);

	switch (info->event_type) {
		case EVENT_FLICK:
			atomic_set(&info->is_start, 1);
			if (info->boost_debug)
				pr_info("%s: flick.......\n", __func__);
			break;
		case EVENT_UP:
			if (info->boost_debug)
				pr_info("%s: up..........\n", __func__);
			break;
		case EVENT_DOWN:
			if (info->boost_type == TOUCH_BOOST)
				atomic_set(&info->is_start, 1);
			if (info->boost_debug)
				pr_info("%s: down..........\n", __func__);
			break;
		default:
			if (info->boost_debug)
				pr_info("%s: no event..........\n", __func__);
			mutex_unlock(&touch_lock);
			return;
	}

	event_type = info->event_type;
	info->event_type = EVENT_NONE;

	boost_time = info->boost_time * boost_time_multi[event_type] * multi_timeout;
	boost_hmp_time = info->boost_hmp_time * boost_time_multi[event_type] * multi_timeout;

	if (info->boost_debug)
		pr_info("%s: event_type = %d, boost_type = %d\n", __func__, event_type, hmp_boost_type[event_type]);

	request_perf_mode(boost_perf_mode[event_type], boost_qos_mode[event_type], hmp_boost_type[event_type], boost_time, boost_hmp_time);

	mutex_unlock(&touch_lock);
}

static void keys_event(struct input_handle *handle,
		     unsigned int type, unsigned int code, int value)
{
	struct tb_private_info *info =
	    list_entry(handle, struct tb_private_info, handle);

	if (!info->enabled) {
		if (info->boost_debug)
			pr_debug("%s: keys booster disabled\n", __func__);
		stop_touch_boost(info);
		return;
	}

	if (info->boost_debug)
		pr_info("%s: type = %d, code = %d, value = %d, request = %d\n", __func__, type, code, value, info->boost_request);

	/* Only speed up for KEY_HOME and KEY_POWER */
	if ((code != KEY_HOME) && (code != KEY_LEFTMETA) && (code != KEY_POWER) && (code != KEY_VOLUMEUP) && (code != KEY_VOLUMEDOWN) && (code != 0))
		return;

	/* When touch down, change request */
	if (info->boost_request == 1 && type != 0) {
		info->boost_request = 0;
		info->event_type = (value == 0) ? EVENT_UP : EVENT_DOWN;

		start_touch_boost(info);
		if (info->boost_debug)
			pr_info("%s: info->event_type = %d\n", __func__, info->event_type);
	}

	if (type != 1)
		info->boost_request = 1;
}

static void touch_event(struct input_handle *handle,
		     unsigned int type, unsigned int code, int value)
{
	struct tb_private_info *info =
		list_entry(handle, struct tb_private_info, handle);
	static int flick_filter_cnt;

	if (!info->enabled) {
		if (info->boost_debug)
			pr_debug("%s: touch booster disabled\n", __func__);
		stop_touch_boost(info);
		return;
	}

	if (info->boost_debug)
		pr_info("%s: type = %d, code = %d, value = %d, request = %d\n",
			__func__, type, code, value, info->boost_request);

	/* When touch down, change request */
	if (info->boost_request == 1 && type != 0) {
		info->boost_request = 0;

		if (type != 1) {
			/* FIXME: Workaround the issue the flick event overrite the down event */
			flick_filter_cnt++;
			if (flick_filter_cnt > 5) {
				flick_filter_cnt = 0;
				info->event_type = EVENT_FLICK;
			}
		} else if (value == 0) {
			flick_filter_cnt = 0;
			info->event_type = EVENT_UP;
			start_touch_boost(info);
		} else {
			flick_filter_cnt = 0;
			info->event_type = EVENT_DOWN;
			start_touch_boost(info);
		}

		if (info->boost_debug)
			pr_info("%s: info->event_type = %d\n", __func__, info->event_type);
	}

	if (type == 3)
		info->boost_request = 1;

}

static bool touch_match(struct input_handler *handler, struct input_dev *dev)
{
	static int touch_matched;

	if (test_bit(EV_ABS, dev->evbit) && test_bit(BTN_TOUCH, dev->keybit)) {
		if (touch_matched) {
			pr_info("%s: handler = %s, device = %s try to match, somebody already matched\n",
				__func__, handler->name, dev->name);
			WARN_ON(1);
			return false;
		}

		pr_info("%s: handler = %s, device = %s matched\n", __func__, handler->name, dev->name);
		touch_matched = 1;
		return true;
	}

	return false;
}

static bool keys_match(struct input_handler *handler, struct input_dev *dev)
{
	static int keys_matched;

	if (test_bit(KEY_POWER, dev->keybit)) {
		if (keys_matched) {
			pr_info("%s: handler = %s, device = %s try to match, somebody already matched\n",
				__func__, handler->name, dev->name);
			WARN_ON(1);
			return false;
		}

		pr_info("%s: handler = %s, device = %s matched\n", __func__, handler->name, dev->name);
		keys_matched = 1;
		return true;
	}

	return false;
}

static ssize_t get_boost_pulse(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	struct tb_private_info *info =
		list_entry(class, struct tb_private_info, tb_class);

	return sprintf(buf, "%d\n", info->enabled);
}

static ssize_t set_boost_pulse(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	struct tb_private_info *info =
	    list_entry(class, struct tb_private_info, tb_class);
	unsigned int pulse = 0;
	int ret;

	ret = sscanf(buf, "%u", &pulse);
	if (ret != 1)
		return -EINVAL;

	info->enabled = pulse;

	if (pulse)
		start_boost(info);
	else
		stop_touch_boost(info);

	return count;
}

static ssize_t get_boost_debug(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	struct tb_private_info *info =
		list_entry(class, struct tb_private_info, tb_class);

	return sprintf(buf, "%d\n", info->boost_debug);
}

static ssize_t set_boost_debug(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	struct tb_private_info *info =
	    list_entry(class, struct tb_private_info, tb_class);
	unsigned int boost_debug = 0;

	sscanf(buf, "%u", &boost_debug);
	info->boost_debug = !!boost_debug;

	return count;
}

static ssize_t get_boost_switch(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", isBoosterEnable);
}

static ssize_t set_boost_switch(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned int boosterSwitch = 0;

	sscanf(buf, "%u", &boosterSwitch);
	isBoosterEnable = !!boosterSwitch;

	return count;
}

static ssize_t get_boost_time(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	struct tb_private_info *info =
		list_entry(class, struct tb_private_info, tb_class);

	return sprintf(buf, "boost_timeout: %lu ms, boost_hmp_time: %lu ms\n",
		info->boost_time / 1000, info->boost_hmp_time);
}

static ssize_t set_boost_time(struct class *class,
			     struct class_attribute *attr,
			     const char *buf, size_t count)
{
	struct tb_private_info *info =
	    list_entry(class, struct tb_private_info, tb_class);
	unsigned int time = 0;

	sscanf(buf, "%u", &time);

	if (time < 100)
		time = 100;

	info->boost_time = time * USEC_PER_MSEC;
	info->boost_hmp_time = time;

	return count;
}

static struct class_attribute boost_class_attrs[] = {
	__ATTR(boost_debug, 0660, get_boost_debug, set_boost_debug),
	__ATTR(boost_switch, 0660, get_boost_switch, set_boost_switch),
	__ATTR(boost_time, 0660, get_boost_time, set_boost_time),
	__ATTR(touch_pulse, 0660, get_boost_pulse, set_boost_pulse),
	__ATTR_NULL
};

static int fb_state_change(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if (val != FB_EVENT_BLANK)
		return 0;
	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		multi_timeout = 2;
		break;
	case FB_BLANK_UNBLANK:
		multi_timeout = 1;
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block fb_block = {
	.notifier_call = fb_state_change,
};
static bool boost_fb_notifier = false;

static int touch_boost_fn(void *data)
{
	struct tb_private_info *info = data;

	while (1) {
		wait_event(info->wait_queue, atomic_read(&info->is_start) != 0);

		start_boost(info);

		if (info->boost_type == TOUCH_BOOST)
			wait_event_timeout(info->wait_queue, 
					atomic_read(&info->is_ondemand) != 0,
					msecs_to_jiffies(DEFAULT_BOOST_TIME_MS - 20));

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int boost_connect(struct input_handler *handler,
		      struct input_dev *dev, const struct input_device_id *id, int boost_type)
{
	int ret = 0;
	struct tb_private_info *info;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 2 };

	if (boost_device_count >= MAX_BOOST_DEVICE)
		return -ENODEV;

	info = kzalloc(sizeof(struct tb_private_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(info))
		return PTR_ERR(info);

	info->task_started = 0;
	info->boost_request = 1;
	atomic_set(&info->is_ondemand, 0);
	init_waitqueue_head(&info->wait_queue);
	info->boost_task = kthread_create(touch_boost_fn, info, handler->name);
	sched_setscheduler_nocheck(info->boost_task, SCHED_FIFO, &param);
	get_task_struct(info->boost_task);
	wake_up_process(info->boost_task);

	info->enabled = 1;
	info->event_type = EVENT_NONE;
	info->boost_type = boost_type;
	info->boost_time = DEFAULT_BOOST_TIME_MS * USEC_PER_MSEC;
	info->boost_hmp_time = DEFAULT_BOOST_HMP_TIME_MS * USEC_PER_MSEC;
	info->boost_debug = 0;
	info->tb_class.name = handler->name;
	info->tb_class.class_attrs = boost_class_attrs;
	info->handle.dev = dev;
	info->handle.handler = handler;
	info->handle.open = 0;

	ret = class_register(&info->tb_class);
	if (ret) {
		pr_err("%s: register sysdev performance error!\n", __func__);
		goto err_sys;
	}

	ret = input_register_handle(&info->handle);
	if (ret) {
		pr_err("%s: register input handler error!\n", __func__);
		goto err_reg;
	}
	ret = input_open_device(&info->handle);
	if (ret) {
		pr_err("%s: Failed to open input device, error %d\n",
		       __func__, ret);
		goto err_open;
	}

#ifdef CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG
	if (!boost_fb_notifier) {
		fb_register_client(&fb_block);
		multi_timeout = 1;
		boost_fb_notifier = true;
	}
#endif

	boost_device_count++;
	return ret;

 err_open:
	input_unregister_handle(&info->handle);
 err_reg:
	class_unregister(&info->tb_class);
 err_sys:
	kfree(info);
	return ret;

}

static int keys_connect(struct input_handler *handler,
		      struct input_dev *dev, const struct input_device_id *id)
{
	return boost_connect(handler, dev, id, KEYS_BOOST);
}

static int touch_connect(struct input_handler *handler,
		      struct input_dev *dev, const struct input_device_id *id)
{
	return boost_connect(handler, dev, id, TOUCH_BOOST);
}


static void boost_disconnect(struct input_handle *handle)
{
	struct tb_private_info *info =
	    list_entry(handle, struct tb_private_info, handle);

	kthread_stop(info->boost_task);
	put_task_struct(info->boost_task);
	info->boost_task = NULL;

	input_close_device(handle);
	input_unregister_handle(handle);
	class_unregister(&info->tb_class);
	kfree(info);
}

static const struct input_device_id touch_boost_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = {BIT_MASK(EV_ABS)},
		.keybit = {[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH)},
	},
	{}
};

static const struct input_device_id keys_boost_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = {BIT_MASK(EV_SYN)},
		.keybit = {[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER)},
	},
	{}
};

static struct input_handler keys_boost_handler = {
	.name = "keys_booster",
	.event = keys_event,
	.match = keys_match,
	.connect = keys_connect,
	.disconnect = boost_disconnect,
	.id_table = keys_boost_ids,
};

static struct input_handler touch_boost_handler = {
	.name = "touch_booster",
	.event = touch_event,
	.match = touch_match,
	.connect = touch_connect,
	.disconnect = boost_disconnect,
	.id_table = touch_boost_ids,
};

static int booster_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = input_register_handler(&keys_boost_handler);
	if (ret != 0) {
		printk("[ERROR] KEY_BOOSTER: Failed to register handler!\r\n");
		goto out;
	}

	ret = input_register_handler(&touch_boost_handler);
	if (ret != 0) {
		input_unregister_handler(&keys_boost_handler);
		printk("[ERROR] TOUCH_BOOSTER: Failed to register handler!\r\n");
		goto out;
	}

out:
	return ret;
}

static int booster_remove(struct platform_device *pdev)
{
	input_unregister_handler(&keys_boost_handler);
	input_unregister_handler(&touch_boost_handler);

	return 0;
}

#ifdef CONFIG_PM
static int booster_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int booster_resume(struct platform_device *dev)
{
	return 0;
}
#else
#define booster_suspend NULL
#define booster_resume NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id booster_device_table[] = {
	{ .compatible = "meizu,booster" },
	{},
};
MODULE_DEVICE_TABLE(of, booster_device_table);
#endif

static struct platform_driver booster_driver = {
	.probe		= booster_probe,
	.remove     	= booster_remove,
	.suspend	= booster_suspend,
	.resume		= booster_resume,
	.driver		= {
		.name	= "booster_booster",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(booster_device_table),
#endif
	},
};

static int __init booster_init(void)
{
	int ret;

	ret = platform_driver_register(&booster_driver);
	if (ret) {
		printk(KERN_ERR "Platform Device Register Failed %d\n", ret);
		return -1;
	}

	return 0;
}

static void __exit booster_exit(void)
{
	platform_driver_unregister(&booster_driver);
}

module_init(booster_init);
module_exit(booster_exit);

