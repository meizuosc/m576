/*
 * OIS IC driver for MEIZU M86.
 * Author: QuDao, qudao@meizu.com
 *
 * Copyright (C) 2015 MEIZU
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
#include "OIS_head.h"
#include "OIS_meizu.h"

static struct ois_meizu *g_ois_meizu;
static _FACT_ADJ meizu_fadj;
static char otp_ois_data[OTP_OIS_SIZE];

extern int fimc_is_rear_power_clk(bool enable);

int mz_ois_i2c_read(struct mz_ois_i2c_data * const mz_i2c_data)
{
	struct i2c_client *client;
	u16 addr_size;
	const u8 *addr_buf;
	u16 rd_size;
	u8 *rd_buf;
	struct i2c_msg xfer[2];
	unsigned long flags;
	unsigned short ori_slavaddr;
	int ret;

	if (IS_ERR_OR_NULL(g_ois_meizu) ||
		IS_ERR_OR_NULL(g_ois_meizu->client)) {
		pr_err("%s(), invalid pointer:g_ois_meizu: %p, client:%p\n",
			__func__, g_ois_meizu,
			g_ois_meizu->client);
		return -EINVAL;
	}

	client = g_ois_meizu->client;

	ori_slavaddr = client->addr;
	spin_lock_irqsave(&g_ois_meizu->slock, flags);
	client->addr = mz_i2c_data->slavaddr;
	spin_unlock_irqrestore(&g_ois_meizu->slock, flags);

	addr_size = mz_i2c_data->addr_len;
	addr_buf = mz_i2c_data->addr_buf;
	/* read rd_size bytes from slave */
	rd_size = mz_i2c_data->data_len;
	rd_buf = mz_i2c_data->data_buf;

	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = addr_size;
	xfer[0].buf = (char *)addr_buf;

	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = rd_size;
	xfer[1].buf = rd_buf;

	ret = i2c_transfer(client->adapter, xfer, 2);

	spin_lock_irqsave(&g_ois_meizu->slock, flags);
	client->addr = ori_slavaddr;
	spin_unlock_irqrestore(&g_ois_meizu->slock, flags);

	if (ret == 2) {
		return 0;
	} else if (ret < 0) {
		pr_err("%s(), failed: %d\n", __func__, ret);
		return ret;
	} else {
		pr_err("%s(), failed with %d\n", __func__, ret);
		return -EIO;
	}

}

int mz_ois_i2c_write(struct mz_ois_i2c_data * const mz_i2c_data)
{
	struct i2c_client *client;
	const u32 max_retry = 5;
	u32 retries = 0;
	u16 data_len;
	const u8 *data_buf;
	unsigned long flags;
	unsigned short ori_slavaddr;
	int ret;

	if (IS_ERR_OR_NULL(g_ois_meizu) ||
		IS_ERR_OR_NULL(g_ois_meizu->client)) {
		pr_err("%s(), invalid pointer:g_ois_meizu: %p, client:%p\n",
			__func__, g_ois_meizu,
			g_ois_meizu->client);
		return -EINVAL;
	}

	client = g_ois_meizu->client;

	ori_slavaddr = client->addr;
	spin_lock_irqsave(&g_ois_meizu->slock, flags);
	client->addr = mz_i2c_data->slavaddr;
	spin_unlock_irqrestore(&g_ois_meizu->slock, flags);

	data_buf = mz_i2c_data->data_buf;
	data_len = mz_i2c_data->data_len;

	for (retries = 0; retries < max_retry; retries++) {
		ret = i2c_master_send(client, data_buf, data_len);
		if (likely(data_len == ret))
			break;
		usleep_range(1000, 1000);
	}

	spin_lock_irqsave(&g_ois_meizu->slock, flags);
	client->addr = ori_slavaddr;
	spin_unlock_irqrestore(&g_ois_meizu->slock, flags);

	if (unlikely(ret < 0)) {
		pr_err("%s: error %d, fail to write for %d times\n",
			__func__, ret, retries);
		return ret;
	}

	return 0;
}

