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
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/exynos_touch_daemon.h>
#include <linux/exynos_touch_daemon_scenario_data.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_JANUARY_BOOSTER
#include <linux/input/janeps_booster.h>
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_DSX
#include "touchscreen/synaptics_dsx/synaptics_i2c_rmi.h"
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS
#include "touchscreen/stm/fts.h"
#endif

#ifdef CONFIG_TOUCHSCREEN_MELFAS
 struct melfas_ts_data
{
    uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct melfas_tsi_platform_data *pdata;
	struct work_struct	pen_event_work;
    struct workqueue_struct	*ts_workqueue;
	uint32_t flags;
	int (*power)(int onoff);
	bool debug_enabled;
};
#endif

struct exynos_touch_daemon_data exynos_touch_daemon_data;
struct touch_point *g_touch_point;
static struct dentry *exynos_touch_daemon_position_dentry;
static struct dentry *exynos_touch_daemon_log_dentry;

static char *exynos_touch_daemon_log_buf;
static char *exynos_touch_daemon_time_buf;
static ssize_t buf_len;
static ssize_t time_len;

static void __iomem *base_tick_base = 0;
static struct clk *rtc_clk;
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_DSX
#define TICK_MS (unsigned int)500//15ms // 333.33   //10ms/TICK_RESOLUTION
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS
#define TICK_MS (unsigned int)400   //10ms/TICK_RESOLUTION
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS
#define TICK_MS (unsigned int)500
#endif

#define RTC_TICK_ADDRESS 0x10590000

static void rtc_tick_init(void)
{
	unsigned int tmp;

	base_tick_base = ioremap(RTC_TICK_ADDRESS, SZ_16K);

	// RTCCON, TICKSEL[7:4], CLKRST[3], CNTSEL[2], CLKSEL[0], CTLEN[0]
	tmp = __raw_readl(base_tick_base + 0x40);
	__raw_writel((tmp & ~(0x1F0)), base_tick_base + 0x40); // TICEN disable, TICCKSEL 32768Hz //1024Hz
}

void rtc_tick_enable(int onoff)
{
	unsigned int tick_position = 0;
	unsigned int tmp;

	if(base_tick_base == 0)
		return;

	if(onoff) {
		tick_position = TICK_MS;
		// TICNT, TICK_TIME_COUNT[31:0]
		__raw_writel(tick_position, base_tick_base + 0x44);
		// RTCCON, TICEN[8], TICEN enable
		tmp = __raw_readl(base_tick_base + 0x40);
		__raw_writel(tmp | 0x100, base_tick_base + 0x40); // TICEN enable
	}
	else
	{
		// RTCCON, TICEN[8], TICEN disable
		tmp = __raw_readl(base_tick_base + 0x40);
		tmp = tmp & 0xFFFFFEFF;
		__raw_writel(tmp, base_tick_base + 0x40); // TICEN disable

		// INTP, Timer TIC
		__raw_writel(0x01, base_tick_base + 0x30);
	}
}

void rtc_reset(void)
{
	unsigned int tmp;

	if(base_tick_base == 0)
		return;

	// RTCCON, TICEN[8], TICEN disable
	tmp = __raw_readl(base_tick_base + 0x40);
	tmp = tmp & 0xFFFFFEFF;
	__raw_writel(tmp, base_tick_base + 0x40); // TICEN disable

	// RTCCON, TICKSEL[7:4], CLKRST[3], CNTSEL[2], CLKSEL[0], CTLEN[0]
	tmp = __raw_readl(base_tick_base + 0x40);
	__raw_writel((tmp & ~(0x1F0)), base_tick_base + 0x40); // TICEN disable, TICCKSEL 32768Hz //1024Hz

	// INTP, Timer TIC
	__raw_writel(0x01, base_tick_base + 0x30);
}

static void exynos_touch_set_point(struct touch_point *curr_touch_point)
{
	g_touch_point = curr_touch_point;
}

