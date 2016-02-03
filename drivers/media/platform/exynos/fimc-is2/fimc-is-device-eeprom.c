/*
 * OTP driver for MEIZU M86.
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
#include <linux/meizu-sys.h>
#include "fimc-is-device-eeprom.h"
#include "fimc-is-core.h"
#include "meizu_camera_special.h"

/* i2c controller state */
enum s3c24xx_i2c_state {
	STATE_IDLE,
	STATE_START,
	STATE_READ,
	STATE_WRITE,
	STATE_STOP
};

struct s3c24xx_i2c {
	struct list_head	node;
	wait_queue_head_t	wait;
	unsigned int            quirks;
	unsigned int		need_hw_init;
	unsigned int		suspended:1;

	struct i2c_msg		*msg;
	unsigned int		msg_num;
	unsigned int		msg_idx;
	unsigned int		msg_ptr;

	unsigned int		tx_setup;
	unsigned int		irq;

	enum s3c24xx_i2c_state	state;
	unsigned long		clkrate;

	void __iomem		*regs;
	struct clk		*rate_clk;
	struct clk		*clk;
	struct device		*dev;
	struct i2c_adapter	adap;

	struct s3c2410_platform_i2c	*pdata;
	int			gpios[2];
	struct pinctrl          *pctrl;
};

static u32 eeprom_status = FIMC_IS_EEPROM_STATE_INIT;
static struct meizu_otp meizu_otp;
extern int fimc_is_rear_power_clk(bool enable);

static int fimc_is_eeprom_power_on(struct fimc_is_device_sensor *device)
{
	int ret;
	ret = fimc_is_rear_power_clk(true);
	if (ret) {
		dev_err(&device->pdev->dev, "%s() failed:%d\n",
			__func__, ret);
	}
	return ret;
}

static int fimc_is_eeprom_power_off(struct fimc_is_device_sensor *device)
{
	int ret;
	ret = fimc_is_rear_power_clk(false);
	if (ret) {
		dev_err(&device->pdev->dev, "%s() failed:%d\n",
			__func__, ret);
	}
	return ret;
}

static int fimc_is_i2c_read(struct i2c_client *client, void *buf, u32 addr, size_t size)
{
	const u32 addr_size = 2, max_retry = 5;
	u8 addr_buf[addr_size];
	int retries = max_retry;
	int ret = 0;

	dev_dbg(&client->dev, "%s +++++\n", __func__);

	if (!client) {
		pr_info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	/* Send addr */
	addr_buf[0] = ((u16)addr) >> 8;
	addr_buf[1] = (u8)addr;

	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_send(client, addr_buf, addr_size);
		if (likely(addr_size == ret))
			break;
		usleep_range(1000, 1000);
	}
	if (unlikely(retries < max_retry))
		/* logging*/;

	if (unlikely(ret < 0)) {
		pr_err("%s: error %d, fail to write 0x%04X\n", __func__, ret, addr);
		return ret;
	}

	/* Receive data */
	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_recv(client, buf, size);
		if (likely(ret == size))
			break;
		usleep_range(1000, 1000);
	}

	if (unlikely(retries < max_retry))
		/* logging*/;

	if (unlikely(ret < 0)) {
		pr_err("%s: error %d, fail to read 0x%04X\n", __func__, ret, addr);
		return ret;
	}

	return 0;
}

#ifdef SAVE_CCM_INFO
static int meizu_write_file(const void *data, unsigned long size)
{
     int ret = 0;
     struct file *fp = NULL;
     mm_segment_t old_fs;
     char *file_name = BCAM_CALI_FILE_PATH;

     old_fs = get_fs();
     set_fs(KERNEL_DS);

     pr_info("%s(), open file %s\n", __func__, file_name);

     fp = filp_open(file_name, O_WRONLY|O_CREAT, 0644);
     if (IS_ERR(fp)) {
          ret = PTR_ERR(fp);
          pr_err("%s(): open file error(%d)\n", __func__, ret);
          goto exit;
     }

     pr_info("%s(), write to %s\n", __func__, file_name);
     if (fp->f_mode & FMODE_WRITE) {
          ret = fp->f_op->write(fp, (const char *)data,
                    size, &fp->f_pos);
          if (ret < 0)
               pr_err("%s:write file %s error(%d)\n", __func__, file_name, ret);
     }

exit:
     if (!IS_ERR(fp))
          filp_close(fp, NULL);

     set_fs(old_fs);

     return ret;
}

