/*
 * Flash IC driver for MEIZU M76. This is based on TI's sample code.
 * Modified and maintained by QuDao, qudao@meizu.com
 *
 * Copyright (C) 2014 MEIZU
 * Copyright (C) 2014 Texas Instruments
 *
 * Contact: Daniel Jeong <gshark.jeong@gmail.com>
 *			Ldd-Mlp <ldd-mlp@list.ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/platform_data/leds-lm3644.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

/* registers definitions */
#define REG_ENABLE		0x01
#define REG_IVFM 0x02
#define REG_FLASH_LED0_BR	0x03
#define REG_FLASH_LED1_BR	0x04
#define REG_TORCH_LED0_BR	0x05
#define REG_TORCH_LED1_BR	0x06
#define REG_FLASH_TOUT		0x08
#define REG_TEMP 0x09
#define REG_FLAG0		0x0a
#define REG_FLAG1		0x0b
#define REG_DEV_ID 0x0c

enum lm3644_devid {
	ID_FLASH0 = 0x0,
	ID_FLASH1,
	ID_TORCH0,
	ID_TORCH1,
	ID_MAX
};

enum lm3644_mode {
	MODE_STDBY = 0x0,
	MODE_IR,
	MODE_TORCH,
	MODE_FLASH,
	MODE_MAX
};

enum lm3644_devfile {
	DFILE_FLASH0_ENABLE = 0,
	DFILE_FLASH0_ONOFF,
	DFILE_FLASH0_SOURCE,
	DFILE_FLASH0_TIMEOUT,
	DFILE_FLASH1_ENABLE,
	DFILE_FLASH1_ONOFF, // 5
	DFILE_TORCH0_ENABLE,
	DFILE_TORCH0_ONOFF,
	DFILE_TORCH0_SOURCE,
	DFILE_TORCH1_ENABLE,
	DFILE_TORCH1_ONOFF,
	DFILE_MAX
};

#define to_lm3644(_ctrl, _no) container_of(_ctrl, struct lm3644, cdev[_no])

struct lm3644 {
	struct device *dev;

	int brightness[ID_MAX];
	struct work_struct work[ID_MAX];
	struct led_classdev cdev[ID_MAX];

	int hwen_gpio;
	int strobe_gpio;
	struct lm3644_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;
};

static int lm3644_cur2reg(struct lm3644 *pchip, enum lm3644_devid devid)
{
	int reg_val = 0;
	int min;
	int max;
	int step;
	int cur = pchip->brightness[devid];

	switch (devid) {
	case ID_FLASH0:
	case ID_FLASH1:
		min = LM3644_MIN_FLASH_CURRENT;
		max = LM3644_MAX_FLASH_CURRENT;
		step = LM3644_FLASH_CURRENT_STEP;
		break;
	case ID_TORCH0:
	case ID_TORCH1:
		min = LM3644_MIN_TORCH_CURRENT;
		max = LM3644_MAX_TORCH_CURRENT;
		step = LM3644_TORCH_CURRENT_STEP;
		break;
	default:
		pr_err("%s(), error : undefined devid %d\n",
			__func__, devid);
		return -EINVAL;
	}

	if (cur < min) {
		pr_warn("%s(), warnning!will set user current to min current %d\n",
			__func__, min);
		cur = min;
	} else if (cur > max){
		pr_warn("%s(), warnning!will set user current to max current %d\n",
			__func__, max);
		cur = max;
	}

	/*
	* update it after sanity check.
	*/
	pchip->brightness[devid] = cur;

	while (min + step * reg_val < cur &&
			min + step * reg_val < max)
		reg_val++;

	pr_info("%s(), user want current is %d, will set %d to reg\n",
		__func__, cur, reg_val);
	return reg_val;
}

static int lm3644_read_byte(struct lm3644 *pchip, u8 addr)
{
	int rval = 0;
	int ret;
	ret = regmap_read(pchip->regmap, addr, &rval);
	if (ret < 0) {
		pr_err("%s(), read err:%d\n", __func__, ret);
		return ret;
	}
	return rval;
}

static void lm3644_read_flag(struct lm3644 *pchip)
{

	int rval;
	unsigned int flag0, flag1;

	rval = regmap_read(pchip->regmap, REG_FLAG0, &flag0);
	rval |= regmap_read(pchip->regmap, REG_FLAG1, &flag1);

	if (rval < 0)
		dev_err(pchip->dev, "i2c access fail.\n");

	dev_info(pchip->dev, "[flag1] 0x%x, [flag0] 0x%x\n",
		 flag1 & 0x1f, flag0);
}