static struct touch_point* exynos_touch_get_point(void)
{
	return g_touch_point;
}

static void exynos_report_sync(int touch_count)
{
#if defined(CONFIG_JANUARY_BOOSTER)
	struct touch_point *point = exynos_touch_get_point();
#endif

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_DSX) || defined(CONFIG_TOUCHSCREEN_FTS)
	input_mt_slot(exynos_touch_daemon_data.touchdata->input_dev, 0);
	input_mt_report_slot_state(exynos_touch_daemon_data.touchdata->input_dev,
				MT_TOOL_FINGER, 0);

#if defined(CONFIG_JANUARY_BOOSTER)
	if(exynos_touch_daemon_data.booster == 1) {
		janeps_input_report(RELEASE, point->x[touch_count],point->y[touch_count]);
	}

#endif
	input_report_key(exynos_touch_daemon_data.touchdata->input_dev,
					BTN_TOUCH, 0);
	input_report_key(exynos_touch_daemon_data.touchdata->input_dev,
					BTN_TOOL_FINGER, 0);
	input_sync(exynos_touch_daemon_data.touchdata->input_dev);
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS
	input_mt_sync(exynos_touch_daemon_data.touchdata->input_dev);
	input_sync(exynos_touch_daemon_data.touchdata->input_dev);
#endif
}


static void exynos_report_touch_event_by_rtc_tick(void)
{
	static int touch_count = 0;
	struct touch_point *point = exynos_touch_get_point();

//	printk("[test] %d, %d %d %d %d\n",touch_count, point->x[touch_count], point->y[touch_count], point->wx[touch_count], point->wy[touch_count]);
	if(touch_count+1 < point->count) {
		clk_enable(rtc_clk);
		rtc_tick_enable(1);
		clk_disable(rtc_clk);
	}

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_DSX
	input_mt_slot(exynos_touch_daemon_data.touchdata->input_dev, 0);
	input_mt_report_slot_state(exynos_touch_daemon_data.touchdata->input_dev,
			MT_TOOL_FINGER, 1);

	input_report_key(exynos_touch_daemon_data.touchdata->input_dev,
			BTN_TOUCH, 1);
	input_report_key(exynos_touch_daemon_data.touchdata->input_dev,
			BTN_TOOL_FINGER, 1);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_POSITION_X, point->x[touch_count]);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_POSITION_Y, point->y[touch_count]);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_TOUCH_MAJOR, max(point->wx[touch_count], point->wy[touch_count]));
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_TOUCH_MINOR, max(point->wx[touch_count], point->wy[touch_count]));
	input_sync(exynos_touch_daemon_data.touchdata->input_dev);
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_TRACKING_ID, 0);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_POSITION_X, point->x[touch_count]);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_POSITION_Y, point->y[touch_count]);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_TOUCH_MAJOR, point->wx[touch_count]);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_PRESSURE, point->wy[touch_count]);

	input_mt_sync(exynos_touch_daemon_data.touchdata->input_dev);

	input_sync(exynos_touch_daemon_data.touchdata->input_dev);
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS
	input_mt_slot(exynos_touch_daemon_data.touchdata->input_dev,0);
	input_mt_report_slot_state(exynos_touch_daemon_data.touchdata->input_dev,
			MT_TOOL_FINGER, 1);
	input_report_key(exynos_touch_daemon_data.touchdata->input_dev,
			BTN_TOUCH, 1);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			BTN_TOOL_FINGER, 1);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_POSITION_X, point->x[touch_count]);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_POSITION_Y, point->y[touch_count]);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_TOUCH_MAJOR, point->wx[touch_count]);
	input_report_abs(exynos_touch_daemon_data.touchdata->input_dev,
			ABS_MT_TOUCH_MINOR, point->wy[touch_count]);
	input_sync(exynos_touch_daemon_data.touchdata->input_dev);
#endif

	touch_count ++;

