/*
 *  tsu6721-muic controller driver
 *
 *  Copyright (C) 2014 Meizu Technology Co.Ltd, Zhuhai, China
 *  Chucheng Luo <luochucheng@meizu.com>
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
#include <linux/major.h>

#include <linux/of_gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include <linux/notifier.h>
#include <linux/muic_tsu6721.h>
#include <linux/wakelock.h>

#define TAG "[tsu6721-muic]"
#define WHO_AM_I 0x12
enum reg_addr{
	DEVICE_ID = 1,
	CONTROL = 2,
	INTERRUPT1 = 3,
	INTERRUPT2 = 4,
	INTERRUPT_MASK1 = 5,
	INTERRUPT_MASK2 = 6,
	ADC = 7,
	TIMING_SET1 =8,
	TIMING_SET2 = 9,
	DEVICE_TYPE1 = 10,
	DEVICE_TYPE2 = 11,
	BUTTON1 = 12,
	BUTTON2 = 13,
	MANUAL_SW1 = 0x13,
	MANUAL_SW2 = 0x14,
	DEVICE_TYPE3 = 0x15,
	RESET = 0x1B,
	TIMER_SETTING = 0x20,
	OCL_OCP_SETTING1 = 0x21,
	OCL_OCP_SETTING2 = 0x22,
	DEVICE_TYPE4 = 0x23,
};

#define BCD_TIMER_BITS (0x7<<3)
#define BCD_TIMER_3P6 (0X5<<3)
#define MANUAL_SWITCH_BITS (1<<2)
enum cable_type{
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_USB,
	CABLE_TYPE_AC,
	CABLE_TYPE_APPLE,
	CABLE_TYPE_OTG,
	CABLE_TYPE_NON_STANDARD,
	CABLE_TYPE_MEIZU_AC,
	CABLE_TYPE_UART,
};

struct muic_data {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex muic_mutex;
	int gpio_int_muic;
	int irq;

	enum cable_type cable_type;
	bool attach;

	struct class *muic_class;
	struct device *muic_device;
	struct wake_lock muic_wake_lock;
	bool en_report_otg;

	struct notifier_block otg_notifier;
	struct delayed_work irq_work;
};

BLOCKING_NOTIFIER_HEAD(muic_notifier_list);
int register_muic_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&muic_notifier_list,n);
}
int unregister_muic_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_unregister(&muic_notifier_list,n);
}

static int tsu6721_chip_init(struct muic_data *chip)
{
	int ret=0,whoami=0,ctrl;

	ret = regmap_read(chip->regmap,DEVICE_ID,&whoami);
	if(ret){
		pr_err(TAG"regmap_read fail in func %s line %d\n",__func__, __LINE__);
		goto done;
	}else{
		if(whoami == WHO_AM_I){
			pr_info(TAG"read chip id success\n");
		}else{
			pr_info(TAG"who am i = 0x%2x should be 0x%2x\n",whoami, WHO_AM_I);
			ret  = -1;
			goto done;
		}
	}
	//only care about attach&detach
	ret = regmap_write(chip->regmap, INTERRUPT_MASK1, 0xfc); //mask interrupt
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto done;
	}
	ret = regmap_write(chip->regmap, INTERRUPT_MASK2, 0xff); //mask interrupt
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto done;
	}
	//unmask interrupt
	ret = regmap_read(chip->regmap, CONTROL, &ctrl);
	if(ret){
		pr_err(TAG"regmap_read fail in func %s line %d\n",__func__, __LINE__);
		goto done;
	}
	ret = regmap_write(chip->regmap, CONTROL, ctrl  & (~1)); //unmask interrupt
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		goto done;
	}
#if 0
	ret = regmap_update_bits(chip->regmap, TIMER_SETTING,BCD_TIMER_BITS,BCD_TIMER_3P6);
	if(ret){
		pr_err(TAG"regmap_update_bits fail in func %s line %d\n",__func__, __LINE__);
		goto done;
	}
#endif
done:
	return ret;
}

static const struct regmap_config tsu6721_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
};

#ifdef CONFIG_OF
static int muic_parse_dt(struct muic_data *chip)
{
	struct device *dev = chip->dev;
	struct device_node *np = dev->of_node;
	int ret;

	if(!np){
		pr_info(TAG"%s(),  parse device tree error", __func__);
		return -EINVAL;
	}
	chip->gpio_int_muic = of_get_named_gpio(np, "int-muic", 0);
	ret = gpio_request(chip->gpio_int_muic, "int-muic");
	if(ret){
		pr_err(TAG"int_muic request error\n");
		return ret;
	}

	chip->irq = gpio_to_irq(chip->gpio_int_muic);

	return 0;
}
#endif
static irqreturn_t tsu6721_muic_isr(int irq, void *dev)
{
	struct muic_data *chip = dev;

	schedule_delayed_work(&chip->irq_work,HZ/10);
	wake_unlock(&chip->muic_wake_lock);
	wake_lock_timeout(&chip->muic_wake_lock,5*HZ);
	return IRQ_WAKE_THREAD;
}

static void update_property(struct muic_data *chip)
{
	blocking_notifier_call_chain(&muic_notifier_list,
			chip->attach?USB_PORT_ATTACH : USB_PORT_DETACH,NULL);
	switch(chip->cable_type){
		case CABLE_TYPE_USB:
			pr_info(TAG"%s usb host %s\n",__func__,chip->attach?"ATTACH":"DETACH");
			blocking_notifier_call_chain(&muic_notifier_list,
					chip->attach?USB_HOST_ATTACH : USB_HOST_DETACH,NULL);
			break;
#if 0

		case CABLE_TYPE_OTG:
			pr_info(TAG"%s usb otg %s\n",__func__,chip->attach?"ATTACH":"DETACH");
#ifdef CONFIG_FACTORY_KERNEL
			if(chip->en_report_otg){
				blocking_notifier_call_chain(&muic_notifier_list,
						chip->attach?USB_OTG_ATTACH : USB_OTG_DETACH,NULL);
			}else{
				blocking_notifier_call_chain(&muic_notifier_list,
						chip->attach?USB_HOST_ATTACH : USB_HOST_DETACH,NULL);
			}
#else
			blocking_notifier_call_chain(&muic_notifier_list,
					chip->attach?USB_OTG_ATTACH : USB_OTG_DETACH,NULL);
#endif
			break;
#endif
		case CABLE_TYPE_UART:
			pr_info(TAG"%s uart %s\n",__func__,chip->attach?"ATTACH":"DETACH");
			blocking_notifier_call_chain(&muic_notifier_list,
					chip->attach?UART_ATTACH : UART_DETACH,NULL);
			break;
		case CABLE_TYPE_NON_STANDARD:
			pr_info(TAG"%s non-standard %s\n",__func__,chip->attach?"ATTACH":"DETACH");
			blocking_notifier_call_chain(&muic_notifier_list,
					chip->attach?NON_STANDARD_ATTACH : NON_STANDARD_DETACH,NULL);
			break;
		default:
			pr_info(TAG"%s muic event %s\n",__func__,chip->attach?"ATTACH":"DETACH");
			break;
	}
}

struct muic_data *g_chip=NULL;

#ifdef CONFIG_TYPEC_FUSB302
extern void check_cable_status_fusb302(void);
#else
void check_cable_status_fusb302(void) {}
#endif

bool check_cable_status(void)
{
	if(g_chip)
		update_property(g_chip);

	check_cable_status_fusb302();
	if(g_chip){
		if(g_chip->attach && g_chip->cable_type == CABLE_TYPE_UART){
			return true;
		}else{
			return false;
		}
	}
	return false;
}
#if 1
static int tsu6721_muic_mask_int(struct muic_data *chip,bool mask)
{
	int ret = 0;
	int ctrl=0;
	//mask interrupt
	ret = regmap_read(chip->regmap, CONTROL, &ctrl);
	if(ret){
		pr_err(TAG"regmap_read fail in func %s line %d\n",__func__, __LINE__);
		return -EIO;
	}
	if(mask){
		ret = regmap_write(chip->regmap, CONTROL, ctrl  | (1)); //mask interrupt
	}else{
		ret = regmap_write(chip->regmap, CONTROL, ctrl  & (~1)); //unmask interrupt
	}
	if(ret){
		pr_err(TAG"regmap_write fail in func %s line %d\n",__func__, __LINE__);
		return -EIO;
	}
	return ret;
}
#endif
static irqreturn_t tsu6721_muic_func(int irq, void *dev)
{
	struct muic_data *chip = dev;
	int int1=0,int2=0;
	int dev1=0;
	int dev3=0;
	int ret=0;

	mutex_lock(&chip->muic_mutex);
	//ret = tsu6721_muic_mask_int(chip,true);
	ret = regmap_read(chip->regmap,INTERRUPT1,&int1);
	ret = regmap_read(chip->regmap,INTERRUPT2,&int2);
	ret = regmap_read(chip->regmap,DEVICE_TYPE1,&dev1);
	ret = regmap_read(chip->regmap,DEVICE_TYPE3,&dev3);
	//ret = tsu6721_muic_mask_int(chip,false);
	//pr_info(TAG"%s int1 = 0x%2x,int2 = 0x%2x, dev1 = 0x%2x, dev3 = 0x%2x\n",__func__,int1,int2,dev1,dev3);
	if(int1 & (1<<0)){
		chip->attach = true;
	}
	if((int1 & (1<<1) )){
		chip->attach = false;
	}

	if(irq==0 && dev1)
		chip->attach = true;
	if(chip->attach){
		if(dev1 & (1<<3)){
			chip->cable_type = CABLE_TYPE_UART;
		}else if(dev1 & (1<<2)){
			chip->cable_type = CABLE_TYPE_USB;
		}else if(dev3 &(1<<2)){
			chip->cable_type = CABLE_TYPE_NON_STANDARD;
		}else{
			chip->cable_type = CABLE_TYPE_NONE;
		}
	}
	mutex_unlock(&chip->muic_mutex);
	update_property(chip);
	return IRQ_HANDLED;
}
static ssize_t attr_set_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	char rw[10];
	int reg,value,ret;
	struct muic_data *chip = dev_get_drvdata(dev);

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
static ssize_t attr_connect_type(struct device *dev,
		struct device_attribute *attr,char *buf)
{
	struct muic_data *chip = dev_get_drvdata(dev);
	strcpy(buf,"unknown\n");
	if(chip->cable_type == CABLE_TYPE_USB)
		strcpy(buf,"usb\n");
	if(chip->cable_type == CABLE_TYPE_OTG)
		strcpy(buf,"otg\n");
	return strlen(buf);
}
static ssize_t attr_dump_reg(struct device *dev,
		struct device_attribute *attr,char *buf)
{
	struct muic_data *chip = dev_get_drvdata(dev);

	int i=0,offset=0,ret =0;
	int val;
	static char buf_bak[20];
	for(i=0x0;i<=0x23;i++){
		ret=regmap_read(chip->regmap, i, &val);
		sprintf(buf_bak,"[0x%2x]=0x%02x\n",i,val);
		strcat(buf,buf_bak);
		offset +=strlen(buf_bak);
	}

	return strlen(buf);
}

static ssize_t attr_report_otg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int en_report=0;
	struct muic_data *chip = dev_get_drvdata(dev);

	sscanf(buf,"%d",&en_report);
	if(en_report){
		chip->en_report_otg = true;
	}else{
		chip->en_report_otg = false;
	}
	return size;
}

static struct device_attribute attributes[] = {
	__ATTR(reg_control, 0222, NULL, attr_set_reg),
	__ATTR(report_otg, 0200, NULL, attr_report_otg),
	__ATTR(connect_type, 0444, attr_connect_type, NULL),
	__ATTR(dump_reg, 0444, attr_dump_reg, NULL),
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
static int manual_switch_enable(struct muic_data *chip,bool on)
{
	return regmap_update_bits(chip->regmap, CONTROL,MANUAL_SWITCH_BITS,(!on)<<2);
}
int switch_to_apusb(struct muic_data *chip)
{
	return regmap_write(chip->regmap, MANUAL_SW1,0x25);
}
int switch_to_apuart(struct muic_data *chip)
{
	return regmap_write(chip->regmap, MANUAL_SW1,0x6d);
}
static int otg_event_notify(struct notifier_block *this, unsigned long code,
		void *unused)
{
	struct muic_data *chip;
	chip = container_of(this, struct muic_data, otg_notifier);
	switch(code){
		case USB_HOST_ATTACH:
			pr_info(TAG"%s usb host attach notify\n",__func__);
			break;
		case USB_HOST_DETACH:
			pr_info(TAG"%s usb host detach notify\n",__func__);
			break;
		case USB_OTG_ATTACH:
			pr_info(TAG"%s otg attach notify\n",__func__);
			tsu6721_muic_mask_int(chip,true);
			manual_switch_enable(chip,true);
			switch_to_apusb(chip);
			break;
		case USB_OTG_DETACH:
			pr_info(TAG"%s otg detach notify\n",__func__);

			tsu6721_muic_mask_int(chip,false);
			manual_switch_enable(chip,false);
			break;
	}
	return NOTIFY_DONE;
}
static void irq_report_work(struct work_struct *work)
{
	struct muic_data*chip =
		container_of(work, struct muic_data, irq_work.work);
	tsu6721_muic_func(1,chip);
}

static dev_t const muic_device_dev_t = MKDEV(MISC_MAJOR, 241);
static  int tsu6721_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct muic_data *chip;
	int ret = 0;

	dev_info(&client->dev, "%s : tsu6721 muic Driver Loading\n", __func__);

	if(!i2c_check_functionality(client->adapter,I2C_FUNC_I2C)){
		dev_err(&client->dev, "muic chip i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &client->dev;
	chip->client = client;
	mutex_init(&chip->muic_mutex);
	wake_lock_init(&chip->muic_wake_lock,WAKE_LOCK_SUSPEND,"muic_wake");
	chip->en_report_otg = true;
	ret = muic_parse_dt(chip);
	if(ret){
		pr_err(TAG"failed to parse device tree ret = %d\n", ret);
		goto err_parse_dt;
	}

	chip->regmap = devm_regmap_init_i2c(client, &tsu6721_regmap);
	if(IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&client->dev, TAG"failed to register regmap\n");
		goto err_parse_dt;
	}
	ret = tsu6721_chip_init(chip);
	if(ret){
		pr_err(TAG"chip init failed %d", ret);
		goto err_parse_dt;
	}
	INIT_DELAYED_WORK(&chip->irq_work,irq_report_work);
	g_chip = chip;
	tsu6721_muic_func(0,chip);
	ret = request_threaded_irq(chip->irq,tsu6721_muic_isr,NULL,IRQF_TRIGGER_FALLING,"tsu6721-muic",chip);
	i2c_set_clientdata(client, chip);
	device_init_wakeup(chip->dev,true);
	enable_irq_wake(chip->irq);

	chip->muic_class = class_create(THIS_MODULE,"muic_class");
	if(IS_ERR(chip->muic_class)){
		ret = PTR_ERR(chip->muic_class);
		goto err_parse_dt;
	}

	chip->muic_device = device_create(chip->muic_class,
			NULL,muic_device_dev_t,chip,"muic_device");
	if(IS_ERR(chip->muic_device)){
		ret = PTR_ERR(chip->muic_device);
		goto err_device_create;
	}

	ret = create_sysfs_files(chip->muic_device);
	if(ret < 0){
		goto err_create_sys;
	}
	chip->otg_notifier.notifier_call = otg_event_notify;
	register_muic_notifier(&chip->otg_notifier);
	return 0;
err_create_sys:
	device_destroy(chip->muic_class,muic_device_dev_t);
err_device_create:
	class_destroy(chip->muic_class);
err_parse_dt:
	kfree(chip);
	return ret;
}

static int  tsu6721_remove(struct i2c_client *client)
{
	int ctrl;
	int ret;
	struct muic_data *chip = i2c_get_clientdata(client);

	ret = regmap_read(chip->regmap, CONTROL, &ctrl);
	ret = regmap_write(chip->regmap, CONTROL, ctrl  |1); //mask interrupt
	mutex_destroy(&chip->muic_mutex);
	i2c_set_clientdata(client, NULL);
	device_destroy(chip->muic_class,muic_device_dev_t);
	class_destroy(chip->muic_class);
	kfree(chip);
	return 0;
}

static void tsu6721_shutdown(struct i2c_client * client)
{
#if 0
	struct muic_data *chip = i2c_get_clientdata(client);
	int ret, ctrl;
	/*reset chip*/
	ret = regmap_read(chip->regmap,RESET,&ctrl);
	ret = regmap_write(chip->regmap,RESET,ctrl|1);
	msleep(10);
	ret = regmap_write(chip->regmap,RESET,ctrl&(~1));
#endif
}

static int tsu6721_suspend(struct device *dev)
{
	return 0;
}

static int tsu6721_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops tsu6721_pm_ops = {
	.suspend	= tsu6721_suspend,
	.resume	= tsu6721_resume,
};

#ifdef CONFIG_OF
static struct of_device_id tsu6721_dt_id[] = {
	{ .compatible = "ti,tsu6721-muic" },
	{ }
};
#endif

static const struct i2c_device_id tsu6721_id[] = {
	{"tsu6721-muic", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tsu6721_id);
static struct i2c_driver tsu6721_muic_driver = {
	.driver = {
		.name = "tsu6721-muic",
		.owner = THIS_MODULE,
		.pm = &tsu6721_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tsu6721_dt_id),
#endif
	},
	.shutdown = tsu6721_shutdown,
	.probe = tsu6721_probe,
	.remove = tsu6721_remove,
	.id_table = tsu6721_id,
};

module_i2c_driver(tsu6721_muic_driver);

MODULE_DESCRIPTION("TI TSU6721 muic switch driver for MEIZU tsu6721 muic");
MODULE_AUTHOR("luochucheng@meizu.com");
MODULE_LICENSE("GPLV2");
