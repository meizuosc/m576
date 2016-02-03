/*
 * cw2015_battery.c
 * fuel-gauge systems for lithium-ion (Li+) batteries
 *
 * Copyright (C) 2013 SSCR
 * Sheng Liang <liang.sheng@samsung.com>
 *
 * Copyright (C) 2009 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#define CW2015_DBG
static int cw2015_dbg = 0;
#ifdef CW2015_DBG
//#define cw2015_print(fmt, args...) printk(fmt, ##args)
#define cw2015_print(fmt, ...) do { if (cw2015_dbg) printk("cw2015: "fmt, ##__VA_ARGS__); } while(0)
#else
//#define cw2015_print(fmt, args...)
#define cw2015_print(fmt, ...)
#endif

#define CW2015_VER				0x00
#define CW2015_VCELL_MSB		0x02
#define CW2015_VCELL_LSB		0x03
#define CW2015_SOC_MSB			0x04
#define CW2015_SOC_LSB			0x05
#define CW2015_RRT_ALRT_MSB	0x06
#define CW2015_RRT_ALRT_LSB	0x07
#define CW2015_CONFIG			0x08
#define CW2015_TCOMP				0x09
#define CW2015_MODE				0x0A
#define CW2015_BATINFO			0x10

#define MODE_SLEEP_MASK		(0x3<<6)
#define MODE_SLEEP				(0x3<<6)
#define MODE_NORMAL			(0x0<<6)
#define MODE_QUICK_START		(0x3<<4)
#define MODE_RESTART			(0xf<<0)

#define FORCE_WAKEUP_CHIP 1

#define CONFIG_UPDATE_FLG (0x1<<1)

/* Low power, and wake up system to power off */
#define LOW_BAT_LVL 1
#define ATHD (LOW_BAT_LVL<<3)   //ATHD =8%
#define SIZE_BATINFO 64 
static char cw2015_bat_config_info[SIZE_BATINFO] = {
};

struct cw2015_battery_info {
	int tech;	// always = POWER_SUPPLY_TECHNOLOGY_LIPO
	int vcell;	// battery voltage * 1000, unit mV
	int soc;	// battery capacity
	int fake100;
	int temp10;	// temperature read from akm8973, then *10
	int charging_source;	// battery? usb? ac?
	int health;	// always = POWER_SUPPLY_HEALTH_GOOD
	int status;	// charging?
	int low_alrt_gpio;
	int low_alrt_irq;
};

struct cw2015_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		battery;
	struct power_supply		*usb;
	struct power_supply		*ac;
	struct cw2015_battery_info	bat_info;
};
static struct cw2015_chip *this_chip = NULL;

typedef enum {
	CHARGER_BATTERY = 0,
	CHARGER_USB,
	CHARGER_AC,
	CHARGER_DISCHARGE
} charger_type_t;

static enum power_supply_property cw2015_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TEMP,
};

#if 1
static char *supply_from_list[] = {
	"usb",
	"ac",
};
#endif

static int cw2015_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct cw2015_chip *chip = container_of(psy,struct cw2015_chip, battery);
	union power_supply_propval getval = {0, };
	int usb_online, ac_online;

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			//val->intval = chip->bat_info.status;
			if (!chip->usb)
				chip->usb = power_supply_get_by_name("usb");
			if (!chip->ac)
				chip->ac = power_supply_get_by_name("ac");
			if (chip->usb)
			{
				chip->usb->get_property(chip->usb, POWER_SUPPLY_PROP_ONLINE, &getval);
				usb_online = getval.intval;
			}
			else
			{
				usb_online = 0;
			}
			if (chip->ac)
			{
				chip->ac->get_property(chip->ac, POWER_SUPPLY_PROP_ONLINE, &getval);
				ac_online = getval.intval;
			}
			else
			{
				ac_online = 0;
			}
			if (usb_online || ac_online)
			{
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
				if (chip->bat_info.soc == 100)
					val->intval = POWER_SUPPLY_STATUS_FULL;
			}
			else
			{
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			cw2015_print("POWER_SUPPLY_PROP_STATUS:, value:%d\n", val->intval);
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = chip->bat_info.vcell*1000;
			//val->intval = 4000;
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			val->intval = chip->bat_info.soc;
			//val->intval = 80;
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = chip->bat_info.health;// POWER_SUPPLY_HEALTH_GOOD;
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = chip->bat_info.tech;// POWER_SUPPLY_TECHNOLOGY_LIPO;
			break;
		case POWER_SUPPLY_PROP_PRESENT:
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = 1;
			break;
		case POWER_SUPPLY_PROP_TEMP:
			val->intval = chip->bat_info.temp10;// get10Temp();
			break;
		default:
			return -EINVAL;
	}
	//cw2015_print("psp:%d, value:%d\n", psp, val->intval);
	return 0;
}