#if defined(CONFIG_JANUARY_BOOSTER)
	if(exynos_touch_daemon_data.booster == 1) {
		if(touch_count >= 1 && touch_count < point->count)
			janeps_input_report(PRESS, point->x[touch_count],point->y[touch_count]);
	}
#endif

	if (touch_count == point->count) {
		exynos_report_sync(touch_count);
		touch_count = 0;
	}
}

/*static*/ irqreturn_t exynos_touch_daemon_thread_irq(int irq, void *data)
{
	clk_enable(rtc_clk);
	rtc_tick_enable(0);
	clk_disable(rtc_clk);
	exynos_report_touch_event_by_rtc_tick();
	return IRQ_HANDLED;
}

static void exynos_touch_daemon_responsiveness(void)
{
	clk_enable(rtc_clk);
	rtc_reset();
	schedule_work(&exynos_touch_daemon_data.work);
	exynos_report_touch_event_by_rtc_tick();
	switch(exynos_touch_daemon_data.mode)
	{
		case V_SCROLL:
		case H_SCROLL:
			ssleep(5);
			break;
		case FLICKING_TO_LEFT:
		case FLICKING_TO_UP:
			ssleep(3);
			break;
	}
	exynos_touch_daemon_data.start = 0;

	printk("[TOUCH DAEMON] Initial Response Time: %d us, Drawing Finish Time: %d ms\n",
		exynos_touch_daemon_data.response_time, exynos_touch_daemon_data.finish_time);
}

static void exynos_touch_daemon_play(void)
{
	unsigned int delay;

	delay = exynos_touch_daemon_data.tp.count * 15;

	clk_enable(rtc_clk);
	rtc_reset();
	schedule_work(&exynos_touch_daemon_data.work);
	exynos_report_touch_event_by_rtc_tick();

	msleep(delay);

	exynos_touch_daemon_data.start = 0;

	printk("Response Time: %d us\n", exynos_touch_daemon_data.response_time);
	printk("Drawing Finish Time: %d ms\n", exynos_touch_daemon_data.finish_time);
}

static void exynos_touch_daemon_scenario_normal(int choice, int period)
{
	struct timespec ts_start, ts_now;
	int step, cnt = 0;
	if(exynos_touch_daemon_data.powerlog == 1) {
		buf_len = 0;
		time_len = 0;
	}

	ktime_get_real_ts(&ts_start);
	printk("%s: start! choice:%d period:%d\n", __func__, choice, period);
	if(exynos_touch_daemon_data.powerlog == 1)
		buf_len += sprintf(exynos_touch_daemon_log_buf + buf_len, "%s: start! choice:%d period:%d\n", __func__,choice, period);

	step = (sizeof((s_normal[choice]))/sizeof(struct touch_point));
	clk_enable(rtc_clk);
	rtc_reset();
	while(cnt < step)		// To do : change step number from hard coding to use 'sizeof' function.
	{
		ktime_get_real_ts(&ts_now);
		if(period >= 1000) {
			if(ts_now.tv_sec - ts_start.tv_sec >= period/1000) {
				printk("%s: run cnt %d\n", __func__, cnt);
				exynos_touch_set_point(&s_normal[choice][cnt]);

				if(exynos_touch_daemon_data.powerlog == 1) {
					schedule_work(&exynos_touch_daemon_data.work);
				}

				exynos_report_touch_event_by_rtc_tick();
				ts_start = ts_now;
				cnt++;

				ssleep(3);

				exynos_touch_daemon_data.start = 0;
				if(exynos_touch_daemon_data.powerlog == 1)
					time_len += sprintf(exynos_touch_daemon_time_buf + time_len, "%8d %8d\n",
						exynos_touch_daemon_data.response_time, exynos_touch_daemon_data.finish_time);
			}
			msleep(50);
		}
		else if (period >= 500) {
			if(ts_now.tv_sec - ts_start.tv_sec >= 1  ||
					ts_now.tv_nsec - ts_start.tv_nsec >= period*1000*1000) {
				printk("%s: run cnt %d\n", __func__, cnt);
				exynos_touch_set_point(&s_normal[choice][cnt]);

				if(exynos_touch_daemon_data.powerlog == 1) {
					schedule_work(&exynos_touch_daemon_data.work);
				}

				exynos_report_touch_event_by_rtc_tick();
				ts_start = ts_now;
				cnt++;

				ssleep(3);

				if(exynos_touch_daemon_data.powerlog == 1) {
					exynos_touch_daemon_data.start = 0;
					time_len += sprintf(exynos_touch_daemon_time_buf + time_len, "%8d %8d\n",
						exynos_touch_daemon_data.response_time, exynos_touch_daemon_data.finish_time);
				}
			}
			msleep(100);
		}
		else
			printk("%s: no supprot period\n",__func__);
	}
	exynos_touch_daemon_data.scenario = 0;
	printk("%s: end\n", __func__);
	if(exynos_touch_daemon_data.powerlog == 1) {
		buf_len += sprintf(exynos_touch_daemon_log_buf + buf_len, "%s: end\n", __func__);
		buf_len += sprintf(exynos_touch_daemon_log_buf + buf_len, "%s\n", exynos_touch_daemon_time_buf);
	}
}