/* torch0 brightness control */
static void lm3644_deferred_torch0_brightness_set(struct work_struct *work)
{
	struct lm3644 *pchip = container_of(work,
					    struct lm3644, work[ID_TORCH0]);

	int reg_val;
	reg_val = lm3644_cur2reg(pchip, ID_TORCH0);

	if (regmap_update_bits(pchip->regmap,
			REG_TORCH_LED0_BR, REG_TORCH0_BR_MASK,
			reg_val)) {
		dev_err(pchip->dev, "%s(), i2c access fail.\n", __func__);
	}
	lm3644_read_flag(pchip);
}

static void lm3644_torch0_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_TORCH0]);

	pchip->brightness[ID_TORCH0] = brightness;
	schedule_work(&pchip->work[ID_TORCH0]);
}

/* torch1 brightness control */
static void lm3644_deferred_torch1_brightness_set(struct work_struct *work)
{
	struct lm3644 *pchip = container_of(work,
					    struct lm3644, work[ID_TORCH1]);

	int reg_val;
	reg_val = lm3644_cur2reg(pchip, ID_TORCH1);

	if (regmap_update_bits(pchip->regmap,
			REG_TORCH_LED1_BR, REG_TORCH1_BR_MASK,
			reg_val)) {
		dev_err(pchip->dev, "%s(), i2c access fail.\n", __func__);
	}
	lm3644_read_flag(pchip);
}

static void lm3644_torch1_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_TORCH1]);

	pchip->brightness[ID_TORCH1] = brightness;
	schedule_work(&pchip->work[ID_TORCH1]);
}

/* flash0 brightness control */
static void lm3644_deferred_flash0_brightness_set(struct work_struct *work)
{
	struct lm3644 *pchip = container_of(work,
					    struct lm3644, work[ID_FLASH0]);
	int reg_val;
	reg_val = lm3644_cur2reg(pchip, ID_FLASH0);

	if (regmap_update_bits(pchip->regmap,
			REG_FLASH_LED0_BR, REG_FLASH0_BR_MASK,
			reg_val)) {
		dev_err(pchip->dev, "%s(), i2c access fail.\n", __func__);
	}
	lm3644_read_flag(pchip);
}

static void lm3644_flash0_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_FLASH0]);

	pchip->brightness[ID_FLASH0] = brightness;
	schedule_work(&pchip->work[ID_FLASH0]);
}

/* flash1 brightness control */
static void lm3644_deferred_flash1_brightness_set(struct work_struct *work)
{
	struct lm3644 *pchip = container_of(work,
					    struct lm3644, work[ID_FLASH1]);

	int reg_val;
	reg_val = lm3644_cur2reg(pchip, ID_FLASH1);

	if (regmap_update_bits(pchip->regmap,
			REG_FLASH_LED1_BR, REG_FLASH1_BR_MASK,
			reg_val)) {
		dev_err(pchip->dev, "%s(), i2c access fail.\n", __func__);
	}
	lm3644_read_flag(pchip);
}

static void lm3644_flash1_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_FLASH1]);

	pchip->brightness[ID_FLASH1] = brightness;
	schedule_work(&pchip->work[ID_FLASH1]);
}

enum led_brightness lm3644_flash0_brightness_get(struct led_classdev *cdev)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_FLASH0]);

	return pchip->brightness[ID_FLASH0];
}

enum led_brightness lm3644_flash1_brightness_get(struct led_classdev *cdev)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_FLASH1]);

	return pchip->brightness[ID_FLASH1];
}

enum led_brightness lm3644_torch0_brightness_get(struct led_classdev *cdev)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_TORCH0]);

	return pchip->brightness[ID_TORCH0];
}

enum led_brightness lm3644_torch1_brightness_get(struct led_classdev *cdev)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_TORCH1]);

	return pchip->brightness[ID_TORCH1];
}

struct lm3644_devices {
	struct led_classdev cdev;
	work_func_t func;
};

