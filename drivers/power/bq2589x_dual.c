/*
 * BQ2589x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/muic_tsu6721.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/alarmtimer.h>

#include "bq2589x_reg.h"
#define TAG "[charger]"
enum bq2589x_vbus_type {
	BQ2589X_VBUS_NONE,
	BQ2589X_VBUS_OTG,
	BQ2589X_VBUS_USB_HOST,
	BQ2589X_VBUS_ADAPTER,
	BQ2589X_VBUS_TYPE_NUM,
};

enum bq2589x_part_no{
	BQ25890 = 0x03,
	BQ25892 = 0x00,
	BQ25895 = 0x07,
	BQ25896 = 0x00,//TODO:
};

struct gpio_config{
	int chg1_int;
	int chg1_en;
	int chg2_int;
	int chg2_en;
}gpio_config;
#define BQ2589X_STATUS_PLUGIN		0x0001    //plugin
#define BQ2589X_STATUS_PG			0x0002    //power good
//#define BQ2589X_STATUS_CHG_DONE	0x0004
#define BQ2589X_STATUS_FAULT		0x0008

#define BQ2589X_STATUS_EXIST		0x0100
#define BQ2589X_STATUS_CHARGE_ENABLE 0x0200
#define DEBUG_CHARGER 1
static int  TEMP_POLL_TIME = 10;

struct bq2589x {
	struct device *dev;
	struct i2c_client *client;

	enum   bq2589x_part_no part_no;
	int    revision;

	unsigned int    status;  //charger status:
	int		vbus_type;		//
	int              last_vbus_type;

	bool    interrupt;

	int     vbus_volt;
	int     vbat_volt;

	struct delayed_work irq_work;
	struct power_supply usb;
	struct power_supply wall;
	bool usb_mode;
	bool non_standard;
	bool otg_mode;
	bool fast_usb_mode;
	bool usb_no_current;
	int temp;
	int last_temp;

	wait_queue_head_t wq;
	struct notifier_block charger_notifier;
	struct notifier_block temp_notifier;

	struct class *charger_class;
	struct device *charger_device;
	struct wake_lock charger_wake;
	struct wake_lock charger_alarm_wake;
	struct wake_lock charger_i2c_wake;
	struct alarm alarm;
	struct work_struct temp_work;
	struct work_struct fb_blank_work;
	struct work_struct fb_unblank_work;
	struct notifier_block fb_notifier;
	int target_voltage;
	struct delayed_work otg_work;
	struct delayed_work status_work;
	bool is_screen_on;
	bool is_already_shutdown;
};

struct volt_control_t{
	int TuneTargetVolt;
	bool toTuneDownVolt;
	bool TuneDownVoltDone;
	bool toTuneUpVolt;
	bool TuneUpVoltDone;
	int TuneCounter;
	bool TuneFail;
};

static struct bq2589x *g_bq1;
static struct bq2589x *g_bq2;
static struct volt_control_t voltcontrol;

static struct task_struct *charger_thread;


static DEFINE_MUTEX(bq2589x_i2c_lock);

static int bq2589x_read_byte(struct bq2589x *bq, u8 *data, u8 reg)
{
	int ret;
	int retry_count=5;

	mutex_lock(&bq2589x_i2c_lock);
	wake_lock(&bq->charger_i2c_wake);
	do{
		ret = i2c_smbus_read_byte_data(bq->client, reg);
		if(ret < 0){
			msleep(50);
			pr_err(TAG"%s read i2c fail\n",__func__);
		}
	}while(ret<0 && retry_count-->0);
	wake_unlock(&bq->charger_i2c_wake);

	*data = (u8)ret;
	mutex_unlock(&bq2589x_i2c_lock);

	return 0;
}

static int bq2589x_write_byte(struct bq2589x *bq, u8 reg, u8 data)
{
	int ret;
	int retry_count=5;

	mutex_lock(&bq2589x_i2c_lock);
	wake_lock(&bq->charger_i2c_wake);
	do{
		ret = i2c_smbus_write_byte_data(bq->client, reg, data);
		if(ret < 0){
			msleep(50);
			pr_err(TAG"%s write i2c fail\n",__func__);
		}
	}while(ret<0 && retry_count-->0);
	wake_unlock(&bq->charger_i2c_wake);
	mutex_unlock(&bq2589x_i2c_lock);
	return ret;
}

static int bq2589x_update_bits(struct bq2589x *bq, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = bq2589x_read_byte(bq, &tmp, reg);

	if (ret)
		return ret;

	tmp &= ~mask;
	tmp |= data & mask;

	return bq2589x_write_byte(bq, reg, tmp);
}

static enum bq2589x_vbus_type bq2589x_get_vbus_type(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;
	if(DEBUG_CHARGER){
		//pr_info(TAG"%s start++++++++++\n",__func__);
	}
#if 0
	wait_event_interruptible_timeout(g_bq1->wq,g_bq1->interrupt,HZ/5);
	if(g_bq1->interrupt)
		return g_bq1->vbus_type;
#endif
	//msleep(100);
	if(g_bq1->otg_mode){
		g_bq1->vbus_type = BQ2589X_VBUS_OTG;
	}else if(g_bq1->usb_mode){
		g_bq1->vbus_type = BQ2589X_VBUS_USB_HOST;
	}else{
		ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
		if (ret < 0) return 0;
		if(val & BQ2589X_VBUS_STAT_MASK){
			g_bq1->vbus_type = BQ2589X_VBUS_ADAPTER;
		}else{
			g_bq1->vbus_type = BQ2589X_VBUS_NONE;
		}
	}
	if(DEBUG_CHARGER){
		switch(g_bq1->vbus_type){
			case BQ2589X_VBUS_NONE:
				pr_info(TAG"%s BQ2589X_VBUS_NONE\n",__func__);
				break;
			case BQ2589X_VBUS_ADAPTER:
				pr_info(TAG"%s BQ2589X_VBUS_ADAPTER\n",__func__);
				break;
			case BQ2589X_VBUS_USB_HOST:
				pr_info(TAG"%s BQ2589X_VBUS_USB_HOST\n",__func__);
				break;
			case BQ2589X_VBUS_OTG:
				pr_info(TAG"%s BQ2589X_VBUS_OTG\n",__func__);
				break;
		}
	}
	return g_bq1->vbus_type;
}


static int bq2589x_update_charge_params(struct bq2589x *bq)
{
	return 0;
}



static int bq2589x_enable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_ENABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_OTG_CONFIG_MASK, val);

}

static int bq2589x_disable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_DISABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_OTG_CONFIG_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_disable_otg);
#if 0
static int bq2589x_set_otg_volt(struct bq2589x *bq, int volt )
{
	u8 val = 0;

	if (volt < BQ2589X_BOOSTV_BASE)
		volt = BQ2589X_BOOSTV_BASE;
	if (volt > BQ2589X_BOOSTV_BASE + (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) * BQ2589X_BOOSTV_LSB)
		volt = BQ2589X_BOOSTV_BASE + (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) * BQ2589X_BOOSTV_LSB;


	val = ((volt - BQ2589X_BOOSTV_BASE)/BQ2589X_BOOSTV_LSB) << BQ2589X_BOOSTV_SHIFT;

	return bq2589x_update_bits(bq,BQ2589X_REG_0A,BQ2589X_BOOSTV_MASK,val);

}
EXPORT_SYMBOL_GPL(bq2589x_set_otg_volt);

static int bq2589x_set_otg_current(struct bq2589x *bq, int curr )
{
	u8 temp;

	if(curr  == 500)
		temp = BQ2589X_BOOST_LIM_500MA;
	else if(curr == 700)
		temp = BQ2589X_BOOST_LIM_700MA;
	else if(curr == 1100)
		temp = BQ2589X_BOOST_LIM_1100MA;
	else if(curr == 1600)
		temp = BQ2589X_BOOST_LIM_1600MA;
	else if(curr == 1800)
		temp = BQ2589X_BOOST_LIM_1800MA;
	else if(curr == 2100)
		temp = BQ2589X_BOOST_LIM_2100MA;
	else if(curr == 2400)
		temp = BQ2589X_BOOST_LIM_2400MA;
	else
		temp = BQ2589X_BOOST_LIM_1300MA;

	return bq2589x_update_bits(bq,BQ2589X_REG_0A,BQ2589X_BOOST_LIM_MASK,temp << BQ2589X_BOOST_LIM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_otg_current);
#endif
static int bq2589x_enable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_ENABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_CHG_CONFIG_MASK, val);
	if(ret == 0)
		bq->status |= BQ2589X_STATUS_CHARGE_ENABLE;
	return ret;
}

static int bq2589x_disable_charger(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_CHG_DISABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_CHG_CONFIG_MASK, val);
	if(ret == 0)
		bq->status &=~BQ2589X_STATUS_CHARGE_ENABLE;
	return ret;
}
int bq2589x_adc_stop(struct bq2589x *bq)//stop continue scan
{
	return bq2589x_update_bits(bq,BQ2589X_REG_02,BQ2589X_CONV_RATE_MASK, BQ2589X_ADC_CONTINUE_DISABLE << BQ2589X_CONV_RATE_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_adc_stop);


/* interfaces that can be called by other module */
int bq2589x_adc_start(struct bq2589x *bq, bool oneshot)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq,&val,BQ2589X_REG_02);
	if(ret < 0){
		dev_err(bq->dev,"%s failed to read register 0x02:%d\n",__func__,ret);
		return ret;
	}

	//if(((val & BQ2589X_CONV_RATE_MASK) >> BQ2589X_CONV_RATE_SHIFT) == BQ2589X_ADC_CONTINUE_ENABLE)
	//return 0; //is doing continuous scan
	if(oneshot){
		bq2589x_adc_stop(bq);
		ret = bq2589x_update_bits(bq,BQ2589X_REG_02,BQ2589X_CONV_START_MASK, BQ2589X_CONV_START << BQ2589X_CONV_START_SHIFT);
	}
	//else
	//ret = bq2589x_update_bits(bq,BQ2589X_REG_02,BQ2589X_CONV_RATE_MASK, BQ2589X_ADC_CONTINUE_ENABLE << BQ2589X_CONV_RATE_SHIFT);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_start);


