/*
 *  drv2605-haptic controller driver
 *
 *  Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *  <luochucheng@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kdev_t.h>
#include <linux/timed_output.h>
#include <linux/workqueue.h>

#include <linux/of_gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/regmap.h>
#include <linux/major.h>

#define MAX_TIMEOUT		1000
#define TAG "[m76-haptic]"

struct haptic_data {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex haptic_mutex;
	struct timed_output_dev tdev;
	struct delayed_work disable_work;
	struct workqueue_struct *vibrator_queue;
	bool already_shutdown;
	int gpio_motor_en;
	int motor_calib_version;
	struct class *haptic_class;
	struct device *haptic_device;
};
enum drv2605_regs{
	STATUS = 0,
	CONTROL = 1,
	RTP = 2,
	LIBRARY_SEL = 3,
	WAIT1 = 4,
	WAIT2 = 5,
	WAIT3 = 6,
	WAIT4 = 7,
	WAIT5 = 8,
	WAIT6 = 9,
	WAIT7 = 0xA,
	WAIT8 = 0xB,
	GO = 0xC,
	ODT = 0xD,
	SPT = 0xE,
	SNT = 0xF,
	BRT = 0x10,
	ATH = 0X11,
	ATH_MININPUT = 0x12,
	ATH_MAXINPUT = 0x13,
	ATH_MINDRIVE = 0x14,
	ATH_MAXDRIVE = 0x15,
	RATED_VOLTAGE = 0x16,
	OD_CLAMP = 0x17,
	ACALCOMP = 0x18,
	ACALBEMF = 0x19,
	ERM_LAR = 0x1A,
	STARTUP_BOOST = 0x1B,
	BIDIR_INPUT = 0x1C,
	NG_THRESH = 0x1D,
	AUTO_CALTIME = 0x1E,
	VBAT = 0x21,
	LRA_PERIOD= 0x22,
};
struct motor_calib{
	u8 motor_calib_version;
	u8 magic;	/*magic number*/
	u8 acc;	/*Auto Calibration Compensation*/
	u8 acb;	/*Auto Calibration Back-EMF*/
	u8 fc;	/*Feedback Control*/
};
static void drv2605_play_rtp(struct haptic_data *chip);

#ifdef __CONFIG_DEBUG_HAPTIC__
static void drv2605_show_regs(struct haptic_data *chip)
{
	unsigned int i = 0;
	unsigned int val;
	int ret;

	pr_info("%s:reg 0~16=", __func__);
	for (i = 0; i < 17; i++) {
		ret = regmap_read(chip->regmap, i, &val);
		if(ret){
			pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		}
		pr_info("0x%02x,", val);
	}
	pr_info("\n");
}
#endif
struct motor_calib gmotor_calib;
struct haptic_data *gchip=NULL;

static void drv2605_haptic_on(struct haptic_data *chip, bool en)
{
	if(chip->already_shutdown)
		return;
	if (en) {
		pr_debug(TAG"enable haptic motor.\n");
		drv2605_play_rtp(chip);
		mdelay(1);
	} else {
		pr_debug(TAG"disable haptic motor.\n");
		regmap_write(chip->regmap, CONTROL, 0x40);
		mdelay(10);
	}
}
int motor_read_register(unsigned int addr)
{
	unsigned int val;
	int ret=0;
	if(gchip){
		ret = regmap_read(gchip->regmap, addr, &val);
		if(ret == 0){
			return val;
		}else{
			return -EINVAL;
		}
	}
	return -EINVAL;
}
EXPORT_SYMBOL(motor_read_register);
int motor_write_register(unsigned int addr,int val)
{
	if(gchip)
		return regmap_write(gchip->regmap, addr, val);
	return -EINVAL;
}
EXPORT_SYMBOL(motor_write_register);
void motor_enable(void)
{
	if(gchip){
		drv2605_haptic_on(gchip,true);
	}
}
EXPORT_SYMBOL(motor_enable);

void motor_disable(void)
{
	if(gchip){
		drv2605_haptic_on(gchip,false);
	}
}
EXPORT_SYMBOL(motor_disable);

static void motor_disable_work_func(struct work_struct *work)
{
	struct haptic_data *chip = container_of(work, struct haptic_data, disable_work.work);

	mutex_lock(&chip->haptic_mutex);
	drv2605_haptic_on(chip, false);
	mutex_unlock(&chip->haptic_mutex);
}

static int haptic_get_time(struct timed_output_dev *tdev)
{
	return 0;
}