#if 0
static enum power_supply_property s5p_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_to_list[] = {
	"battery",
};

static int s5p_power_get_property(struct power_supply *bat_ps, 
		enum power_supply_property psp, 
		union power_supply_propval *val)
{
	charger_type_t charger = this_chip->bat_info.charging_source;

	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			if (bat_ps->type == POWER_SUPPLY_TYPE_MAINS)
				val->intval = (charger == CHARGER_AC ? 1 : 0);
			else if (bat_ps->type == POWER_SUPPLY_TYPE_USB)
				val->intval = (charger == CHARGER_USB ? 1 : 0);
			else
				val->intval = 0;
			break;
		default:
			return -EINVAL;
	}

	return 0;
}
#endif

static int cw2015_verify_update_battery_info(void)
{
	int ret = 0;
	int i;
	char value = 0;
	char reg_mode_value = 0;
	//short value16 = 0;
	char buffer[SIZE_BATINFO*2];
	struct i2c_client *client = this_chip->client;

	/* make sure not in sleep mode */
	ret = i2c_smbus_read_byte_data(client, CW2015_MODE);
	if(ret < 0) {
		dev_err(&client->dev, "Error read mode\n");
		return ret;
	}

	value = ret;
	reg_mode_value = value; /* save MODE value for later use */
	if((value & MODE_SLEEP_MASK) == MODE_SLEEP) {
		dev_err(&client->dev, "Error, device in sleep mode, cannot update battery info\n");
		return -1;
	}

	/* update new battery info */
	for(i=0; i<SIZE_BATINFO; i++) {
		ret = i2c_smbus_write_byte_data(client, CW2015_BATINFO+i, cw2015_bat_config_info[i]);
		if(ret < 0) {
			dev_err(&client->dev, "Error update battery info @ offset %d, ret = 0x%x\n", i, ret);
			return ret;
		}
	}

	/* readback & check */
	for(i=0; i<SIZE_BATINFO; i++) {
		ret = i2c_smbus_read_byte_data(client, CW2015_BATINFO+i);
		if(ret < 0) {
			dev_err(&client->dev, "Error read origin battery info @ offset %d, ret = 0x%x\n", i, ret);
			return ret;
		}

		buffer[i] = ret;
		//cw2015_print("%s %d: %x\n", __FUNCTION__, i, buffer[i]);
	}

	if(0 != memcmp(buffer, cw2015_bat_config_info, SIZE_BATINFO)) {
		dev_info(&client->dev, "battery info NOT matched, after readback.\n");
		return -1;
	} else {
		dev_info(&client->dev, "battery info matched, after readback.\n");
	}

	/* set 2015 to use new battery info */
	ret = i2c_smbus_read_byte_data(client, CW2015_CONFIG);
	if(ret < 0) {
		dev_err(&client->dev, "Error to read CONFIG\n");
		return ret;
	}
	value = ret;

	value |= CONFIG_UPDATE_FLG;/* set UPDATE_FLAG */
	value &= ~(0x2);  /* clear ATHD */
	value |= ATHD; /* set ATHD */

	ret = i2c_smbus_write_byte_data(client, CW2015_CONFIG, value);
	if(ret < 0) {
		dev_err(&client->dev, "Error to update flag for new battery info\n");
		return ret;
	}

	/* check 2015 for ATHD&update_flag */
	ret = i2c_smbus_read_byte_data(client, CW2015_CONFIG);
	if(ret < 0) {
		dev_err(&client->dev, "Error to read CONFIG\n");
		return ret;
	}
	value = ret;

	if (!(value & CONFIG_UPDATE_FLG)) {
		dev_info(&client->dev, "update flag for new battery info have not set\n");
	}
	if ((value & 0xf8) != ATHD) {
		dev_info(&client->dev, "the new ATHD have not set %d\n", value);
	}	  

	reg_mode_value &= ~(MODE_RESTART);  /* RSTART */
	ret = i2c_smbus_write_byte_data(client, CW2015_MODE, reg_mode_value|MODE_RESTART);
	if(ret < 0) {
		dev_err(&client->dev, "Error to restart battery info1\n");
		return ret;
	}
	ret = i2c_smbus_write_byte_data(client, CW2015_MODE, reg_mode_value|0);
	if(ret < 0) {
		dev_err(&client->dev, "Error to restart battery info2\n");
		return ret;
	}
	return 0;
}