int bq2589x_auto_ico(struct bq2589x *bq,bool en)//stop auto ico
{
	int val=0;
	if(en)
		val = BQ2589X_ICO_ENABLE;
	else
		val = BQ2589X_ICO_DISABLE;
	return bq2589x_update_bits(bq,BQ2589X_REG_02,BQ2589X_ICOEN_MASK,  val<< BQ2589X_ICOEN_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_auto_ico);


int bq2589x_adc_read_battery_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0E);
	if(ret < 0){
		dev_err(bq->dev,"read battery voltage failed :%d\n",ret);
		return ret;
	}
	else{
		volt = BQ2589X_BATV_BASE + ((val & BQ2589X_BATV_MASK) >> BQ2589X_BATV_SHIFT) * BQ2589X_BATV_LSB ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_battery_volt);


int bq2589x_adc_read_sys_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0F);
	if(ret < 0){
		dev_err(bq->dev,"read system voltage failed :%d\n",ret);
		return ret;
	}
	else{
		volt = BQ2589X_SYSV_BASE + ((val & BQ2589X_SYSV_MASK) >> BQ2589X_SYSV_SHIFT) * BQ2589X_SYSV_LSB ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_sys_volt);

int bq2589x_adc_read_vbus_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_11);
	if(ret < 0){
		dev_err(bq->dev,"read vbus voltage failed :%d\n",ret);
		return ret;
	}
	else{
		volt = BQ2589X_VBUSV_BASE + ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT) * BQ2589X_VBUSV_LSB ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_vbus_volt);

int bq2589x_adc_read_temperature(struct bq2589x *bq)
{
	uint8_t val;
	int temp;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_10);
	if(ret < 0){
		dev_err(bq->dev,"read temperature failed :%d\n",ret);
		return ret;
	}
	else{
		temp = BQ2589X_TSPCT_BASE + ((val & BQ2589X_TSPCT_MASK) >> BQ2589X_TSPCT_SHIFT) * BQ2589X_TSPCT_LSB ;
		return temp;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_temperature);

int bq2589x_adc_read_charge_current(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_12);
	if(ret < 0){
		dev_err(bq->dev,"read charge current failed :%d\n",ret);
		return ret;
	}
	else{
		volt = (int)(BQ2589X_ICHGR_BASE + ((val & BQ2589X_ICHGR_MASK) >> BQ2589X_ICHGR_SHIFT) * BQ2589X_ICHGR_LSB) ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_charge_current);

int bq2589x_set_chargecurrent(struct bq2589x *bq,int curr)
{

	u8 ichg;

	if(DEBUG_CHARGER){
		pr_info(TAG"%s set current to %d\n", __func__,curr);
	}
	ichg = (curr - BQ2589X_ICHG_BASE)/BQ2589X_ICHG_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_04,BQ2589X_ICHG_MASK, ichg << BQ2589X_ICHG_SHIFT);

}
EXPORT_SYMBOL_GPL(bq2589x_set_chargecurrent);
int bq2589x_get_chargecurrent(struct bq2589x *bq)
{
	u8 ichg=0;

	if(DEBUG_CHARGER){
		//pr_info(TAG"%s set current to %d\n", __func__,curr);
	}
	bq2589x_read_byte(bq,&ichg,BQ2589X_REG_04);
	return ((ichg&BQ2589X_ICHG_MASK) >> BQ2589X_ICHG_SHIFT) * BQ2589X_ICHG_LSB;
}

int bq2589x_set_term_current(struct bq2589x *bq,int curr)
{
	u8 iterm;

	iterm = (curr - BQ2589X_ITERM_BASE) / BQ2589X_ITERM_LSB;

	return bq2589x_update_bits(bq, BQ2589X_REG_05,BQ2589X_ITERM_MASK, iterm << BQ2589X_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_term_current);


int bq2589x_set_prechg_current(struct bq2589x *bq,int curr)
{
	u8 iprechg;

	iprechg = (curr - BQ2589X_IPRECHG_BASE) / BQ2589X_IPRECHG_LSB;

	return bq2589x_update_bits(bq, BQ2589X_REG_05,BQ2589X_IPRECHG_MASK, iprechg << BQ2589X_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_prechg_current);

int bq2589x_set_chargevoltage(struct bq2589x *bq,int volt)
{
	u8 val;

	val = (volt - BQ2589X_VREG_BASE)/BQ2589X_VREG_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_06,BQ2589X_VREG_MASK, val << BQ2589X_VREG_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_chargevoltage);


int bq2589x_set_input_volt_limit(struct bq2589x *bq,int volt)
{
	u8 val;
	val = (volt - BQ2589X_VINDPM_BASE)/BQ2589X_VINDPM_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_0D,BQ2589X_VINDPM_MASK, val << BQ2589X_VINDPM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_input_volt_limit);

int bq2589x_set_input_current_limit(struct bq2589x *bq,int curr)
{
	u8 val;

	pr_info(TAG"%s set input current to %d\n",__func__,curr);
	val = (curr - BQ2589X_IINLIM_BASE)/BQ2589X_IINLIM_LSB;
	bq2589x_update_bits(g_bq2, BQ2589X_REG_00,BQ2589X_IINLIM_MASK, val << BQ2589X_IINLIM_SHIFT);
	return bq2589x_update_bits(bq, BQ2589X_REG_00,BQ2589X_IINLIM_MASK, val << BQ2589X_IINLIM_SHIFT);
}
int bq2589x_get_input_current_limit(struct bq2589x *bq)
{
	u8 val=0;
	int ret =0;

	ret = bq2589x_read_byte(bq,&val,BQ2589X_REG_00);
	if(ret < 0){
		pr_err(TAG"%s read fail\n",__func__);
	}

	return (val & BQ2589X_IINLIM_MASK) * BQ2589X_IINLIM_LSB + BQ2589X_IINLIM_BASE;
}

int bq2589x_disable_ilim_pin(struct bq2589x *bq)
{
	u8 val = BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK, val);
}

int bq2589x_force_vindpm(struct bq2589x *bq)
{
	u8 val = BQ2589X_FORCE_VINDPM_ENABLE << BQ2589X_FORCE_VINDPM_SHIFT;
	return bq2589x_update_bits(bq, BQ2589X_REG_0D,BQ2589X_FORCE_VINDPM_MASK, val);
}

EXPORT_SYMBOL_GPL(bq2589x_set_input_current_limit);


int bq2589x_set_vindpm_offset(struct bq2589x *bq, int offset)
{
	u8 val;

	val = (offset - BQ2589X_VINDPMOS_BASE)/BQ2589X_VINDPMOS_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_01,BQ2589X_VINDPMOS_MASK, val << BQ2589X_VINDPMOS_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_vindpm_offset);


void bq2589x_start_charging(struct bq2589x *bq)
{
	bq2589x_update_charge_params(bq);
	bq2589x_enable_charger(bq);// in case of charger enable bit is cleared due to fault
}
EXPORT_SYMBOL_GPL(bq2589x_start_charging);

void bq2589x_stop_charging(struct bq2589x *bq)
{
	bq2589x_disable_charger(bq);// in case of charger enable bit is cleared due to fault
}
EXPORT_SYMBOL_GPL(bq2589x_stop_charging);

int bq2589x_get_charging_status(struct bq2589x *bq)
{
	u8 val = 0;
	int ret;

	ret = bq2589x_read_byte(bq,&val, BQ2589X_REG_0B);
	if(ret < 0){
		dev_err(bq->dev,"%s Failed to read register 0x0b:%d\n",__func__,ret);
		return ret;
	}
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;
	return val;
}
EXPORT_SYMBOL_GPL(bq2589x_get_charging_status);

void bq2589x_set_otg(struct bq2589x *bq,int enable)
{
	int ret;

	if(enable){
		ret = bq2589x_enable_otg(bq);
		if(ret < 0){
			dev_err(bq->dev,"%s:Failed to enable otg-%d\n",__func__,ret);
			return;
		}
	}
	else{
		ret = bq2589x_disable_otg(bq);
		if(ret < 0){
			dev_err(bq->dev,"%s:Failed to disable otg-%d\n",__func__,ret);
		}
	}
}
EXPORT_SYMBOL_GPL(bq2589x_set_otg);

int bq2589x_set_watchdog_timer(struct bq2589x *bq,u8 timeout)
{
	return bq2589x_update_bits(bq,BQ2589X_REG_07,BQ2589X_WDT_MASK, (u8)((timeout - BQ2589X_WDT_BASE)/BQ2589X_WDT_LSB)<< BQ2589X_WDT_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_set_watchdog_timer);

int bq2589x_disable_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_DISABLE << BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2589x_disable_watchdog_timer);
int bq2589x_termination_enable(struct bq2589x *bq,bool on)
{
	return bq2589x_update_bits(bq,BQ2589X_REG_07,BQ2589X_EN_TERM_MASK, (!!on)<< BQ2589X_EN_TERM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_termination_enable);

int bq2589x_reset_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_RESET << BQ2589X_WDT_RESET_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03, BQ2589X_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2589x_reset_watchdog_timer);

int bq2589x_force_dpdm(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_FORCE_DPDM_MASK, val);
	if(ret) return ret;

	//
	mdelay(10);//TODO: how much time needed to finish dpdm detect?
	return bq2589x_update_charge_params(bq);

}
EXPORT_SYMBOL_GPL(bq2589x_force_dpdm);

