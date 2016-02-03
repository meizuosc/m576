#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/of.h>

extern int rtc_is_suspend;

struct delayed_work timestamp_delayed_work;
void timestamp_delayed_work_func(struct work_struct *work)
{
	struct rtc_time tm;
	struct timespec tv;
	struct rtc_device *rtc0 = rtc_class_open("rtc0");

	if (rtc0 == NULL)
	{
		printk("No RTC available! Do not use timestamp!\n");
		return ;
	}
	if (!rtc_is_suspend) {
		rtc_read_time(rtc0, &tm);
		rtc_tm_to_time(&tm, &tv.tv_sec);
		printk("Timestamp: %d-%02d-%02d %02d:%02d:%02d UTC (%u)\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned int) tv.tv_sec);
	}
	schedule_delayed_work(&timestamp_delayed_work, 60*HZ);
}

static int exynos_debug_tool_probe(struct platform_device *pdev)
{
	INIT_DELAYED_WORK(&timestamp_delayed_work, timestamp_delayed_work_func);
	schedule_delayed_work(&timestamp_delayed_work, 60*HZ);
	return 0;
};

static int exynos_debug_tool_suspend(struct platform_device *pdev, pm_message_t state)
{
	cancel_delayed_work_sync(&timestamp_delayed_work);
	return 0;
}

static int exynos_debug_tool_resume(struct platform_device *pdev)
{
	schedule_delayed_work(&timestamp_delayed_work, 60*HZ);
	return 0;
}

//suspend, resume
static struct of_device_id exynos_debug_tool_of_match_table[] = {
	{.compatible = "samsung,exynos_debug_tool",},
	{},
};

static struct platform_driver exynos_debug_tool_driver = {
	.probe = exynos_debug_tool_probe,
	.driver = {
		.name = "exynos_debug_tool",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_debug_tool_of_match_table),
	},
	.suspend = exynos_debug_tool_suspend,
	.resume = exynos_debug_tool_resume,
};

static int __init exynos_debug_tool_init(void)
{
	return platform_driver_register(&exynos_debug_tool_driver);
}

module_init(exynos_debug_tool_init);
MODULE_LICENSE("Dual BSD/GPL");
