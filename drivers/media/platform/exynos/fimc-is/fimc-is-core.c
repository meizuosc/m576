/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <mach/regs-clock.h>
#include <linux/pm_qos.h>
#include <linux/bug.h>
#include <linux/v4l2-mediabus.h>
#include <mach/devfreq.h>
#include <mach/bts.h>
#include <linux/gpio.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-clk-gate.h"
#include "fimc-is-dvfs.h"
#include "fimc-is-fan53555.h"
#include "fimc-is-ncp6335b.h"

#include "sensor/fimc-is-device-2p2.h"
#include "sensor/fimc-is-device-3h5.h"
#include "sensor/fimc-is-device-3h7.h"
#include "sensor/fimc-is-device-3h7_sunny.h"
#include "sensor/fimc-is-device-3l2.h"
#include "sensor/fimc-is-device-4e5.h"
#include "sensor/fimc-is-device-6a3.h"
#include "sensor/fimc-is-device-6b2.h"
#include "sensor/fimc-is-device-8b1.h"
#include "sensor/fimc-is-device-6d1.h"
#include "sensor/fimc-is-device-imx134.h"
#include "sensor/fimc-is-device-imx135.h"
#include "sensor/fimc-is-device-imx175.h"
#include "sensor/fimc-is-device-imx240.h"
#include "sensor/fimc-is-device-4h5.h"
#include "sensor/fimc-is-device-3l2.h"
#include "sensor/fimc-is-device-2p2.h"
#include "sensor/fimc-is-device-2p2_12m.h"
#ifdef CONFIG_USE_VENDER_FEATURE
#include "fimc-is-sec-define.h"
#ifdef CONFIG_OIS_USE
#include "fimc-is-device-ois.h"
#endif
#endif
#ifdef CONFIG_AF_HOST_CONTROL
#include "fimc-is-device-af.h"
#endif

#ifdef USE_OWN_FAULT_HANDLER
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
#include <linux/exynos_iovmm.h>
#else
#include <plat/sysmmu.h>
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#define PM_QOS_CAM_THROUGHPUT	PM_QOS_RESERVED
#endif

struct fimc_is_from_info *sysfs_finfo = NULL;
struct fimc_is_from_info *sysfs_pinfo = NULL;

struct class *camera_class = NULL;
struct device *camera_front_dev;
struct device *camera_rear_dev;
#ifdef CONFIG_OIS_USE
struct device *camera_ois_dev;
#endif

struct device *fimc_is_dev = NULL;
struct fimc_is_core *sysfs_core;

#ifdef CONFIG_USE_VENDER_FEATURE
extern bool crc32_fw_check;
extern bool crc32_check;
extern bool crc32_check_factory;
extern bool fw_version_crc_check;
extern bool is_latest_cam_module;
extern bool is_final_cam_module;
#if defined(CONFIG_SOC_EXYNOS5433)
extern bool is_right_prj_name;
#endif
#ifdef CONFIG_COMPANION_USE
extern bool crc32_c1_fw_check;
extern bool crc32_c1_check;
extern bool crc32_c1_check_factory;
#endif /* CONFIG_COMPANION_USE */
#endif

extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;
extern struct pm_qos_request exynos_isp_qos_disp;

extern int fimc_is_3a0_video_probe(void *data);
extern int fimc_is_3a1_video_probe(void *data);
extern int fimc_is_isp_video_probe(void *data);
extern int fimc_is_scc_video_probe(void *data);
extern int fimc_is_scp_video_probe(void *data);
extern int fimc_is_vdc_video_probe(void *data);
extern int fimc_is_vdo_video_probe(void *data);
extern int fimc_is_3a0c_video_probe(void *data);
extern int fimc_is_3a1c_video_probe(void *data);

/* sysfs global variable for debug */
struct fimc_is_sysfs_debug sysfs_debug;

static int fimc_is_ischain_allocmem(struct fimc_is_core *this)
{
	int ret = 0;
	void *fw_cookie;
	size_t fw_size =
#ifdef ENABLE_ODC
				SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF +
#endif
#ifdef ENABLE_VDIS
				SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF +
#endif
#ifdef ENABLE_TDNR
				SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF +
#endif
			FIMC_IS_A5_MEM_SIZE;

	fw_size = PAGE_ALIGN(fw_size);
	dbg_core("Allocating memory for FIMC-IS firmware.\n");

	fw_cookie = vb2_ion_private_alloc(this->mem.alloc_ctx, fw_size, 1, 0);

	if (IS_ERR(fw_cookie)) {
		err("Allocating bitprocessor buffer failed");
		fw_cookie = NULL;
		ret = -ENOMEM;
		goto exit;
	}

	ret = vb2_ion_dma_address(fw_cookie, &this->minfo.dvaddr);
	if ((ret < 0) || (this->minfo.dvaddr  & FIMC_IS_FW_BASE_MASK)) {
		err("The base memory is not aligned to 64MB.");
		vb2_ion_private_free(fw_cookie);
		this->minfo.dvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto exit;
	}
	dbg_core("Device vaddr = %pa , size = %08x\n",
		&this->minfo.dvaddr, FIMC_IS_A5_MEM_SIZE);

	this->minfo.kvaddr = (ulong)vb2_ion_private_vaddr(fw_cookie);
	if (IS_ERR((void *)this->minfo.kvaddr)) {
		err("Bitprocessor memory remap failed");
		vb2_ion_private_free(fw_cookie);
		this->minfo.kvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto exit;
	}

	vb2_ion_sync_for_device(fw_cookie, 0, fw_size, DMA_BIDIRECTIONAL);

exit:
	info("[COR] Device virtual for internal: %08lx\n", this->minfo.kvaddr);
	this->minfo.fw_cookie = fw_cookie;

	return ret;
}

static int fimc_is_ishcain_initmem(struct fimc_is_core *this)
{
	int ret = 0;
	u32 offset;

	dbg_core("fimc_is_init_mem - ION\n");

	ret = fimc_is_ischain_allocmem(this);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		ret = -ENOMEM;
		goto exit;
	}

	offset = FW_SHARED_OFFSET;
	this->minfo.dvaddr_fshared = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_fshared = this->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE;
	this->minfo.dvaddr_region = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_region = this->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE;
#ifdef ENABLE_ODC
	this->minfo.dvaddr_odc = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_odc = this->minfo.kvaddr + offset;
	offset += (SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF);
#else
	this->minfo.dvaddr_odc = 0;
	this->minfo.kvaddr_odc = 0;
#endif

#ifdef ENABLE_VDIS
	this->minfo.dvaddr_dis = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_dis = this->minfo.kvaddr + offset;
	offset += (SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF);
#else
	this->minfo.dvaddr_dis = 0;
	this->minfo.kvaddr_dis = 0;
#endif

#ifdef ENABLE_TDNR
	this->minfo.dvaddr_3dnr = this->minfo.dvaddr + offset;
	this->minfo.kvaddr_3dnr = this->minfo.kvaddr + offset;
	offset += (SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF);
#else
	this->minfo.dvaddr_3dnr = 0;
	this->minfo.kvaddr_3dnr = 0;
#endif

	dbg_core("fimc_is_init_mem done\n");

exit:
	return ret;
}

#if defined(CONFIG_ARM_EXYNOS5433_BUS_DEVFREQ) && defined(CONFIG_CPU_THERMAL_IPA)
static int fimc_is_mif_throttling_notifier(struct notifier_block *nb,
		unsigned long val, void *v)
{
	struct fimc_is_core *core = NULL;
	struct fimc_is_device_sensor *device = NULL;
	int i;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is null");
		goto exit;
	}

	for (i = 0; i < FIMC_IS_MAX_NODES; i++) {
		if (test_bit(FIMC_IS_SENSOR_OPEN, &core->sensor[i].state)) {
			device = &core->sensor[i];
			break;
		}
	}

	if (device && !test_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state))
		/* Set DTP */
		set_bit(FIMC_IS_MIF_THROTTLING_STOP, &device->force_stop);
	else
		err("any sensor is not opened");