int bq2589x_reset_chip(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_RESET << BQ2589X_RESET_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_14, BQ2589X_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_reset_chip);

int bq2589x_enter_ship_mode(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_BATFET_OFF << BQ2589X_BATFET_DIS_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_BATFET_DIS_MASK, val);
	return ret;
	//any other work to do?

}
EXPORT_SYMBOL_GPL(bq2589x_enter_ship_mode);

int bq2589x_enter_hiz_mode(struct bq2589x *bq)
{
	u8 val = BQ2589X_HIZ_ENABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_enter_hiz_mode);

int bq2589x_exit_hiz_mode(struct bq2589x *bq)
{

	u8 val = BQ2589X_HIZ_DISABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_exit_hiz_mode);

int bq2589x_get_hiz_mode(struct bq2589x *bq,u8* state)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_00);
	if (ret) return ret;
	*state = (val & BQ2589X_ENHIZ_MASK) >> BQ2589X_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(bq2589x_get_hiz_mode);


int bq2589x_pumpx_enable(struct bq2589x *bq,int enable)
{
	u8 val;
	int ret;

	if(enable)
		val = BQ2589X_PUMPX_ENABLE << BQ2589X_EN_PUMPX_SHIFT;
	else
		val = BQ2589X_PUMPX_DISABLE << BQ2589X_EN_PUMPX_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_04, BQ2589X_EN_PUMPX_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_enable);

int bq2589x_set_ir_compensate(struct bq2589x *bq, int ir)
{
	int ret;
	u8 val;

	val = BQ2589X_BAT_COMP_BASE +( (ir/BQ2589X_BAT_COMP_LSB) << BQ2589X_BAT_COMP_SHIFT);

	ret = bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_BAT_COMP_MASK, val);

	return ret;
}
int bq2589x_set_thermal(struct bq2589x *bq, int thermal)
{
	int ret;
	u8 val;

	val = thermal << BQ2589X_TREG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_08, BQ2589X_TREG_MASK, val);

	return ret;
}


int bq2589x_pumpx_increase_volt(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_PUMPX_UP << BQ2589X_PUMPX_UP_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_PUMPX_UP_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_increase_volt);

int bq2589x_pumpx_increase_volt_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_09);
	if(ret) return ret;

	if(val & BQ2589X_PUMPX_UP_MASK)
		return 1;   // not finished
	else
		return 0;   // pumpx up finished

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_increase_volt_done);

int bq2589x_pumpx_decrease_volt(struct bq2589x *bq)
{
	u8 val;
	int ret;

	val = BQ2589X_PUMPX_DOWN << BQ2589X_PUMPX_DOWN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_PUMPX_DOWN_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_decrease_volt);

int bq2589x_pumpx_decrease_volt_done(struct bq2589x *bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_09);
	if(ret) return ret;

	if(val & BQ2589X_PUMPX_DOWN_MASK)
		return 1;   // not finished
	else
		return 0;   // pumpx down finished

}
EXPORT_SYMBOL_GPL(bq2589x_pumpx_decrease_volt_done);

static int bq2589x_force_ico(struct bq2589x* bq)
{
	u8 val;
	int ret;

	val = BQ2589X_FORCE_ICO << BQ2589X_FORCE_ICO_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_09, BQ2589X_FORCE_ICO_MASK, val);

	return ret;
}

static int bq2589x_check_force_ico_done(struct bq2589x* bq)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_14);
	if(ret) return ret;

	if(val & BQ2589X_ICO_OPTIMIZED_MASK)
		return 1;  //finished
	else
		return 0;   // in progress
}

static int bq2589x_read_idpm_limit(struct bq2589x* bq)
{
	uint8_t val;
	int curr;
	int ret;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_13);
	if(ret < 0){
		dev_err(bq->dev,"read vbus voltage failed :%d\n",ret);
		return ret;
	}
	else{
		curr = BQ2589X_IDPM_LIM_BASE + ((val & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT) * BQ2589X_IDPM_LIM_LSB ;
		return curr;
	}
}

static inline int bq2589x_is_charge_enable(struct bq2589x* bq)
{
	return(bq->status & BQ2589X_STATUS_CHARGE_ENABLE);
}
#if 0
static bool bq2589x_is_charge_done(struct bq2589x* bq)
{
	int ret;
	u8 val;

	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if(ret < 0){
		dev_err(bq->dev,"%s:read REG0B failed :%d\n",__func__,ret);
		return false;
	}
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;

	return(val == BQ2589X_CHRG_STAT_CHGDONE);
}
#endif
static int bq2589x_init_device(struct bq2589x *bq)
{
	int ret;
	ret = bq2589x_disable_watchdog_timer(bq);
	ret = bq2589x_auto_ico(bq,false);
	ret = bq2589x_exit_hiz_mode(bq);
	ret = bq2589x_set_term_current(bq,64);

	ret = bq2589x_set_input_current_limit(bq,2000);
	ret = bq2589x_disable_ilim_pin(bq);
	ret = bq2589x_force_vindpm(bq);
	ret = bq2589x_set_input_volt_limit(bq,4400);
	ret = bq2589x_set_chargecurrent(bq,2200);
	ret = bq2589x_set_chargevoltage(bq,4352);
	ret = bq2589x_disable_ilim_pin(bq);
	ret = bq2589x_enable_charger(bq);
	ret = bq2589x_adc_stop(bq);
	ret = bq2589x_set_thermal(bq,BQ2589X_TREG_80C);
	return ret;
}

static bool bq2589x_get_pg(struct bq2589x *bq)
{
	int ret = 0;
	u8 val = 0;
	ret = bq2589x_read_byte(bq, &val, BQ2589X_REG_0B);
	if(ret < 0) return false;

	return val & BQ2589X_PG_STAT_MASK;
}

static int bq2589x_charge_status(struct bq2589x * bq)
{
	u8 val = 0;
	int ret=0;

	ret = bq2589x_read_byte(bq,&val, BQ2589X_REG_0B);
	if(ret < 0){
		pr_err(TAG"%s read fail\n",__func__);
	}
	val &= BQ2589X_CHRG_STAT_MASK;
	val >>= BQ2589X_CHRG_STAT_SHIFT;
	if(bq2589x_get_pg(bq)){
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	}else{
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}
	switch(val){
		case BQ2589X_CHRG_STAT_FASTCHG:
			return POWER_SUPPLY_CHARGE_TYPE_FAST;
		case BQ2589X_CHRG_STAT_PRECHG:
			return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		case BQ2589X_CHRG_STAT_CHGDONE:
			return POWER_SUPPLY_CHARGE_TYPE_SLOW;
		case BQ2589X_CHRG_STAT_IDLE:
			return POWER_SUPPLY_CHARGE_TYPE_NONE;
		default:
			return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}
}