static void haptic_enable(struct timed_output_dev *tdev, int value)
{
	struct haptic_data *chip =
		container_of(tdev, struct haptic_data, tdev);

	mutex_lock(&chip->haptic_mutex);

	if(delayed_work_pending(&chip->disable_work))
		cancel_delayed_work(&chip->disable_work);

	if (value > 0) {
		value = min(value, MAX_TIMEOUT);
		queue_delayed_work(chip->vibrator_queue,&chip->disable_work,msecs_to_jiffies(value));
		regmap_write(chip->regmap, RTP, 0x7F);
		drv2605_haptic_on(chip, true);
	}else{
		drv2605_haptic_on(chip, false);
	}
	mutex_unlock(&chip->haptic_mutex);
}
//auto calibration procedure
int drv2605_auto_calib(struct haptic_data *chip)
{
	int ret = 0;
	int go_clear = 0,status = 0;
	u8 calib_save[3];
	struct motor_calib motor_calib;
	//step 1: parameters (Type.ERM,LRA : rated voltage : allow overdrive voltage
	//step 2: supply voltage
	//step 3: go in auto calibration mode[0x01]=0x07
	ret =  regmap_write(chip->regmap, CONTROL, 7);
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
	//step 7: start auto calibration [0x0c] = 0x01
	ret =  regmap_write(chip->regmap, GO, 1);
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
	do{
		ret =  regmap_read(chip->regmap, GO, &go_clear);
		if(ret){
			pr_err(TAG"regmap_read fail in func %s line %d\n",__func__, __LINE__);
			goto out;
		}
		msleep(1);
	}while(go_clear);
	//step 8: check ok? Diag_Result bit.
	ret =  regmap_read(chip->regmap, STATUS, &status);
	if(ret){
		pr_err(TAG"regmap_read fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
	if(status & (1<<3)){
		pr_info(TAG"calibration fail\n");
	}else{
		pr_info(TAG"calibration success\n");
	}
	//step 9: record calibration params
	motor_calib.magic = 0x5A;
	motor_calib.motor_calib_version=chip->motor_calib_version;
	ret = regmap_bulk_read(chip->regmap,ACALCOMP,calib_save,3);
	if(ret){
		pr_err(TAG"regmap_bulk_read fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
	motor_calib.acc = calib_save[0];
	motor_calib.acb = calib_save[1];
	motor_calib.fc = calib_save[2];
	gmotor_calib = motor_calib;

out:
	return ret;
}
static int drv2605_chip_init(struct haptic_data *chip)
{
	int ret=0;
#if 0
	int reset_clear=0;
	//step 1: power up & wait 250 us
	//this chip uses VSYS which is always on,so skip this step
	//step 2: pull EN pin high(enable)
	//step 3: write mode [0x01] = 0x00;
	ret = regmap_write(chip->regmap, CONTROL, 0x80);//reset chip
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
	do{
		ret =  regmap_read(chip->regmap, CONTROL, &reset_clear);
		if(ret){
			pr_err(TAG"regmap_read fail in func %s line %d\n",__func__, __LINE__);
			goto out;
		}
		msleep(1);
	}while(reset_clear&0x80);//reset done
	msleep(8);
#endif
	ret = regmap_write(chip->regmap, CONTROL, 0);
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
	msleep(2);
	//step 5: write rated voltage [0x16] = xxx
	ret =  regmap_write(chip->regmap, RATED_VOLTAGE, 0x50);
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
	//step 6: write overdrive clamp voltage [0x17] = xxx
	ret =  regmap_write(chip->regmap, OD_CLAMP, 0x84);
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}

	//step 4: [0x1A] = 0x34 for ERM & [0x1A] = 0xA4 for LRA
	ret =  regmap_update_bits(chip->regmap, ERM_LAR, 0x80, 0x80);
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
	//step 4: if non-volatile, skip to step 6
	ret = regmap_write(chip->regmap, LIBRARY_SEL, 0x06);
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
	ret = regmap_write(chip->regmap, RTP, 0x7F);
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
	ret = regmap_write(chip->regmap, BIDIR_INPUT, 0xFF);
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
#if 0
	//step 5: perform auto calibration procedure
	if(gmotor_calib.magic != 0x5A || gmotor_calib.motor_calib_version != chip->motor_calib_version){
		ret = drv2605_auto_calib(chip);
	}else{
		regmap_write(chip->regmap, ACALCOMP,gmotor_calib.acc);
		regmap_write(chip->regmap, ACALBEMF, gmotor_calib.acb);
		regmap_update_bits(chip->regmap, ERM_LAR, 0x03,gmotor_calib.fc&0x03);
	}
#endif
	//step 6: choose library
	//setp 7: set mode
	//setp 8: standby
	ret = regmap_write(chip->regmap, CONTROL, 0x40);
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto out;
	}
out:
	return ret;
}
/* play Real Time Playback Waveform*/
static void drv2605_play_rtp(struct haptic_data *chip)
{
	regmap_write(chip->regmap, CONTROL, 5);
}
static const struct regmap_config drv2605_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
};
static int drv2605_register_timed_ouput(struct haptic_data *chip)
{
	chip->tdev.name = "vibrator";
	chip->tdev.get_time = haptic_get_time;
	chip->tdev.enable = haptic_enable;
	return timed_output_dev_register(&chip->tdev);

}
#ifdef CONFIG_OF
static int haptic_parse_dt(struct haptic_data *chip)
{
	struct device *dev = chip->dev;
	struct device_node *np = dev->of_node;

	if(!np){
		pr_info(TAG"%s(),  parse device tree error", __func__);
		return -EINVAL;
	}

	chip->gpio_motor_en = of_get_gpio(np, 0);
	of_property_read_u32(np, "motor_calib_version",
			&chip->motor_calib_version);

	return 0;
}
#endif
static ssize_t attr_set_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	char rw[10];
	int reg,value,ret;
	struct haptic_data *chip = dev_get_drvdata(dev);

	sscanf(buf,"%s %x %x",rw,&reg, &value);
	if(!strcmp(rw,"read")){
		ret = regmap_read(chip->regmap,reg,&value);
		pr_info(TAG"read from [%x] value = 0x%2x\n", reg, value);
	}else if(!strcmp(rw,"write")){
#ifndef CONFIG_USER_KERNEL
		ret = regmap_write(chip->regmap, reg, value);
		pr_info(TAG"write to [%x] value = 0x%2x\n", reg, value);
#else
		pr_info(TAG"write is disabled from userspace\n");
#endif
	}
	return size;
}
static ssize_t attr_auto_calib(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int calib=0;
	struct haptic_data *chip = dev_get_drvdata(dev);

	sscanf(buf,"%d",&calib);
	if(calib){
		drv2605_auto_calib(chip);
		pr_info(TAG"%s auto calibration done\n",__func__);
	}else{
		pr_info(TAG"%s calib info from mmc: magic = 0x%x\nacc=0x%x\nacb=0x%x\nversion=0x%x\n",
				__func__,gmotor_calib.magic,gmotor_calib.acc,
				gmotor_calib.acb,gmotor_calib.motor_calib_version);
	}
	return size;
}

