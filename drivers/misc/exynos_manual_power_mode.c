#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/stat.h>

#include <mach/cpufreq.h>
#include <linux/pm_qos.h>

extern int get_real_max_freq(cluster_type cluster);

#define GPU_MAX_FREQ	(772)
#define POWER_MODE_NAME_LEN	(15)

static DEFINE_MUTEX(exynos_manual_power_lock);

struct manual_power_pm_qos {
	struct pm_qos_request cluster1_max_freq_qos;
	struct pm_qos_request cluster0_max_freq_qos;
	struct pm_qos_request cluster1_max_num_qos;
	struct pm_qos_request cluster0_max_num_qos;
	struct pm_qos_request cluster1_min_num_qos;
	struct pm_qos_request gpu_max_freq_qos;
};

struct power_mode_lock_info {
	char		*name;
	unsigned int	cluster1_max_freq;
	unsigned int	cluster0_max_freq;
	unsigned int	cluster1_max_num;
	unsigned int	cluster0_max_num;
	unsigned int	cluster1_min_num;
	unsigned int	gpu_max_freq;
};

enum manual_power_mode_idx {
	MANUAL_POWER_MODE_LOW,
	MANUAL_POWER_MODE_NORMAL,
	MANUAL_POWER_MODE_HIGH,
	MANUAL_POWER_MODE_END,
};

struct manual_power_mode_info {
	int power_mode_debug;
	int exynos_manual_power_mode_init_complete;
	struct power_mode_lock_info *cur_power_mode;
};

static struct manual_power_mode_info g_power_mode_info = {
	.power_mode_debug = 0,
	.exynos_manual_power_mode_init_complete = 0,
	.cur_power_mode = NULL,
};

static struct manual_power_pm_qos g_power_pm_qos;

static struct power_mode_lock_info g_power_mode_lock_info[MANUAL_POWER_MODE_END] = {
	/* name,		cluster1_max_freq, cluster0_max_freq, cluster1_max_num, cluster0_max_num, cluster1_min_num,	gpu_max_freq*/
	{ "low",       	1500000,							1500000,    			 0,									4,							0,							420},
	{ "normal",    	2100000,							1500000,   			 	 4,									4,							0,							772},
	{ "high",      	2100000,							1500000,   			 	 4,									4,							4,							772},
};

/* request_power_mode(): Request a mode of power
 *
 * @mode: MANUAL_POWER_MODE_LOW...MANUAL_POWER_MODE_HIGH
 */

static void request_power_mode(unsigned int mode)
{
	struct power_mode_lock_info *lock_info;
	struct manual_power_pm_qos *pm_qos;

	if (mode >= MANUAL_POWER_MODE_END || mode < MANUAL_POWER_MODE_LOW)
		return;

	lock_info = &g_power_mode_lock_info[mode];
	pm_qos = &g_power_pm_qos;

	/* Lock Cluster1_max_num cpus: 4~7 */
	pm_qos_update_request(&pm_qos->cluster1_max_freq_qos, lock_info->cluster1_max_freq);

	/* Lock Cluster0_max_num cpus: 0~3 */
	pm_qos_update_request(&pm_qos->cluster0_max_freq_qos, lock_info->cluster0_max_freq);

#ifdef CONFIG_EXYNOS_GPU_PM_QOS
	/* Lock GPU_max_freq */
	pm_qos_update_request(&pm_qos->gpu_max_freq_qos, lock_info->gpu_max_freq);
#endif
#ifdef CONFIG_EXYNOS_CPU_CORE_NUM_PM_QOS
	/* Lock Cluster1_max_num */
	pm_qos_update_request(&pm_qos->cluster1_max_num_qos, lock_info->cluster1_max_num);

	/* Lock Cluster0_max_num */
	pm_qos_update_request(&pm_qos->cluster0_max_num_qos, lock_info->cluster0_max_num);

	/* Lock Cluster1_min_num */
	pm_qos_update_request(&pm_qos->cluster1_min_num_qos, lock_info->cluster1_min_num);
#endif

	if (g_power_mode_info.power_mode_debug)
		printk("%s: cluster0 max freq:%u cluster1 max freq:%u Cluster1 max num:%u Cluster0 min num:%u Cluster1 min num:%u gpu max freq:%u \n",
		__func__,	lock_info->cluster0_max_freq, lock_info->cluster1_max_freq, lock_info->cluster1_max_num, lock_info->cluster0_max_num, lock_info->cluster1_min_num, lock_info->gpu_max_freq);
}

static void show_power_mode_list(void)
{
	int i;
	pr_debug("========Exynos Manual Power Mode ====================\n");

	printk("%10s %18s %18s %18s %18s %18s %14s\n", "name", "cluster1_max_freq", "cluster0_max_freq", "cluster1_max_num", "cluster0_max_num", "cluster1_min_num", "gpu_max_freq");
	for (i = 0; i < MANUAL_POWER_MODE_END; i++) {
		printk("%10s		%10u		%10u		%12u		%12u		%11u		%11u\n",
			g_power_mode_lock_info[i].name, g_power_mode_lock_info[i].cluster1_max_freq, g_power_mode_lock_info[i].cluster0_max_freq,
			g_power_mode_lock_info[i].cluster1_max_num, g_power_mode_lock_info[i].cluster0_max_num, g_power_mode_lock_info[i].cluster1_min_num, g_power_mode_lock_info[i].gpu_max_freq);
	}
}