int mz_ois_power_clk(bool enable)
{
	struct i2c_client *client;
	struct ois_meizu *ois_meizu = NULL;
	int ret;

	if (IS_ERR_OR_NULL(g_ois_meizu) ||
		IS_ERR_OR_NULL(g_ois_meizu->client)) {
		pr_err("%s(), invalid pointer:g_ois_meizu: %p, client:%p\n",
			__func__, g_ois_meizu,
			g_ois_meizu->client);
		return -EINVAL;
	}

	pr_info("%s(), ++++, enable %d\n", __func__, enable);
	client = g_ois_meizu->client;
	ois_meizu = i2c_get_clientdata(client);

	if (enable == ois_meizu->poweron) {
		pr_warn("%s(), OIS power status is already %d\n",
			__func__, ois_meizu->poweron);
		return 0;
	}

	ret = fimc_is_rear_power_clk(enable);
	if (ret) {
		pr_err("%s(), power clk %d failed, ret:%d\n",
			__func__, enable, ret);
		goto flag;
	}

	pr_info("%s(), power %d sucess\n",
		__func__, enable);
	ois_meizu->poweron = enable;

flag:
	return ret;
}

int mz_ois_get_fadj_data(_FACT_ADJ **p_fact_adj)
{
	if (!g_ois_meizu->fadj_ready) {
		pr_err("%s(), data not ready\n", __func__);
		return -ENXIO;
	}

	*p_fact_adj = &meizu_fadj;
	return 0;
}

static int mz_ois_prepare_fadj_data(void)
{
	struct meizu_otp *pmeizu_otp = NULL;
	char * const buf = otp_ois_data;
	int ois_start;
	int ois_size;

	if (g_ois_meizu->fadj_ready) {
		pr_info("%s(), fadj data has already been prepared\n",
			__func__);
		return 0;
	}

	fimc_is_eeprom_get_cal_buf(&pmeizu_otp);
	if (IS_ERR_OR_NULL(pmeizu_otp)) {
		pr_err("%s(), pmeizu_otp is invalid:%p\n",
			__func__, pmeizu_otp);
		return -EIO;
	}

	ois_start = pmeizu_otp->ois_start;
	ois_size = pmeizu_otp->ois_size;
	if (ois_size != OTP_OIS_SIZE || ois_start < 0) {
		pr_err("%s(), err!para invalid, ois_size:0x%x, ois_start:0x%x\n,",
			__func__, ois_size, ois_start);
		return -EINVAL;
	}

	memcpy(buf, &(pmeizu_otp->data[ois_start]), OTP_OIS_SIZE);

	meizu_fadj.gl_CURDAT = buf[0x4] | (buf[0x5] << 8);
	meizu_fadj.gl_HALOFS_X = buf[0x6] | (buf[0x7] << 8);
	meizu_fadj.gl_HALOFS_Y= buf[0x8] | (buf[0x9] << 8);
	meizu_fadj.gl_HX_OFS = buf[0xa] | (buf[0xb] << 8);
	meizu_fadj.gl_HY_OFS = buf[0xc] | (buf[0xd] << 8);
	meizu_fadj.gl_PSTXOF = buf[0xe] | (buf[0xf] << 8);
	meizu_fadj.gl_PSTYOF = buf[0x10] | (buf[0x11] << 8);
	meizu_fadj.gl_GX_OFS = buf[0x12] | (buf[0x13] << 8);
	meizu_fadj.gl_GY_OFS = buf[0x14] | (buf[0x15] << 8);
	meizu_fadj.gl_KgxHG = buf[0x16] | (buf[0x17] << 8);
	meizu_fadj.gl_KgyHG = buf[0x18] | (buf[0x19] << 8);
	meizu_fadj.gl_KGXG = buf[0x1a] | (buf[0x1b] << 8);
	meizu_fadj.gl_KGYG = buf[0x1c] | (buf[0x1d] << 8);

	meizu_fadj.gl_SFTHAL_X = buf[0x1e] | (buf[0x1f] << 8);
	meizu_fadj.gl_SFTHAL_Y = buf[0x20] | (buf[0x21] << 8);
	meizu_fadj.gl_TMP_X_ = buf[0x22] | (buf[0x23] << 8);
	meizu_fadj.gl_TMP_Y_ = buf[0x24] | (buf[0x25] << 8);
	meizu_fadj.gl_KgxH0 = buf[0x26] | (buf[0x27] << 8);
	meizu_fadj.gl_KgyH0 = buf[0x28] | (buf[0x29] << 8);

	g_ois_meizu->fadj_ready = true;
	return 0;
}