static int cw2015_init_charger(void)
{
	int i=0;
	int ret = 0;
	char value = 0;
	char buffer[SIZE_BATINFO*2];
	struct i2c_client *client = this_chip->client;
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;

	of_property_read_u8_array(np, "bat_conf_info", cw2015_bat_config_info, SIZE_BATINFO);
#if 0
	for (i=0; i<SIZE_BATINFO; i++)
	{
		cw2015_print("bat_info %d : 0x%x\n", i, cw2015_bat_config_info[i]);
	}
#endif
#if FORCE_WAKEUP_CHIP
	value = MODE_SLEEP;
#else
	/* check if sleep mode, bring up */
	ret = i2c_smbus_read_byte_data(client, CW2015_MODE);
	if(ret < 0) {
		dev_err(&client->dev, "Error read mode\n");
		return ret;
	}

	value = ret;
#endif

	if((value & MODE_SLEEP_MASK) == MODE_SLEEP) {
		/* do wakeup cw2015 */
		ret = i2c_smbus_write_byte_data(client, CW2015_MODE, MODE_NORMAL);
		if(ret < 0) {
			dev_err(&client->dev, "Error update mode, ret:%d\n", ret);
			return ret;
		}

		/* check 2015 if not set ATHD */
		ret = i2c_smbus_read_byte_data(client, CW2015_CONFIG);
		if(ret < 0) {
			dev_err(&client->dev, "Error to read CONFIG\n");
			return ret;
		}
		value = ret;

		if ((value & 0xf8) != ATHD) {
			dev_info(&client->dev, "the new ATHD have not set %d\n", value);
			value &= ~(0x2);  /* clear ATHD */
			value |= ATHD; 
			/* set ATHD */
			ret = i2c_smbus_write_byte_data(client, CW2015_CONFIG, value);
			if(ret < 0) {
				dev_err(&client->dev, "Error to set new ATHD\n");
				return ret;
			}
		}

		/* check 2015 for update_flag */
		ret = i2c_smbus_read_byte_data(client, CW2015_CONFIG);
		if(ret < 0) {
			dev_err(&client->dev, "Error to read CONFIG\n");
			return ret;
		}
		value = ret;  	    	 
		/* not set UPDATE_FLAG,do update_battery_info  */
		if (!(value & CONFIG_UPDATE_FLG)) {
			dev_info(&client->dev, "update flag for new battery info have not set\n");
			cw2015_verify_update_battery_info();
		}
		else
		{

			/* read origin info */
			for(i=0; i<SIZE_BATINFO; i++) {
				ret = i2c_smbus_read_byte_data(client, CW2015_BATINFO+i);
				if(ret < 0) {
					dev_err(&client->dev, "Error read origin battery info @offset %d, ret = 0x%x\n", i, ret);
					return ret;
				}
				buffer[i] = ret;
			}

			if(0 != memcmp(buffer, cw2015_bat_config_info, SIZE_BATINFO)) {
				dev_info(&client->dev, "battery info NOT matched.\n");
				/* battery info not matched,do update_battery_info  */
				cw2015_verify_update_battery_info();
			} else {
				dev_info(&client->dev, "battery info matched.\n");
			}
		}

#if 1
		ret = i2c_smbus_write_byte_data(client, CW2015_MODE, MODE_QUICK_START);
		if(ret < 0) {
			dev_err(&client->dev, "Error to quick start\n");
			return ret;
		}
#endif
	}

	return 0;
}