int mz_save_rear_cali(void)
{
	int ret = 0;
	char * const calibration_buf = meizu_otp.data;
	int calibration_buf_size = sizeof(meizu_otp.data);

	if (eeprom_status == FIMC_IS_EEPROM_STATE_SAVED) {
		return 0;
	}

	if (eeprom_status == FIMC_IS_EEPROM_STATE_READONE) {
		ret = meizu_write_file(calibration_buf, calibration_buf_size);
		if (ret > 0) {
			eeprom_status = FIMC_IS_EEPROM_STATE_SAVED;
		}
	} else {
		pr_err("%s(), err eeprom_status %d\n",
			__func__, eeprom_status);
		return -EINVAL;
	}

	return ret;
}

#else
int mz_save_rear_cali(void)
{
	return 0;
}
#endif

static int fimc_is_eeprom_read(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	int i = 0;
	u32 checksumofidx = 0;
	u32 info_start = 0;
	u32 info_size = 0;
	u32 awb_start = 0;
	u32 awb_size = 0;
	u32 af_start = 0;
	u32 af_size = 0;
	u32 lsc_start = 0;
	u32 lsc_size = 0;
	u32 ois_start = 0;
	u32 ois_size = 0;
	u32 pdaf_start = 0;
	u32 pdaf_size = 0;

	u32 checksumidx = 0;
	u32 checksuminfo = 0;
	u32 checksumawb = 0;
	u32 checksumaf = 0;
	u32 checksumlsc = 0;
	#ifdef CONFIG_MZ_ROHM_OIS
	u32 checksumois = 0;
	#endif
	u32 checksumpdaf = 0;

	u32 manufacture_id = 0;
	u32 vcm_driver_ic_id;
#ifdef EEPROM_CHECKSUM_DEBUG
	u32 add_base;
	//infro section
	u32 module_version = 0;
	u32 year = 0;
	u32 month = 0;
	u32 day = 0;

	//awb section
	u32 rn_coeff = 0;
	u32 rn_const = 0;
	u32 bn_coeff = 0;
	u32 bn_const = 0;

	//af section
	u32 af_inf_pos_typ = 0;
	u32 af_inf_pos_worst = 0;
	u32 af_macro_pos_typ = 0;
	u32 af_macro_pos_worst = 0;
	u32 af_default = 0;
	u32 af_mid_typ = 0;
	u32 af_mid_worst = 0;
	u32 vcm_bottom = 0;
	u32 vcm_top = 0;
	u32 vcm_bias = 0;
	u32 vcm_offset = 0;

	//lsc section
	u32 lsc_ver = 0;
	u32 lsc_length = 0;
	u32 lsc_ori = 0;
	u32 lsc_i0 = 0;
	u32 lsc_j0 = 0;
	u32 lsc_scale = 0;
	u32 lsc_acoff = 0;
	u32 lsc_bcoff = 0;
#endif

	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_core *core;
	struct i2c_client			*client = NULL;
	struct mz_module_info *module_info;
	char * const calibration_buf = meizu_otp.data;
	char *module_name = NULL;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);
	BUG_ON(!device->private_data);

	core = device->private_data;
	pdata = device->pdata;
	module_info = &device->mz_modu_info;

	BUG_ON(!core->eeprom_client0);

	pr_info("%s: start read \n", __func__);

	ret = fimc_is_i2c_read(core->eeprom_client0, &calibration_buf[0], 0x00, EEPROM_SECTION1_SIZE);
	if (unlikely(ret)) {
		pr_err("%s(), failed to fimc_is_i2c_read (%d)\n", __func__, ret);
		eeprom_status = FIMC_IS_EEPROM_STATE_I2CFAIL;
		/*
		* Assign default value.
		*/
		client = core->eeprom_client0;
		ret = -EINVAL;
		goto exit;
	} else {
		client = core->eeprom_client0;
	}

	checksumofidx 	= ( calibration_buf[0] ) | ( calibration_buf[1] << 8 ) | ( calibration_buf[2] << 16) | ( calibration_buf[3] << 24) ;
	info_start 		= ( calibration_buf[4] ) | ( calibration_buf[5] << 8 ) | ( calibration_buf[6] << 16) | ( calibration_buf[7] << 24) ;
	info_size 		= ( calibration_buf[8] ) | ( calibration_buf[9] << 8 ) | ( calibration_buf[10] << 16) | ( calibration_buf[11] << 24) ;
	awb_start 		= ( calibration_buf[12] ) | ( calibration_buf[13] << 8 ) | ( calibration_buf[14] << 16) | ( calibration_buf[15] << 24) ;
	awb_size 		= ( calibration_buf[16] ) | ( calibration_buf[17] << 8 ) | ( calibration_buf[18] << 16) | ( calibration_buf[19] << 24) ;
	af_start 		= ( calibration_buf[20] ) | ( calibration_buf[21] << 8 ) | ( calibration_buf[22] << 16) | ( calibration_buf[23] << 24) ;
	af_size 		= ( calibration_buf[24] ) | ( calibration_buf[25] << 8 ) | ( calibration_buf[26] << 16) | ( calibration_buf[27] << 24) ;
	lsc_start 		= ( calibration_buf[28] ) | ( calibration_buf[29] << 8 ) | ( calibration_buf[30] << 16) | ( calibration_buf[31] << 24) ;
	lsc_size 		= ( calibration_buf[32] ) | ( calibration_buf[33] << 8 ) | ( calibration_buf[34] << 16) | ( calibration_buf[35] << 24) ;
	ois_start = ( calibration_buf[36] ) | ( calibration_buf[37] << 8 ) | ( calibration_buf[38] << 16) | ( calibration_buf[39] << 24) ;
	ois_size = ( calibration_buf[40] ) | ( calibration_buf[41] << 8 ) | ( calibration_buf[42] << 16) | ( calibration_buf[43] << 24) ;
	pdaf_start = ( calibration_buf[44] ) | ( calibration_buf[45] << 8 ) | ( calibration_buf[46] << 16) | ( calibration_buf[47] << 24) ;
	pdaf_size = ( calibration_buf[48] ) | ( calibration_buf[49] << 8 ) | ( calibration_buf[50] << 16) | ( calibration_buf[51] << 24) ;