static void exynos_touch_daemon_scenario_repeat(int choice, int period, int total_cnt)
{
	struct timespec ts_start, ts_now;
	int step, cnt = 0;
	int total_cnt_local = 0;

	if(exynos_touch_daemon_data.powerlog == 1) {
		time_len = 0;
		buf_len = 0;
	}

	ktime_get_real_ts(&ts_start);
	printk("%s: start! choice:%d period:%d\n", __func__,choice, period);
	step = (sizeof((s_repeat[choice]))/sizeof(struct touch_point));
	clk_enable(rtc_clk);
	rtc_reset();
	while(total_cnt_local < total_cnt)
	{
		if(cnt >= step)
			cnt = 0;

		ktime_get_real_ts(&ts_now);
		if(period >= 1000) {
			if(ts_now.tv_sec - ts_start.tv_sec >= period/1000) {
				if(exynos_touch_daemon_data.powerlog == 1) {
					if(exynos_touch_daemon_data.start == 1) {
						exynos_touch_daemon_data.start = 0;
							time_len += sprintf(exynos_touch_daemon_time_buf + time_len, "%8d %8d\n",
								exynos_touch_daemon_data.response_time, exynos_touch_daemon_data.finish_time);
					}
				}

				printk("%s: run cnt %d, total cnt %d\n", __func__, cnt, total_cnt_local);
				exynos_touch_set_point(&s_repeat[choice][cnt]);

				if(exynos_touch_daemon_data.powerlog == 1) {
					schedule_work(&exynos_touch_daemon_data.work);
				}

				exynos_report_touch_event_by_rtc_tick();
				ts_start = ts_now;
				cnt++;
				total_cnt_local++;
			}
			msleep(50);
		}
		else if (period >= 500) {
			if(ts_now.tv_sec - ts_start.tv_sec >= 1  ||
					ts_now.tv_nsec - ts_start.tv_nsec >= period*1000*1000) {
				if(exynos_touch_daemon_data.powerlog == 1) {
					if(exynos_touch_daemon_data.start == 1) {
						exynos_touch_daemon_data.start = 0;
						time_len += sprintf(exynos_touch_daemon_time_buf + time_len, "%8d %8d\n",
							exynos_touch_daemon_data.response_time, exynos_touch_daemon_data.finish_time);
					}
				}

				printk("%s: run cnt %d, total cnt %d\n", __func__, cnt, total_cnt_local);
				exynos_touch_set_point(&s_repeat[choice][cnt]);

				if(exynos_touch_daemon_data.powerlog == 1) {
					schedule_work(&exynos_touch_daemon_data.work);
				}

				exynos_report_touch_event_by_rtc_tick();
				ts_start = ts_now;
				cnt++;
				total_cnt_local++;
			}
			msleep(100);
		}
		else
			printk("%s: no supprot period\n",__func__);
	}
	if(exynos_touch_daemon_data.powerlog == 1)
		msleep(period);		// for the last touch input

	exynos_touch_daemon_data.scenario = 0;
	exynos_touch_daemon_data.start = 0;
	if(exynos_touch_daemon_data.powerlog == 1)
		time_len += sprintf(exynos_touch_daemon_time_buf + time_len, "%8d %8d\n",
			exynos_touch_daemon_data.response_time, exynos_touch_daemon_data.finish_time);
	printk("%s: end\n", __func__);
	if(exynos_touch_daemon_data.powerlog == 1) {
		buf_len += sprintf(exynos_touch_daemon_log_buf + buf_len, "%s: end\n", __func__);
		buf_len += sprintf(exynos_touch_daemon_log_buf + buf_len, "%s\n", exynos_touch_daemon_time_buf);
	}
}