static enum power_supply_property bq2589x_charger_props[] = {
	POWER_SUPPLY_PROP_CHARGE_TYPE, /* Charger status output */
	POWER_SUPPLY_PROP_ONLINE, /* External power source */
};

static int bq2589x_usb_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{

	struct bq2589x *bq = container_of(psy, struct bq2589x, usb);
	static int saved_online = 0;

	switch(psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			if(g_bq1->is_already_shutdown){
				val->intval = saved_online;
			}else{
				if(!bq2589x_get_pg(bq)){
					val->intval = 0;
				}else{
					val->intval = bq->usb_mode;
				}
				saved_online = val->intval;
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			val->intval = bq2589x_charge_status(bq);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int bq2589x_wall_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{

	struct bq2589x *bq = container_of(psy, struct bq2589x, wall);
	static int saved_online = 0;

	switch(psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			if(g_bq1->is_already_shutdown){
				val->intval = saved_online;
			}else{
				if(!bq2589x_get_pg(bq)){
					val->intval = 0;
				}else if(g_bq1->vbus_type == BQ2589X_VBUS_ADAPTER){
					val->intval = 1;
				}else{
					val->intval = 0;
				}
				saved_online = val->intval;
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			val->intval = bq2589x_charge_status(bq);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}



static int bq2589x_psy_register(struct bq2589x *bq)
{
	int ret;

	bq->usb.name = "usb";
	bq->usb.type = POWER_SUPPLY_TYPE_USB;
	bq->usb.properties = bq2589x_charger_props;
	bq->usb.num_properties = ARRAY_SIZE(bq2589x_charger_props);
	bq->usb.get_property = bq2589x_usb_get_property;
	bq->usb.external_power_changed = NULL;

	ret = power_supply_register(bq->dev, &bq->usb);
	if(ret < 0){
		dev_err(bq->dev,"%s:failed to register usb psy:%d\n",__func__,ret);
		return ret;
	}


	bq->wall.name = "Wall";
	bq->wall.type = POWER_SUPPLY_TYPE_MAINS;
	bq->wall.properties = bq2589x_charger_props;
	bq->wall.num_properties = ARRAY_SIZE(bq2589x_charger_props);
	bq->wall.get_property = bq2589x_wall_get_property;
	bq->wall.external_power_changed = NULL;

	ret = power_supply_register(bq->dev, &bq->wall);
	if(ret < 0){
		dev_err(bq->dev,"%s:failed to register wall psy:%d\n",__func__,ret);
		goto fail_1;
	}

	return 0;

fail_1:
	power_supply_unregister(&bq->usb);

	return ret;
}

static int bq2589x_detect_device(struct bq2589x* bq)
{
	int ret;
	u8 data;

	ret = bq2589x_read_byte(bq,&data,BQ2589X_REG_14);
	if(ret == 0){
		bq->part_no = (data & BQ2589X_PN_MASK) >> BQ2589X_PN_SHIFT;
		bq->revision = (data & BQ2589X_DEV_REV_MASK) >> BQ2589X_DEV_REV_SHIFT;
	}

	return ret;
}

int bq2589x_read_vbus_volt(struct bq2589x *bq)
{
	int ret;
	u8 val;

	ret = bq2589x_adc_start(bq,true); // oneshot
	if(ret < 0){
		dev_err(bq->dev,"%s:failed to start adc\n",__func__);
		return ret;
	}

	do{
		wait_event_interruptible_timeout(g_bq1->wq,g_bq1->interrupt,500*HZ/1000);
		if(g_bq1->interrupt){
			pr_info(TAG"%s has been interrupted\n",__func__);
			return g_bq1->vbus_volt;
		}
		//msleep(500);
		ret = bq2589x_read_byte(bq,&val,BQ2589X_REG_02);
	}while(ret == 0 && (val & BQ2589X_CONV_START_MASK));

	ret = g_bq1->vbus_volt= bq2589x_adc_read_vbus_volt(bq);
	//pr_info(TAG"battery_volt = %d\n",bq2589x_adc_read_battery_volt(bq));
	//pr_info(TAG"charge_current = %d\n",bq2588x_adc_read_charge_current(bq));
	//pr_info(TAG"sys_volt = %d\n",bq2589x_adc_read_sys_volt(bq));
	//pr_info(TAG"chip temperature = %d\n",bq2589x_adc_read_temperature(bq));
	//bq2589x_adc_stop(bq);

	return ret;

}
void bq2589x_ir_compensation(void)
{

	int ret=0;
	u8 val=0;
	int vbat_charger=0;
	int vbat_gauge=0;
	int ibat_current=0;

	int resistor=0;
	struct power_supply *gauge;
	union power_supply_propval gauge_val;

	ret = bq2589x_adc_start(g_bq1,true); // oneshot
	if(ret < 0){
		dev_err(g_bq1->dev,"%s:failed to start adc\n",__func__);
	}

	do{
		wait_event_interruptible_timeout(g_bq1->wq,g_bq1->interrupt,500*HZ/1000);
		if(g_bq1->interrupt){
			pr_info(TAG"%s has been interrupted\n",__func__);
			return;
		}
		//msleep(500);
		ret = bq2589x_read_byte(g_bq1,&val,BQ2589X_REG_02);
	}while(ret == 0 && (val & BQ2589X_CONV_START_MASK));

	vbat_charger = bq2589x_adc_read_battery_volt(g_bq1);
	bq2589x_adc_stop(g_bq1);


	gauge = power_supply_get_by_name("bq2753x-0");
	if(gauge){
		gauge->get_property(gauge, POWER_SUPPLY_PROP_VOLTAGE_NOW,&gauge_val);
		vbat_gauge = gauge_val.intval;

		gauge->get_property(gauge, POWER_SUPPLY_PROP_CURRENT_NOW,&gauge_val);
		ibat_current = gauge_val.intval;

		resistor = (vbat_charger*1000 - vbat_gauge)*1000/ibat_current;
		if(resistor > 0){
			bq2589x_set_ir_compensate(g_bq1,resistor);
			bq2589x_set_ir_compensate(g_bq2,resistor);
		}
	}
	if(DEBUG_CHARGER)
		pr_info(TAG"%s vbat_charger = %d,ibat_current = %d,vbat_gauge = %d, resistor = %d\n",__func__,vbat_charger,ibat_current,vbat_gauge,resistor);
}
bool battery_capacity_less_than(int capacity)
{
	struct power_supply *gauge;
	union power_supply_propval gauge_val;
	int cur_capacity=0;

	gauge = power_supply_get_by_name("bq2753x-0");
	if(gauge){
		gauge->get_property(gauge, POWER_SUPPLY_PROP_CAPACITY,&gauge_val);
		cur_capacity = gauge_val.intval;
	}
	return cur_capacity < capacity;
}

static int bq2589x_set_optimized_ilimit(void)
{
	int ret=0;
	int curr=0;

	ret = bq2589x_read_idpm_limit(g_bq1);
	if(DEBUG_CHARGER){
		pr_info(TAG"%s idpm = %d\n",__func__,ret);
	}
	if(ret > 0){
		curr = ret;
		ret = bq2589x_set_input_current_limit(g_bq1,curr-120);
	}
	return  curr;
}
static void bq2589x_turn_up_voltage(void)
{
	bq2589x_enter_hiz_mode(g_bq2);
	bq2589x_auto_ico(g_bq1,false);
	bq2589x_auto_ico(g_bq2,false);
	bq2589x_read_vbus_volt(g_bq1);
	voltcontrol.TuneCounter=0;
	pr_info(TAG"%s vbus_volt = %d target_voltage = %d\n",__func__,g_bq1->vbus_volt,g_bq1->target_voltage);
	while(g_bq1->vbus_volt < g_bq1->target_voltage-1000&& !g_bq1->interrupt && !g_bq1->is_screen_on)
	{
		if(voltcontrol.TuneCounter++ > 6){
			if(DEBUG_CHARGER)
				pr_info("%s tried 10 times to turn up voltage\n",__func__);
			break;
		}
		bq2589x_get_vbus_type(g_bq1);
		if(g_bq1->vbus_type != BQ2589X_VBUS_ADAPTER){
			if(DEBUG_CHARGER)
				pr_info("%s plug out\n",__func__);
			break;
		}
		if(!bq2589x_is_charge_enable(g_bq1)){
			if(DEBUG_CHARGER)
				pr_info("%s charge disabled\n",__func__);
			break;
		}
		bq2589x_set_input_current_limit(g_bq1,2000);
		bq2589x_enter_hiz_mode(g_bq2);
		msleep(10);
		bq2589x_pumpx_enable( g_bq1,true);
		bq2589x_pumpx_increase_volt(g_bq1);
		if(DEBUG_CHARGER)
			pr_info(TAG"%s tune voltage not done, try again vbus_volt = %d\n",__func__,g_bq1->vbus_volt);
		wait_event_interruptible_timeout(g_bq1->wq,g_bq1->interrupt,2500*HZ/1000);
		if(g_bq1->interrupt){
			pr_info(TAG"%s has been interrupted\n",__func__);
			return;
		}
		//msleep(2500);
		bq2589x_read_vbus_volt(g_bq1);
	}
	bq2589x_exit_hiz_mode(g_bq2);
}
static void bq2589x_turn_down_voltage(void)
{
	bq2589x_enter_hiz_mode(g_bq2);
	bq2589x_auto_ico(g_bq1,false);
	bq2589x_auto_ico(g_bq2,false);
	bq2589x_read_vbus_volt(g_bq1);
	voltcontrol.TuneCounter=0;
	while(g_bq1->vbus_volt > g_bq1->target_voltage+1000 && !g_bq1->interrupt)
	{
		if(voltcontrol.TuneCounter++ > 6){
			if(DEBUG_CHARGER)
				pr_info("%s tried 10 times to turn up voltage\n",__func__);
			break;
		}
		bq2589x_get_vbus_type(g_bq1);
		if(g_bq1->vbus_type != BQ2589X_VBUS_ADAPTER){
			if(DEBUG_CHARGER)
				pr_info("%s plug out\n",__func__);
			break;
		}
		if(!bq2589x_is_charge_enable(g_bq1)){
			if(DEBUG_CHARGER)
				pr_info("%s charge disabled\n",__func__);
			break;
		}
		bq2589x_set_input_current_limit(g_bq1,2000);
		bq2589x_enter_hiz_mode(g_bq2);
		msleep(10);
		bq2589x_pumpx_enable( g_bq1,true);
		bq2589x_pumpx_decrease_volt(g_bq1);
		if(DEBUG_CHARGER)
			pr_info(TAG"%s tune voltage not done, try again vbus_volt = %d\n",__func__,g_bq1->vbus_volt);
		wait_event_interruptible_timeout(g_bq1->wq,g_bq1->interrupt,2500*HZ/1000);
		if(g_bq1->interrupt){
			pr_info(TAG"%s has been interrupted\n",__func__);
			return;
		}
		//msleep(2500);
		bq2589x_read_vbus_volt(g_bq1);
	}
	bq2589x_exit_hiz_mode(g_bq2);
}

bool  bq2589x_adjust_ico(void)
{
	static int ret=0;
	int ilimit=0;

	if(DEBUG_CHARGER){
		pr_info(TAG"%s\n",__func__);
	}
	bq2589x_set_input_current_limit(g_bq1,2000);
	bq2589x_auto_ico(g_bq1,true);
	bq2589x_force_ico(g_bq1);
	wait_event_interruptible_timeout(g_bq1->wq,g_bq1->interrupt,HZ);
	if(g_bq1->interrupt){
		pr_info(TAG"%s has been interrupted\n",__func__);
		return false;
	}
	//msleep(1000);
	ret = bq2589x_check_force_ico_done(g_bq1);
	if(ret == 1){//done
		if(DEBUG_CHARGER)
			pr_info(TAG"%s force ico done\n",__func__);
		ilimit = bq2589x_set_optimized_ilimit();
	}else{
		if(DEBUG_CHARGER)
			pr_info(TAG"%s force ico not done yet\n",__func__);
	}

	bq2589x_auto_ico(g_bq1,false);
	if(ilimit > 1800){
		bq2589x_set_input_current_limit(g_bq1,1700);
	}
	if(ret && ilimit > 1600)
		return true;
	return false;
}
static void bq2589x_plug_out_event(void)
{
	//disable charging
	bq2589x_stop_charging(g_bq1);
	bq2589x_stop_charging(g_bq2);
	//restore charge current
	bq2589x_set_chargecurrent(g_bq1,2200);
	bq2589x_set_chargecurrent(g_bq2,2200);
	bq2589x_set_input_current_limit(g_bq1,2000);
	//disable watchdog timer
	bq2589x_disable_watchdog_timer(g_bq1);
	bq2589x_disable_watchdog_timer(g_bq2);
	bq2589x_get_vbus_type(g_bq1);
	if(g_bq1->vbus_type == BQ2589X_VBUS_NONE){
		alarm_cancel(&g_bq1->alarm);
		g_bq1->target_voltage = 12000;
	}
}
extern int charger_get_manufacture(void);
//0 sony, 1 atl, -1 unknown

static void bq2589x_adjust_temp(void)
{
	bq2589x_start_charging(g_bq1);
	bq2589x_start_charging(g_bq2);
	switch(g_bq1->temp){
		case SONY_FREEZE_TEMP:
		case ATL_FREEZE_TEMP:
			if(DEBUG_CHARGER){
				pr_info(TAG"%s FREEZE_TEMP disable charging\n",__func__);
			}
			bq2589x_plug_out_event();
			break;
		case SONY_ALARM_TEMP:
		case ATL_ALARM_TEMP:
		case SONY_HOT_TEMP:
		case ATL_HOT_TEMP:
		case BOARD_HOT_TEMP:
			if(DEBUG_CHARGER){
				pr_info(TAG"%s ALARM_TEMP disable charging\n",__func__);
			}
			bq2589x_plug_out_event();
			g_bq1->target_voltage = 9000;
			break;
			//sony
		case SONY_COLD_TEMP:
			if(DEBUG_CHARGER){
				pr_info(TAG"%s SONY_COLD_TEMP \n",__func__);
			}//903mA
			bq2589x_set_chargecurrent(g_bq1,450);
			bq2589x_set_chargecurrent(g_bq2,453);
			break;
		case SONY_ROOM_TEMP:
			if(DEBUG_CHARGER){
				pr_info(TAG"%s SONY_ROOM_TEMP\n",__func__);
			}//3010mA
			bq2589x_set_chargecurrent(g_bq1,1500);
			bq2589x_set_chargecurrent(g_bq2,1510);
			break;
		case SONY_WARM_TEMP:
			if(DEBUG_CHARGER){
				pr_info(TAG"%s SONY_WARM_TEMP\n",__func__);
			}//4515mA
			bq2589x_set_chargecurrent(g_bq1,2250);
			bq2589x_set_chargecurrent(g_bq2,2265);
			break;
#if 0
		case SONY_HOT_TEMP:
			if(DEBUG_CHARGER){
				pr_info(TAG"%s SONY_HOT_TEMP\n",__func__);
			}//1505mA to 4.1V
			bq2589x_set_chargecurrent(g_bq1,750);
			bq2589x_set_chargecurrent(g_bq2,755);

			bq2589x_set_chargevoltage(g_bq1,4100);
			bq2589x_set_chargevoltage(g_bq2,4100);
			break;
#endif
			//atl
		case ATL_COLD_TEMP:
			if(DEBUG_CHARGER){
				pr_info(TAG"%s ATL_COLD_TEMP \n",__func__);
			}//0.3C = 900mA
			bq2589x_set_chargecurrent(g_bq1,450);
			bq2589x_set_chargecurrent(g_bq2,450);
			break;
		case ATL_ROOM_TEMP:
			if(DEBUG_CHARGER){
				pr_info(TAG"%s ATL_ROOM_TEMP\n",__func__);
			}//1.5c to 4.2V = 4500mA 0.5c to 4.35V

			bq2589x_set_chargecurrent(g_bq1,2250);
			bq2589x_set_chargecurrent(g_bq2,2250);
			break;
		case ATL_WARM_TEMP:
			if(DEBUG_CHARGER){
				pr_info(TAG"%s ATL_WARM_TEMP\n",__func__);
			}//1.5c to 4.35V
			bq2589x_set_chargecurrent(g_bq1,2250);
			bq2589x_set_chargecurrent(g_bq2,2250);
			break;
#if 0
		case ATL_HOT_TEMP:
			if(DEBUG_CHARGER){
				pr_info(TAG"%s SONY_HOT_TEMP\n",__func__);
			}//0.7C to 4.1V = 2100mA
			bq2589x_set_chargecurrent(g_bq1,1050);
			bq2589x_set_chargecurrent(g_bq2,1050);

			bq2589x_set_chargevoltage(g_bq1,4100);
			bq2589x_set_chargevoltage(g_bq2,4100);
			break;
#endif
		default:
			bq2589x_set_chargecurrent(g_bq1,2250);
			bq2589x_set_chargecurrent(g_bq2,2250);
			break;
	}
}
static void bq2589x_plug_in_event(void)
{
	TEMP_POLL_TIME = 10;

	if(DEBUG_CHARGER){
		pr_info(TAG"%s\n",__func__);
	}
	bq2589x_get_vbus_type(g_bq1);
	if(g_bq1->vbus_type == BQ2589X_VBUS_USB_HOST){//usb host plug
		if(g_bq1->fast_usb_mode){
			bq2589x_set_input_current_limit(g_bq1,1000);
		}else{
			bq2589x_set_input_current_limit(g_bq1,500);
		}
		bq2589x_start_charging(g_bq1);
		bq2589x_start_charging(g_bq2);
		bq2589x_set_chargecurrent(g_bq1,2200);
		bq2589x_set_chargecurrent(g_bq2,2200);
		bq2589x_adjust_temp();
	}else if(g_bq1->vbus_type == BQ2589X_VBUS_OTG){//otg plug
	}else if(g_bq1->vbus_type == BQ2589X_VBUS_ADAPTER){//adapter plug
		//bq2589x_set_watchdog_timer(g_bq1,40);
		bq2589x_adjust_temp();
		if(!g_bq1->is_screen_on){
			bq2589x_set_input_current_limit(g_bq1,2000);

			if(battery_capacity_less_than(95) && !g_bq1->non_standard){
				if(!battery_capacity_less_than(80)){
					g_bq1->target_voltage = 9000;
				}
				if(bq2589x_get_chargecurrent(g_bq1)*2 > 1600){
					bq2589x_turn_up_voltage();
					bq2589x_turn_down_voltage();
				}
			}
		}
		bq2589x_ir_compensation();

		bq2589x_read_vbus_volt(g_bq1);
		if(g_bq1->vbus_volt <6000){
			bq2589x_adjust_ico();
		}else if(g_bq1->vbus_volt >11000){
			bq2589x_set_input_current_limit(g_bq1,1900);
		}

		if(g_bq1->is_screen_on){
			schedule_work(&g_bq1->fb_unblank_work);
		}
	}
	if(bq2589x_get_input_current_limit(g_bq1)>800)
		alarm_start_relative(&g_bq1->alarm, ktime_set(TEMP_POLL_TIME,0));

}
static void bq2589x_thread_wake(void)
{
	bq2589x_get_vbus_type(g_bq1);
	power_supply_changed(&g_bq1->wall);
	schedule_delayed_work(&g_bq1->status_work,HZ);
	if(g_bq1->vbus_type == BQ2589X_VBUS_NONE){
		bq2589x_plug_out_event();
	}else{
		bq2589x_plug_in_event();
	}

}
static int bq2589x_charger_thread(void *data)
{
	do{
		wait_event_interruptible_timeout(g_bq1->wq,g_bq1->interrupt,50*HZ);
		if(!g_bq1->interrupt)
			continue;
		if(DEBUG_CHARGER){
			pr_info(TAG"charger thread has been woken up\n");
		}
		g_bq1->interrupt = false;

		wake_lock(&g_bq1->charger_wake);
		bq2589x_thread_wake();
		wake_unlock(&g_bq1->charger_wake);

	}while(!kthread_should_stop());

	return 0;
}

static void bq2589x_charger1_irq_workfunc(struct work_struct *work)
{
	struct bq2589x *bq = container_of(work, struct bq2589x, irq_work.work);
	u8 status = 0;
	u8 fault = 0;
	int ret;

	if(DEBUG_CHARGER)
		pr_info(TAG"%s charger1 interrupt triggered\n",__func__);
	/* Read STATUS and FAULT registers */
	ret = bq2589x_read_byte(bq, &status, BQ2589X_REG_0B);
	if (ret) {
		dev_err(bq->dev, "%s:i2c failure:%d\n",__func__, ret);
		return;
	}else{
		//if(DEBUG_CHARGER)
		//pr_info(TAG"%s status register = 0x%2x\n",__func__,status);
		bq2589x_termination_enable(g_bq1,false);
		bq2589x_set_term_current(g_bq1,64);
	}

	ret = bq2589x_read_byte(bq, &fault, BQ2589X_REG_0C);
	if (ret) {
		dev_err(bq->dev, "%s:i2c failure: %d\n",__func__, ret);
		return;
	}else{
		//	if(DEBUG_CHARGER)
		//pr_info(TAG"%s fault register = 0x%2x\n",__func__,fault);
	}
	memset(&voltcontrol, 0, sizeof(struct volt_control_t));

	g_bq1->interrupt = true;
	wake_up(&g_bq1->wq);
}


static irqreturn_t bq2589x_charger1_interrupt(int irq, void *data)
{
	struct bq2589x *bq = data;

	schedule_delayed_work(&bq->irq_work,HZ/5);
	return IRQ_HANDLED;
}

static int chg1_parse_dt(struct bq2589x *bq)
{
	struct device *dev = bq->dev;
	struct device_node *np = dev->of_node;
	int gpio;
	int ret;

	if(!np){
		pr_info(TAG"%s(),  parse device tree error", __func__);
		return -EINVAL;
	}
	gpio = of_get_named_gpio(np, "chg1-int", 0);
	if(gpio < 0){
		return -EINVAL;
	}
	ret = gpio_request(gpio, "chg1-int");
	if(ret){
		pr_err(TAG"chg1-int request error\n");
		return ret;
	}
	gpio_config.chg1_int = gpio_to_irq(gpio);

	gpio = of_get_named_gpio(np, "chg1-en", 0);
	if(gpio < 0){
		return -EINVAL;
	}
	ret = gpio_request(gpio, "chg1_en");
	if(ret){
		pr_err(TAG"chg1_en request error\n");
		return ret;
	}
	gpio_config.chg1_en = gpio;


	return 0;
}

static int muic_event_notify(struct notifier_block *this, unsigned long code,
		void *unused)
{
	struct bq2589x *bq;

	bq = container_of(this, struct bq2589x, charger_notifier);
	switch(code){
		case USB_HOST_ATTACH:
			if(DEBUG_CHARGER)
				pr_info(TAG"usb host attach notify\n");
			bq->usb_mode = true;
			break;
		case USB_HOST_DETACH:
			bq->usb_mode = false;
			if(DEBUG_CHARGER)
				pr_info(TAG"usb host detach notify\n");
			break;
		case USB_OTG_ATTACH:
			bq->otg_mode = true;
			bq2589x_enable_otg(bq);
			bq2589x_set_watchdog_timer(g_bq1,40);
			schedule_delayed_work(&g_bq1->otg_work,0);
			if(DEBUG_CHARGER)
				pr_info(TAG"otg attach notify\n");
			break;
		case USB_OTG_DETACH:
			bq->otg_mode = false;
			bq2589x_disable_otg(bq);
			bq2589x_disable_watchdog_timer(g_bq1);
			if(DEBUG_CHARGER)
				pr_info(TAG"otg detach notify\n");
			break;
		case NON_STANDARD_ATTACH:
			if(DEBUG_CHARGER)
				pr_info(TAG"non-standard attach notify\n");
			bq->non_standard= true;
			break;
		case NON_STANDARD_DETACH:
			bq->non_standard= false;
			if(DEBUG_CHARGER)
				pr_info(TAG"non-standard detach notify\n");
			break;
	}

	power_supply_changed(&bq->usb);
	schedule_delayed_work(&g_bq1->status_work,HZ);
	pr_info(TAG"%s usb power_supply_changed\n",__func__);
	return NOTIFY_DONE;
}
static int temp_event_notify(struct notifier_block *this, unsigned long code,
		void *unused)
{
	struct bq2589x *bq;

	bq = container_of(this, struct bq2589x, temp_notifier);

	bq->temp = code;
	if(bq->last_temp != bq->temp){
		memset(&voltcontrol, 0, sizeof(struct volt_control_t));
		g_bq1->interrupt = true;
		wake_up(&g_bq1->wq);
		bq->last_temp = bq->temp;
	}
	return NOTIFY_DONE;
}

static ssize_t attr_set_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	char rw[10];
	int reg,ret,value;

	struct bq2589x *bq = dev_get_drvdata(dev);

	sscanf(buf,"%s %x %x",rw,&reg, &value);
	if(!strcmp(rw,"read")){
		ret = bq2589x_read_byte(bq, (u8 *)&value,  reg);
		pr_info(TAG"read from [%x] value = 0x%2x\n", reg, value);
	}else if(!strcmp(rw,"write")){
		ret =bq2589x_write_byte(bq, reg, (u8)value);
		pr_info(TAG"write to [%x] value = 0x%2x\n", reg, value);
	}
	return size;
}
static ssize_t bq2589x_show_registers(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret ;

	idx = sprintf(buf,"%s:\n","Charger 1:");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(g_bq1, &val, addr);
		if(ret == 0){
			len = sprintf(tmpbuf,"Reg[0x%.2x] = 0x%.2x\n",addr,val);
			memcpy(&buf[idx],tmpbuf,len);
			idx += len;
		}
	}

	idx += sprintf(&buf[idx],"%s:\n","Charger 2:");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(g_bq2, &val, addr);
		if(ret == 0){
			len = sprintf(tmpbuf,"Reg[0x%.2x] = 0x%.2x\n",addr,val);
			memcpy(&buf[idx],tmpbuf,len);
			idx += len;
		}
	}

	return idx;
}
static ssize_t usb_fast_charge_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int fast_en=0;
	struct bq2589x *bq = dev_get_drvdata(dev);

	sscanf(buf,"%d",&fast_en);
	if(fast_en){
		bq->fast_usb_mode = true;
		pr_info("enable usb fast charge\n");
	}else{
		bq->fast_usb_mode = false;
		pr_info("disable usb fast charge\n");
	}
	g_bq1->interrupt = true;
	wake_up(&g_bq1->wq);
	return size;
}
static ssize_t usb_fast_charge_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct bq2589x *bq = dev_get_drvdata(dev);

	sprintf(buf,"%d\n",bq->fast_usb_mode);
	return 2;
}
static ssize_t usb_no_current_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int no_current_en=0;
	struct bq2589x *bq = dev_get_drvdata(dev);

	sscanf(buf,"%d",&no_current_en);
	if(no_current_en){
		bq->usb_no_current= true;
		bq2589x_enter_hiz_mode(g_bq1);
		bq2589x_enter_hiz_mode(g_bq2);
		pr_info("enable usb no current\n");
	}else{
		bq->usb_no_current= false;
		bq2589x_exit_hiz_mode(g_bq1);
		bq2589x_exit_hiz_mode(g_bq2);
		pr_info("disable usb no current\n");
	}
	g_bq1->interrupt = true;
	wake_up(&g_bq1->wq);
	return size;
}
static ssize_t usb_no_current_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct bq2589x *bq = dev_get_drvdata(dev);

	sprintf(buf,"%d\n",bq->usb_no_current);
	return 2;
}