#ifdef EEPROM_CHECKSUM_DEBUG
	pr_info("EEPROM Read checksumofidx 0x%x \n",checksumofidx);
	pr_info("EEPROM Read info_start 0x%x \n",info_start);
	pr_info("EEPROM Read info_size 0x%x \n",info_size);
	pr_info("EEPROM Read awb_start 0x%x \n",awb_start);
	pr_info("EEPROM Read awb_size 0x%x \n",awb_size);
	pr_info("EEPROM Read af_start 0x%x \n",af_start);
	pr_info("EEPROM Read af_size 0x%x \n",af_size);
	pr_info("EEPROM Read lsc_start 0x%x \n",lsc_start);
	pr_info("EEPROM Read lsc_size 0x%x \n",lsc_size);
	pr_info("EEPROM Read ois_start 0x%x \n",ois_start);
	pr_info("EEPROM Read ois_size 0x%x \n",ois_size);
	pr_info("EEPROM Read pdaf_start 0x%x \n",pdaf_start);
	pr_info("EEPROM Read pdaf_size 0x%x \n",pdaf_size);
#endif

	for(i = EEPROM_CHECKSUM_SIZE; i < EEPROM_SECTION1_SIZE; i++) {
		checksumidx += calibration_buf[i];
	}

	if(checksumofidx != checksumidx) {
		dev_err(&client->dev, "%s(), EEPROM section index check sum fail",
			__func__);
		eeprom_status = FIMC_IS_EEPROM_STATE_CHECKSUM_FAIL;
		ret = -EINVAL;
		goto exit;
	}

	checksumofidx = 0;
	ret = fimc_is_i2c_read(client, &calibration_buf[info_start], info_start, info_size);
	if (unlikely(ret)) {
		dev_err(&client->dev, "failed to info fimc_is_i2c_read (%d)\n", ret);
		eeprom_status = FIMC_IS_EEPROM_STATE_I2CFAIL;
		ret = -EINVAL;
		goto exit;
	}

	checksumofidx 		= ( calibration_buf[info_start+0] ) | ( calibration_buf[info_start+1] << 8 ) | ( calibration_buf[info_start+2] << 16) | ( calibration_buf[info_start+3] << 24) ;

	manufacture_id 		= ( calibration_buf[info_start+20] ) ;
	if (2 == manufacture_id) {
		module_info->vendor_id = REAR_SHARP_MODULE;
		module_name = "SHARP";
	} else if (3 == manufacture_id) {
		module_info->vendor_id = REAR_PRIMAX_MODULE;
		module_name = "PRIMAX";
	} else {
		module_info->vendor_id = UNDEFINED_MODULE;
		module_name = "UNKNOWN";
		dev_err(&client->dev, "%s(), unkown module id:%d\n",
			__func__, manufacture_id);
	}

	vcm_driver_ic_id = ( calibration_buf[info_start+32] ) ;
	if (VCM_DRIVER_IC_BU63165 == vcm_driver_ic_id) {
		module_info->driver_ic_id = VCM_DRIVER_IC_BU63165;
		pr_warn("%s(), !!!WARNING, OBSOLETE VCM DRIVER IC FOUND!!!\n",
			__func__);
	} else if (VCM_DRIVER_IC_LC898212 == vcm_driver_ic_id) {
		module_info->driver_ic_id = VCM_DRIVER_IC_LC898212;
		pr_info("%s(), NEW VCM DRIVER IC FOUND\n", __func__);
	} else {
		dev_err(&client->dev, "%s(), unkown vcm driver ic id:%d\n",
			__func__, vcm_driver_ic_id);
	}