static struct lm3644_devices lm3644_leds[ID_MAX] = {
	[ID_FLASH0] = {
		       .cdev.name = "flash0",
		       .cdev.brightness = 0,
		       .cdev.max_brightness = LM3644_MAX_FLASH_CURRENT,
		       .cdev.brightness_set = lm3644_flash0_brightness_set,
		       .cdev.brightness_get = lm3644_flash0_brightness_get,
		       .cdev.default_trigger = "flash0",
		       .func = lm3644_deferred_flash0_brightness_set},
	[ID_FLASH1] = {
		       .cdev.name = "flash1",
		       .cdev.brightness = 0,
		       .cdev.max_brightness = LM3644_MAX_FLASH_CURRENT,
		       .cdev.brightness_set = lm3644_flash1_brightness_set,
		       .cdev.brightness_get = lm3644_flash1_brightness_get,
		       .cdev.default_trigger = "flash1",
		       .func = lm3644_deferred_flash1_brightness_set},
	[ID_TORCH0] = {
		       .cdev.name = "torch0",
		       .cdev.brightness = 0,
		       .cdev.max_brightness = LM3644_MAX_TORCH_CURRENT,
		       .cdev.brightness_set = lm3644_torch0_brightness_set,
		       .cdev.brightness_get = lm3644_torch0_brightness_get,
		       .cdev.default_trigger = "torch0",
		       .func = lm3644_deferred_torch0_brightness_set},
	[ID_TORCH1] = {
		       .cdev.name = "torch1",
		       .cdev.brightness = 0,
		       .cdev.max_brightness = LM3644_MAX_TORCH_CURRENT,
		       .cdev.brightness_set = lm3644_torch1_brightness_set,
				.cdev.brightness_get = lm3644_torch1_brightness_get,
		       .cdev.default_trigger = "torch1",
		       .func = lm3644_deferred_torch1_brightness_set},
};

static void lm3644_led_unregister(struct lm3644 *pchip, enum lm3644_devid id)
{
	int icnt;

	for (icnt = id; icnt > 0; icnt--)
		led_classdev_unregister(&pchip->cdev[icnt - 1]);
}

static int lm3644_led_register(struct lm3644 *pchip)
{
	int icnt, rval;

	for (icnt = 0; icnt < ID_MAX; icnt++) {
		INIT_WORK(&pchip->work[icnt], lm3644_leds[icnt].func);
		pchip->cdev[icnt].name = lm3644_leds[icnt].cdev.name;
		pchip->cdev[icnt].max_brightness =
		    lm3644_leds[icnt].cdev.max_brightness;
		pchip->cdev[icnt].brightness =
		    lm3644_leds[icnt].cdev.brightness;
		pchip->cdev[icnt].brightness_set =
		    lm3644_leds[icnt].cdev.brightness_set;
		pchip->cdev[icnt].brightness_get =
		    lm3644_leds[icnt].cdev.brightness_get;
		pchip->cdev[icnt].default_trigger =
		    lm3644_leds[icnt].cdev.default_trigger;
		rval = led_classdev_register((struct device *)
					     pchip->dev, &pchip->cdev[icnt]);
		if (rval < 0) {
			lm3644_led_unregister(pchip, icnt);
			return rval;
		}
	}
	return 0;
}

/* device files to control registers */
struct lm3644_commands {
	char *str;
	int size;
};

enum lm3644_cmd_id {
	CMD_ENABLE = 0,
	CMD_DISABLE,
	CMD_ON,
	CMD_OFF,
	CMD_IRMODE,
	CMD_OVERRIDE,
	CMD_MAX // 6
};

struct lm3644_commands cmds[CMD_MAX] = {
	[CMD_ENABLE] = {"enable", 6},
	[CMD_DISABLE] = {"disable", 7},
	[CMD_ON] = {"on", 2},
	[CMD_OFF] = {"off", 3},
	[CMD_IRMODE] = {"irmode", 6},
	[CMD_OVERRIDE] = {"override", 8},
};

struct lm3644_files {
	enum lm3644_devid id;
	struct device_attribute attr;
};