static struct device_attribute attributes[] = {
	__ATTR(reg_control, 0200, NULL, attr_set_reg),
	__ATTR(reg_dump, 0444, bq2589x_show_registers, NULL),
	__ATTR(usb_fast_charge_enable, 0600, usb_fast_charge_show, usb_fast_charge_store),
	__ATTR(usb_no_current_enable, 0600, usb_no_current_show, usb_no_current_store),
};
static int create_sysfs_files(struct device *dev)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(attributes); i++)
		if(device_create_file(dev, attributes + i))
			goto err;
	return 0;
err:
	for(; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	pr_err(TAG"unable to create sysfs interface\n");
	return -1;
}
extern int charger_get_temperature(void);
static void battery_temp_poll(struct work_struct *work)
{
	int temp;
	int vbat_charger;
	struct power_supply *gauge;
	union power_supply_propval gauge_val;

	if(DEBUG_CHARGER){
		pr_info(TAG"%s\n",__func__);
	}
	alarm_start_relative(&g_bq1->alarm, ktime_set(TEMP_POLL_TIME,0));
	//bq2589x_reset_watchdog_timer(g_bq1);
	temp = charger_get_temperature();

	if(!battery_capacity_less_than(100)){
		TEMP_POLL_TIME = 100;
		bq2589x_termination_enable(g_bq1,true);
		bq2589x_set_term_current(g_bq1,128);
		bq2589x_set_term_current(g_bq2,128);
	}


	if(g_bq1->temp == ATL_ROOM_TEMP){
		//get vbat

		gauge = power_supply_get_by_name("bq2753x-0");
		if(gauge){
			gauge->get_property(gauge, POWER_SUPPLY_PROP_VOLTAGE_NOW,&gauge_val);
			vbat_charger = gauge_val.intval;
		}
		if(DEBUG_CHARGER){
			pr_info(TAG"%s battery voltage = %d\n",__func__,vbat_charger);
		}
		//0.5c to 4.35v
		if(vbat_charger > 4200000){
			bq2589x_set_chargecurrent(g_bq1,750);
			bq2589x_set_chargecurrent(g_bq2,750);
		}else{
			bq2589x_set_chargecurrent(g_bq1,2250);
			bq2589x_set_chargecurrent(g_bq2,2250);
		}
	}
}
static void fb_blank_work(struct work_struct *work)
{
	pr_info(TAG"%s\n",__func__);
	g_bq1->interrupt = true;
	wake_up(&g_bq1->wq);
}
static void fb_unblank_work(struct work_struct *work)
{
	pr_info(TAG"%s\n",__func__);

	// turn down to 5V
	if(g_bq1->vbus_type == BQ2589X_VBUS_ADAPTER){
		bq2589x_enter_hiz_mode(g_bq1);
		bq2589x_enter_hiz_mode(g_bq2);
		wait_event_interruptible_timeout(g_bq1->wq,g_bq1->interrupt,HZ);
		if(g_bq1->interrupt){
			bq2589x_exit_hiz_mode(g_bq1);
			bq2589x_exit_hiz_mode(g_bq2);
			pr_info(TAG"%s has been interrupted\n",__func__);
			return;
		}
		bq2589x_exit_hiz_mode(g_bq1);
		bq2589x_exit_hiz_mode(g_bq2);
		bq2589x_adjust_ico();
	}
}