exit:
	err("MIF: cause of mif_throttling, mif_qos is [%lu]!!!\n", val);

	return NOTIFY_OK;
}

static struct notifier_block exynos_fimc_is_mif_throttling_nb = {
	.notifier_call = fimc_is_mif_throttling_notifier,
};
#endif

#ifdef CONFIG_USE_VENDER_FEATURE
static ssize_t camera_front_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_platform_fimc_is_sensor *sensor = dev_get_drvdata(dev);

	dev_info(dev, "%s: E", __func__);

	if (unlikely(!sensor)) {
		dev_err(dev, "%s: sensor null\n", __func__);
		return -EFAULT;
	}

	return sprintf(buf, "%d\n", sensor->sensor_id);
}

static ssize_t camera_rear_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_platform_fimc_is_sensor *sensor = dev_get_drvdata(dev);

	dev_info(dev, "%s: E", __func__);

	if (unlikely(!sensor)) {
		dev_err(dev, "%s: sensor null\n", __func__);
		return -EFAULT;
	}

	return sprintf(buf, "%d\n", sensor->sensor_id);
}

static DEVICE_ATTR(front_sensorid, S_IRUGO, camera_front_sensorid_show, NULL);
static DEVICE_ATTR(rear_sensorid, S_IRUGO, camera_rear_sensorid_show, NULL);

static ssize_t camera_front_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char type[50] = "SLSI_S5K6B2YX_FIMC_IS";
#if defined(CONFIG_CAMERA_SENSOR_8B1)
	strcpy(type, "SLSI_S5K8B1YX_FIMC_IS");
#elif defined(CONFIG_CAMERA_SENSOR_6D1)
	strcpy(type, "SLSI_S5K6D1YX_FIMC_IS");
#endif
	return sprintf(buf, "%s\n", type);
}

static ssize_t camera_front_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char type[50] = "S5K6B2YX";
#if defined(CONFIG_CAMERA_SENSOR_8B1)
	strcpy(type, "S5K8B1YX");
#elif defined(CONFIG_CAMERA_SENSOR_6D1)
	strcpy(type, "S5K6D1YX");
#endif
	return sprintf(buf, "%s N\n", type);
}

static DEVICE_ATTR(front_camtype, S_IRUGO,
		camera_front_camtype_show, NULL);
static DEVICE_ATTR(front_camfw, S_IRUGO, camera_front_camfw_show, NULL);

static struct fimc_is_from_info *pinfo = NULL;
static struct fimc_is_from_info *finfo = NULL;

int read_from_firmware_version(void)
{
	char fw_name[100];
	char setf_name[100];
#ifdef CONFIG_COMPANION_USE
	char master_setf_name[100];
	char mode_setf_name[100];
#endif
	struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;

	fimc_is_sec_get_sysfs_pinfo(&pinfo);
	fimc_is_sec_get_sysfs_finfo(&finfo);

	if (!finfo->is_caldata_read) {
		if (finfo->bin_start_addr != 0x80000) {
			//fimc_is_sec_set_camid(CAMERA_DUAL_FRONT);
#if defined(CONFIG_PM_RUNTIME)
			pr_debug("pm_runtime_suspended = %d\n",
			pm_runtime_suspended(is_dev));
			pm_runtime_get_sync(is_dev);
#else
			fimc_is_runtime_resume(is_dev);
			printk(KERN_INFO "%s - fimc_is runtime resume complete\n", __func__);
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT)
			fimc_is_sec_fw_sel_eeprom(is_dev, fw_name, setf_name, 1);
#else
			fimc_is_sec_fw_sel(sysfs_core, is_dev, fw_name, setf_name, 1);
#endif
#ifdef CONFIG_COMPANION_USE
			fimc_is_sec_concord_fw_sel(sysfs_core, is_dev, fw_name, master_setf_name, mode_setf_name, 1);
#endif
#if defined(CONFIG_PM_RUNTIME)
			pm_runtime_put_sync(is_dev);
			pr_debug("pm_runtime_suspended = %d\n",
				pm_runtime_suspended(is_dev));
#else
			fimc_is_runtime_suspend(is_dev);
#endif
		}
	}
	return 0;
}

static ssize_t camera_rear_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char type[] = "SLSI_S5K2P2_FIMC_IS";
#if 0
	char type_unknown[] = "UNKNOWN_UNKNOWN_FIMC_IS";

	read_from_firmware_version(false);
	if (fimc_is_sec_fw_module_compare(FW_2P2, finfo->header_ver))
		return sprintf(buf, "%s\n", type);
	else
		return sprintf(buf, "%s\n", type_unknown);
#endif
	return sprintf(buf, "%s\n", type);
}

static ssize_t camera_rear_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };
	char *loaded_fw;

	read_from_firmware_version();
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT)
	loaded_fw = pinfo->header_ver;
#else
	fimc_is_sec_get_loaded_fw(&loaded_fw);
#endif

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
#ifdef CONFIG_COMPANION_USE
		    && crc32_c1_fw_check && crc32_c1_check_factory
#endif
		) {
			return sprintf(buf, "%s %s\n", finfo->header_ver, loaded_fw);
		} else {
			strcpy(command_ack, "NG_");
			if (!crc32_fw_check)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, "CD");
#ifdef CONFIG_COMPANION_USE
			if (!crc32_c1_fw_check)
				strcat(command_ack, "FW1");
			if (!crc32_c1_check_factory)
				strcat(command_ack, "CD1");
#endif
			if (finfo->header_ver[3] != 'L')
				strcat(command_ack, "_Q");
			return sprintf(buf, "%s %s\n", finfo->header_ver, command_ack);
		}
	} else {
		strcpy(command_ack, "NG_");
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT)
		strcat(command_ack, "CD");
#else
		strcat(command_ack, "FWCD");
#endif
#ifdef CONFIG_COMPANION_USE
		strcat(command_ack, "FW1CD1");
#endif
		return sprintf(buf, "%s %s\n", finfo->header_ver, command_ack);
	}
}

static ssize_t camera_rear_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };
	char *loaded_fw;

	read_from_firmware_version();
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT)
	loaded_fw = pinfo->header_ver;
#else
	fimc_is_sec_get_loaded_fw(&loaded_fw);
#endif

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
#ifdef CONFIG_COMPANION_USE
		    && crc32_c1_fw_check && crc32_c1_check_factory
#endif
		) {
			return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, loaded_fw);
		} else {
			strcpy(command_ack, "NG_");
			if (!crc32_fw_check)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, "CD");
#ifdef CONFIG_COMPANION_USE
			if (!crc32_c1_fw_check)
				strcat(command_ack, "FW1");
			if (!crc32_c1_check_factory)
				strcat(command_ack, "CD1");
#endif
			if (finfo->header_ver[3] != 'L')
				strcat(command_ack, "_Q");
			return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, command_ack);
		}
	} else {
		strcpy(command_ack, "NG_");
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT)
		strcat(command_ack, "CD");
#else
		strcat(command_ack, "FWCD");
#endif
#ifdef CONFIG_COMPANION_USE
		strcat(command_ack, "FW1CD1");
#endif
		return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, command_ack);
	}
}

static ssize_t camera_rear_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version();

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
#ifdef CONFIG_COMPANION_USE
		    && crc32_c1_fw_check && crc32_c1_check_factory
#endif
		) {
			if (!is_latest_cam_module
#if defined(CONFIG_SOC_EXYNOS5433)
				|| !is_right_prj_name
#endif
			) {
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_rear_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version();
	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
#ifdef CONFIG_COMPANION_USE
		    && crc32_c1_fw_check && crc32_c1_check_factory
#endif
		) {
			if (!is_final_cam_module
#if defined(CONFIG_SOC_EXYNOS5433)
				|| !is_right_prj_name
#endif
			) {
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		return sprintf(buf, "%s\n", "NG");
	}
}

#ifdef CONFIG_COMPANION_USE
static ssize_t camera_rear_companionfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *loaded_c1_fw;

	read_from_firmware_version();
	fimc_is_sec_get_loaded_c1_fw(&loaded_c1_fw);

	return sprintf(buf, "%s %s\n",
		finfo->concord_header_ver, loaded_c1_fw);
}