static size_t lm3644_ctrl(struct device *dev,
			  const char *buf, enum lm3644_devid id,
			  enum lm3644_devfile dfid, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644 *pchip = to_lm3644(led_cdev, id);
	enum lm3644_cmd_id icnt;
	int tout, rval;

	pr_info("%s(), id:%d, dfid: %d, pchip:0x%p\n",
		__func__, id, dfid ,pchip);
	mutex_lock(&pchip->lock);
	for (icnt = 0; icnt < CMD_MAX; icnt++) {
		if (strncmp(buf, cmds[icnt].str, cmds[icnt].size) == 0)
			break;
	}

	if (icnt >= CMD_MAX) {
		pr_err("%s(), input %s is not match predefined cmds , icnt is:%d\n",
			__func__, buf, icnt);
	} else {
		pr_err("%s(), icnt is:%d\n", __func__, icnt);
	}

	switch (dfid) {
		/* led 0 enable */
	case DFILE_FLASH0_ENABLE:
	case DFILE_TORCH0_ENABLE:
		if (icnt == CMD_ENABLE) {
			pr_info("%s(),  enable LED0\n", __func__);
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE,
						REG_LED0_ENALBE_MASK, REG_LED0_ENABLE);
		} else if (icnt == CMD_DISABLE) {
			pr_info("%s(),  disable LED0\n", __func__);
			rval = regmap_update_bits(pchip->regmap, REG_ENABLE,
						REG_LED0_ENALBE_MASK, ~REG_LED0_ENABLE);
		} else {
			pr_err("%s(), for dfid %d, icnt %d is not support\n",
				__func__, dfid, icnt);
		}
		break;
		/* led 1 enable, flash override */
	case DFILE_FLASH1_ENABLE:
		if (icnt == CMD_ENABLE) {
			pr_info("%s(),  enable LED1 of FLASH\n", __func__);
			/*
			* LED1 Flash current is not set to LED0 Flash current
			*/
			rval = regmap_update_bits(pchip->regmap,
						  REG_FLASH_LED0_BR, 0x80, 0x0);
			rval |=
				regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_LED1_ENALBE_MASK, REG_LED1_ENABLE);
		} else if (icnt == CMD_DISABLE) {
			pr_info("%s(),  disable LED1 of FLASH\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_LED1_ENALBE_MASK, ~REG_LED1_ENABLE);
		} else if (icnt == CMD_OVERRIDE) {
			pr_info("%s(),  enable LED1 of FLASH and override its flash current with FLASH0's\n",
				__func__);
			/*
			* LED1 Flash current is set to LED0 Flash current
			*/
			rval = regmap_update_bits(pchip->regmap,
						  REG_FLASH_LED0_BR, 0x80,
						  0x80);
			rval |=
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_LED1_ENALBE_MASK, REG_LED1_ENABLE);
		} else {
				pr_err("%s(), for dfid %d, icnt %d is not support\n",
					__func__, dfid, icnt);
		}
		break;
		/* led 1 enable, torch override */
	case DFILE_TORCH1_ENABLE:
		if (icnt == CMD_ENABLE) {
			pr_info("%s(),  enable LED1 of TORCH\n", __func__);
			/*
			* LED1 Torch current is not set to LED0 Torch current
			*/
			rval = regmap_update_bits(pchip->regmap,
						  REG_TORCH_LED0_BR, 0x80, 0x0);
			rval |=
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_LED1_ENALBE_MASK, REG_LED1_ENABLE);
		} else if (icnt == CMD_DISABLE) {
			pr_info("%s(),  disable LED1 of TORCH\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_LED1_ENALBE_MASK, ~REG_LED1_ENABLE);
		} else if (icnt == CMD_OVERRIDE) {
			pr_info("%s(),  enable LED1 of TORCH and override its torch current with TORCH0's\n",
				__func__);
			/*
			* LED1 Torch current is set to LED0 Torch current
			*/
			rval = regmap_update_bits(pchip->regmap,
						  REG_TORCH_LED0_BR, 0x80,
						  0x80);
			rval |=
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_LED1_ENALBE_MASK, REG_LED1_ENABLE);
		} else {
				pr_err("%s(), for dfid %d, icnt %d is not support\n",
					__func__, dfid, icnt);
		}
		break;
		/* mode control flash/ir */
	case DFILE_FLASH0_ONOFF:
	case DFILE_FLASH1_ONOFF:
		if (icnt == CMD_ON) {
			pr_info("%s(), set mode to FLASH\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_MODE_MASK, REG_MODE_FLASH);
		} else if (icnt == CMD_OFF) {
			pr_info("%s(), set mode to STANDBY\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_MODE_MASK, REG_MODE_STANDBY);
		} else if (icnt == CMD_IRMODE) {
			pr_info("%s(), set mode to IR\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_MODE_MASK, REG_MODE_IR);
		} else {
			pr_err("%s(), for dfid %d, icnt %d is not support\n",
				__func__, dfid, icnt);
		}
		break;
		/* mode control torch */
	case DFILE_TORCH0_ONOFF:
	case DFILE_TORCH1_ONOFF:
		if (icnt == CMD_ON) {
			pr_info("%s(), set mode to TORCH\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_MODE_MASK, REG_MODE_TORCH);
		} else if (icnt == CMD_OFF) {
			pr_info("%s(), set mode to STANDBY\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_MODE_MASK, REG_MODE_STANDBY);
		} else {
			pr_err("%s(), for dfid %d, icnt %d is not support\n",
				__func__, dfid, icnt);
		}
		break;
		/* strobe pin control */
	case DFILE_FLASH0_SOURCE:
		if (icnt == CMD_ON) {
			pr_info("%s(), enable STROBE pin\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_STROBE_ENABLE_MASK, REG_STROBE_ENABLE);
		} else if (icnt == CMD_OFF) {
			pr_info("%s(), disable STROBE pin\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_STROBE_ENABLE_MASK, ~REG_STROBE_ENABLE);
		} else {
			pr_err("%s(), for dfid %d, icnt %d is not support\n",
				__func__, dfid, icnt);
		}
		break;
	case DFILE_TORCH0_SOURCE:
		if (icnt == CMD_ON) {
			pr_info("%s(), enable TORCH_NTC pin\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_TORCH_NTC_ENABLE_MASK, REG_TORCH_NTC_ENABLE);
		} else if (icnt == CMD_OFF) {
			pr_info("%s(), disable TORCH_NTC pin\n", __func__);
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE,
					REG_TORCH_NTC_ENABLE_MASK, ~REG_TORCH_NTC_ENABLE);
		} else {
			pr_err("%s(), for dfid %d, icnt %d is not support\n",
				__func__, dfid, icnt);
		}
		break;
		/* flash time out */
	case DFILE_FLASH0_TIMEOUT:
		rval = kstrtouint((const char *)buf, 10, &tout);
		if (rval < 0) {
			pr_err("%s(), input %s is illegal!\n", __func__, buf);
			break;
		} else {
			pr_info("%s(), for %d, set flash timeout valut to %d\n",
				__func__, dfid, tout);
		}
		rval = regmap_update_bits(pchip->regmap,
					  REG_FLASH_TOUT, 0x0f, tout);
		break;
	default:
		dev_err(pchip->dev, "error : undefined dev file\n");
		break;
	}

	lm3644_read_flag(pchip);
	mutex_unlock(&pchip->lock);
	return size;
}

static ssize_t lm3644_chip_reg_store(struct device *dev,
				       struct device_attribute *devAttr,
				       const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644 *pchip = to_lm3644(led_cdev, ID_TORCH0);

	int rval, i;

	for (i = 1; i <= 0xd; i++) {
		rval = lm3644_read_byte(pchip, i);
		if (rval >= 0) {
			pr_info("%s(), reg 0x%02x = 0x%02x\n",
				__func__, i, rval);
		}
	}

	return size;
}
static DEVICE_ATTR(chip_reg, 0220, NULL, lm3644_chip_reg_store);

static ssize_t lm3644_hwen_store(struct device *dev,
				       struct device_attribute *devAttr,
				       const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3644 *pchip = to_lm3644(led_cdev, ID_TORCH0);
	unsigned long flag;

	if (!strncmp(buf, "enable", strlen("enable"))) {
		flag = GPIOF_OUT_INIT_HIGH;
		pr_info("%s(), hwen enable\n", __func__);
	} else if (!strncmp(buf, "disable", strlen("disable"))) {
		pr_info("%s(), hwen disable\n", __func__);
		flag = GPIOF_OUT_INIT_LOW;
	} else {
		pr_info("%s(), err! unsupport input!\n", __func__);
		return -EINVAL;
	}

	gpio_request_one(pchip->hwen_gpio, flag, "hwen-gpio");
	gpio_free(pchip->hwen_gpio);

	return size;
}
static DEVICE_ATTR(hwen, 0220, NULL, lm3644_hwen_store);

/* flash enable control */
static ssize_t lm3644_flash0_enable_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_ENABLE, size);
}

static ssize_t lm3644_flash0_enable_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

static ssize_t lm3644_flash1_enable_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH1, DFILE_FLASH1_ENABLE, size);
}

static ssize_t lm3644_flash1_enable_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

/* flash onoff control */
static ssize_t lm3644_flash0_onoff_store(struct device *dev,
					 struct device_attribute *devAttr,
					 const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_ONOFF, size);
}