#ifdef EEPROM_CHECKSUM_DEBUG
	module_version		= ( calibration_buf[info_start+21] ) ;
	year 				= ( calibration_buf[info_start+38] ) ;
	month 				= ( calibration_buf[info_start+39] ) ;
	day 				= ( calibration_buf[info_start+40] ) ;

	pr_info("EEPROM Read checksumofidx 0x%x \n",checksumofidx);
	pr_info("EEPROM Read manufacture_id 0x%x \n",manufacture_id);
	pr_info("EEPROM Read module_version 0x%x \n",module_version);
	pr_info("EEPROM Read year 0x%x \n",year);
	pr_info("EEPROM Read month 0x%x \n",month);
	pr_info("EEPROM Read day 0x%x \n",day);
#endif

	for(i = EEPROM_CHECKSUM_SIZE; i < info_size; i++) {
		checksuminfo += calibration_buf[i+info_start];
	}

	if(checksumofidx != checksuminfo) {
		dev_err(&client->dev, "EEPROM section information check sum fail");
		eeprom_status = FIMC_IS_EEPROM_STATE_CHECKSUM_FAIL;
		ret = -EINVAL;
		goto exit;
	}

	checksumofidx = 0;
	ret = fimc_is_i2c_read(client, &calibration_buf[0+awb_start], awb_start, awb_size);
	if (unlikely(ret)) {
		dev_err(&client->dev, "failed to awb fimc_is_i2c_read (%d)\n", ret);
		eeprom_status = FIMC_IS_EEPROM_STATE_I2CFAIL;
		ret = -EINVAL;
		goto exit;
	}

	checksumofidx	= ( calibration_buf[0+awb_start] ) | ( calibration_buf[1+awb_start] << 8 ) | ( calibration_buf[2+awb_start] << 16) | ( calibration_buf[3+awb_start] << 24) ;
#ifdef EEPROM_CHECKSUM_DEBUG
	rn_coeff		= ( calibration_buf[4+awb_start] ) | ( calibration_buf[5+awb_start] << 8 ) | ( calibration_buf[6+awb_start] << 16) | ( calibration_buf[7+awb_start] << 24) ;
	rn_const		= ( calibration_buf[8+awb_start] ) | ( calibration_buf[9+awb_start] << 8 ) | ( calibration_buf[10+awb_start] << 16) | ( calibration_buf[11+awb_start] << 24) ;
	bn_coeff		= ( calibration_buf[12+awb_start] ) | ( calibration_buf[13+awb_start] << 8 ) | ( calibration_buf[14+awb_start] << 16) | ( calibration_buf[15+awb_start] << 24) ;
	bn_const		= ( calibration_buf[16+awb_start] ) | ( calibration_buf[17+awb_start] << 8 ) | ( calibration_buf[18+awb_start] << 16) | ( calibration_buf[19+awb_start] << 24) ;

	pr_info("EEPROM Read checksumofidx 0x%x \n",checksumofidx);
	pr_info("EEPROM Read rn_coeff 0x%x \n",rn_coeff);
	pr_info("EEPROM Read rn_const 0x%x \n",rn_const);
	pr_info("EEPROM Read bn_coeff 0x%x \n",bn_coeff);
	pr_info("EEPROM Read bn_const 0x%x \n",bn_const);
