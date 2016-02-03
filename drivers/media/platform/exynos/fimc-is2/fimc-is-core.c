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
#include "fimc-is-debug.h"
#include "fimc-is-hw.h"
#include "fimc-is-err.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-clk-gate.h"
#include "fimc-is-dvfs.h"
#include "include/fimc-is-module.h"
#include "fimc-is-device-sensor.h"

#ifdef CONFIG_COMPANION_USE
#include "fimc-is-fan53555.h"
#include "fimc-is-ncp6335b.h"
#endif
#ifdef CONFIG_USE_VENDER_FEATURE
#include "fimc-is-sec-define.h"
#endif

#ifdef ENABLE_FAULT_HANDLER
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
#include <linux/exynos_iovmm.h>
#else
#include <plat/sysmmu.h>
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#define PM_QOS_CAM_THROUGHPUT	PM_QOS_RESERVED
#endif

struct device *fimc_is_dev = NULL;

extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;

extern int fimc_is_30s_video_probe(void *data);
extern int fimc_is_30c_video_probe(void *data);
extern int fimc_is_30p_video_probe(void *data);
extern int fimc_is_31s_video_probe(void *data);
extern int fimc_is_31c_video_probe(void *data);
extern int fimc_is_31p_video_probe(void *data);
extern int fimc_is_i0s_video_probe(void *data);
extern int fimc_is_i0c_video_probe(void *data);
extern int fimc_is_i0p_video_probe(void *data);
extern int fimc_is_i1s_video_probe(void *data);
extern int fimc_is_i1c_video_probe(void *data);
extern int fimc_is_i1p_video_probe(void *data);
extern int fimc_is_dis_video_probe(void *data);
extern int fimc_is_scc_video_probe(void *data);
extern int fimc_is_scp_video_probe(void *data);
#ifdef CONFIG_USE_VENDER_FEATURE
extern int fimc_is_create_sysfs(struct fimc_is_core *core);
extern int fimc_is_destroy_sysfs(struct fimc_is_core *core);
#endif

/* sysfs global variable for debug */
struct fimc_is_sysfs_debug sysfs_debug;