static ssize_t store_start(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int value;

	ret = sscanf(buf, "%u", &value);

	if (ret != 1)
		goto out;

	pr_info("exynos_touch_daemon_start : %d\n", value);

	if(value == 1) {
		exynos_touch_set_point(&responsiveness_point[exynos_touch_daemon_data.mode]);
		exynos_touch_daemon_responsiveness();
	} else if (value == 0) {
		exynos_touch_daemon_data.start = 0;
	} else {
		pr_info("Input wrong.. Try again..\n");
	}
out:
	return count;
}

static ssize_t store_record(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int value;

	ret = sscanf(buf, "%u", &value);

	exynos_touch_daemon_data.record = value;

	pr_info("Touch point coordinate recording start!\n");
	exynos_touch_daemon_data.tp.count = 0;

	return count;
}

static ssize_t store_play(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int value;

	ret = sscanf(buf, "%u", &value);

	if (ret != 1)
		return count;

	pr_info("exynos_touch_daemon_start : %d\n", value);

	if(value == 1) {
		exynos_touch_set_point(&exynos_touch_daemon_data.tp);
		exynos_touch_daemon_play();
	} else if (value == 0) {
		exynos_touch_daemon_data.start = 0;
	} else {
		pr_info("Input wrong.. Try again..\n");
	}
	return count;
}

static ssize_t show_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Touch mode is %d\n", exynos_touch_daemon_data.mode);
}

static ssize_t store_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int value;

	ret = sscanf(buf, "%u", &value);

	if (ret != 1)
		return count;

	if(value < 4) {
		exynos_touch_daemon_data.mode = value;
	} else {
		printk("Selected wrong number. Try agian.. ( 0:VERTICAL SCROLL, 1:HORIZAONTAL SCROLL, 2:FLICKING to LEFT, 3:FLICKING to RIGHT )\n");
		return count;
	}

	switch(exynos_touch_daemon_data.mode)
	{
		case V_SCROLL:
			pr_info("Touch daemon is set for responsiveness test of VERTICAL SCROLL\n");
			break;
		case H_SCROLL:
			pr_info("Touch daemon is set for responsiveness test of HORIZONTAL SCROLL\n");
			break;
		case FLICKING_TO_LEFT:
			pr_info("Touch daemon is set for responsiveness test of FLICKING to LEFT\n");
			break;
		case FLICKING_TO_UP:
			pr_info("Touch daemon is set for responsiveness test of FLICKING to UP\n");
			break;
	}
	return count;
}

static ssize_t store_power(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int value;

	ret = sscanf(buf, "%u", &value);

	if (ret != 1)
		return count;

	exynos_touch_daemon_data.scenario = value;

	switch(exynos_touch_daemon_data.scenario)
	{
		case 1:
		case 2:
		case 3:
			exynos_touch_daemon_scenario_normal(exynos_touch_daemon_data.scenario - 1, 5000);
			break;
		case 4:
			exynos_touch_daemon_scenario_repeat(exynos_touch_daemon_data.scenario - 4, 5000, 11);
			break;
		case 5:
			exynos_touch_daemon_scenario_repeat(exynos_touch_daemon_data.scenario - 4, 500, 119);
			break;
		case 6:
			exynos_touch_daemon_scenario_repeat(exynos_touch_daemon_data.scenario - 4, 500, 119);
			break;
	}
	return count;
}