static ssize_t camera_rear_companionfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *loaded_c1_fw;

	read_from_firmware_version();
	fimc_is_sec_get_loaded_c1_fw(&loaded_c1_fw);

	return sprintf(buf, "%s %s %s\n",
		finfo->concord_header_ver, pinfo->concord_header_ver, loaded_c1_fw);
}
#endif

static ssize_t camera_rear_camfw_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = -EINVAL;

	if ((size == 1 || size == 2) && (buf[0] == 'F' || buf[0] == 'f')) {
		fimc_is_sec_set_force_caldata_dump(true);
		ret = size;
	} else {
		fimc_is_sec_set_force_caldata_dump(false);
	}
	return ret;
}

static ssize_t camera_rear_calcheck_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char rear_sensor[10] = {0, };
#ifdef CONFIG_COMPANION_USE
	char rear_companion[10] = {0, };
#endif

	read_from_firmware_version();

	if (crc32_check_factory)
		strcpy(rear_sensor, "Normal");
	else
		strcpy(rear_sensor, "Abnormal");

#ifdef CONFIG_COMPANION_USE
	if (crc32_c1_check_factory)
		strcpy(rear_companion, "Normal");
	else
		strcpy(rear_companion, "Abnormal");

	return sprintf(buf, "%s %s %s\n", rear_sensor, rear_companion, "Null");
#else
	return sprintf(buf, "%s %s\n", rear_sensor, "Null");
#endif
}

#ifdef CONFIG_COMPANION_USE
static ssize_t camera_isp_core_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int sel;

	if (DCDC_VENDOR_NONE == sysfs_core->companion_dcdc.type)
		return sprintf(buf, "none\n");

	sel = fimc_is_power_binning(sysfs_core);
	return sprintf(buf, "%s\n", sysfs_core->companion_dcdc.get_vout_str(sel));
}
#endif

#ifdef CONFIG_OIS_USE
static ssize_t camera_ois_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)

{
	switch (buf[0]) {
	case '0':
		fimc_is_ois_gpio_off(sysfs_core->companion);
		break;
	case '1':
		fimc_is_ois_gpio_on(sysfs_core->companion);
		msleep(150);
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return count;
}

static ssize_t camera_ois_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result_total = 0;
	bool result_offset = 0, result_selftest = 0;
	int selftest_ret = 0;
	long raw_data_x = 0, raw_data_y = 0;

	fimc_is_ois_offset_test(sysfs_core, &raw_data_x, &raw_data_y);
	msleep(50);
	selftest_ret = fimc_is_ois_self_test(sysfs_core);

	if (selftest_ret == 0x0) {
		result_selftest = true;
	} else {
		result_selftest = false;
	}

	if (abs(raw_data_x) > 35000 || abs(raw_data_y) > 35000)  {
		result_offset = false;
	} else {
		result_offset = true;
	}

	if (result_offset && result_selftest) {
		result_total = 0;
	} else if (!result_offset && !result_selftest) {
		result_total = 3;
	} else if (!result_offset) {
		result_total = 1;
	} else if (!result_selftest) {
		result_total = 2;
	}

	if (raw_data_x < 0 && raw_data_y < 0) {
		return sprintf(buf, "%d,-%ld.%03ld,-%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else if (raw_data_x < 0) {
		return sprintf(buf, "%d,-%ld.%03ld,%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
			raw_data_y /1000, raw_data_y % 1000);
	} else if (raw_data_y < 0) {
		return sprintf(buf, "%d,%ld.%03ld,-%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else {
		return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
			raw_data_y /1000, raw_data_y % 1000);
	}
}

static ssize_t camera_ois_rawdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	long raw_data_x = 0, raw_data_y = 0;

	fimc_is_ois_get_offset_data(sysfs_core, &raw_data_x, &raw_data_y);

	if (raw_data_x < 0 && raw_data_y < 0) {
		return sprintf(buf, "-%ld.%03ld,-%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else if (raw_data_x < 0) {
		return sprintf(buf, "-%ld.%03ld,%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
			raw_data_y /1000, raw_data_y % 1000);
	} else if (raw_data_y < 0) {
		return sprintf(buf, "%ld.%03ld,-%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else {
		return sprintf(buf, "%ld.%03ld,%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
			raw_data_y /1000, raw_data_y % 1000);
	}
}

static ssize_t camera_ois_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_ois_info *ois_minfo = NULL;
	struct fimc_is_ois_info *ois_pinfo = NULL;

	if (!sysfs_core->ois_ver_read && !sysfs_core->running_rear_camera) {
		fimc_is_ois_gpio_on(sysfs_core->companion);
		msleep(150);
		fimc_is_ois_check_fw(sysfs_core);

		if (!sysfs_core->running_rear_camera) {
			fimc_is_ois_gpio_off(sysfs_core->companion);
		}
	}

	fimc_is_ois_get_module_version(&ois_minfo);
	fimc_is_ois_get_phone_version(&ois_pinfo);

	return sprintf(buf, "%s %s\n", ois_minfo->header_ver, ois_pinfo->header_ver);
}

static ssize_t camera_ois_diff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result = 0;
	int x_diff = 0, y_diff = 0;

	result = fimc_is_ois_diff_test(sysfs_core, &x_diff, &y_diff);

	return sprintf(buf, "%d,%d,%d\n", result == true ? 0 : 1, x_diff, y_diff);
}

static ssize_t camera_ois_fw_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_OIS_FW_UPDATE_THREAD_USE
	fimc_is_ois_init_thread(sysfs_core);
#else
	fimc_is_ois_gpio_on(sysfs_core->companion);
	msleep(150);

	fimc_is_ois_fw_update(sysfs_core);
	fimc_is_ois_gpio_off(sysfs_core->companion);
#endif

	return sprintf(buf, "%s\n", "Ois update done.");
}

static ssize_t camera_ois_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_ois_info *ois_minfo = NULL;
	struct fimc_is_ois_info *ois_pinfo = NULL;
	struct fimc_is_ois_info *ois_uinfo = NULL;
	struct fimc_is_ois_exif *ois_exif = NULL;

	fimc_is_ois_get_module_version(&ois_minfo);
	fimc_is_ois_get_phone_version(&ois_pinfo);
	fimc_is_ois_get_user_version(&ois_uinfo);
	fimc_is_ois_get_exif_data(&ois_exif);

	return sprintf(buf, "%s %s %s %d %d", ois_minfo->header_ver, ois_pinfo->header_ver,
		ois_uinfo->header_ver, ois_exif->error_data, ois_exif->status_data);
}
#endif
static DEVICE_ATTR(rear_camtype, S_IRUGO,
		camera_rear_camtype_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO,
		camera_rear_camfw_show, camera_rear_camfw_write);
static DEVICE_ATTR(rear_camfw_full, S_IRUGO,
		camera_rear_camfw_full_show, NULL);
#ifdef CONFIG_COMPANION_USE
static DEVICE_ATTR(rear_companionfw, S_IRUGO,
		camera_rear_companionfw_show, NULL);
static DEVICE_ATTR(rear_companionfw_full, S_IRUGO,
		camera_rear_companionfw_full_show, NULL);
#endif
static DEVICE_ATTR(rear_calcheck, S_IRUGO,
		camera_rear_calcheck_show, NULL);
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO,
		camera_rear_checkfw_user_show, NULL);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO,
		camera_rear_checkfw_factory_show, NULL);
#ifdef CONFIG_COMPANION_USE
static DEVICE_ATTR(isp_core, S_IRUGO,
		camera_isp_core_show, NULL);
#endif
#ifdef CONFIG_OIS_USE
static DEVICE_ATTR(selftest, S_IRUGO,
		camera_ois_selftest_show, NULL);
static DEVICE_ATTR(ois_power, S_IWUSR,
		NULL, camera_ois_power_store);