static ssize_t lm3644_flash0_onoff_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

static ssize_t lm3644_flash1_onoff_store(struct device *dev,
					 struct device_attribute *devAttr,
					 const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH1, DFILE_FLASH1_ONOFF, size);
}

static ssize_t lm3644_flash1_onoff_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

/* flash timeout control */
static ssize_t lm3644_flash0_timeout_store(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_TIMEOUT, size);
}

static ssize_t lm3644_flash0_timeout_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

/* flash source control */
static ssize_t lm3644_flash0_source_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_SOURCE, size);
}

static ssize_t lm3644_flash0_source_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

/* torch enable control */
static ssize_t lm3644_torch0_enable_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_TORCH0, DFILE_TORCH0_ENABLE, size);
}

static ssize_t lm3644_torch0_enable_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

static ssize_t lm3644_torch1_enable_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_TORCH1, DFILE_TORCH1_ENABLE, size);
}

static ssize_t lm3644_torch1_enable_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

/* torch onoff control */
static ssize_t lm3644_torch0_onoff_store(struct device *dev,
					 struct device_attribute *devAttr,
					 const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_TORCH0, DFILE_TORCH0_ONOFF, size);
}

static ssize_t lm3644_torch0_onoff_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

static ssize_t lm3644_torch1_onoff_store(struct device *dev,
					 struct device_attribute *devAttr,
					 const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_TORCH1, DFILE_TORCH1_ONOFF, size);
}