static int cw2015_get_vcell(void)
{
	struct i2c_client *client = this_chip->client;
	unsigned short tmp[3];
	unsigned short min, max, sum = 0;
	int voltage;
	int ret;
	int i;

	for (i = 0; i < 3; i++)
	{
		ret = i2c_smbus_read_word_data(client, CW2015_VCELL_MSB);
		if (ret < 0)
		{
			dev_err(&client->dev, "Error read soc %d\n", i);
			return 0;
		}
		tmp[i] = be16_to_cpu(ret);
		sum += tmp[i];
		if (i == 0 || tmp[i] > max)
			max = tmp[i];
		if (i == 0 || tmp[i] < min)
			min = tmp[i];
	}
	ret = sum-min-max;
	/* 1 voltage LSB is 305uV, ~312/1024mV */
	// voltage = value16 * 305 / 1000;
	voltage = ret*305/1000;
	this_chip->bat_info.vcell = voltage;

	dev_info(&client->dev, "\033[0;34m %s() value: 0x%x, voltage %dmV \033[m\n",
			__FUNCTION__, ret, voltage);

	return voltage;
}

int cw2015_get_soc(void)
{
	struct i2c_client *client = this_chip->client;
	unsigned short value16 = 0;
	int soc;
	int ret;

	ret = i2c_smbus_read_word_data(client, CW2015_SOC_MSB);
	if (ret < 0)
	{
		dev_err(&client->dev, "Error read SOC\n");
		soc = 85;
		return soc;
	}
	value16 = be16_to_cpu(ret);
	soc = value16 >> 8;
	// TODO: soc may be wrong, need to deal with it.
	this_chip->bat_info.soc = soc;
	dev_info(&client->dev, "\033[0;34m %s() value: 0x%x, soc: %d.%02d%% \033[m\n",
			__FUNCTION__, value16, soc, (value16&0xff)*100/256);

	return soc;
}

int cw2015_get_rrt(void)
{
	struct i2c_client *client = this_chip->client;
	unsigned short value16 = 0;
	int ret;
	int alrt, rrt;
	ret = i2c_smbus_read_word_data(client, CW2015_RRT_ALRT_MSB);
	if (ret < 0)
	{
		dev_err(&client->dev, "error read rrt\n");
		return 0;
	}
	value16 = be16_to_cpu(ret);
	rrt = value16&0x1fff;
	alrt = value16>>15;
	cw2015_print("alrt: %d, rrt: %dmin\n", alrt, rrt);
	return 0;
}

static int cw2015_get_version(void)
{
	struct i2c_client *client = this_chip->client;
	u8 ver;
	int ret;

	ret = i2c_smbus_read_byte_data(client, CW2015_VER);
	if(ret < 0) {
		dev_err(&client->dev, "Error read mode\n");
		return ret;
	}
	ver = (u8)ret;

	dev_info(&client->dev, "CW2015 Fuel-Gauge Ver 0x%x\n", ver);
	return 0;
}