static DEVICE_ATTR(ois_rawdata, S_IRUGO,
		camera_ois_rawdata_show, NULL);
static DEVICE_ATTR(oisfw, S_IRUGO,
		camera_ois_version_show, NULL);
static DEVICE_ATTR(ois_diff, S_IRUGO,
		camera_ois_diff_show, NULL);
static DEVICE_ATTR(fw_update, S_IRUGO,
		camera_ois_fw_update_show, NULL);
static DEVICE_ATTR(ois_exif, S_IRUGO,
		camera_ois_exif_show, NULL);
#endif
#endif /* CONFIG_USE_VENDER_FEATURE */

static int fimc_is_suspend(struct device *dev)
{
	pr_debug("FIMC_IS Suspend\n");
	return 0;
}

static int fimc_is_resume(struct device *dev)
{
	pr_debug("FIMC_IS Resume\n");
	return 0;
}

#ifdef USE_OWN_FAULT_HANDLER
static void fimc_is_print_minfo(struct fimc_is_minfo *minfo)
{
	if (minfo) {
		err("dvaddr : 0x%pa, size : %zd", &minfo->dvaddr, minfo->size);
		err("dvaddr_debug  : 0x%pa, dvaddr_region : 0x%pa", &minfo->dvaddr_debug, &minfo->dvaddr_region);
		err("dvaddr_shared : 0x%pa, dvaddr_odc    : 0x%pa", &minfo->dvaddr_shared, &minfo->dvaddr_odc);
		err("dvaddr_dis    : 0x%pa, dvaddr_3dnr   : 0x%pa", &minfo->dvaddr_dis, &minfo->dvaddr_3dnr);
	} else {
		err("core->minfo is NULL");
	}
}

static void __fimc_is_fault_handler(struct device *dev)
{
	u32 i, j, k;
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_framemgr *framemgr;

	core = dev_get_drvdata(dev);
	if (core) {

		fimc_is_print_minfo(&core->minfo);
		fimc_is_hw_logdump(&core->interface);
		/* dump FW page table 1nd(~16KB), 2nd(16KB~32KB) */
		fimc_is_hw_memdump(&core->interface,
			core->minfo.kvaddr + 0x010F8000 /* TTB_BASE ~ 32KB */,
			core->minfo.kvaddr + 0x010F8000 + 0x8000);

		/* REAR SENSOR */
		sensor = &core->sensor[0];
		if (test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state)) {
			framemgr = &sensor->vctx->q_dst.framemgr;
			for (i = 0; i < FRAMEMGR_MAX_REQUEST; ++i) {
				pr_err("LITE0 BUF[%d][0] = %X, 0x%08X, 0x%08X\n", i,
					(u32)framemgr->frame[i].memory,
					framemgr->frame[i].dvaddr_buffer[0],
					framemgr->frame[i].kvaddr_buffer[0]);
			}
		}

		/* FRONT SENSOR */
		sensor = &core->sensor[1];
		if (test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state)) {
			framemgr = &sensor->vctx->q_dst.framemgr;
			for (i = 0; i < FRAMEMGR_MAX_REQUEST; ++i) {
				pr_err("LITE1 BUF[%d][0] = %X, 0x%08X. 0x%08X\n", i,
					(u32)framemgr->frame[i].memory,
					framemgr->frame[i].dvaddr_buffer[0],
					framemgr->frame[i].kvaddr_buffer[0]);
			}
		}

		/* ISCHAIN */
		for (i = 0; i < FIMC_IS_MAX_NODES; i++) {
			if (test_bit(FIMC_IS_ISCHAIN_OPEN, &(core->ischain[i].state))) {
				ischain = &core->ischain[i];
				/* 3AA */
				if (test_bit(FIMC_IS_SUBDEV_START, &ischain->group_3aa.leader.state)) {
					framemgr = &ischain->group_3aa.leader.vctx->q_src.framemgr;
					for (j = 0; j < framemgr->frame_cnt; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							pr_err("[3AA:%d] BUF[%d][%d] = %X, 0x%08X, 0x%08X\n",
								i, j, k,
								(u32)framemgr->frame[j].memory,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].kvaddr_buffer[k]);
						}
					}
				}
				/* 3AAC */
				if (test_bit(FIMC_IS_SUBDEV_START, &ischain->taac.state)) {
					framemgr = &ischain->taac.leader->vctx->q_dst.framemgr;
					for (j = 0; j < framemgr->frame_cnt; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							pr_err("[3AAC:%d] BUF[%d][%d] = %X, 0x%08X, 0x%08X\n",
								i, j, k,
								(u32)framemgr->frame[j].memory,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].kvaddr_buffer[k]);
						}
					}
				}
				/* 3AAP */
				if (test_bit(FIMC_IS_SUBDEV_START, &ischain->taap.state)) {
					framemgr = &ischain->taap.leader->vctx->q_dst.framemgr;
					for (j = 0; j < framemgr->frame_cnt; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							pr_err("[3AAP:%d] BUF[%d][%d] = %X, 0x%08X, 0x%08X\n",
								i, j, k,
								(u32)framemgr->frame[j].memory,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].kvaddr_buffer[k]);
						}
					}
				}
				/* ISP */
				if (test_bit(FIMC_IS_SUBDEV_START, &ischain->group_isp.leader.state)) {
					framemgr = &ischain->group_isp.leader.vctx->q_src.framemgr;
					for (j = 0; j < framemgr->frame_cnt; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							pr_err("[ISP:%d] BUF[%d][%d] = %X, 0x%08X, 0x%08X\n",
								i, j, k,
								(u32)framemgr->frame[j].memory,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].kvaddr_buffer[k]);
						}
					}
				}
				/* SCC */
				if (test_bit(FIMC_IS_SUBDEV_START, &ischain->scc.state)) {
					framemgr = &ischain->scc.vctx->q_dst.framemgr;
					for (j = 0; j < framemgr->frame_cnt; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							pr_err("[SCC:%d] BUF[%d][%d] = %X, 0x%08X, 0x%08X\n",
								i, j, k,
								(u32)framemgr->frame[j].memory,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].kvaddr_buffer[k]);
						}
					}
				}
				/* VDC */
				if (test_bit(FIMC_IS_SUBDEV_START, &ischain->dis.state)) {
					framemgr = &ischain->dis.vctx->q_dst.framemgr;
					for (j = 0; j < framemgr->frame_cnt; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							pr_err("[VDC:%d] BUF[%d][%d] = %X, 0x%08X, 0x%08X\n",
								i, j, k,
								(u32)framemgr->frame[j].memory,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].kvaddr_buffer[k]);
						}
					}
				}
				/* VDO */
				if (test_bit(FIMC_IS_SUBDEV_START, &ischain->group_dis.leader.state)) {
					framemgr = &ischain->group_dis.leader.vctx->q_src.framemgr;
					for (j = 0; j < framemgr->frame_cnt; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							pr_err("[VDO:%d] BUF[%d][%d] = %X, 0x%08X, 0x%08X\n",
								i, j, k,
								(u32)framemgr->frame[j].memory,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].kvaddr_buffer[k]);
						}
					}
				}
				/* SCP */
				if (test_bit(FIMC_IS_SUBDEV_START, &ischain->scp.state)) {
					framemgr = &ischain->scp.vctx->q_dst.framemgr;
					for (j = 0; j < framemgr->frame_cnt; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							pr_err("[SCP:%d] BUF[%d][%d] = %X, 0x%08X, 0x%08X\n",
								i, j, k,
								(u32)framemgr->frame[j].memory,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].kvaddr_buffer[k]);
						}
					}
				}
			}
		}
	} else {
		pr_err("failed to get core\n");
	}
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0))
#define SECT_ORDER 20
#define LPAGE_ORDER 16
#define SPAGE_ORDER 12

#define lv1ent_page(sent) ((*(sent) & 3) == 1)

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) (((iova) & 0xFF000) >> SPAGE_ORDER)
#define lv2table_base(sent) (*(sent) & 0xFFFFFC00)