static ssize_t lm3644_torch1_onoff_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

/* torch source control */
static ssize_t lm3644_torch0_source_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_TORCH0, DFILE_TORCH0_SOURCE, size);
}

static ssize_t lm3644_torch0_source_show(struct device *dev,
	struct device_attribute *devAttr, char *buf)
{
	pr_info("%s(), do nothing\n", __func__);
	return 0;
}

#define lm3644_attr(_name, _show, _store)\
{\
	.attr = {\
		.name = _name,\
		.mode = 0644,\
	},\
	.show = _show,\
	.store = _store,\
}

static struct lm3644_files lm3644_devfiles[DFILE_MAX] = {
	[DFILE_FLASH0_ENABLE] = {
				 .id = ID_FLASH0,
				 .attr =
				 lm3644_attr("enable", lm3644_flash0_enable_show,
					     lm3644_flash0_enable_store),
				 },
	[DFILE_FLASH0_ONOFF] = {
				.id = ID_FLASH0,
				.attr =
				lm3644_attr("onoff", lm3644_flash0_onoff_show,
					    lm3644_flash0_onoff_store),
				},
	[DFILE_FLASH0_SOURCE] = {
				 .id = ID_FLASH0,
				 .attr =
				 lm3644_attr("source", lm3644_flash0_source_show,
					     lm3644_flash0_source_store),
				 },
	[DFILE_FLASH0_TIMEOUT] = {
				  .id = ID_FLASH0,
				  .attr =
				  lm3644_attr("timeout", lm3644_flash0_timeout_show,
					      lm3644_flash0_timeout_store),
				  },
	[DFILE_FLASH1_ENABLE] = {
				 .id = ID_FLASH1,
				 .attr =
				 lm3644_attr("enable", lm3644_flash1_enable_show,
					     lm3644_flash1_enable_store),
				 },
	[DFILE_FLASH1_ONOFF] = {
				.id = ID_FLASH1,
				.attr =
				lm3644_attr("onoff", lm3644_flash1_onoff_show,
					    lm3644_flash1_onoff_store),
				},
	[DFILE_TORCH0_ENABLE] = {
				 .id = ID_TORCH0,
				 .attr =
				 lm3644_attr("enable", lm3644_torch0_enable_show,
					     lm3644_torch0_enable_store),
				 },
	[DFILE_TORCH0_ONOFF] = {
				.id = ID_TORCH0,
				.attr =
				lm3644_attr("onoff", lm3644_torch0_onoff_show,
					    lm3644_torch0_onoff_store),
				},
	[DFILE_TORCH0_SOURCE] = {
				 .id = ID_TORCH0,
				 .attr =
				 lm3644_attr("source", lm3644_torch0_source_show,
					     lm3644_torch0_source_store),
				 },
	[DFILE_TORCH1_ENABLE] = {
				 .id = ID_TORCH1,
				 .attr =
				 lm3644_attr("enable", lm3644_torch1_enable_show,
					     lm3644_torch1_enable_store),
				 },
	[DFILE_TORCH1_ONOFF] = {
				.id = ID_TORCH1,
				.attr =
				lm3644_attr("onoff", lm3644_torch1_onoff_show,
					    lm3644_torch1_onoff_store),
				}
};

static void lm3644_df_remove(struct lm3644 *pchip, enum lm3644_devfile dfid)
{
	enum lm3644_devfile icnt;

	for (icnt = dfid; icnt > 0; icnt--)
		device_remove_file(pchip->cdev[lm3644_devfiles[icnt - 1].id].
				   dev, &lm3644_devfiles[icnt - 1].attr);
}

