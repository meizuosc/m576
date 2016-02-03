/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/syscore_ops.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/export.h>
#include <linux/auto_sleep_wakeup_test.h>


#define DEFAULT_SLEEP_TIME 3
#define MIN_SLEEP_TIME 2

#define DEFAULT_WAKEUP_TIME 5
#define MIN_WAKEUP_TIME 5

static int sleep_time = DEFAULT_SLEEP_TIME;
static int wakeup_time = DEFAULT_WAKEUP_TIME;
static int sleep_wakeup_test_onoff = 0;
static int sleep_wakeup_cnt = 0;

#define CONFIG_WAKEALARM_RTC "rtc0"

struct auto_sleep_wakeup_time_info {
	struct delayed_work polling_work;		
};

static struct auto_sleep_wakeup_time_info auto_test_info;

static BLOCKING_NOTIFIER_HEAD(auto_sleep_wakeup_test_notifier_list);

/**
 *  auto_sleep_wakeup_test_register_client - register a client notifier
 *  @nb: notifier block to callback on events
 */
int auto_sleep_wakeup_test_register_client(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&auto_sleep_wakeup_test_notifier_list, nb);
}
EXPORT_SYMBOL(auto_sleep_wakeup_test_register_client);

/**
 *  fb_unregister_client - unregister a client notifier
 *  @nb: notifier block to callback on events
 */
int auto_sleep_wakeup_test_unregister_client(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&auto_sleep_wakeup_test_notifier_list, nb);
}
EXPORT_SYMBOL(auto_sleep_wakeup_test_unregister_client);

/**
 * auto_sleep_wakeup_test_notifier_call_chain - notify clients of fb_events
 *
 */
int auto_sleep_wakeup_test_notifier_call_chain(unsigned long val, void *v)
{
    return blocking_notifier_call_chain(&auto_sleep_wakeup_test_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(auto_sleep_wakeup_test_notifier_call_chain);
  


static int set_wakealarm(void)
{
	int retval = 0;
	unsigned long now, alarm;
	struct rtc_wkalrm alm;
	struct rtc_device *rtc;

	rtc = rtc_class_open(CONFIG_WAKEALARM_RTC);
	if (!rtc) {
		return -1;
	}

	/* Only request alarms that trigger in the future.  Disable them
	 * by writing another time, e.g. 0 meaning Jan 1 1970 UTC.
	 */
	retval = rtc_read_time(rtc, &alm.time);
	if (retval < 0)
		goto close_rtc;
	rtc_tm_to_time(&alm.time, &now);

	alarm = now + sleep_time;
	
	if (alarm > now) {
		/* Avoid accidentally clobbering active alarms; we can't
		 * entirely prevent that here, without even the minimal
		 * locking from the /dev/rtcN api.
		 */
		retval = rtc_read_alarm(rtc, &alm);
		if (retval < 0)
			goto close_rtc;

		alm.enabled = 1;
	} else {
		alm.enabled = 0;

		/* Provide a valid future alarm time.  Linux isn't EFI,
		 * this time won't be ignored when disabling the alarm.
		 */
		alarm = now + 300;
	}

	rtc_time_to_tm(alarm, &alm.time);
	retval = rtc_set_alarm(rtc, &alm);

close_rtc:
	rtc_class_close(rtc);
	return retval;
}

static int restore_wakealarm(void)
{
	struct rtc_wkalrm	alm;
	struct rtc_device	*rtc = NULL;

	rtc = rtc_class_open(CONFIG_WAKEALARM_RTC);
	if (!rtc) {
		return -1;
	}
	alm.enabled = false;
	rtc_set_alarm(rtc, &alm);
	rtc_class_close(rtc);
	return 0;
}

static int auto_sleep_wakeup_test_suspend(struct device *dev)
{
	cancel_delayed_work_sync(&auto_test_info.polling_work);		
	if(sleep_wakeup_test_onoff == 1)
		set_wakealarm();
	return 0;
}

static int auto_sleep_wakeup_test_resume(struct device *dev)
{
	if(sleep_wakeup_test_onoff == 1) {
		auto_sleep_wakeup_test_notifier_call_chain(AUTO_SLEEP_WAKEUP_TEST_PRESS_POWER_BUTTON,NULL);
#ifdef CONFIG_AUTO_SLEEP_WAKEUP_MODEM
		auto_sleep_wakeup_test_notifier_call_chain(AUTO_SLEEP_WAKEUP_TEST_WAKEUP_MODEM,NULL);
#endif
		restore_wakealarm();
		cancel_delayed_work_sync(&auto_test_info.polling_work);		
		schedule_delayed_work(&auto_test_info.polling_work, msecs_to_jiffies(wakeup_time*1000));	
		sleep_wakeup_cnt++;
		printk("auto sleep wakeup test!! cnt=%d\n",sleep_wakeup_cnt);
		
	}
	return 0;
}

static ssize_t show_wakeuptime(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", wakeup_time);
}


static ssize_t store_wakeuptime(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;	
	unsigned int value;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		goto out;		

	if(value >= MIN_WAKEUP_TIME)
		wakeup_time = value;
	else {
		wakeup_time = MIN_WAKEUP_TIME;
		printk("set %d to wakeup time. The min value is %d.\n",wakeup_time,wakeup_time);
	}

out:
	return count;
}

static ssize_t show_sleeptime(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sleep_time);
}


static ssize_t store_sleeptime(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;	
	unsigned int value;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		goto out;		

	if(value >= MIN_SLEEP_TIME)
		sleep_time = value;
	else {
		sleep_time = MIN_SLEEP_TIME;
		printk("set %d to sleep time. The min value is %d.\n",sleep_time,sleep_time);
	}
out:
	return count;
}

static ssize_t show_onoff(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sleep_wakeup_test_onoff);
}