static unsigned long *section_entry(unsigned long *pgtable, unsigned long iova)
{
	return pgtable + lv1ent_offset(iova);
}

static unsigned long *page_entry(unsigned long *sent, unsigned long iova)
{
	return (unsigned long *)__va(lv2table_base(sent)) + lv2ent_offset(iova);
}

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PAGE FAULT",
	"AR MULTI-HIT FAULT",
	"AW MULTI-HIT FAULT",
	"BUS ERROR",
	"AR SECURITY PROTECTION FAULT",
	"AR ACCESS PROTECTION FAULT",
	"AW SECURITY PROTECTION FAULT",
	"AW ACCESS PROTECTION FAULT",
	"UNKNOWN FAULT"
};

static int fimc_is_fault_handler(struct device *dev, const char *mmuname,
					enum exynos_sysmmu_inttype itype,
					unsigned long pgtable_base,
					unsigned long fault_addr)
{
	unsigned long *ent;

	if ((itype >= SYSMMU_FAULTS_NUM) || (itype < SYSMMU_PAGEFAULT))
		itype = SYSMMU_FAULT_UNKNOWN;

	pr_err("%s occured at 0x%lx by '%s'(Page table base: 0x%lx)\n",
		sysmmu_fault_name[itype], fault_addr, mmuname, pgtable_base);

	ent = section_entry(__va(pgtable_base), fault_addr);
	pr_err("\tLv1 entry: 0x%lx\n", *ent);

	if (lv1ent_page(ent)) {
		ent = page_entry(ent, fault_addr);
		pr_err("\t Lv2 entry: 0x%lx\n", *ent);
	}

	__fimc_is_fault_handler(dev);

	pr_err("Generating Kernel OOPS... because it is unrecoverable.\n");

	BUG();

	return 0;
}
#else
static int fimc_is_fault_handler(struct iommu_domain *domain,
	struct device *dev,
	unsigned long fault_addr,
	int fault_flag,
	void *token)
{
	pr_err("<FIMC-IS FAULT HANDLER>\n");
	pr_err("Device virtual(0x%X) is invalid access\n", (u32)fault_addr);

	__fimc_is_fault_handler(dev);

	return -EINVAL;
}
#endif
#endif /* USE_OWN_FAULT_HANDLER */

static ssize_t show_clk_gate_mode(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.clk_gate_mode);
}

static ssize_t store_clk_gate_mode(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
#ifdef HAS_FW_CLOCK_GATE
	switch (buf[0]) {
	case '0':
		sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_HOST;
		break;
	case '1':
		sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_FW;
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}
#endif
	return count;
}

static ssize_t show_en_clk_gate(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.en_clk_gate);
}

static ssize_t store_en_clk_gate(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
#ifdef ENABLE_CLOCK_GATE
	switch (buf[0]) {
	case '0':
		sysfs_debug.en_clk_gate = false;
		sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_HOST;
		break;
	case '1':
		sysfs_debug.en_clk_gate = true;
		sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_HOST;
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}
#endif
	return count;
}

static ssize_t show_en_dvfs(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_debug.en_dvfs);
}

static ssize_t store_en_dvfs(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
#ifdef ENABLE_DVFS
	struct fimc_is_core *core =
		(struct fimc_is_core *)platform_get_drvdata(to_platform_device(dev));
	struct fimc_is_resourcemgr *resourcemgr;
	int i;

	BUG_ON(!core);

	resourcemgr = &core->resourcemgr;

	switch (buf[0]) {
	case '0':
		sysfs_debug.en_dvfs = false;
		/* update dvfs lever to max */
		mutex_lock(&resourcemgr->dvfs_ctrl.lock);
		for (i = 0; i < FIMC_IS_MAX_NODES; i++) {
			if (test_bit(FIMC_IS_ISCHAIN_OPEN, &((core->ischain[i]).state)))
				fimc_is_set_dvfs(&(core->ischain[i]), FIMC_IS_SN_MAX);
		}
		fimc_is_dvfs_init(resourcemgr);
		resourcemgr->dvfs_ctrl.static_ctrl->cur_scenario_id = FIMC_IS_SN_MAX;
		mutex_unlock(&resourcemgr->dvfs_ctrl.lock);
		break;
	case '1':
		/* It can not re-define static scenario */
		sysfs_debug.en_dvfs = true;
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}
#endif
	return count;
}

static DEVICE_ATTR(en_clk_gate, 0644, show_en_clk_gate, store_en_clk_gate);
static DEVICE_ATTR(clk_gate_mode, 0644, show_clk_gate_mode, store_clk_gate_mode);
static DEVICE_ATTR(en_dvfs, 0644, show_en_dvfs, store_en_dvfs);

static struct attribute *fimc_is_debug_entries[] = {
	&dev_attr_en_clk_gate.attr,
	&dev_attr_clk_gate_mode.attr,
	&dev_attr_en_dvfs.attr,
	NULL,
};
static struct attribute_group fimc_is_debug_attr_group = {
	.name	= "debug",
	.attrs	= fimc_is_debug_entries,
};

static int fimc_is_probe(struct platform_device *pdev)
{
	struct exynos_platform_fimc_is *pdata;
	struct resource *mem_res;
	struct resource *regs_res;
	struct fimc_is_core *core;
	int ret = -ENODEV;

	info("%s:start\n", __func__);

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
#ifdef CONFIG_OF
		pdata = fimc_is_parse_dt(&pdev->dev);
		if (IS_ERR(pdata))
#endif
			return PTR_ERR(pdata);
	}

	core = kzalloc(sizeof(struct fimc_is_core), GFP_KERNEL);
	if (!core) {
		err("core is NULL");
		return -ENOMEM;
	}

	fimc_is_dev = &pdev->dev;
	ret = dev_set_drvdata(fimc_is_dev, core);
	if (ret) {
		err("dev_set_drvdata is fail(%d)", ret);
		kfree(core);
		return ret;
	}

#ifdef CONFIG_USE_VENDER_FEATURE
#ifdef CONFIG_OIS_USE
	core->use_ois = pdata->use_ois;
#endif /* CONFIG_OIS_USE */
	core->use_ois_hsi2c = pdata->use_ois_hsi2c;
	core->use_module_check = pdata->use_module_check;
#endif

#ifdef USE_ION_ALLOC
	core->fimc_ion_client = ion_client_create(ion_exynos, "fimc-is");