static ssize_t show_booster(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	if(exynos_touch_daemon_data.booster == 0)
		len += sprintf(buf + len, "Disable\n");
	else if(exynos_touch_daemon_data.booster == 1)
		len += sprintf(buf + len, "Endable\n");

	return len;
}

static ssize_t store_booster(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int value;

	ret = sscanf(buf, "%u", &value);

	if (ret != 1)
		return count;

	if(value == 1) {
		pr_info("Touch Booster for daemon is turned on\n");
		exynos_touch_daemon_data.booster = 1;
	} else if(value == 0) {
		pr_info("Touch Booster for daemon is turned off\n");
		exynos_touch_daemon_data.booster = 0;
	} else {
		pr_info("Input wrong.. Try again..\n");
	}
	return count;
}

static ssize_t show_powerlog(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	if(exynos_touch_daemon_data.powerlog == 0)
		len += sprintf(buf + len, "Disable\n");
	else if(exynos_touch_daemon_data.powerlog == 1)
		len += sprintf(buf + len, "Endable\n");

	return len;
}

static ssize_t store_powerlog(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int value;

	ret = sscanf(buf, "%u", &value);

	if (ret != 1)
		return count;

	if(value == 1) {
		pr_info("Enable\n");
		exynos_touch_daemon_data.powerlog = 1;
	} else if(value == 0) {
		pr_info("Disable\n");
		exynos_touch_daemon_data.powerlog = 0;
	} else {
		pr_info("Input wrong.. Try again..\n");
	}
	return count;
}


static DEVICE_ATTR(start, S_IRUGO | S_IWUSR, NULL, store_start);
static DEVICE_ATTR(record, S_IRUGO | S_IWUSR, NULL, store_record);
static DEVICE_ATTR(play, S_IRUGO | S_IWUSR, NULL, store_play);
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, show_mode, store_mode);
static DEVICE_ATTR(booster, S_IRUGO | S_IWUSR, show_booster, store_booster);
static DEVICE_ATTR(power, S_IRUGO | S_IWUSR, NULL, store_power);
static DEVICE_ATTR(powerlog, S_IRUGO | S_IWUSR, show_powerlog, store_powerlog);

static struct attribute *exynos_touch_daemon_attrs[] = {
	&dev_attr_start.attr,
	&dev_attr_record.attr,
	&dev_attr_play.attr,
	&dev_attr_mode.attr,
	&dev_attr_booster.attr,
	&dev_attr_power.attr,
	&dev_attr_powerlog.attr,
	NULL,
};

static struct attribute_group exynos_touch_daemon_attr_group = {
	.attrs = exynos_touch_daemon_attrs,
	.name = "daemon",
};