static enum alarmtimer_restart alarm_func(struct alarm *alarm, ktime_t time)
{
	struct bq2589x *bq = container_of(alarm, struct bq2589x, alarm);
	if(DEBUG_CHARGER){
		//pr_info(TAG"%s\n",__func__);
	}
	wake_lock_timeout(&bq->charger_alarm_wake,HZ);
	schedule_work(&g_bq1->temp_work);
	return ALARMTIMER_NORESTART;
}
static int fb_event_notify(struct notifier_block *this, unsigned long code,
		void *data)
{
	struct bq2589x *bq;
	struct fb_event *evdata = data;
	unsigned int blank;

	if(code != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)(evdata->data);
	bq = container_of(this, struct bq2589x, fb_notifier);

	switch(blank) {
		case FB_BLANK_POWERDOWN:
			g_bq1->is_screen_on = false;
			schedule_work(&g_bq1->fb_blank_work);
			break;
		case FB_BLANK_UNBLANK:
			g_bq1->is_screen_on = true;
			schedule_work(&g_bq1->fb_unblank_work);
			break;
	}
	return NOTIFY_OK;
}
static void otg_soc_poll(struct work_struct *work)
{
	struct bq2589x*bq =
		container_of(work, struct bq2589x, otg_work.work);
	if(battery_capacity_less_than(10)){
		bq2589x_disable_otg(bq);
	}
	if(g_bq1->otg_mode){
		schedule_delayed_work(&g_bq1->otg_work,3*HZ);
		bq2589x_reset_watchdog_timer(g_bq1);
	}else{
		bq2589x_disable_watchdog_timer(g_bq1);
	}
}
static void status_report_work(struct work_struct *work)
{
	struct bq2589x*bq =
		container_of(work, struct bq2589x, status_work.work);
	pr_info(TAG"%s\n",__func__);
	bq2589x_get_vbus_type(g_bq1);
	power_supply_changed(&bq->usb);
	power_supply_changed(&bq->wall);
}