#endif
	core->pdev = pdev;
	core->pdata = pdata;
	core->id = pdev->id;
	core->debug_cnt = 0;
	core->running_rear_camera = false;
	core->running_front_camera = false;

	device_init_wakeup(&pdev->dev, true);

	/* init mutex for spi read */
	mutex_init(&core->spi_lock);

	/* for mideaserver force down */
	atomic_set(&core->rsccount, 0);
	clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		goto p_err1;
	}

	regs_res = request_mem_region(mem_res->start, resource_size(mem_res),
					pdev->name);
	if (!regs_res) {
		dev_err(&pdev->dev, "Failed to request io memory region\n");
		goto p_err1;
	}

	core->regs_res = regs_res;
	core->regs =  ioremap_nocache(mem_res->start, resource_size(mem_res));
	if (!core->regs) {
		dev_err(&pdev->dev, "Failed to remap io region\n");
		goto p_err2;
	}

	core->irq = platform_get_irq(pdev, 0);
	if (core->irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		goto p_err3;
	}

	ret = fimc_is_mem_probe(&core->mem, core->pdev);
	if (ret) {
		err("fimc_is_mem_probe is fail(%d)", ret);
		goto p_err3;
	}

	fimc_is_interface_probe(&core->interface,
		(ulong)core->regs,
		core->irq,
		core);

	fimc_is_resource_probe(&core->resourcemgr, core);

	/* group initialization */
	fimc_is_groupmgr_probe(&core->groupmgr);

	ret = sensor_6b2_probe(NULL, NULL);
	if (ret) {
		err("sensor_6b2_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_8b1_probe(NULL, NULL);
	if (ret) {
		err("sensor_8b1_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_6d1_probe(NULL, NULL);
	if (ret) {
		err("sensor_6d1_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_6a3_probe(NULL, NULL);
	if (ret) {
		err("sensor_6a3_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_imx135_probe(NULL, NULL);
	if (ret) {
		err("sensor_imx135_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_3l2_probe(NULL, NULL);
	if (ret) {
		err("sensor_3l2_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_2p2_probe(NULL, NULL);
	if (ret) {
		err("sensor_2p2_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_2p2_12m_probe(NULL, NULL);
	if (ret) {
		err("sensor_2p2_12m_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_3h5_probe(NULL, NULL);
	if (ret) {
		err("sensor_3h5_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_3h7_probe(NULL, NULL);
	if (ret) {
		err("sensor_3h7_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_3h7_sunny_probe(NULL, NULL);
	if (ret) {
		err("sensor_3h7_sunny_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_4e5_probe(NULL, NULL);
	if (ret) {
		err("sensor_4e5_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_imx175_probe(NULL, NULL);
	if (ret) {
		err("sensor_imx175_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_4h5_probe(NULL, NULL);
	if (ret) {
		err("sensor_4h5_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = sensor_imx240_probe(NULL, NULL);
	if (ret) {
		err("sensor_imx240_probe is fail(%d)", ret);
		goto p_err3;
	}

	/* device entity - ischain0 */
	fimc_is_ischain_probe(&core->ischain[0],
		&core->interface,
		&core->resourcemgr,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		0);

	/* device entity - ischain1 */
	fimc_is_ischain_probe(&core->ischain[1],
		&core->interface,
		&core->resourcemgr,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		1);

	/* device entity - ischain2 */
	fimc_is_ischain_probe(&core->ischain[2],
		&core->interface,
		&core->resourcemgr,
		&core->groupmgr,
		&core->mem,
		core->pdev,
		2);

	ret = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register fimc-is v4l2 device\n");
		goto p_err4;
	}

	/* video entity - 3a0 */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a0))
		fimc_is_3a0_video_probe(core);

	/* video entity - 3a0 capture */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a0))
		fimc_is_3a0c_video_probe(core);

	/* video entity - 3a1 */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a1))
		fimc_is_3a1_video_probe(core);

	/* video entity - 3a1 capture */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, 3a1))
		fimc_is_3a1c_video_probe(core);

	/* video entity - isp */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, isp))
		fimc_is_isp_video_probe(core);

	/*front video entity - scalerC */
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, scc))
		fimc_is_scc_video_probe(core);

	/* back video entity - scalerP*/
	if (GET_FIMC_IS_NUM_OF_SUBIP(core, scp))
		fimc_is_scp_video_probe(core);

	if (GET_FIMC_IS_NUM_OF_SUBIP(core, dis)) {
		/* vdis video entity - vdis capture*/
		fimc_is_vdc_video_probe(core);
		/* vdis video entity - vdis output*/
		fimc_is_vdo_video_probe(core);
	}

	platform_set_drvdata(pdev, core);

	ret = fimc_is_ishcain_initmem(core);
	if (ret) {
		err("fimc_is_ishcain_initmem is fail(%d)", ret);
		goto p_err4;
	}


#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->mem.alloc_ctx)
		vb2_ion_attach_iommu(core->mem.alloc_ctx);
#endif
#endif
	EXYNOS_MIF_ADD_NOTIFIER(&exynos_fimc_is_mif_throttling_nb);

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
#endif

#ifdef CONFIG_USE_VENDER_FEATURE
	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class)) {
			pr_err("Failed to create class(camera)!\n");
			ret = PTR_ERR(camera_class);
			goto p_err5;
		}
	}

	camera_front_dev = device_create(camera_class, NULL, 0, NULL, "front");
	if (IS_ERR(camera_front_dev)) {
		printk(KERN_ERR "failed to create front device!\n");
	} else {
		if (device_create_file(camera_front_dev,
				&dev_attr_front_sensorid) < 0) {
			printk(KERN_ERR "failed to create front device file, %s\n",
					dev_attr_front_sensorid.attr.name);
		}

		if (device_create_file(camera_front_dev,
					&dev_attr_front_camtype)
				< 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_camtype.attr.name);
		}
		if (device_create_file(camera_front_dev,
					&dev_attr_front_camfw) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_camfw.attr.name);
		}
	}
	camera_rear_dev = device_create(camera_class, NULL, 1, NULL, "rear");
	if (IS_ERR(camera_rear_dev)) {
		printk(KERN_ERR "failed to create rear device!\n");
	} else {
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_sensorid) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_sensorid.attr.name);
		}

		if (device_create_file(camera_rear_dev, &dev_attr_rear_camtype)
				< 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_camtype.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_camfw) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_camfw.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_camfw_full) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_camfw_full.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_checkfw_user) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_checkfw_user.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_checkfw_factory) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_checkfw_factory.attr.name);
		}
#ifdef CONFIG_COMPANION_USE
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_companionfw) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_companionfw.attr.name);
		}
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_companionfw_full) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_companionfw_full.attr.name);
		}
#endif
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_calcheck) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_calcheck.attr.name);
		}
#ifdef CONFIG_COMPANION_USE
		if (device_create_file(camera_rear_dev,
					&dev_attr_isp_core) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_isp_core.attr.name);
		}
#endif
#ifdef CONFIG_OIS_USE
		if (device_create_file(camera_rear_dev,
					&dev_attr_fw_update) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_fw_update.attr.name);
		}
#endif
	}

#ifdef CONFIG_OIS_USE
	camera_ois_dev = device_create(camera_class, NULL, 2, NULL, "ois");
	if (IS_ERR(camera_ois_dev)) {
		printk(KERN_ERR "failed to create ois device!\n");
	} else {
		if (device_create_file(camera_ois_dev,
					&dev_attr_selftest) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_selftest.attr.name);
		}
		if (device_create_file(camera_ois_dev,
					&dev_attr_ois_power) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_ois_power.attr.name);
		}
		if (device_create_file(camera_ois_dev,
					&dev_attr_ois_rawdata) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_ois_rawdata.attr.name);
		}
		if (device_create_file(camera_ois_dev,
					&dev_attr_oisfw) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_oisfw.attr.name);
		}
		if (device_create_file(camera_ois_dev,
					&dev_attr_ois_diff) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_ois_diff.attr.name);
		}
		if (device_create_file(camera_ois_dev,
					&dev_attr_ois_exif) < 0) {
			printk(KERN_ERR
				"failed to create ois device file, %s\n",
				dev_attr_ois_exif.attr.name);
		}
	}
#endif

	sysfs_core = core;
#endif

#ifdef USE_OWN_FAULT_HANDLER
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0))
	exynos_sysmmu_set_fault_handler(fimc_is_dev, fimc_is_fault_handler);
#else
	iovmm_set_fault_handler(fimc_is_dev, fimc_is_fault_handler, NULL);
#endif
#endif

	dbg("%s : fimc_is_front_%d probe success\n", __func__, pdev->id);

	/* set sysfs for debuging */
	sysfs_debug.en_clk_gate = 0;
	sysfs_debug.en_dvfs = 1;
#ifdef ENABLE_CLOCK_GATE
	sysfs_debug.en_clk_gate = 1;
#ifdef HAS_FW_CLOCK_GATE
	sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_FW;
#else
	sysfs_debug.clk_gate_mode = CLOCK_GATE_MODE_HOST;
#endif
#endif
	ret = sysfs_create_group(&core->pdev->dev.kobj, &fimc_is_debug_attr_group);

#ifdef ENABLE_DVFS
	{
		struct fimc_is_resourcemgr *resourcemgr;
		resourcemgr = &core->resourcemgr;
		/* dvfs controller init */
		ret = fimc_is_dvfs_init(resourcemgr);
		if (ret)
			err("%s: fimc_is_dvfs_init failed!\n", __func__);
	}
#endif

	info("%s:end\n", __func__);
	return 0;