static int lm3644_df_create(struct lm3644 *pchip)
{
	enum lm3644_devfile icnt;
	int rval;

	for (icnt = 0; icnt < DFILE_MAX; icnt++) {
		rval =
		    device_create_file(pchip->cdev[lm3644_devfiles[icnt].id].
				       dev, &lm3644_devfiles[icnt].attr);
		if (rval < 0) {
			lm3644_df_remove(pchip, icnt);
			return rval;
		}
	}
	return 0;
}

static const struct regmap_config lm3644_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

int lm3644_def_setting(struct lm3644 *pchip)
{
	int rval;
	/*
	* enable LED0 && LED1
	*/
	rval = regmap_update_bits(pchip->regmap, REG_ENABLE,
		REG_LED0_ENALBE_MASK | REG_LED1_ENALBE_MASK,
		REG_LED0_ENABLE | REG_LED1_ENALBE_MASK);

	/*
	* enable TORCH_NTC pin, enable STROBE pin, disable TX pin
	*/
	rval = regmap_update_bits(pchip->regmap, REG_ENABLE,
		REG_TORCH_NTC_ENABLE_MASK | REG_STROBE_ENABLE_MASK |
		REG_TX_ENABLE_MASK,
		(REG_TORCH_NTC_ENABLE |	REG_STROBE_ENABLE |
		~REG_TX_ENABLE));

	/*
	* Enable UVLO
	*/
	rval = regmap_update_bits(pchip->regmap, REG_IVFM,
		REG_UVLO_MASK, REG_UVLO_ENABLE);

#ifdef USE_NTC
	/*
	* use NTC feature
	* 7: About 45 degree
	* def: About 55 degree
	*/
	rval = regmap_update_bits(pchip->regmap, REG_TEMP,
		REG_TORCH_TEMP_FUNC_MASK | REG_TEMP_DETECT_VOL_MASK |
		REG_NTC_SHORT_FAULT_MASK | REG_NTC_OPEN_FAULT_MASK,
		REG_TEMP_FUNC |
		REG_NTC_SHORT_FAULT_ENABLE | REG_NTC_OPEN_FAULT_ENABLE);
#endif

	/*
	* set Flash Timeout to max
	*/
	rval |= regmap_update_bits(pchip->regmap, REG_FLASH_TOUT,
		REG_FLASH_TOUT_MASK, REG_FLASH_TOUT_MAX);

	/*
	* set Flash0 && FLASH1's current to max current,
	* Torch0 && Torch1's current to 100mA
	*/
	pchip->brightness[ID_FLASH0] = LM3644_MAX_FLASH_CURRENT;
	schedule_work(&pchip->work[ID_FLASH0]);
	pchip->brightness[ID_FLASH1] = LM3644_MAX_FLASH_CURRENT;
	schedule_work(&pchip->work[ID_FLASH1]);
	pchip->brightness[ID_TORCH0] = 100000;
	schedule_work(&pchip->work[ID_TORCH0]);
	pchip->brightness[ID_TORCH1] = 100000;
	schedule_work(&pchip->work[ID_TORCH1]);

	return rval;
}

#ifdef CONFIG_OF
struct lm3644_platform_data *lm3644_parse_dt(struct i2c_client *client)
{
	struct lm3644 *pchip = NULL;
	struct device_node *np = client->dev.of_node;
	int gpio = -1;
	char *pin_name = NULL;
	struct pinctrl *pinctrl;

	pchip = i2c_get_clientdata(client);

	if (IS_ERR_OR_NULL(pchip)) {
		pr_err("%s(), err!pchip is not ready:0x%p\n",
			__func__, pchip);
		return NULL;
	}

	pin_name = "hwen-gpio";
	if (of_property_read_bool(np, pin_name)) {
		gpio = of_get_named_gpio_flags(np, pin_name, 0, NULL);
		pr_info("%s(), got gpio %s:%d\n", __func__, pin_name, gpio);
		pchip->hwen_gpio = gpio;
		gpio_request_one(gpio, GPIOF_OUT_INIT_HIGH,
				    pin_name);
	} else {
		pr_err("%s(), Err! can not get gpio %s\n", __func__, pin_name);
	}


	pin_name = "strobe-gpio";
	if (of_property_read_bool(np, pin_name)) {
		gpio = of_get_named_gpio_flags(np, pin_name, 0, NULL);
		pr_info("%s(), got gpio %s:%d\n", __func__, pin_name, gpio);
		pchip->strobe_gpio = gpio;
		gpio_request_one(gpio, GPIOF_OUT_INIT_LOW,
				    pin_name);
	} else {
		pr_err("%s(), Err! can not get gpio %s\n", __func__, pin_name);
	}