#endif

	for(i=EEPROM_CHECKSUM_SIZE; i<awb_size; i++) {
		checksumawb += calibration_buf[i+awb_start];
	}

	if(checksumofidx != checksumawb) {
		dev_err(&client->dev, "EEPROM section awb check sum fail");
		#ifndef IGNORE_CHECKSUM
		eeprom_status = FIMC_IS_EEPROM_STATE_CHECKSUM_FAIL;
		ret = -EINVAL;
		goto exit;
		#endif
	}

	checksumofidx = 0;
	ret = fimc_is_i2c_read(client, &calibration_buf[0 + af_start], af_start, af_size);
	if (unlikely(ret)) {
		dev_err(&client->dev, "failed to af fimc_is_i2c_read (%d)\n", ret);
		eeprom_status = FIMC_IS_EEPROM_STATE_I2CFAIL;
		ret = -EINVAL;
		goto exit;
	}

	checksumofidx			= ( calibration_buf[0+ af_start] ) | ( calibration_buf[1+ af_start] << 8 ) | ( calibration_buf[2+ af_start] << 16) | ( calibration_buf[3+ af_start] << 24) ;
#ifdef EEPROM_CHECKSUM_DEBUG
	af_inf_pos_typ			= ( calibration_buf[4+ af_start] ) | ( calibration_buf[5+ af_start] << 8 ) | ( calibration_buf[6+ af_start] << 16) | ( calibration_buf[7+ af_start] << 24) ;
	af_inf_pos_worst		= ( calibration_buf[8+ af_start] ) | ( calibration_buf[9+ af_start] << 8 ) | ( calibration_buf[10+ af_start] << 16) | ( calibration_buf[11+ af_start] << 24) ;
	af_macro_pos_typ		= ( calibration_buf[12+ af_start] ) | ( calibration_buf[13+ af_start] << 8 ) | ( calibration_buf[14+ af_start] << 16) | ( calibration_buf[15+ af_start] << 24) ;
	af_macro_pos_worst 		= ( calibration_buf[16+ af_start] ) | ( calibration_buf[17+ af_start] << 8 ) | ( calibration_buf[18+ af_start] << 16) | ( calibration_buf[19+ af_start] << 24) ;
	af_default				= ( calibration_buf[20+ af_start] ) | ( calibration_buf[21+ af_start] << 8 ) | ( calibration_buf[22+ af_start] << 16) | ( calibration_buf[23+ af_start] << 24) ;
	af_mid_typ = ( calibration_buf[24+ af_start] ) | ( calibration_buf[25+ af_start] << 8 ) | ( calibration_buf[26+ af_start] << 16) | ( calibration_buf[27+ af_start] << 24) ;
	af_mid_worst = ( calibration_buf[28+ af_start] ) | ( calibration_buf[29+ af_start] << 8 ) | ( calibration_buf[30+ af_start] << 16) | ( calibration_buf[31+ af_start] << 24) ;
	add_base = af_start + 40;
	vcm_bottom	 = otp_assin4(calibration_buf, add_base);
	add_base = af_start + 44;
	vcm_top = otp_assin4(calibration_buf, add_base);
	add_base = af_start + 48;
	vcm_bias = otp_assin4(calibration_buf, add_base);
	add_base = af_start + 52;
	vcm_offset = otp_assin4(calibration_buf, add_base);

	pr_info("EEPROM Read af_inf_pos_typ 0x%x \n",af_inf_pos_typ);
	pr_info("EEPROM Read af_inf_pos_worst 0x%x \n",af_inf_pos_worst);
	pr_info("EEPROM Read af_macro_pos_typ 0x%x \n",af_macro_pos_typ);
	pr_info("EEPROM Read af_macro_pos_worst 0x%x \n",af_macro_pos_worst);
	pr_info("EEPROM Read af_default 0x%x \n",af_default);
	pr_info("EEPROM Read af_mid_typ 0x%x \n",af_mid_typ);
	pr_info("EEPROM Read vcm_bottom 0x%x \n",vcm_bottom);
	pr_info("EEPROM Read vcm_top 0x%x \n",vcm_top);
	pr_info("EEPROM Read vcm_bias 0x%x \n",vcm_bias);
	pr_info("EEPROM Read vcm_offset 0x%x \n",vcm_offset);