#ifdef CONFIG_USE_VENDER_FEATURE
p_err5:
#if defined(CONFIG_PM_RUNTIME)
	__pm_runtime_disable(&pdev->dev, false);
#endif
#endif /* CONFIG_USE_VENDER_FEATURE */
p_err4:
	v4l2_device_unregister(&core->v4l2_dev);
p_err3:
	iounmap(core->regs);
p_err2:
	release_mem_region(regs_res->start, resource_size(regs_res));
p_err1:
	kfree(core);
	return ret;
}

static int fimc_is_remove(struct platform_device *pdev)
{
	dbg("%s\n", __func__);

#ifdef CONFIG_USE_VENDER_FEATURE
	if (camera_front_dev) {
		device_remove_file(camera_front_dev, &dev_attr_front_sensorid);
		device_remove_file(camera_front_dev, &dev_attr_front_camtype);
		device_remove_file(camera_front_dev, &dev_attr_front_camfw);
	}

	if (camera_rear_dev) {
		device_remove_file(camera_rear_dev, &dev_attr_rear_sensorid);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camtype);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camfw);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camfw_full);
		device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_user);
		device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_factory);
#ifdef CONFIG_COMPANION_USE
		device_remove_file(camera_rear_dev, &dev_attr_rear_companionfw);
		device_remove_file(camera_rear_dev, &dev_attr_rear_companionfw_full);
#endif
		device_remove_file(camera_rear_dev, &dev_attr_rear_calcheck);
#ifdef CONFIG_COMPANION_USE
		device_remove_file(camera_rear_dev, &dev_attr_isp_core);
#endif
#ifdef CONFIG_OIS_USE
		device_remove_file(camera_ois_dev, &dev_attr_fw_update);
#endif
	}

#ifdef CONFIG_OIS_USE
	if (camera_ois_dev) {
		device_remove_file(camera_ois_dev, &dev_attr_selftest);
		device_remove_file(camera_ois_dev, &dev_attr_ois_power);
		device_remove_file(camera_ois_dev, &dev_attr_ois_rawdata);
		device_remove_file(camera_ois_dev, &dev_attr_oisfw);
		device_remove_file(camera_ois_dev, &dev_attr_ois_diff);
		device_remove_file(camera_ois_dev, &dev_attr_ois_exif);
	}
#endif

	if (camera_class) {
		if (camera_front_dev)
			device_destroy(camera_class, camera_front_dev->devt);

		if (camera_rear_dev)
			device_destroy(camera_class, camera_rear_dev->devt);

#ifdef CONFIG_OIS_USE
		if (camera_ois_dev)
			device_destroy(camera_class, camera_ois_dev->devt);
#endif
	}

	class_destroy(camera_class);
#endif

	return 0;
}

static const struct dev_pm_ops fimc_is_pm_ops = {
	.suspend		= fimc_is_suspend,
	.resume			= fimc_is_resume,
	.runtime_suspend	= fimc_is_runtime_suspend,
	.runtime_resume		= fimc_is_runtime_resume,
};

#ifdef CONFIG_USE_VENDER_FEATURE
#if defined(CONFIG_COMPANION_USE) || defined(CONFIG_CAMERA_EEPROM_SUPPORT)
static int fimc_is_i2c0_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct fimc_is_core *core;
	static bool probe_retried = false;

	if (!fimc_is_dev)
		goto probe_defer;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		goto probe_defer;

	core->client0 = client;

	pr_info("%s %s: fimc_is_i2c0 driver probed!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));

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

static int fimc_is_i2c0_remove(struct i2c_client *client)
{
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id fimc_is_i2c0_dt_ids[] = {
	{ .compatible = "samsung,fimc_is_i2c0",},
	{},
};
MODULE_DEVICE_TABLE(of, fimc_is_i2c0_dt_ids);
#endif

static const struct i2c_device_id fimc_is_i2c0_id[] = {
	{"fimc_is_i2c0", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, fimc_is_i2c0_id);

static struct i2c_driver fimc_is_i2c0_driver = {
	.driver = {
		.name = "fimc_is_i2c0",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = fimc_is_i2c0_dt_ids,
#endif
	},
	.probe = fimc_is_i2c0_probe,
	.remove = fimc_is_i2c0_remove,
	.id_table = fimc_is_i2c0_id,
};
module_i2c_driver(fimc_is_i2c0_driver);
#endif

#if defined(CONFIG_OF) && defined(CONFIG_COMPANION_USE)
static int of_fimc_is_spi_dt(struct device *dev, struct fimc_is_spi_gpio *spi_gpio, struct fimc_is_core *core)
{
	struct device_node *np;
	int ret;

	np = of_find_compatible_node(NULL,NULL,"samsung,fimc_is_spi1");
	if(np == NULL) {
		pr_err("compatible: fail to read, spi_parse_dt\n");
		return -ENODEV;
	}

	ret = of_property_read_string(np, "fimc_is_spi_sclk", (const char **) &spi_gpio->spi_sclk);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		return -ENODEV;
	}

	ret = of_property_read_string(np, "fimc_is_spi_ssn",(const char **) &spi_gpio->spi_ssn);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		return -ENODEV;
	}

	ret = of_property_read_string(np, "fimc_is_spi_miso",(const char **) &spi_gpio->spi_miso);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		return -ENODEV;
	}

	ret = of_property_read_string(np, "fimc_is_spi_mois",(const char **) &spi_gpio->spi_mois);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		return -ENODEV;
	}

	pr_info("sclk = %s, ssn = %s, miso = %s, mois = %s\n", spi_gpio->spi_sclk, spi_gpio->spi_ssn, spi_gpio->spi_miso, spi_gpio->spi_mois);

	return 0;
}
#endif
#endif

#ifdef CONFIG_OF
static int fimc_is_spi_probe(struct spi_device *spi)
{
	int ret = 0;
	struct fimc_is_core *core;

	BUG_ON(!fimc_is_dev);

	dbg_core("%s\n", __func__);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}
	spi->mode = SPI_MODE_0;

	/* spi->bits_per_word = 16; */
	if (spi_setup(spi)) {
		pr_err("failed to setup spi for fimc_is_spi\n");
		ret = -EINVAL;
		goto exit;
	}

	if (!strncmp(spi->modalias, "fimc_is_spi0", 12))
		core->spi0 = spi;

	if (!strncmp(spi->modalias, "fimc_is_spi1", 12)) {
		core->spi1 = spi;
#ifdef CONFIG_COMPANION_USE
		ret = of_fimc_is_spi_dt(&spi->dev,&core->spi_gpio, core);
		if (ret) {
			pr_err("[%s] of_fimc_is_spi_dt parse dt failed\n", __func__);
			return ret;
		}
#endif
	}

exit:
	return ret;
}

static int fimc_is_spi_remove(struct spi_device *spi)
{
	return 0;
}

#if defined(CONFIG_USE_VENDER_FEATURE) && defined(CONFIG_COMPANION_USE)
static int fimc_is_fan53555_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct fimc_is_core *core;
	int ret = 0;
#ifdef CONFIG_SOC_EXYNOS5422
	struct regulator *regulator = NULL;
	const char power_name[] = "CAM_IO_1.8V_AP";
#endif
	struct device_node *np;
	int gpio_comp_en;

	BUG_ON(!fimc_is_dev);

	pr_info("%s start\n",__func__);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	np = of_find_compatible_node(NULL,NULL,"samsung,fimc_is_fan53555");
	if(np == NULL) {
		pr_err("compatible: fail to read, fan_parse_dt\n");
		return -ENODEV;
	}

	gpio_comp_en = of_get_named_gpio(np, "comp_en", 0);
	if (!gpio_is_valid(gpio_comp_en))
		pr_err("failed to get comp en gpio\n");

	ret = gpio_request(gpio_comp_en,"COMP_EN");
	if (ret < 0 )
		pr_err("gpio_request_error(%d)\n",ret);

	gpio_direction_output(gpio_comp_en,1);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: SMBUS Byte Data not Supported\n", __func__);
		ret = -EIO;
		goto err;
	}

	core->companion_dcdc.client = client;
	core->companion_dcdc.type = DCDC_VENDOR_FAN53555;
	core->companion_dcdc.get_vout_val = fan53555_get_vout_val;
	core->companion_dcdc.get_vout_str = fan53555_get_vout_str;
	core->companion_dcdc.set_vout = fan53555_set_vsel0_vout;