int mz_ois_get_module_id(void)
{
	int ret;

	ret = mz_get_module_id(0);
	if (ret < 0) {
		pr_err("%s(), get module id failed:%d\n",
			__func__, ret);
	}

	return ret;
}

#ifdef CONFIG_OF
int mz_ois_parse_dt(struct i2c_client *client)
{
	struct ois_meizu *ois_meizu = NULL;
	struct device_node *np = client->dev.of_node;
	int gpio = -1;
	char *pin_name = NULL;

	ois_meizu = i2c_get_clientdata(client);

	if (IS_ERR_OR_NULL(ois_meizu)) {
		pr_err("%s(), err!ois_meizu is not ready:0x%p\n",
			__func__, ois_meizu);
		return -EINVAL;
	}

	pin_name = "gpio_vcm_en";
	if (of_property_read_bool(np, pin_name)) {
		gpio = of_get_named_gpio_flags(np, pin_name, 0, NULL);
		pr_info("%s(), got gpio %s:%d\n", __func__, pin_name, gpio);
		ois_meizu->gpio_vcm_en = gpio;
	} else {
		pr_err("%s(), Err! can not get gpio %s\n", __func__, pin_name);
		return -ENODEV;
	}

	return 0;
}
#else
int mz_ois_parse_dt(struct i2c_client *client)
{
	return -EINVAL;
}
#endif

