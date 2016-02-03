#include <linux/cpu.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/suspend.h>

static struct delayed_work exynos_hotplug;
static struct workqueue_struct *khotplug_wq;

enum hstate {
	H0,
	H1,
	H2,
	MAX_HSTATE,
};

static ktime_t time_start;

struct hotplug_hstates_usage {
	unsigned long usage;
	unsigned long time;
};

struct exynos_hotplug_ctrl {
	int sampling_rate;
	int down_change_duration;
	int up_change_duration;
	int task_per_cpu;
	int force_hstate;
	int up_differential;
	enum hstate old_state;
	bool suspended;
	struct hotplug_hstates_usage usage[MAX_HSTATE];
};

struct hotplug_hstate {
	char *name;
	int core_on;
	int last_residency;
	enum hstate state;
};

static struct hotplug_hstate hstate_set[] = {
	[H0] = {
		.name		= "H0",
		.core_on	= NR_CPUS,
		.state		= H0,
	},
	[H1] = {
		.name		= "H1",
		.core_on	= NR_CPUS / 2,
		.state		= H1,
	},
	[H2] = {
		.name		= "H2",
		.core_on	= 1,
		.state		= H2,
	},
};

static struct exynos_hotplug_ctrl ctrl_hotplug = {
	.sampling_rate = 500,		/* ms */
	.down_change_duration = 3000,	/* ms */
	.up_change_duration = 2000,	/* ms */
	.task_per_cpu = 5,
	.up_differential = 4,
	.force_hstate = 0,
	.old_state = H0,
};

static DEFINE_MUTEX(hotplug_lock);

static int get_core_count(enum hstate state)
{
	int old = ctrl_hotplug.old_state;

	/* CPU UP */
	if (ctrl_hotplug.old_state < state)
		return hstate_set[old].core_on - hstate_set[state].core_on;
	else
		return hstate_set[state].core_on - hstate_set[old].core_on;
}

static void __ref cluster_down(enum hstate state)
{
	int i, count, cpu;

	count = get_core_count(state);

	for (i = 0; i < count; i++) {
		cpu = num_online_cpus() - 1;
		if (cpu > 0 && cpu_online(cpu))
			cpu_down(cpu);
	}
}

static void __ref cluster_up(enum hstate state)
{
	int i, count, cpu;

	count = get_core_count(state);

	for (i = 0; i < count; i++) {
		cpu = num_online_cpus();

		if (cpu < num_possible_cpus() && !cpu_online(cpu))
			cpu_up(cpu);
	}
}

static void hotplug_enter_hstate(bool up, enum hstate state)
{
	struct hotplug_hstate *target_state = &hstate_set[state];
	ktime_t time_end;
	s64 diff;

	if (ctrl_hotplug.suspended)
		return;

	time_end = ktime_get();

	diff = ktime_to_ms(ktime_sub(time_end, time_start));
	if (diff > INT_MAX)
		diff = INT_MAX;

	if (up && diff < ctrl_hotplug.up_change_duration)
		return;
	else if (!up && diff < ctrl_hotplug.down_change_duration)
		return;

	if (up)
		cluster_up(state);
	else
		cluster_down(state);

	target_state->last_residency = (int) diff;

	time_start = ktime_get();

	ctrl_hotplug.usage[ctrl_hotplug.old_state].time += target_state->last_residency;
	ctrl_hotplug.usage[ctrl_hotplug.old_state].usage++;

	ctrl_hotplug.old_state = state;
	ctrl_hotplug.force_hstate = state;
}

static bool select_up_down(void)
{
	int threshold = num_online_cpus() * ctrl_hotplug.task_per_cpu;
	int nr = nr_running();
	bool up = true;

	if (nr <= threshold)
		up = false;

	return up;
}

static enum hstate hotplug_adjust_state(bool up)
{
	int threshold = num_online_cpus() * ctrl_hotplug.task_per_cpu;
	int nr = nr_running();

	if (up && nr > threshold && nr < threshold + ctrl_hotplug.up_differential)
		return ctrl_hotplug.old_state;

	if ((ctrl_hotplug.old_state == H1 && !up) ||
	    (ctrl_hotplug.old_state == H2 && !up))
		return H2;
	else if (ctrl_hotplug.old_state == H2 && up)
		return H1;

	return up ? H0 : H1;
}

static void exynos_work(struct work_struct *dwork)
{
	bool up = select_up_down();
	enum hstate target_state;

	mutex_lock(&hotplug_lock);
	target_state = hotplug_adjust_state(up);

	if (ctrl_hotplug.old_state == target_state)
		goto out;

	hotplug_enter_hstate(up, target_state);

out:
	queue_delayed_work_on(0, khotplug_wq, &exynos_hotplug, msecs_to_jiffies(ctrl_hotplug.sampling_rate));
	mutex_unlock(&hotplug_lock);
}