static ssize_t attr_dump_reg(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct haptic_data *chip = dev_get_drvdata(dev);
	int i=0,offset=0,ret =0;
	int val;
	static char buf_bak[20];
	for(i=0x0;i<=0x22;i++){
		//skip these registers
		if(i == 0x1f || i == 0x20)
			continue;
		ret=regmap_read(chip->regmap, i, &val);
		sprintf(buf_bak,"0x%02x\n",val);
		strcat(buf,buf_bak);
		offset +=strlen(buf_bak);
	}
	return offset;
}
static struct device_attribute attributes[] = {
	__ATTR(reg_control, 0200, NULL, attr_set_reg),
	__ATTR(dump_reg, 0444, attr_dump_reg, NULL),
	__ATTR(auto_calib, 0222,  NULL,attr_auto_calib),
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
static dev_t const haptic_device_dev_t = MKDEV(MISC_MAJOR, 246);

static  int drv2605_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct haptic_data *chip;
	int ret = 0;

	dev_info(&client->dev, "%s : drv2605 Haptic Driver Loading\n", __func__);

	if(!i2c_check_functionality(client->adapter,I2C_FUNC_I2C)){
		dev_err(&client->dev, "motor chip i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &client->dev;
	chip->client = client;
	mutex_init(&chip->haptic_mutex);
	ret = haptic_parse_dt(chip);


	chip->regmap = devm_regmap_init_i2c(client, &drv2605_regmap);
	if(IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&client->dev, TAG"failed to register regmap\n");
		return ret;
	}

	ret = gpio_request(chip->gpio_motor_en, "gpio_motor_en");
	if(ret){
		pr_err(TAG"gpio_motor_en request error\n");
		return ret;
	}
	gpio_direction_output(chip->gpio_motor_en, 1);


	ret = drv2605_chip_init(chip);

	ret = drv2605_register_timed_ouput(chip);
	if (ret < 0) {
		pr_err(TAG"Failed to register timed_output : %d\n", ret);
		ret = -EFAULT;
		goto err_timed_output;
	}

	INIT_DELAYED_WORK(&chip->disable_work, motor_disable_work_func);
	chip->vibrator_queue = create_singlethread_workqueue(TAG);
	if(!chip->vibrator_queue) {
		pr_err(TAG"Unable to create workqueue\n");
		ret = -EFAULT;
		goto err_timed_output;
	}
	i2c_set_clientdata(client, chip);
	chip->haptic_class = class_create(THIS_MODULE,"haptic_class");
	if(IS_ERR(chip->haptic_class)){
		ret = PTR_ERR(chip->haptic_class);
		pr_err(TAG"%s haptic class create fail\n", __func__);
		goto err_class_create;
	}

	chip->haptic_device = device_create(chip->haptic_class,
			NULL,haptic_device_dev_t,chip,"haptic_device");
	if(IS_ERR(chip->haptic_device)){
		ret = PTR_ERR(chip->haptic_device);
		pr_err(TAG"%s haptic device create fail\n", __func__);
		goto err_device_create;
	}

	ret = create_sysfs_files(chip->haptic_device);
	if(ret < 0){
		pr_err(TAG"%s sysfiles create fail\n", __func__);
		goto err_create_sys;
	}
	gchip = chip;
	return 0;

err_class_create:
err_timed_output:
	kfree(chip);
err_create_sys:
	device_destroy(chip->haptic_class,haptic_device_dev_t);
err_device_create:
	class_destroy(chip->haptic_class);

	return ret;
}

static int  drv2605_remove(struct i2c_client *client)
{
	struct haptic_data *chip = i2c_get_clientdata(client);

	mutex_destroy(&chip->haptic_mutex);
	i2c_set_clientdata(client, NULL);
	timed_output_dev_unregister(&chip->tdev);
	kfree(chip);
	return 0;
}

/*
   this function will be called when power off the machine,
   to avoid non-stop vibration problem when shutdown.
   */
static void drv2605_shutdown(struct i2c_client * client)
{
	struct haptic_data *chip = i2c_get_clientdata(client);

	pr_debug("%s disable motor when power off!", __func__);
	drv2605_haptic_on(chip, false);
	chip->already_shutdown = true;
}

static int drv2605_suspend(struct device *dev)
{
	struct haptic_data *chip = dev_get_drvdata(dev);
	drv2605_haptic_on(chip, false);
	return 0;
}

static int drv2605_resume(struct device *dev)
{
	struct haptic_data *chip = dev_get_drvdata(dev);
	drv2605_haptic_on(chip, false);
	return 0;
}

static const struct dev_pm_ops drv2605_pm_ops = {
	.suspend	= drv2605_suspend,
	.resume	= drv2605_resume,
};

#ifdef CONFIG_OF
static struct of_device_id drv2605_dt_id[] = {
	{ .compatible = "ti,drv2605-haptic" },
	{ }
};
#endif

static const struct i2c_device_id drv2605_id[] = {
	{"drv2605-haptic", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, drv2605_id);
static struct i2c_driver drv2605_haptic_driver = {
	.driver = {
		.name = "drv2605-haptic",
		.owner = THIS_MODULE,
		.pm = &drv2605_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(drv2605_dt_id),
#endif
	},
	.shutdown = drv2605_shutdown,
	.probe = drv2605_probe,
	.remove = drv2605_remove,
	.id_table = drv2605_id,
};

static int __init drv2605_haptic_init(void)
{
	int rc;

	rc = i2c_add_driver(&drv2605_haptic_driver);
	if (rc) {
		pr_err("%s failed: i2c_add_driver rc=%d\n", __func__, rc);
		goto init_exit;
	}
	return 0;

init_exit:
	return rc;
}

static void __exit drv2605_haptic_exit(void)
{
	i2c_del_driver(&drv2605_haptic_driver);
}

late_initcall(drv2605_haptic_init);
module_exit(drv2605_haptic_exit);



MODULE_DESCRIPTION("TI DRV2605 haptic control driver for MEIZU m76");
MODULE_AUTHOR("luochucheng@meizu.com");
MODULE_LICENSE("GPLV2");