static ssize_t store_onoff(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int value;

	ret = sscanf(buf, "%u", &value);

	if (ret != 1)
		goto out;

	pr_info("auto sleep wakeup test : value=%d\n",value);		
	if(value == 0) {
		sleep_wakeup_test_onoff = 0;
	}
	else if(value == 1) {
		if(sleep_time < MIN_SLEEP_TIME)
			sleep_time = DEFAULT_SLEEP_TIME;
		sleep_wakeup_test_onoff = 1;
	}
	else {
		pr_info("auto sleep wakeup test error : value=%d\n",value);
	}
out:			
	return count;
}

static DEVICE_ATTR(sleeptime, S_IRUGO | S_IWUSR, show_sleeptime, store_sleeptime);
static DEVICE_ATTR(wakeuptime, S_IRUGO | S_IWUSR, show_wakeuptime, store_wakeuptime);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR, show_onoff, store_onoff);		   

static struct attribute *auto_sleep_wakeup_test_attrs[] = {
	&dev_attr_onoff.attr,
	&dev_attr_sleeptime.attr,
	&dev_attr_wakeuptime.attr,
	NULL,
};


static void auto_test_polling_work(struct work_struct *work)
{
	if(sleep_wakeup_test_onoff == 1) {
		auto_sleep_wakeup_test_notifier_call_chain(AUTO_SLEEP_WAKEUP_TEST_PRESS_POWER_BUTTON,NULL);	
	}
}


static struct attribute_group auto_sleep_wakeup_test_attr_group = {
	.attrs =auto_sleep_wakeup_test_attrs,
	.name = "auto_sleep_wakeup_test",	
};

static int auto_sleep_wakeup_test_probe(struct platform_device *pdev)
{
	int error = 0;
	struct device *dev = &pdev->dev;

	error = sysfs_create_group(&pdev->dev.kobj, &auto_sleep_wakeup_test_attr_group);

	if (error) {

		dev_err(dev, "Unable to make sysfs auto sleep wakeup test, error: %d\n",
			error);
		sysfs_remove_group(&pdev->dev.kobj, &auto_sleep_wakeup_test_attr_group);
	}
	
	INIT_DELAYED_WORK(&auto_test_info.polling_work, auto_test_polling_work);		
	return 0;
}

static int auto_sleep_wakeup_teest_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &auto_sleep_wakeup_test_attr_group);
	return 0;
}	

static const struct dev_pm_ops auto_sleep_wakeup_test_pm = {
	.suspend = auto_sleep_wakeup_test_suspend,
	.resume	=auto_sleep_wakeup_test_resume,
};

static struct platform_driver auto_sleep_wakeup_test_driver = {
	.probe		= auto_sleep_wakeup_test_probe,
	.remove		= auto_sleep_wakeup_teest_remove,
	.driver		= {
		.name	= "auto_sleep_wakeup_test",
		.owner	= THIS_MODULE,
		.pm	= &auto_sleep_wakeup_test_pm,
	}
};

static struct platform_device auto_sleep_wakeup_test_device = {
	.name	= "auto_sleep_wakeup_test",
	.id	= -1,
};

static int __init auto_sleep_wakeup_test_int(void)
{
	int ret;
		
	ret = platform_device_register(&auto_sleep_wakeup_test_device);
	if (ret)
		return ret;
		
	return platform_driver_register(&auto_sleep_wakeup_test_driver);
}

static void __exit auto_sleep_wakeup_test_exit(void)
{
	platform_driver_unregister(&auto_sleep_wakeup_test_driver);
	platform_device_unregister(&auto_sleep_wakeup_test_device);
}

module_init(auto_sleep_wakeup_test_int);
module_exit(auto_sleep_wakeup_test_exit);

MODULE_DESCRIPTION("AUTO SLEEP WAKEUP TEST DRIVER");
MODULE_LICENSE("GPL");