#endif

	for(i = EEPROM_CHECKSUM_SIZE; i < af_size; i++) {
		checksumaf += calibration_buf[i+ af_start];
	}

	if(checksumofidx != checksumaf) {
		dev_err(&client->dev, "EEPROM section af check sum fail");
		eeprom_status = FIMC_IS_EEPROM_STATE_CHECKSUM_FAIL;
		ret = -EINVAL;
		goto exit;
	}

	checksumofidx = 0;
	ret = fimc_is_i2c_read(client, &calibration_buf[0 + lsc_start], lsc_start, lsc_size);
	if (unlikely(ret)) {
		dev_err(&client->dev, "failed to lsc fimc_is_i2c_read (%d)\n", ret);
		eeprom_status = FIMC_IS_EEPROM_STATE_I2CFAIL;
		ret = -EINVAL;
		goto exit;
	}

	checksumofidx		= ( calibration_buf[0 + lsc_start] ) | ( calibration_buf[1+ lsc_start] << 8 ) | ( calibration_buf[2+ lsc_start] << 16) | ( calibration_buf[3+ lsc_start] << 24) ;
#ifdef EEPROM_CHECKSUM_DEBUG
	lsc_ver				= ( calibration_buf[4+ lsc_start] ) | ( calibration_buf[5+ lsc_start] << 8 );
	lsc_length			= ( calibration_buf[6+ lsc_start] ) | ( calibration_buf[7+ lsc_start] << 8 );
	lsc_ori				= ( calibration_buf[8+ lsc_start] ) | ( calibration_buf[9+ lsc_start] << 8 );
	lsc_i0				= ( calibration_buf[10+ lsc_start] ) | ( calibration_buf[11+ lsc_start] << 8 );
	lsc_j0				= ( calibration_buf[12+ lsc_start] ) | ( calibration_buf[13+ lsc_start] << 8 );
	lsc_scale			= ( calibration_buf[14+ lsc_start] ) | ( calibration_buf[25+ lsc_start] << 8 ) | ( calibration_buf[26+ lsc_start] << 16) | ( calibration_buf[27+ lsc_start] << 24) ;
	add_base = lsc_start + 16;
	lsc_acoff = otp_assin4(calibration_buf, add_base);
	add_base = lsc_start + 20;
	lsc_bcoff = otp_assin4(calibration_buf, add_base);

	pr_info("EEPROM Read lsc_ver 0x%x \n",lsc_ver);
	pr_info("EEPROM Read lsc_length 0x%x \n",lsc_length);
	pr_info("EEPROM Read lsc_ori 0x%x \n",lsc_ori);
	pr_info("EEPROM Read lsc_i0 0x%x \n",lsc_i0);
	pr_info("EEPROM Read lsc_j0 0x%x \n",lsc_j0);
	pr_info("EEPROM Read lsc_scale 0x%x \n",lsc_scale);
	pr_info("EEPROM Read lsc_acoff 0x%x \n",lsc_acoff);
	pr_info("EEPROM Read lsc_bcoff 0x%x \n",lsc_bcoff);