static  dev_t  charger1_device_dev_t ;
static  dev_t charger2_device_dev_t;

static int bq2589x_charger1_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct bq2589x *bq;

	int ret;

	bq = kzalloc(sizeof(struct bq2589x),GFP_KERNEL);
	if(!bq){
		dev_err(&client->dev,"%s: out of memory\n",__func__);
		return -ENOMEM;
	}
	wake_lock_init(&bq->charger_i2c_wake,WAKE_LOCK_SUSPEND,"charger1_i2c_wake");
	bq->dev = &client->dev;
	bq->client = client;
	i2c_set_clientdata(client,bq);
	chg1_parse_dt(bq);

	ret = bq2589x_detect_device(bq);
	if(ret == 0){
		if(bq->part_no == BQ25892){
			bq->status |= BQ2589X_STATUS_EXIST;
			dev_info(bq->dev,"%s: charger device bq25892 detected, revision:%d\n",__func__,bq->revision);
		}
		else{
			dev_err(bq->dev, "%s: unexpected charger device detected\n",__func__);
			kfree(bq);
			return -ENODEV;
		}
	}
	else{
		kfree(bq);
		return -ENODEV;
	}

	g_bq1 = bq;

	ret = bq2589x_init_device(g_bq1);
	if (ret) {
		dev_err(bq->dev, "device init failure: %d\n", ret);
		goto err_0;
	}    // platform setup, irq,...
	bq2589x_set_term_current(g_bq2,64);

	g_bq1->target_voltage = 12000;
	INIT_DELAYED_WORK(&bq->otg_work,otg_soc_poll);
	INIT_DELAYED_WORK(&bq->status_work,status_report_work);

	ret = bq2589x_psy_register(bq);
	if(ret) goto err_0;

	bq->charger_class = class_create(THIS_MODULE,"charger1_class");
	if(IS_ERR(bq->charger_class)){
		ret = PTR_ERR(bq->charger_class);
		pr_err(TAG"%s charger class create fail\n", __func__);
		goto err_class_create;
	}

	alloc_chrdev_region(&charger1_device_dev_t,0,1,"charger1_device");
	bq->charger_device = device_create(bq->charger_class,
			NULL,charger1_device_dev_t,bq,"charger_device");
	if(IS_ERR(bq->charger_device)){
		ret = PTR_ERR(bq->charger_device);
		pr_err(TAG"%s charger device create fail\n", __func__);
		goto err_device_create;
	}

	ret = create_sysfs_files(bq->charger_device);
	if(ret < 0){
		pr_err(TAG"%s sysfiles create fail\n", __func__);
		goto err_create_sys;
	}
	check_cable_status();
	INIT_WORK(&g_bq1->temp_work, battery_temp_poll);
	INIT_WORK(&g_bq1->fb_blank_work, fb_blank_work);
	INIT_WORK(&g_bq1->fb_unblank_work, fb_unblank_work);
	alarm_init(&g_bq1->alarm, ALARM_BOOTTIME, alarm_func);
	wake_lock_init(&g_bq1->charger_wake,WAKE_LOCK_SUSPEND,"charger_wake");
	wake_lock_init(&g_bq1->charger_alarm_wake,WAKE_LOCK_SUSPEND,"charger_alarm_wake");
	bq->fb_notifier.notifier_call = fb_event_notify;
	ret = fb_register_client(&bq->fb_notifier);