static void exynos_touch_daemon_frame_counter_work(struct work_struct *work)
{
	struct timespec ts;
	s64 time;
	static int previous_frame = 0;

	ktime_get_real_ts(&exynos_touch_daemon_data.ts);
	exynos_touch_daemon_data.start_time = timespec_to_ns(&exynos_touch_daemon_data.ts);
	exynos_touch_daemon_data.frame = 0;
	exynos_touch_daemon_data.response_time = 0;
	exynos_touch_daemon_data.finish_time = 0;

	exynos_touch_daemon_data.start = 1;

	while(exynos_touch_daemon_data.start)
	{
		ktime_get_real_ts(&ts);
		time = timespec_to_ns(&ts) - timespec_to_ns(&exynos_touch_daemon_data.ts);

		if(time >= 50000000L) {	// 50ms
			//printk("frame = %4d ( %lld ns )\n", exynos_touch_daemon_data.frame, time);		// for DEBUG

			if(exynos_touch_daemon_data.scenario == 0)
				printk("frame = %4d\n", exynos_touch_daemon_data.frame);
			else
				if(exynos_touch_daemon_data.powerlog == 1)
					buf_len += sprintf(exynos_touch_daemon_log_buf + buf_len, "%4d\n", exynos_touch_daemon_data.frame);

			exynos_touch_daemon_data.ts = ts;

			// for drawing finish time
			if(exynos_touch_daemon_data.finish_time == 0) {
				if(exynos_touch_daemon_data.frame != 0 && previous_frame == exynos_touch_daemon_data.frame) {
					exynos_touch_daemon_data.finish_time = (unsigned int)(timespec_to_ns(&ts) - exynos_touch_daemon_data.start_time)/1000000;
				} else {
					previous_frame = exynos_touch_daemon_data.frame;
				}
			}
		}
		usleep_range(100, 500);
	}
	previous_frame = 0;
	return;
}

static int exynos_touch_daemon_suspend(struct device *dev)
{
	return 0;
}

static int exynos_touch_daemon_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops exynos_touch_daemon_pm = {
	.suspend = exynos_touch_daemon_suspend,
	.resume	= exynos_touch_daemon_resume,
};

struct exynos_daemon_data {
	int irq;
};

static int exynos_touch_daemon_probe(struct platform_device *pdev)
{
	int error = 0;
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct exynos_daemon_data *data;

	error = sysfs_create_group(&pdev->dev.kobj, &exynos_touch_daemon_attr_group);

	if (error) {

		dev_err(dev, "Unable to make sysfs of exynos_touch_daemon, error: %d\n",
			error);
		sysfs_remove_group(&pdev->dev.kobj, &exynos_touch_daemon_attr_group);
	}

	/* Set Default Value */
	exynos_touch_daemon_data.start = 0;
	exynos_touch_daemon_data.record = 0;
	exynos_touch_daemon_data.mode = H_SCROLL;
	exynos_touch_daemon_data.tp.count = 0;
	exynos_touch_daemon_data.booster = 0;
	exynos_touch_daemon_data.start_time = 0;
	exynos_touch_daemon_data.response_time = 0;
	exynos_touch_daemon_data.finish_time = 0;
	exynos_touch_daemon_data.scenario = 0;
	exynos_touch_daemon_data.powerlog = 0;

	data = devm_kzalloc(&pdev->dev, sizeof(struct exynos_daemon_data), GFP_KERNEL);

	INIT_WORK(&exynos_touch_daemon_data.work, exynos_touch_daemon_frame_counter_work);

	rtc_clk = devm_clk_get(&pdev->dev, "gate_rtc");
	if (IS_ERR(rtc_clk)) {
		dev_err(&pdev->dev, "failed to find rtc clock source\n");
		ret = PTR_ERR(rtc_clk);
		rtc_clk = NULL;
		return ret;
	}

	ret = clk_prepare_enable(rtc_clk);
	printk("[%s] clk = %d\n",__func__,ret);
	rtc_tick_init();
//	clk_disable(rtc_clk);
	data->irq = platform_get_irq(pdev, 0);

	ret = request_threaded_irq(data->irq, NULL, exynos_touch_daemon_thread_irq,
				  IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				  "exynos_touch_daemon", data);
	exynos_touch_daemon_log_buf = vmalloc(sizeof(char) * 50000);
	exynos_touch_daemon_time_buf = vmalloc(sizeof(char) * 10000);

	return 0;
}

static int exynos_touch_daemon_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &exynos_touch_daemon_attr_group);
	vfree(exynos_touch_daemon_log_buf);
	vfree(exynos_touch_daemon_time_buf);

	return 0;
}

static struct of_device_id exynos_touch_daemon_dt_ids[] = {
	{ .compatible = "exynos_touch_daemon" },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos_touch_daemon_dt_ids);