static int fb_state_change(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank = evdata->data;
	enum hstate target_state;

	if (event == FB_EVENT_BLANK) {
		switch (*blank) {
		case FB_BLANK_POWERDOWN:
			mutex_lock(&hotplug_lock);
			target_state = hotplug_adjust_state(false);
			if (ctrl_hotplug.old_state != target_state)
				hotplug_enter_hstate(false, target_state);
			mutex_unlock(&hotplug_lock);
			break;
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block fb_block = {
	.notifier_call = fb_state_change,
};

#define define_show_state_function(_name) \
static ssize_t show_##_name(struct device *dev, struct device_attribute *attr, \
			char *buf) \
{ \
	return sprintf(buf, "%d\n", ctrl_hotplug._name); \
}

#define define_store_state_function(_name) \
static ssize_t store_##_name(struct device *dev, struct device_attribute *attr, \
		const char *buf, size_t count) \
{ \
	unsigned long value; \
	int ret; \
	ret = kstrtoul(buf, 10, &value); \
	if (ret) \
		return ret; \
	ctrl_hotplug._name = value; \
	return ret ? ret : count; \
}

define_show_state_function(task_per_cpu)
define_store_state_function(task_per_cpu)

define_show_state_function(up_differential)
define_store_state_function(up_differential)

define_show_state_function(sampling_rate)
define_store_state_function(sampling_rate)

define_show_state_function(down_change_duration)
define_store_state_function(down_change_duration)

define_show_state_function(up_change_duration)
define_store_state_function(up_change_duration)

define_show_state_function(force_hstate)
static ssize_t store_force_hstate(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	bool up = false;
	int ret, target_state;

	ret = sscanf(buf, "%d", &target_state);
	if (ret != 1 || target_state > H2)
		return -EINVAL;

	if (target_state < 0) {
		if (!delayed_work_pending(&exynos_hotplug))
			queue_delayed_work_on(0, khotplug_wq, &exynos_hotplug,
					msecs_to_jiffies(ctrl_hotplug.sampling_rate));
		goto out;
	}

	if (delayed_work_pending(&exynos_hotplug))
		cancel_delayed_work_sync(&exynos_hotplug);

	if (ctrl_hotplug.old_state == target_state)
		goto out;
	else if (ctrl_hotplug.old_state > target_state)
		up = true;

	mutex_lock(&hotplug_lock);
	hotplug_enter_hstate(up, target_state);
	mutex_unlock(&hotplug_lock);

out:
	return count;
}

static ssize_t show_time_in_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;

	for (i = 0; i < MAX_HSTATE; i++) {
		len += sprintf(buf + len, "%s %llu\n", hstate_set[i].name,
				(unsigned long long)ctrl_hotplug.usage[i].time);
	}
	return len;
}

static DEVICE_ATTR(task_per_cpu, S_IRUGO | S_IWUSR, show_task_per_cpu, store_task_per_cpu);
static DEVICE_ATTR(up_differential, S_IRUGO | S_IWUSR, show_up_differential, store_up_differential);
static DEVICE_ATTR(sampling_rate, S_IRUGO | S_IWUSR, show_sampling_rate, store_sampling_rate);
static DEVICE_ATTR(down_change_duration, S_IRUGO | S_IWUSR, show_down_change_duration, store_down_change_duration);
static DEVICE_ATTR(up_change_duration, S_IRUGO | S_IWUSR, show_up_change_duration, store_up_change_duration);
static DEVICE_ATTR(force_hstate, S_IRUGO | S_IWUSR, show_force_hstate, store_force_hstate);

static DEVICE_ATTR(time_in_state, S_IRUGO, show_time_in_state, NULL);

static struct attribute *clusterhotplug_default_attrs[] = {
	&dev_attr_task_per_cpu.attr,
	&dev_attr_up_differential.attr,
	&dev_attr_sampling_rate.attr,
	&dev_attr_down_change_duration.attr,
	&dev_attr_up_change_duration.attr,
	&dev_attr_force_hstate.attr,
	&dev_attr_time_in_state.attr,
	NULL
};

static struct attribute_group clusterhotplug_attr_group = {
	.attrs = clusterhotplug_default_attrs,
	.name = "clusterhotplug",
};

static int exynos_pm_notify(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	mutex_lock(&hotplug_lock);
	if (event == PM_SUSPEND_PREPARE) {
		ctrl_hotplug.suspended = true;

		if (delayed_work_pending(&exynos_hotplug))
			cancel_delayed_work_sync(&exynos_hotplug);
	} else if (event == PM_POST_SUSPEND) {
		ctrl_hotplug.suspended = false;

		queue_delayed_work_on(0, khotplug_wq, &exynos_hotplug,
				msecs_to_jiffies(ctrl_hotplug.sampling_rate));
	}
	mutex_unlock(&hotplug_lock);

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpu_pm_notifier = {
	.notifier_call = exynos_pm_notify,
};

static int __init dm_cluster_hotplug_init(void)
{
	int ret;

	INIT_DEFERRABLE_WORK(&exynos_hotplug, exynos_work);

	khotplug_wq = alloc_workqueue("khotplug", WQ_FREEZABLE, 0);
	if (!khotplug_wq) {
		pr_err("Failed to create khotplug workqueue\n");
		ret = -EFAULT;
		goto err_wq;
	}

	ret = sysfs_create_group(&cpu_subsys.dev_root->kobj, &clusterhotplug_attr_group);
	if (ret) {
		pr_err("Failed to create sysfs for hotplug\n");
		goto err_sys;
	}

	ret = fb_register_client(&fb_block);
	if (ret) {
		pr_err("Failed to register fb notifier\n");
		goto err_fb;
	}

	register_pm_notifier(&exynos_cpu_pm_notifier);

	queue_delayed_work_on(0, khotplug_wq, &exynos_hotplug, msecs_to_jiffies(ctrl_hotplug.sampling_rate));

	return 0;
err_fb:
	sysfs_remove_group(&cpu_subsys.dev_root->kobj, &clusterhotplug_attr_group);
err_sys:
	destroy_workqueue(khotplug_wq);
err_wq:
	return ret;
}
late_initcall(dm_cluster_hotplug_init);