static void cw2015_work(struct work_struct *work)
{
	struct cw2015_chip *chip = this_chip;
	static int last_vcell = 0;
	static int last_soc = 0;
	int vcell, soc;

	//cw2015_print("%s()\n", __FUNCTION__);
	if(NULL == chip)
	{
		dev_err(&chip->client->dev, "Error in %s(), chip is NULL!!!\n",__FUNCTION__);
	}
	else
	{
		vcell = cw2015_get_vcell();
		soc = cw2015_get_soc();
		cw2015_get_rrt();
		if (last_soc != soc)
		{
			last_vcell = vcell;
			last_soc = soc;
			power_supply_changed(&this_chip->battery);
		}
	}
	schedule_delayed_work(&chip->work, msecs_to_jiffies(10000));
}

static ssize_t cw2015_show_ver(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = this_chip->client;
	u8 ver;
	int ret;

	ret = i2c_smbus_read_byte_data(client, CW2015_VER);
	if(ret < 0) {
		dev_err(&client->dev, "cw2015_show_ver err\n");
		return ret;
	}
	ver = (u8)ret;

	return sprintf(buf, "%d\n", ver);
}

static DEVICE_ATTR(version, S_IRUGO, cw2015_show_ver, NULL);

static ssize_t cw2015_show_dbg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cw2015_dbg);
}

static ssize_t cw2015_store_dbg(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	if (buf[0]!='0')
		cw2015_dbg = 1;
	else
		cw2015_dbg = 0;
	return count;
}

static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR, cw2015_show_dbg, cw2015_store_dbg);

static struct attribute *cw2015_attributes[] = {
	&dev_attr_debug.attr,
	&dev_attr_version.attr,
	NULL
};

static const struct attribute_group cw2015_attr_group = {
	.attrs = cw2015_attributes,
};

static irqreturn_t low_alrt_func(int irqno, void *param)
{
	int low_alrt = gpio_get_value(this_chip->bat_info.low_alrt_gpio);
	cw2015_print("%s, %d\n", __FUNCTION__, low_alrt);
	return IRQ_HANDLED;
}

static int cw2015_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cw2015_chip *chip;
	struct device_node *np = client->dev.of_node;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct pinctrl *pinctrl;
	int ret;

	dev_info(&client->dev, "%s: addr=0x%x @ IIC%d, irq=%d\n",
			client->name,client->addr,client->adapter->nr,client->irq);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))//I2C_FUNC_SMBUS_BYTE
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	this_chip = chip;
	chip->client = client;

	i2c_set_clientdata(client, chip);

	of_property_read_u32(np, "debug", &cw2015_dbg);

	chip->bat_info.charging_source = CHARGER_BATTERY;
	chip->bat_info.health = POWER_SUPPLY_HEALTH_GOOD;
	chip->bat_info.tech = POWER_SUPPLY_TECHNOLOGY_LIPO;
	chip->bat_info.fake100 = 0;
	chip->bat_info.temp10 = 20*10;

	ret = cw2015_init_charger();
	if (ret < 0)
	{
		goto err_init_charger;
	}
	
	cw2015_get_version();

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
#if 1
	chip->battery.supplied_from	= supply_from_list;
	chip->battery.num_supplies	= ARRAY_SIZE(supply_from_list);
#endif
	chip->battery.get_property	= cw2015_get_property;
	chip->battery.properties	= cw2015_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(cw2015_battery_props);

#if 0
	chip->usb.name		= "usb";
	chip->usb.type		= POWER_SUPPLY_TYPE_USB;
	//chip->usb.supplied_to	= supply_to_list;
	//chip->usb.num_supplicants = ARRAY_SIZE(supply_to_list);
	chip->usb.get_property	= s5p_power_get_property;
	chip->usb.properties	= s5p_power_props;
	chip->usb.num_properties	= ARRAY_SIZE(s5p_power_props);

	chip->ac.name		= "ac";
	chip->ac.type		= POWER_SUPPLY_TYPE_MAINS;
	//chip->ac.supplied_to	= supply_to_list;
	//chip->ac.num_supplicants = ARRAY_SIZE(supply_to_list);
	chip->ac.get_property	= s5p_power_get_property;
	chip->ac.properties	= s5p_power_props;
	chip->ac.num_properties	= ARRAY_SIZE(s5p_power_props);