static struct platform_driver exynos_touch_daemon_platform_driver = {
	.probe		= exynos_touch_daemon_probe,
	.remove		= exynos_touch_daemon_remove,
	.driver		= {
		.name	= "exynos_touch_daemon",
		.owner	= THIS_MODULE,
		.pm	= &exynos_touch_daemon_pm,
		.of_match_table = exynos_touch_daemon_dt_ids,
	}
};

static int show_frame(struct seq_file *buf, void *unused)
{
	seq_printf(buf, "%s\n", exynos_touch_daemon_log_buf);
	return 0;
}

static int exynos_touch_daemon_frame_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_frame, NULL);
}

static const struct file_operations exynos_touch_daemon_frame_fops = {
	.owner = THIS_MODULE,
	.open = exynos_touch_daemon_frame_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int show_recorded_position(struct seq_file *buf, void *unused)
{
	ssize_t len = 0;
	int i;

	len += seq_printf(buf + len, "touch count = %d\n", exynos_touch_daemon_data.tp.count);

	len += seq_printf(buf + len, "x =\n");
	for(i=0; i<exynos_touch_daemon_data.tp.count; i++)
	{
		len += seq_printf(buf + len, "%d,\t", exynos_touch_daemon_data.tp.x[i]);
		if((i+1)%20 == 0)
			len += seq_printf(buf + len, "\b\n");
	}
	len += seq_printf(buf + len, "\n\n");

	len += seq_printf(buf + len, "y =\n");
	for(i=0; i<exynos_touch_daemon_data.tp.count; i++)
	{
		len += seq_printf(buf + len, "%d,\t", exynos_touch_daemon_data.tp.y[i]);
		if((i+1)%20 == 0)
			len += seq_printf(buf + len, "\b\n");
	}
	len += seq_printf(buf + len, "\n\n");

	len += seq_printf(buf + len, "wx =\n");
	for(i=0; i<exynos_touch_daemon_data.tp.count; i++)
	{
		len += seq_printf(buf + len, "%d,\t", exynos_touch_daemon_data.tp.wx[i]);
		if((i+1)%20 == 0)
			len += seq_printf(buf + len, "\b\n");
	}
	len += seq_printf(buf + len, "\n\n");

	len += seq_printf(buf + len, "wy =\n");
	for(i=0; i<exynos_touch_daemon_data.tp.count; i++)
	{
		len += seq_printf(buf + len, "%d,\t", exynos_touch_daemon_data.tp.wy[i]);
		if((i+1)%20 == 0)
			len += seq_printf(buf + len, "\b\n");
	}
	len += seq_printf(buf + len, "\n\n");

	return 0;
}

static int exynos_touch_daemon_position_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_recorded_position, NULL);
}

static const struct file_operations exynos_touch_daemon_position_fops = {
	.owner = THIS_MODULE,
	.open = exynos_touch_daemon_position_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init exynos_touch_daemon_init(void)
{
	exynos_touch_daemon_position_dentry = debugfs_create_file("exynos_touch_daemon_position",
			S_IRUGO, NULL, NULL, &exynos_touch_daemon_position_fops);
	exynos_touch_daemon_log_dentry = debugfs_create_file("exynos_touch_daemon_frame_log",
			S_IRUGO, NULL, NULL, &exynos_touch_daemon_frame_fops);

	return platform_driver_register(&exynos_touch_daemon_platform_driver);
}

static void __exit exynos_touch_daemon_exit(void)
{
	platform_driver_unregister(&exynos_touch_daemon_platform_driver);
}

module_init(exynos_touch_daemon_init);
module_exit(exynos_touch_daemon_exit);

MODULE_ALIAS("exynos_touch_daemon");
MODULE_DESCRIPTION("exynos_touch_daemon");
MODULE_AUTHOR("seongmin.ahn<seongmin.ahn@samsung.com>");
MODULE_LICENSE("GPL");