	pin_name = "torch-temp";
	pinctrl = devm_pinctrl_get_select(&client->dev, pin_name);
	if (IS_ERR(pinctrl)) {
		dev_err(&client->dev, "%s(), pinctrl %s select failed: %ld\n",
			__func__, pin_name, PTR_ERR(pinctrl));
	} else {
		dev_info(&client->dev, "%s(), pinctrl %s select success\n",
			__func__, pin_name);
	}
	return NULL;
}
#else
struct lm3644_platform_data *lm3644_parse_dt(struct i2c_client *client)
{
	return NULL;
}
#endif

static int lm3644_check_access(struct lm3644 *pchip)
{
	int rval = -1;

	rval = lm3644_read_byte(pchip, REG_DEV_ID);
	if (rval < 0) {
		pr_err("%s(), read reversion failed:%d\n", __func__, rval);
	} else {
		pr_info("%s(), CHIP_ID/REV[0x%x]\n", __func__, rval);
	}
	return rval;
}

static int lm3644_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct lm3644 *pchip;
	int rval;

	dev_info(&client->dev, "%s(), addr:0x%x, name:%s\n",
		__func__, client->addr, client->name);

	/* i2c check */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	pchip = devm_kzalloc(&client->dev, sizeof(struct lm3644), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	pchip->dev = &client->dev;
	pchip->regmap = devm_regmap_init_i2c(client, &lm3644_regmap);
	if (IS_ERR(pchip->regmap)) {
		rval = PTR_ERR(pchip->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			rval);
		return rval;
	}
	mutex_init(&pchip->lock);
	i2c_set_clientdata(client, pchip);

	lm3644_parse_dt(client);

	rval = lm3644_check_access(pchip);
	if (rval < 0) {
		dev_err(&client->dev, "%s(), check access failed:%d\n",
			__func__, rval);
		goto err;
	}

	/* led class register */
	rval = lm3644_led_register(pchip);
	if (rval < 0) {
		dev_err(&client->dev, "%s(), led register failed:%d\n",
			__func__, rval);
		goto err;
	}

	/* create dev files */
	rval = lm3644_df_create(pchip);
	if (rval < 0) {
		lm3644_led_unregister(pchip, ID_MAX);
		dev_err(&client->dev, "%s(), create dev files failed:%d\n",
			__func__, rval);
		goto err;
	}

	/* lm3644_def_setting(pchip); */

	rval = device_create_file(pchip->cdev[ID_TORCH0].dev,
				 &dev_attr_chip_reg);
	if (rval < 0) {
		dev_err(pchip->dev, "%s(), create dev flash reg failed: %d\n",
			__func__, rval);
	}

	rval = device_create_file(pchip->cdev[ID_TORCH0].dev,
				 &dev_attr_hwen);
	if (rval < 0) {
		dev_err(pchip->dev, "%s(), create dev hw en failed: %d\n",
			__func__, rval);
	}

	gpio_set_value(pchip->hwen_gpio, 0);

	dev_info(pchip->dev, "%s(), pchip:0x%p, probed sucessfully\n",
		__func__, pchip);

err:
	gpio_free(pchip->hwen_gpio);
	gpio_free(pchip->strobe_gpio);
	return rval;
}

static int lm3644_remove(struct i2c_client *client)
{
	struct lm3644 *pchip = i2c_get_clientdata(client);

	lm3644_df_remove(pchip, DFILE_MAX);
	lm3644_led_unregister(pchip, ID_MAX);

	return 0;
}

static const struct i2c_device_id lm3644_id[] = {
	{LM3644_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lm3644_id);

#ifdef CONFIG_OF
static struct of_device_id lm3644_of_match_table[] = {
	{
		.compatible = "ti,leds-lm3644",
	},
	{},
};
MODULE_DEVICE_TABLE(of, lm3644_of_match_table);
#else
#define lm3644_of_match_table NULL
#endif

static struct i2c_driver lm3644_i2c_driver = {
	.driver = {
		   .name = LM3644_NAME,
		   .owner = THIS_MODULE,
		   .pm = NULL,
			.of_match_table = lm3644_of_match_table,
		   },
	.probe = lm3644_probe,
	.remove = lm3644_remove,
	.id_table = lm3644_id,
};

module_i2c_driver(lm3644_i2c_driver);

MODULE_DESCRIPTION("Flash IC driver for MEIZU M86");
MODULE_AUTHOR("QuDao <qudao@meizu.com>");
MODULE_AUTHOR("Daniel Jeong <daniel.jeong@ti.com>");
MODULE_LICENSE("GPL v2");