#ifdef CONFIG_SOC_EXYNOS5422
	regulator = regulator_get(NULL, power_name);
        if (IS_ERR(regulator)) {
                pr_err("%s : regulator_get(%s) fail\n", __func__, power_name);
                return PTR_ERR(regulator);
        }

        if (regulator_is_enabled(regulator)) {
                pr_info("%s regulator is already enabled\n", power_name);
        } else {
                ret = regulator_enable(regulator);
                if (unlikely(ret)) {
                        pr_err("%s : regulator_enable(%s) fail\n", __func__, power_name);
                        goto err;
                }
        }
        usleep_range(1000, 1000);
#endif

	ret = i2c_smbus_write_byte_data(client, REG_VSEL0, VSEL0_INIT_VAL);
	if (ret < 0){
		pr_err("%s: write error = %d , try again\n", __func__, ret);
		ret = i2c_smbus_write_byte_data(client, REG_VSEL0, VSEL0_INIT_VAL);
		if (ret < 0)
			pr_err("%s: write 2nd error = %d\n", __func__, ret);
	}

        ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
	if(ret < 0){
		pr_err("%s: read error = %d , try again\n", __func__, ret);
		ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
		if (ret < 0)
			pr_err("%s: read 2nd error = %d\n", __func__, ret);
	}
        pr_err("[%s::%d]fan53555 [Read :: %x ,%x]\n\n", __func__, __LINE__, ret,VSEL0_INIT_VAL);

#ifdef CONFIG_SOC_EXYNOS5422
        ret = regulator_disable(regulator);
        if (unlikely(ret)) {
                pr_err("%s: regulator_disable(%s) fail\n", __func__, power_name);
                goto err;
        }
	regulator_put(regulator);
#endif
	gpio_direction_output(gpio_comp_en,0);
	gpio_free(gpio_comp_en);

        pr_info(" %s end\n",__func__);

	return 0;

err:
	gpio_direction_output(gpio_comp_en, 0);
	gpio_free(gpio_comp_en);

#ifdef CONFIG_SOC_EXYNOS5422
	if (!IS_ERR_OR_NULL(regulator)) {
		ret = regulator_disable(regulator);
		if (unlikely(ret)) {
			pr_err("%s: regulator_disable(%s) fail\n", __func__, power_name);
		}
		regulator_put(regulator);
	}
#endif
        return ret;
}

static int fimc_is_fan53555_remove(struct i2c_client *client)
{
	return 0;
}

static int fimc_is_ncp6335b_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct fimc_is_core *core;
	int ret = 0;

	struct device_node *np;
	int gpio_comp_en;

	BUG_ON(!fimc_is_dev);

	pr_info("%s start\n",__func__);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		pr_err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	np = of_find_compatible_node(NULL, NULL, "samsung,fimc_is_ncp6335b");
	if(np == NULL) {
		pr_err("compatible: fail to read, fan_parse_dt\n");
		return -ENODEV;
	}

	gpio_comp_en = of_get_named_gpio(np, "comp_en", 0);
	if (!gpio_is_valid(gpio_comp_en))
		pr_err("failed to get comp en gpio\n");

	ret = gpio_request(gpio_comp_en,"COMP_EN");
	if (ret < 0 )
		pr_err("gpio_request_error(%d)\n",ret);

	gpio_direction_output(gpio_comp_en,1);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: SMBUS Byte Data not Supported\n", __func__);
		ret = -EIO;
		goto err;
	}

	core->companion_dcdc.client = client;
	core->companion_dcdc.type = DCDC_VENDOR_NCP6335B;
	core->companion_dcdc.get_vout_val = ncp6335b_get_vout_val;
	core->companion_dcdc.get_vout_str = ncp6335b_get_vout_str;
	core->companion_dcdc.set_vout = ncp6335b_set_voltage;

	ret = ncp6335b_set_voltage(client, 0xC0);
	if (ret < 0) {
		pr_err("%s: error, fail to set voltage\n", __func__);
		goto err;
	}

	ret = ncp6335b_read_voltage(client);
	if (ret < 0) {
		pr_err("%s: error, fail to read voltage\n", __func__);
		goto err;
	}

	pr_info("%s %s: ncp6335b probed\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));

err:
	gpio_direction_output(gpio_comp_en,0);
	gpio_free(gpio_comp_en);

	return ret;
}

static int fimc_is_ncp6335b_remove(struct i2c_client *client)
{
	return 0;
}
#endif

static const struct of_device_id exynos_fimc_is_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is",
	},
	{
		.compatible = "samsung,fimc_is_spi0",
	},
	{
		.compatible = "samsung,fimc_is_spi1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_match);

static struct spi_driver fimc_is_spi0_driver = {
	.driver = {
		.name = "fimc_is_spi0",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = exynos_fimc_is_match,
	},
	.probe	= fimc_is_spi_probe,
	.remove	= fimc_is_spi_remove,
};

module_spi_driver(fimc_is_spi0_driver);

static struct spi_driver fimc_is_spi1_driver = {
	.driver = {
		.name = "fimc_is_spi1",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = exynos_fimc_is_match,
	},
	.probe	= fimc_is_spi_probe,
	.remove = fimc_is_spi_remove,
};

module_spi_driver(fimc_is_spi1_driver);

#ifdef CONFIG_COMPANION_USE
static struct of_device_id fan53555_dt_ids[] = {
        { .compatible = "samsung,fimc_is_fan53555",},
        {},
};
MODULE_DEVICE_TABLE(of, fan53555_dt_ids);

static const struct i2c_device_id fan53555_id[] = {
        {"fimc_is_fan53555", 0},
        {}
};
MODULE_DEVICE_TABLE(i2c, fan53555_id);

static struct i2c_driver fan53555_driver = {
        .driver = {
                .name = "fimc_is_fan53555",
                .owner  = THIS_MODULE,
                .of_match_table = fan53555_dt_ids,
        },
        .probe = fimc_is_fan53555_probe,
        .remove = fimc_is_fan53555_remove,
        .id_table = fan53555_id,
};
module_i2c_driver(fan53555_driver);

static struct of_device_id ncp6335b_dt_ids[] = {
        { .compatible = "samsung,fimc_is_ncp6335b",},
        {},
};
MODULE_DEVICE_TABLE(of, ncp6335b_dt_ids);

static const struct i2c_device_id ncp6335b_id[] = {
        {"fimc_is_ncp6335b", 0},
        {}
};
MODULE_DEVICE_TABLE(i2c, ncp6335b_id);

static struct i2c_driver ncp6335b_driver = {
        .driver = {
                .name = "fimc_is_ncp6335b",
                .owner  = THIS_MODULE,
                .of_match_table = ncp6335b_dt_ids,
        },
        .probe = fimc_is_ncp6335b_probe,
        .remove = fimc_is_ncp6335b_remove,
        .id_table = ncp6335b_id,
};
module_i2c_driver(ncp6335b_driver);
#endif

static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove		= fimc_is_remove,
	.driver = {
		.name	= FIMC_IS_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
		.of_match_table = exynos_fimc_is_match,
	}
};

module_platform_driver(fimc_is_driver);
#else
static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove	= __devexit_p(fimc_is_remove),
	.driver = {
		.name	= FIMC_IS_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
	}
};

static int __init fimc_is_init(void)
{
	int ret = platform_driver_register(&fimc_is_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);
	return ret;
}

static void __exit fimc_is_exit(void)
{
	platform_driver_unregister(&fimc_is_driver);
}
module_init(fimc_is_init);
module_exit(fimc_is_exit);
#endif

MODULE_AUTHOR("Jiyoung Shin<idon.shin@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS2 driver");
MODULE_LICENSE("GPL");