#if 1
	init_waitqueue_head(&bq->wq);

	charger_thread = kthread_run(bq2589x_charger_thread,NULL,"BQ2589X Charger Thread");
	if(IS_ERR(charger_thread)){
		dev_err(bq->dev,"failed to create charger thread!\n");
		goto err_irq;
	}
#endif

	INIT_DELAYED_WORK(&bq->irq_work, bq2589x_charger1_irq_workfunc);
	ret = request_threaded_irq(gpio_config.chg1_int,NULL,bq2589x_charger1_interrupt,IRQF_TRIGGER_FALLING| IRQF_ONESHOT,"charger1",bq);
	enable_irq_wake(gpio_config.chg1_int);

	bq->interrupt = true;
	wake_up(&g_bq1->wq);

	bq->charger_notifier.notifier_call = muic_event_notify;
	register_muic_notifier(&bq->charger_notifier);

	bq->temp_notifier.notifier_call = temp_event_notify;
	register_temp_notifier(&bq->temp_notifier);
	return 0;
err_irq:
	cancel_delayed_work_sync(&bq->irq_work);
err_create_sys:
	device_destroy(bq->charger_class,charger1_device_dev_t);
err_device_create:
	class_destroy(bq->charger_class);
err_class_create:

err_0:
	kfree(bq);
	g_bq1 = NULL;
	return ret;
}
static int chg2_parse_dt(struct bq2589x *bq)
{
	struct device *dev = bq->dev;
	struct device_node *np = dev->of_node;
	int gpio;
	int ret;

	if(!np){
		pr_info(TAG"%s(),  parse device tree error", __func__);
		return -EINVAL;
	}
	gpio = of_get_named_gpio(np, "chg2-int", 0);
	if(gpio < 0){
		return -EINVAL;
	}
	ret = gpio_request(gpio, "chg2_int");
	if(ret){
		pr_err(TAG"chg2_int request error\n");
		return ret;
	}
	gpio_config.chg2_int = gpio_to_irq(gpio);

	gpio = of_get_named_gpio(np, "chg2-en", 0);
	if(gpio < 0){
		return -EINVAL;
	}
	ret = gpio_request(gpio, "chg2_en");
	if(ret){
		pr_err(TAG"chg2_en request error\n");
		return ret;
	}
	gpio_config.chg2_en = gpio;


	return 0;
}



static int bq2589x_charger2_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct bq2589x *bq;

	int ret;

	bq = kzalloc(sizeof(struct bq2589x),GFP_KERNEL);
	if(!bq){
		dev_err(&client->dev,"%s: out of memory\n",__func__);
		return -ENOMEM;
	}
	wake_lock_init(&bq->charger_i2c_wake,WAKE_LOCK_SUSPEND,"charger2_i2c_wake");

	bq->dev = &client->dev;
	bq->client = client;
	i2c_set_clientdata(client,bq);
	chg2_parse_dt(bq);

	ret = bq2589x_detect_device(bq);

	if(ret == 0){
		if(bq->part_no == BQ25892){
			bq->status |= BQ2589X_STATUS_EXIST;
			dev_info(bq->dev,"%s: charger device bq25892 detected, revision:%d\n",__func__,bq->revision);
		}
		else{
			dev_err(bq->dev, "%s: unexpected charger device detected\n",__func__);
			kfree(bq);
			return -ENODEV;
		}
	}
	else{
		kfree(bq);
		return -ENODEV;
	}

	g_bq2 = bq;

	// initialize bq2589x, disable charger 2 by default
	ret = bq2589x_init_device(g_bq2);
	if(ret){
		dev_err(bq->dev,"%s:Failed to initialize bq2589x charger",__func__);
	}{
		dev_err(bq->dev,"%s: Initialize bq2589x charger successfully!",__func__);
	}
	bq->charger_class = class_create(THIS_MODULE,"charger2_class");
	if(IS_ERR(bq->charger_class)){
		ret = PTR_ERR(bq->charger_class);
		pr_err(TAG"%s charger class create fail\n", __func__);
		goto err_class_create;
	}
	alloc_chrdev_region(&charger2_device_dev_t,0,1,"charger2_device");
	bq->charger_device = device_create(bq->charger_class,
			NULL,charger2_device_dev_t,bq,"charger_device");
	if(IS_ERR(bq->charger_device)){
		ret = PTR_ERR(bq->charger_device);
		pr_err(TAG"%s charger device create fail\n", __func__);
		goto err_device_create;
	}

	ret = create_sysfs_files(bq->charger_device);
	if(ret < 0){
		pr_err(TAG"%s sysfiles create fail\n", __func__);
		goto err_create_sys;
	}
	return ret;
err_create_sys:
	device_destroy(bq->charger_class,charger2_device_dev_t);
err_device_create:
	class_destroy(bq->charger_class);
err_class_create:
	return ret;

}

static void bq2589x_shutdown(struct i2c_client *client)
{
	g_bq1->is_already_shutdown = true;
	cancel_work_sync(&g_bq1->temp_work);
	bq2589x_disable_otg(g_bq1);
	bq2589x_reset_chip(g_bq2);
	bq2589x_reset_chip(g_bq1);
}
static struct of_device_id bq2589x_charger1_match_table[] = {
	{.compatible = "ti,bq2589x-1",},
	{},
};


static const struct i2c_device_id bq2589x_charger1_id[] = {
	{ "bq25896-1", BQ25896 },
	{},
};

MODULE_DEVICE_TABLE(i2c, bq2589x_charger1_id);

static struct i2c_driver bq2589x_charger1_driver = {
	.driver		= {
		.name	= "bq2589x-1",
		.of_match_table = bq2589x_charger1_match_table,
	},
	.id_table	= bq2589x_charger1_id,

	.probe		= bq2589x_charger1_probe,
	.shutdown = bq2589x_shutdown,
};


static struct of_device_id bq2589x_charger2_match_table[] = {
	{.compatible = "ti,bq2589x-2",},
	{},
};

static const struct i2c_device_id bq2589x_charger2_id[] = {
	{ "bq25896-2", BQ25896 },
	{},
};

MODULE_DEVICE_TABLE(i2c, bq2589x_charger2_id);


static struct i2c_driver bq2589x_charger2_driver = {
	.driver		= {
		.name	= "bq2589x-2",
		.of_match_table = bq2589x_charger2_match_table,
	},

	.id_table	= bq2589x_charger2_id,

	.probe		= bq2589x_charger2_probe,
};

static int __init bq2589x_charger_init(void)
{


	if(i2c_add_driver(&bq2589x_charger2_driver)){
		printk("%s, failed to register bq2589x_charger2_driver.\n",__func__);
	}
	else{
		printk("%s, bq2589x_charger2_driver register successfully!\n",__func__);
	}


	if(i2c_add_driver(&bq2589x_charger1_driver)){
		printk("%s, failed to register bq2589x_charger1_driver.\n",__func__);
	}
	else{
		printk("%s, bq2589x_charger1_driver register successfully!\n",__func__);
	}

	return 0;
}

static void __exit bq2589x_charger_exit(void)
{
	i2c_del_driver(&bq2589x_charger1_driver);
	i2c_del_driver(&bq2589x_charger2_driver);
}

module_init(bq2589x_charger_init);
module_exit(bq2589x_charger_exit);

MODULE_DESCRIPTION("TI BQ2589x Dual Charger Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments");