#endif

	power_supply_register(&client->dev, &chip->battery);
#if 0
	power_supply_register(&client->dev, &chip->usb);
	power_supply_register(&client->dev, &chip->ac);
#else
	chip->usb = power_supply_get_by_name("usb");
	chip->ac = power_supply_get_by_name("ac");
#endif

	ret = sysfs_create_group(&client->dev.kobj, &cw2015_attr_group);
	if (ret)
	{
		dev_err(&client->dev, "create sysfs error\n");
		goto err_create_sysfs;
	}

#if 1
	pinctrl = devm_pinctrl_get_select_default(&client->dev);
	if (IS_ERR(pinctrl))
	{
		dev_err(&client->dev, "pinctrl error\n");
		goto err_pinctrl;
	}
#endif

	chip->bat_info.low_alrt_gpio = of_get_named_gpio(np, "cw2015,bat_alrt_gpio", 0);
	chip->bat_info.low_alrt_irq = gpio_to_irq(chip->bat_info.low_alrt_gpio);
	ret = request_irq(chip->bat_info.low_alrt_irq, low_alrt_func, 
			IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING, "low_alrt_int", NULL);
	if (ret)
	{
		dev_err(&client->dev, "request_irq error\n");
		goto err_request_irq;
	}

	INIT_DELAYED_WORK(&chip->work, cw2015_work);
	schedule_delayed_work(&chip->work, 0);

	return 0;

err_request_irq:
err_pinctrl:
	sysfs_remove_group(&client->dev.kobj, &cw2015_attr_group);
err_create_sysfs:
	power_supply_unregister(&chip->battery);
err_init_charger:
	kfree(chip);
	return -EINVAL;
}

static int cw2015_remove(struct i2c_client *client)
{
	free_irq(this_chip->bat_info.low_alrt_irq, NULL);
	cancel_delayed_work(&this_chip->work);
	sysfs_remove_group(&client->dev.kobj, &cw2015_attr_group);
	power_supply_unregister(&this_chip->battery);
	kfree(this_chip);
	return 0;
}

#ifdef CONFIG_PM

static int cw2015_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct cw2015_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work(&chip->work);
	return 0;
}

static int cw2015_resume(struct i2c_client *client)
{
	struct cw2015_chip *chip = i2c_get_clientdata(client);

	schedule_delayed_work(&chip->work, msecs_to_jiffies(10));
	return 0;
}

#else

#define cw2015_suspend NULL
#define cw2015_resume NULL

#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static struct of_device_id cw2015_of_match_table[] = {
	{
		.compatible = "cellwise,cw2015-fg",
	},
	{},
};
MODULE_DEVICE_TABLE(of, cw2015_of_match_table);
#endif

#define CW2015_I2C_NAME "cw2015_i2c"//TODO

static const struct i2c_device_id cw2015_id[] = {
	{ CW2015_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cw2015_id);

static struct i2c_driver cw2015_i2c_driver = {
	.driver	= {
		.name	= CW2015_I2C_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = cw2015_of_match_table,
#endif
	},
	.probe		= cw2015_probe,
	.remove		= cw2015_remove,
	.suspend	= cw2015_suspend,
	.resume		= cw2015_resume,
	.id_table	= cw2015_id,
};

#if 0
static int __init cw2015_init(void)
{
	printk("CW2015 Fuel Gauge driver: initialize\n");
	return i2c_add_driver(&cw2015_i2c_driver);
}
module_init(cw2015_init);
// NOTE: I do not know why it can not boot while using fs_initcall()
//fs_initcall(cw2015_init);

static void __exit cw2015_exit(void)
{
	i2c_del_driver(&cw2015_i2c_driver);
}
module_exit(cw2015_exit);
#else
module_i2c_driver(cw2015_i2c_driver);
#endif

MODULE_AUTHOR("Sheng Liang <liang.sheng@samsung.com>");
MODULE_DESCRIPTION("CW2015 Fuel Gauge");
MODULE_LICENSE("GPL");