#ifdef CONFIG_CPU_THERMAL_IPA
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

	for (i = 0; i < FIMC_IS_STREAM_COUNT; i++) {
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

#ifdef ENABLE_FAULT_HANDLER
static void __fimc_is_fault_handler(struct device *dev)
{
	u32 i, j, k;
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_subdev *subdev;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_resourcemgr *resourcemgr;

	core = dev_get_drvdata(dev);
	if (core) {
		resourcemgr = &core->resourcemgr;

		fimc_is_hw_fault(&core->interface);
		/* dump FW page table 1nd(~16KB), 2nd(16KB~32KB) */
		fimc_is_hw_memdump(&core->interface,
			resourcemgr->minfo.kvaddr + 0x017F8000, /* TTB_BASE ~ 32KB */
			resourcemgr->minfo.kvaddr + 0x017F8000 + 0x8000);
		fimc_is_hw_logdump(&core->interface);

		/* SENSOR */
		for (i = 0; i < FIMC_IS_STREAM_COUNT; i++) {
			sensor = &core->sensor[i];
			framemgr = GET_FRAMEMGR(sensor->vctx);
			if (test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state) && framemgr) {
				for (j = 0; j < FRAMEMGR_MAX_REQUEST; ++j) {
					for (k = 0; k < framemgr->frame[j].planes; k++) {
						#ifdef PRINT_BUFADDR
						pr_err("[SS%d] BUF[%d][%d] = 0x%08X(0x%lX) size %d \n", i, j, k,
							framemgr->frame[j].dvaddr_buffer[k],
							framemgr->frame[j].memory,
							framemgr->frame[j].sizes[k]);
						#else
						pr_err("[SS%d] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
							framemgr->frame[j].dvaddr_buffer[k],
							framemgr->frame[j].memory);
						#endif
					}
				}
			}
		}

		/* ISCHAIN */
		for (i = 0; i < FIMC_IS_STREAM_COUNT; i++) {
			ischain = &core->ischain[i];
			if (test_bit(FIMC_IS_ISCHAIN_OPEN, &ischain->state)) {
				/* 3AA */
				subdev = &ischain->group_3aa.leader;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < FRAMEMGR_MAX_REQUEST; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							#ifdef PRINT_BUFADDR
							pr_err("[%d][3XS] BUF[%d][%d] = 0x%08X(0x%lX) size %d \n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory,
								framemgr->frame[j].sizes[k]);
							#else
							pr_err("[%d][3XS] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory);
							#endif
						}
					}
				}
				/* 3AAC */
				subdev = &ischain->txc;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < FRAMEMGR_MAX_REQUEST; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							#ifdef PRINT_BUFADDR
							pr_err("[%d][3XC] BUF[%d][%d] = 0x%08X(0x%lX) size %d \n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory,
								framemgr->frame[j].sizes[k]);
							#else
							pr_err("[%d][3XC] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory);
							#endif
						}
					}
				}
				/* 3AAP */
				subdev = &ischain->txp;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < FRAMEMGR_MAX_REQUEST; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							#ifdef PRINT_BUFADDR
							pr_err("[%d][3XP] BUF[%d][%d] = 0x%08X(0x%lX) size %d \n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory,
								framemgr->frame[j].sizes[k]);
							#else
							pr_err("[%d][3XP] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory);
							#endif
						}
					}
				}
				/* ISP */
				subdev = &ischain->group_isp.leader;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < FRAMEMGR_MAX_REQUEST; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							#ifdef PRINT_BUFADDR
							pr_err("[%d][IXS] BUF[%d][%d] = 0x%08X(0x%lX) size %d \n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory,
								framemgr->frame[j].sizes[k]);
							#else
							pr_err("[%d][IXS] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory);
							#endif
						}
					}
				}
				/* ISPC */
				subdev = &ischain->ixc;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < FRAMEMGR_MAX_REQUEST; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							#ifdef PRINT_BUFADDR
							pr_err("[%d][IXC] BUF[%d][%d] = 0x%08X(0x%lX) size %d \n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory,
								framemgr->frame[j].sizes[k]);
							#else
							pr_err("[%d][IXC] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory);
							#endif
						}
					}
				}
				/* ISPP */
				subdev = &ischain->ixp;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < FRAMEMGR_MAX_REQUEST; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							#ifdef PRINT_BUFADDR
							pr_err("[%d][IXP] BUF[%d][%d] = 0x%08X(0x%lX) size %d \n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory,
								framemgr->frame[j].sizes[k]);
							#else
							pr_err("[%d][IXP] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory);
							#endif
						}
					}
				}
				/* DIS */
				subdev = &ischain->group_dis.leader;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < FRAMEMGR_MAX_REQUEST; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							#ifdef PRINT_BUFADDR
							pr_err("[%d][DIS] BUF[%d][%d] = 0x%08X(0x%lX) size %d \n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory,
								framemgr->frame[j].sizes[k]);
							#else
							pr_err("[%d][DIS] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory);
							#endif
						}
					}
				}
				/* SCC */
				subdev = &ischain->scc;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < FRAMEMGR_MAX_REQUEST; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							#ifdef PRINT_BUFADDR
							pr_err("[%d][SCC] BUF[%d][%d] = 0x%08X(0x%lX) size %d \n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory,
								framemgr->frame[j].sizes[k]);
							#else
							pr_err("[%d][SCC] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory);
							#endif
						}
					}
				}
				/* SCP */
				subdev = &ischain->scp;
				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state) && framemgr) {
					for (j = 0; j < FRAMEMGR_MAX_REQUEST; ++j) {
						for (k = 0; k < framemgr->frame[j].planes; k++) {
							#ifdef PRINT_BUFADDR
							pr_err("[%d][SCP] BUF[%d][%d] = 0x%08X(0x%lX) size %d \n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory,
								framemgr->frame[j].sizes[k]);
							#else
							pr_err("[%d][SCP] BUF[%d][%d] = 0x%08X(0x%lX)\n", i, j, k,
								framemgr->frame[j].dvaddr_buffer[k],
								framemgr->frame[j].memory);
							#endif
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
#endif /* ENABLE_FAULT_HANDLER */

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
		for (i = 0; i < FIMC_IS_STREAM_COUNT; i++) {
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
	u32 stream;

	info("%s:start\n", __func__);

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
#ifdef CONFIG_OF
		ret = fimc_is_parse_dt(pdev);
		if (ret) {
			err("fimc_is_parse_dt is fail(%d)", ret);
			return ret;
		}

		pdata = dev_get_platdata(&pdev->dev);
#else
		BUG();
#endif
	}

	core = kzalloc(sizeof(struct fimc_is_core), GFP_KERNEL);
	if (!core) {
		probe_err("core is NULL");
		return -ENOMEM;
	}

	fimc_is_dev = &pdev->dev;
	ret = dev_set_drvdata(fimc_is_dev, core);
	if (ret) {
		probe_err("dev_set_drvdata is fail(%d)", ret);
		kfree(core);
		return ret;
	}

#ifdef USE_ION_ALLOC
	core->fimc_ion_client = ion_client_create(ion_exynos, "fimc-is");
#endif
	core->pdev = pdev;
	core->pdata = pdata;
	core->running_rear_camera = false;
	core->running_front_camera = false;
	core->current_position = SENSOR_POSITION_REAR;
	device_init_wakeup(&pdev->dev, true);

	/* init mutex for spi read */
	mutex_init(&core->spi_lock);

	/* for mideaserver force down */
	atomic_set(&core->rsccount, 0);

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

	ret = fimc_is_mem_probe(&core->resourcemgr.mem, core->pdev);
	if (ret) {
		probe_err("fimc_is_mem_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = fimc_is_resourcemgr_probe(&core->resourcemgr, core);
	if (ret) {
		probe_err("fimc_is_resourcemgr_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = fimc_is_interface_probe(&core->interface,
		&core->resourcemgr.minfo,
		(ulong)core->regs,
		core->irq,
		core);
	if (ret) {
		probe_err("fimc_is_interface_probe is fail(%d)", ret);
		goto p_err3;
	}

	ret = fimc_is_debug_probe();
	if (ret) {
		probe_err("fimc_is_deubg_probe is fail(%d)", ret);
		goto p_err3;
	}

	/* group initialization */
	ret = fimc_is_groupmgr_probe(&core->groupmgr);
	if (ret) {
		probe_err("fimc_is_groupmgr_probe is fail(%d)", ret);
		goto p_err3;
	}

	for (stream = 0; stream < FIMC_IS_STREAM_COUNT; ++stream) {
		ret = fimc_is_ischain_probe(&core->ischain[stream],
			&core->interface,
			&core->resourcemgr,
			&core->groupmgr,
			&core->resourcemgr.mem,
			core->pdev,
			stream);
		if (ret) {
			probe_err("fimc_is_ischain_probe(%d) is fail(%d)", stream, ret);
			goto p_err3;
		}
	}

	ret = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register fimc-is v4l2 device\n");
		goto p_err3;
	}

#ifdef SOC_30S
	/* video entity - 3a0 */
	fimc_is_30s_video_probe(core);
#endif

#ifdef SOC_30C
	/* video entity - 3a0 capture */
	fimc_is_30c_video_probe(core);
#endif

#ifdef SOC_30P
	/* video entity - 3a0 preview */
	fimc_is_30p_video_probe(core);
#endif

#ifdef SOC_31S
	/* video entity - 3a1 */
	fimc_is_31s_video_probe(core);
#endif

#ifdef SOC_31C
	/* video entity - 3a1 capture */
	fimc_is_31c_video_probe(core);
#endif

#ifdef SOC_31P
	/* video entity - 3a1 preview */
	fimc_is_31p_video_probe(core);
#endif

#ifdef SOC_I0S
	/* video entity - isp0 */
	fimc_is_i0s_video_probe(core);
#endif

#ifdef SOC_I0C
	/* video entity - isp0 capture */
	fimc_is_i0c_video_probe(core);
#endif

#ifdef SOC_I0P
	/* video entity - isp0 preview */
	fimc_is_i0p_video_probe(core);
#endif

#ifdef SOC_I1S
	/* video entity - isp1 */
	fimc_is_i1s_video_probe(core);
#endif

#ifdef SOC_I1C
	/* video entity - isp1 capture */
	fimc_is_i1c_video_probe(core);
#endif

#ifdef SOC_I1P
	/* video entity - isp1 preview */
	fimc_is_i1p_video_probe(core);
#endif

	/* video entity - dis */
	fimc_is_dis_video_probe(core);

#ifdef SOC_SCC
	/* video entity - scc */
	fimc_is_scc_video_probe(core);
#endif

#ifdef SOC_SCP
	/* video entity - scp */
	fimc_is_scp_video_probe(core);
#endif

	platform_set_drvdata(pdev, core);

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->resourcemgr.mem.alloc_ctx)
		vb2_ion_attach_iommu(core->resourcemgr.mem.alloc_ctx);
#endif
#endif

	EXYNOS_MIF_ADD_NOTIFIER(&exynos_fimc_is_mif_throttling_nb);

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
#endif

#ifdef CONFIG_USE_VENDER_FEATURE
	if (fimc_is_create_sysfs(core)) {
		probe_err("fimc_is_create_sysfs is failed");
		goto p_err4;
	}
#endif

#ifdef ENABLE_FAULT_HANDLER
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0))
	exynos_sysmmu_set_fault_handler(fimc_is_dev, fimc_is_fault_handler);
#else
	iovmm_set_fault_handler(fimc_is_dev, fimc_is_fault_handler, NULL);
#endif
#endif

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

	probe_info("%s:end\n", __func__);
	return 0;

#ifdef CONFIG_USE_VENDER_FEATURE
p_err4:
#if defined(CONFIG_PM_RUNTIME)
	__pm_runtime_disable(&pdev->dev, false);
#endif
	v4l2_device_unregister(&core->v4l2_dev);
#endif /* CONFIG_USE_VENDER_FEATURE */
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

#ifdef CONFIG_USE_VENDER_FEATURE
	struct fimc_is_core *core = dev_get_drvdata(&pdev->dev);
	fimc_is_destroy_sysfs(core);
	dbg("%s\n", __func__);
#endif

	return 0;
}

static const struct dev_pm_ops fimc_is_pm_ops = {
	.suspend		= fimc_is_suspend,
	.resume			= fimc_is_resume,
	.runtime_suspend	= fimc_is_ischain_runtime_suspend,
	.runtime_resume		= fimc_is_ischain_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_match);

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

MODULE_AUTHOR("Gilyeon im<kilyeon.im@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS2 driver");
MODULE_LICENSE("GPL");