#endif

	for(i=EEPROM_CHECKSUM_SIZE; i<lsc_size; i++) {
		checksumlsc+= calibration_buf[i+ lsc_start];
	}

	if(checksumofidx != checksumlsc) {
		dev_err(&client->dev, "EEPROM section lsc check sum fail");
		#ifndef IGNORE_CHECKSUM
		eeprom_status = FIMC_IS_EEPROM_STATE_CHECKSUM_FAIL;
		ret = -EINVAL;
		goto exit;
		#endif
	}

	#ifdef CONFIG_MZ_ROHM_OIS
	/* OIS */
	checksumofidx = 0;
	ret = fimc_is_i2c_read(client, &calibration_buf[0 + ois_start], ois_start, ois_size);
	if (unlikely(ret)) {
		dev_err(&client->dev, "failed to ois fimc_is_i2c_read (%d)\n", ret);
		eeprom_status = FIMC_IS_EEPROM_STATE_I2CFAIL;
		ret = -EINVAL;
		goto exit;
	}

	checksumofidx = otp_assin4(calibration_buf, ois_start);
	for(i = EEPROM_CHECKSUM_SIZE; i < ois_size; i++) {
		checksumois+= calibration_buf[i+ ois_start];
	}

	if(checksumofidx != checksumois) {
		dev_err(&client->dev, "EEPROM section ois check sum fail");
		eeprom_status = FIMC_IS_EEPROM_STATE_CHECKSUM_FAIL;
		ret = -EINVAL;
		goto exit;
	}
	meizu_otp.ois_start = ois_start;
	meizu_otp.ois_size = ois_size;
	#endif

	/* PDAF */
	checksumofidx = 0;
	ret = fimc_is_i2c_read(client, &calibration_buf[0 + pdaf_start], pdaf_start, pdaf_size);
	if (unlikely(ret)) {
		dev_err(&client->dev, "failed to pdaf fimc_is_i2c_read (%d)\n", ret);
		eeprom_status = FIMC_IS_EEPROM_STATE_I2CFAIL;
		ret = -EINVAL;
		goto exit;
	}

	checksumofidx = otp_assin4(calibration_buf, pdaf_start);
	for(i = EEPROM_CHECKSUM_SIZE; i < pdaf_size; i++) {
		checksumpdaf+= calibration_buf[i+ pdaf_start];
	}

	if(checksumofidx != checksumpdaf) {
		dev_err(&client->dev, "EEPROM section pdaf check sum fail");
		eeprom_status = FIMC_IS_EEPROM_STATE_CHECKSUM_FAIL;
		ret = -EINVAL;
		goto exit;
	}

	eeprom_status = FIMC_IS_EEPROM_STATE_READONE;
	module_info->valid = true;
	pr_info("%s: end read, %s module found\n", __func__,
		module_name);

exit:
	{
		struct platform_device *i2c_pdev = to_platform_device(client->adapter->dev.parent);
		struct s3c24xx_i2c *i2c_pdata = (struct s3c24xx_i2c *)platform_get_drvdata(i2c_pdev);
		dev_info(&i2c_pdev->dev, "i2c_pdev:%p, i2c_pdata:%p, free i2c irq:%d\n",
			i2c_pdev, i2c_pdata, i2c_pdata->irq);
		devm_free_irq((client->adapter->dev.parent), i2c_pdata->irq, i2c_pdata);
	}

	return ret;
}

int fimc_is_ext_eeprom_read(struct fimc_is_device_sensor *device)
{
	int ret;

	ret = fimc_is_eeprom_power_on(device);
	if (ret) {
		pr_err("%s(), return directly since power on failed\n", __func__);
		return ret;
	}

	fimc_is_eeprom_read(device);

	fimc_is_eeprom_power_off(device);

	return 0;
}

u32 fimc_is_eeprom_check_state(void)
{
	return eeprom_status;
}

int fimc_is_eeprom_get_cal_buf(struct meizu_otp **pmeizu_otp)
{
	*pmeizu_otp = &meizu_otp;
	return 0;
}