static ssize_t ois_test_store(struct device *dev,
				       struct device_attribute *devAttr,
				       const char *buf, size_t size)
{
	switch (buf[0]) {
	case '0':
		mz_ois_power_clk(false);
		break;

	case '1':
		mz_ois_prepare_fadj_data();
		mz_ois_power_clk(true);
		pr_info("%s(), read per 0 is 0x%x\n",
			__func__, I2C_OIS_per__read(0));
		main();
		break;

	case '2':
	{
		int val;
		int ret;
		val = 0x0;
		I2C_OIS_F0123_wr_( 0x90,0x00, val );
		msleep(1000);
		ret = I2C_OIS_F0123__rd();
		pr_info("%s(), set DAC val 0x%x, result is:0x%x\n", __func__, val, ret);

	}
		break;

	case '3':
	{
		int val;
		int ret;
		val = 0x3ff;
		I2C_OIS_F0123_wr_( 0x90,0x00, val );
		msleep(1000);
		ret = I2C_OIS_F0123__rd();
		pr_info("%s(), set DAC val 0x%x, result is:0x%x\n", __func__, val, ret);

	}
		break;

	case '4':
	{
		int ret;
		pr_info("%s(), turn on OIS\n", __func__);
		ret = I2C_OIS_mem__read( _M_EQCTL );
		pr_info("%s(), ori mem 0x%x = 0x%x\n",
			__func__, _M_EQCTL, ret);

		ret |= ((1<<8) | (1 << 0));
		I2C_OIS_mem_write( _M_EQCTL, ret);

		ret = I2C_OIS_mem__read( _M_EQCTL );
		pr_info("%s(), after mem 0x%x = 0x%x\n",
			__func__, _M_EQCTL, ret);
	}
		break;

	case '5':
	{
		int ret;
		pr_info("%s(), turn off OIS\n", __func__);
		ret = I2C_OIS_mem__read( _M_EQCTL );
		pr_info("%s(), ori mem 0x%x = 0x%x\n",
			__func__, _M_EQCTL, ret);

		ret &= (~((1<<8) | (1 << 0)));
		I2C_OIS_mem_write( _M_EQCTL, ret);

		ret = I2C_OIS_mem__read( _M_EQCTL );
		pr_info("%s(), mem 0x%x = 0x%x\n",
			__func__, _M_EQCTL, ret);
	}
		break;

	case '6':
	{
		int ret;
		pr_info("%s(), turn on Servo\n", __func__);
		ret = I2C_OIS_mem__read( _M_EQCTL );
		pr_info("%s(), ori mem 0x%x = 0x%x\n",
			__func__, _M_EQCTL, ret);

		ret |= ((3<<10) | (3 << 2));
		I2C_OIS_mem_write( _M_EQCTL, ret);

		ret = I2C_OIS_mem__read( _M_EQCTL );
		pr_info("%s(), after mem 0x%x = 0x%x\n",
			__func__, _M_EQCTL, ret);
	}
		break;

	case '7':
	{
		int ret;
		pr_info("%s(), turn off Servo\n", __func__);
		ret = I2C_OIS_mem__read( _M_EQCTL );
		pr_info("%s(), ori mem 0x%x = 0x%x\n",
			__func__, _M_EQCTL, ret);

		ret &= (~((3<<10) | (3 << 2)));
		I2C_OIS_mem_write( _M_EQCTL, ret);

		ret = I2C_OIS_mem__read( _M_EQCTL );
		pr_info("%s(), mem 0x%x = 0x%x\n",
			__func__, _M_EQCTL, ret);
	}
		break;

	case '8':
	{
		int i = 0;
		int ret;
		for (i = 0x06; i <= 0xf7; i++) {
			ret = I2C_OIS_mem__read( i );
			pr_info("%s(), mem 0x%x = 0x%x\n",
				__func__, i, ret);
		}
	}
		break;

	case '9':
	{
		int ret = 0;
		char data[1];
		data[0] = 0x18;
		ret = RD_I2C(SENSOR_SLAV_ADDR, 1, data);
		pr_info("%s(), mem 0x%x = 0x%x\n",
			__func__, data[0], ret);
	}
		break;

	case 'a':
	{
		mz_func_adj_angle_limit(50);
	}
		break;


	default:
		pr_err("%s: unkown input %c\n", __func__, buf[0]);
		return -EINVAL;
	}

	return size;
}
static DEVICE_ATTR(ois_test, 0220, NULL, ois_test_store);

static int mz_ois_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct ois_meizu *ois_meizu = NULL;

	dev_info(&client->dev, "%s(), addr:0x%x, name:%s\n",
		__func__, client->addr, client->name);

	ois_meizu = devm_kzalloc(&client->dev, sizeof(struct ois_meizu), GFP_KERNEL);

	ois_meizu->client = client;
	ois_meizu->fadj_ready = false;
	spin_lock_init(&ois_meizu->slock);
	i2c_set_clientdata(client, ois_meizu);

	ret = mz_ois_parse_dt(client);

	ret = device_create_file(&client->dev,
				 &dev_attr_ois_test);

	g_ois_meizu = ois_meizu;
	return ret;
}

static int mz_ois_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id mz_ois_id[] = {
	{OIS_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, mz_ois_id);

#ifdef CONFIG_OF
static struct of_device_id mz_ois_of_match_table[] = {
	{
		.compatible = "meizu,rhom-ois-bu63165",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mz_ois_of_match_table);
#else
#define mz_ois_of_match_table NULL
#endif

static struct i2c_driver mz_ois_i2c_driver = {
	.driver = {
		   .name = OIS_NAME,
		   .owner = THIS_MODULE,
		   .pm = NULL,
			.of_match_table = mz_ois_of_match_table,
		   },
	.probe = mz_ois_probe,
	.remove = mz_ois_remove,
	.id_table = mz_ois_id,
};

module_i2c_driver(mz_ois_i2c_driver);

MODULE_DESCRIPTION("OIS IC driver for MEIZU M86");
MODULE_AUTHOR("QuDao <qudao@meizu.com>");
MODULE_LICENSE("GPL v2");