static ssize_t show_manual_power_mode(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	int ret;

	mutex_lock(&exynos_manual_power_lock);
	ret = sprintf(buf, "manual_power_mode: %s\n", g_power_mode_info.cur_power_mode->name);
	mutex_unlock(&exynos_manual_power_lock);
	show_power_mode_list();

	return ret;
}

static ssize_t store_manual_power_mode(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	char str_power_mode[POWER_MODE_NAME_LEN];
	int ret , i;

	ret = sscanf(buf, "%11s", str_power_mode);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&exynos_manual_power_lock);
	for (i = 0; i < MANUAL_POWER_MODE_END; i++) {
		if (!strnicmp(g_power_mode_lock_info[i].name, str_power_mode, POWER_MODE_NAME_LEN)) {
			break;
		}
	}

	if (i < MANUAL_POWER_MODE_END) {
		g_power_mode_info.cur_power_mode = &g_power_mode_lock_info[i];
		request_power_mode(i);

		pr_info("store manual_power_mode to %s \n", g_power_mode_info.cur_power_mode->name);
	}
	mutex_unlock(&exynos_manual_power_lock);

	return count;
}

static ssize_t show_manual_debug_onoff(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	int ret;

	mutex_lock(&exynos_manual_power_lock);
	ret = sprintf(buf, "debug: %u\n", g_power_mode_info.power_mode_debug);
	mutex_unlock(&exynos_manual_power_lock);

	return ret;
}

static ssize_t store_manual_debug_onoff(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	unsigned int debug;

	mutex_lock(&exynos_manual_power_lock);

	ret = sscanf(buf, "%u", &debug);
	if (ret != 1)
		goto fail;

	g_power_mode_info.power_mode_debug = debug;
	pr_info("%s: debug = %u\n", __func__, g_power_mode_info.power_mode_debug);
	mutex_unlock(&exynos_manual_power_lock);

	return count;

fail:
	pr_err("usage: echo debug > /sys/devices/system/exynos_manual_power_mode/manual_debug_onoff\n\n");
	mutex_unlock(&exynos_manual_power_lock);

	return -EINVAL;
}

static struct kobj_attribute manual_power_debug_onoff_attr =
		__ATTR(manual_debug_onoff, S_IRUGO | S_IWUSR,
			show_manual_debug_onoff, store_manual_debug_onoff);

static struct kobj_attribute manual_power_mode_attr =
		__ATTR(manual_power_mode, S_IRUGO | S_IWUSR,
			show_manual_power_mode, store_manual_power_mode);

static struct attribute *exynos_manual_power_mode_sysfs_attrs[] = {
	&manual_power_debug_onoff_attr.attr,
	&manual_power_mode_attr.attr,
	NULL,
};

static struct attribute_group exynos_manual_power_mode_sysfs_group = {
	.attrs = exynos_manual_power_mode_sysfs_attrs,
};

static const struct attribute_group *exynos_manual_power_mode_sysfs_groups[] = {
	&exynos_manual_power_mode_sysfs_group,
	NULL,
};

static struct bus_type exynos_manual_power_mode_subsys = {
	.name = "exynos_manual_power_mode",
	.dev_name = "exynos_manual_power_mode",
};

int setup_exynos_manual_power_mode_sysfs(void)
{
	int ret = 0;

	ret = subsys_system_register(&exynos_manual_power_mode_subsys, exynos_manual_power_mode_sysfs_groups);
	if (ret)
		pr_err("fail to register exynos_manual_power_mode subsys\n");

	return ret;
}

int setup_exynos_manual_power_mode_pmqos(void)
{
	int ret = 0;
	pm_qos_add_request(&g_power_pm_qos.cluster1_max_freq_qos, PM_QOS_CLUSTER1_FREQ_MAX, get_real_max_freq(1));
	pm_qos_add_request(&g_power_pm_qos.cluster0_max_freq_qos, PM_QOS_CLUSTER0_FREQ_MAX, get_real_max_freq(0));
#ifdef CONFIG_EXYNOS_CPU_CORE_NUM_PM_QOS
	pm_qos_add_request(&g_power_pm_qos.cluster1_max_num_qos, PM_QOS_CLUSTER1_NUM_MAX, NR_CLUST1_CPUS);
	pm_qos_add_request(&g_power_pm_qos.cluster0_max_num_qos, PM_QOS_CLUSTER0_NUM_MAX, NR_CLUST0_CPUS);
	pm_qos_add_request(&g_power_pm_qos.cluster1_min_num_qos, PM_QOS_CLUSTER1_NUM_MIN, 0);
#endif
#ifdef CONFIG_EXYNOS_GPU_PM_QOS
	pm_qos_add_request(&g_power_pm_qos.gpu_max_freq_qos, PM_QOS_GPU_FREQ_MAX, GPU_MAX_FREQ);
#endif
	return ret;
}

static int exynos_manual_power_mode_init(void)
{
	int error;

	printk("Exynos Manual Power mode init\n");

	error = setup_exynos_manual_power_mode_sysfs();

	if (error)
		goto err;

	mutex_init(&exynos_manual_power_lock);
	g_power_mode_info.cur_power_mode = &g_power_mode_lock_info[MANUAL_POWER_MODE_NORMAL];
	setup_exynos_manual_power_mode_pmqos();
	request_power_mode(MANUAL_POWER_MODE_NORMAL);
	/* show_power_mode_list(); */
	show_power_mode_list();
	g_power_mode_info.exynos_manual_power_mode_init_complete = 1;

err:
	return error;
}

late_initcall_sync(exynos_manual_power_mode_init);