int mz_get_module_id(int sensor_id)
{
	struct fimc_is_device_sensor *device;
	struct mz_module_info *module_info;
	struct fimc_is_core *core;

	if (fimc_is_dev == NULL) {
		pr_err("%s(), fimc_is_dev is not yet probed\n",
			__func__);
		return -EPROBE_DEFER;
	}

	if (sensor_id != SENSOR_POSITION_REAR &&
		sensor_id != SENSOR_POSITION_FRONT) {
		pr_err("%s(), invalid sensor id:%d\n", __func__, sensor_id);
		return -EINVAL;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	device = &core->sensor[sensor_id];
	module_info = &device->mz_modu_info;

	if (module_info->vendor_id == UNDEFINED_MODULE ||
		!module_info->valid) {
		pr_info("%s(), camera %d 's module info is unready, valid:%d, vendor_id:%d\n",
			__func__, sensor_id, module_info->valid,
			module_info->vendor_id);
		return -ENODATA;
	}

	return module_info->vendor_id;
}

static ssize_t test_store(struct device *dev,
				       struct device_attribute *devAttr,
				       const char *buf, size_t size)
{
	struct i2c_client *client;
	struct fimc_is_core *core;

	client = to_i2c_client(dev);
	core = i2c_get_clientdata(client);

	switch (buf[0]) {
	case '0':
		break;
	case '1':
		fimc_is_eeprom_read(&core->sensor[0]);
		break;
	default:
		pr_err("%s: unkown input %c\n", __func__, buf[0]);
		return -EINVAL;
	}

	return size;
}
static DEVICE_ATTR(test, 0220, NULL, test_store);

static ssize_t show_sn_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	int offset;
	unsigned char module_id;
	char *sn_pre = NULL;
	char *cali_buf = meizu_otp.data;
	char *p = buf;

	if (fimc_is_eeprom_check_state() <
		FIMC_IS_EEPROM_STATE_READONE) {
		pr_err("%s(), err!eeprom data is not retrieved\n", __func__);
		return -ENODATA;
	}

	offset = EEPROM_SN_OFFSET;
	module_id = cali_buf[EEPROM_MANUFAC_OFFSET];
	if (module_id == 3) {
		sn_pre = "MK3U";
	} else if (module_id == 2){
		sn_pre = "SHAR";
	} else {
		sn_pre = "XXXX";
	}

	pr_info("rear module:0x%x\n", module_id);

	p += sprintf(p, sn_pre);
	p += sprintf(p, "%d", cali_buf[EEPROM_YEAR_OFFSET]);
	p += sprintf(p, "%d", cali_buf[EEPROM_WEEK_OFFSET]);
	p += sprintf(p, "%c%c%c%c%c%c\n",
		cali_buf[offset], cali_buf[offset + 1], cali_buf[offset + 2],
		cali_buf[offset + 3], cali_buf[offset + 4], cali_buf[offset + 5]);

	return (p - buf);
}

static DEVICE_ATTR(sn_info,  S_IRUSR | S_IRGRP | S_IROTH, show_sn_info, NULL);


static int fimc_is_otp_i2c0_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *device;
	static bool probe_retried = false;

	if (!fimc_is_dev)
		goto probe_defer;
	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		goto probe_defer;

	device = &core->sensor[SENSOR_POSITION_REAR];
	if (device->private_data != core) {
		pr_err("%s(), err! rear sensor struct is not initialized\n",
			__func__);
		return -EINVAL;
	}

	core->eeprom_client0 = client;

	i2c_set_clientdata(client, core);
	device_create_file(&client->dev, &dev_attr_test);
	meizu_otp.ois_start = meizu_otp.ois_size = -1;

	device_create_file(&client->dev, &dev_attr_sn_info);

	#if defined(CONFIG_MEIZU_CAMERA_SPECIAL)
	meizu_special_feature_probe(client);
	#endif

	fimc_is_eeprom_read(device);

	meizu_sysfslink_register(&client->dev, "rear_cam");

	pr_info("%s() \n", __func__);

	return 0;

probe_defer:
	if (probe_retried) {
		err("probe has already been retried!!");
		BUG();
	}

	probe_retried = true;
	err("core device is not yet probed");
	return -EPROBE_DEFER;
}

static int fimc_is_otp_i2c0_remove(struct i2c_client *client)
{
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id fimc_is_otp_i2c0_dt_ids[] = {
	{ .compatible = "meizu,otp-primax",},
	{},
};
MODULE_DEVICE_TABLE(of, fimc_is_otp_i2c0_dt_ids);
#endif

static const struct i2c_device_id fimc_is_i2c0_id[] = {
	{PRIMAX_OTP_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, fimc_is_i2c0_id);

static struct i2c_driver fimc_is_otp_i2c0_driver = {
	.driver = {
		.name = PRIMAX_OTP_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = fimc_is_otp_i2c0_dt_ids,
#endif
	},
	.probe = fimc_is_otp_i2c0_probe,
	.remove = fimc_is_otp_i2c0_remove,
	.id_table = fimc_is_i2c0_id,
};
module_i2c_driver(fimc_is_otp_i2c0_driver);

