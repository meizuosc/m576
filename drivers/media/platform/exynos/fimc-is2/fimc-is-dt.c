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

#include <linux/sched.h>
#include <mach/exynos-fimc-is-module.h>
#include <mach/exynos-fimc-is-sensor.h>
#include <mach/exynos-fimc-is.h>
#include <media/exynos_mc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include "fimc-is-config.h"
#include "fimc-is-dt.h"
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"

#ifdef CONFIG_OF
static int get_pin_lookup_state(struct pinctrl *pinctrl,
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX])
{
	int ret = 0;
	u32 i, j, k;
	char pin_name[30];
	struct pinctrl_state *s;

	for (i = 0; i < SENSOR_SCENARIO_MAX; ++i) {
		for (j = 0; j < GPIO_SCENARIO_MAX; ++j) {
			for (k = 0; k < GPIO_CTRL_MAX; ++k) {
				if (pin_ctrls[i][j][k].act == PIN_FUNCTION) {
					snprintf(pin_name, sizeof(pin_name), "%s%d",
						pin_ctrls[i][j][k].name,
						pin_ctrls[i][j][k].value);
					s = pinctrl_lookup_state(pinctrl, pin_name);
					if (IS_ERR_OR_NULL(s)) {
						err("pinctrl_lookup_state(%s) is failed", pin_name);
						ret = -EINVAL;
						goto p_err;
					}

					pin_ctrls[i][j][k].pin = (ulong)s;
				}
			}
		}
	}

p_err:
	return ret;
}

static int parse_gate_info(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	int ret = 0;
	struct device_node *group_np = NULL;
	struct device_node *gate_info_np;
	struct property *prop;
	struct property *prop2;
	const __be32 *p;
	const char *s;
	u32 i = 0, u = 0;
	struct exynos_fimc_is_clk_gate_info *gate_info;

	/* get subip of fimc-is info */
	gate_info = kzalloc(sizeof(struct exynos_fimc_is_clk_gate_info), GFP_KERNEL);
	if (!gate_info) {
		printk(KERN_ERR "%s: no memory for fimc_is gate_info\n", __func__);
		return -EINVAL;
	}

	s = NULL;
	/* get gate register info */
	prop2 = of_find_property(np, "clk_gate_strs", NULL);
	of_property_for_each_u32(np, "clk_gate_enums", prop, p, u) {
		printk(KERN_INFO "int value: %d\n", u);
		s = of_prop_next_string(prop2, s);
		if (s != NULL) {
			printk(KERN_INFO "String value: %d-%s\n", u, s);
			gate_info->gate_str[u] = s;
		}
	}

	/* gate info */
	gate_info_np = of_find_node_by_name(np, "clk_gate_ctrl");
	if (!gate_info_np) {
		printk(KERN_ERR "%s: can't find fimc_is clk_gate_ctrl node\n", __func__);
		ret = -ENOENT;
		goto p_err;
	}
	i = 0;
	while ((group_np = of_get_next_child(gate_info_np, group_np))) {
		struct exynos_fimc_is_clk_gate_group *group =
				&gate_info->groups[i];
		of_property_for_each_u32(group_np, "mask_clk_on_org", prop, p, u) {
			printk(KERN_INFO "(%d) int1 value: %d\n", i, u);
			group->mask_clk_on_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_self_org", prop, p, u) {
			printk(KERN_INFO "(%d) int2 value: %d\n", i, u);
			group->mask_clk_off_self_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int3 value: %d\n", i, u);
			group->mask_clk_off_depend |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_cond_for_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int4 value: %d\n", i, u);
			group->mask_cond_for_depend |= (1 << u);
		}
		i++;
		printk(KERN_INFO "(%d) [0x%x , 0x%x, 0x%x, 0x%x\n", i,
			group->mask_clk_on_org,
			group->mask_clk_off_self_org,
			group->mask_clk_off_depend,
			group->mask_cond_for_depend
		);
	}

	pdata->gate_info = gate_info;
	pdata->gate_info->clk_on_off = exynos_fimc_is_clk_gate;

	return 0;
p_err:
	kfree(gate_info);
	return ret;
}

static int parse_dvfs_data(struct exynos_platform_fimc_is *pdata, struct device_node *np, int index)
{
	u32 temp;
	char *pprop;

	if (index < 0 || index >= FIMC_IS_DVFS_TABLE_IDX_MAX) {
		err("dvfs index(%d) is invalid", index);
		return -EINVAL;
	}

	DT_READ_U32(np, "default_int", pdata->dvfs_data[index][FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "default_cam", pdata->dvfs_data[index][FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "default_mif", pdata->dvfs_data[index][FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "default_i2c", pdata->dvfs_data[index][FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_preview_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_preview_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_preview_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_preview_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_video_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_video_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_video_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_video_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_video_whd_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_video_whd_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_video_whd_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_video_whd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_video_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_video_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_video_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_video_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_video_whd_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_video_whd_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_video_whd_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_video_whd_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_vt1_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_vt1_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_vt1_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_vt1_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_vt2_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_vt2_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_vt2_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_vt2_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_preview_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_PREVIEW][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_preview_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_PREVIEW][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_preview_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_PREVIEW][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_preview_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_PREVIEW][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_video_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_video_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_video_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_video_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_video_whd_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_video_whd_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_video_whd_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_video_whd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_video_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_video_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_video_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_video_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_video_whd_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_video_whd_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_video_whd_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_video_whd_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_fhd_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_fhd_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_fhd_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_fhd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_whd_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_whd_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_whd_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_whd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_uhd_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_uhd_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_uhd_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_uhd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_video_fhd_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_video_fhd_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_video_fhd_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_video_fhd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_video_uhd_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_video_uhd_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_video_uhd_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_video_uhd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_video_fhd_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_video_fhd_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_video_fhd_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_video_fhd_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_video_uhd_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_video_uhd_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_video_uhd_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_video_uhd_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_preview_int", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_preview_cam", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_preview_mif", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_preview_i2c", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_video_int", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_video_cam", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_video_mif", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_video_i2c", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_video_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_video_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_video_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_video_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "preivew_high_speed_fps_int", pdata->dvfs_data[index][FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "preivew_high_speed_fps_cam", pdata->dvfs_data[index][FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "preivew_high_speed_fps_mif", pdata->dvfs_data[index][FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "preivew_high_speed_fps_i2c", pdata->dvfs_data[index][FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "video_high_speed_60fps_int", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "video_high_speed_60fps_cam", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "video_high_speed_60fps_mif", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "video_high_speed_60fps_i2c", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "video_high_speed_120fps_int", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "video_high_speed_120fps_cam", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "video_high_speed_120fps_mif", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "video_high_speed_120fps_i2c", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "video_high_speed_240fps_int", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "video_high_speed_240fps_cam", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "video_high_speed_240fps_mif", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "video_high_speed_240fps_i2c", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS][FIMC_IS_DVFS_I2C]);
	#if defined(CONFIG_CAMERA_CUSTOM_SUPPORT)
	DT_READ_U32(np, "front_video_high_speed_120fps_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_video_high_speed_120fps_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_video_high_speed_120fps_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_video_high_speed_120fps_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_I2C]);
	#endif
	DT_READ_U32(np, "max_int", pdata->dvfs_data[index][FIMC_IS_SN_MAX][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "max_cam", pdata->dvfs_data[index][FIMC_IS_SN_MAX][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "max_mif", pdata->dvfs_data[index][FIMC_IS_SN_MAX][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "max_i2c", pdata->dvfs_data[index][FIMC_IS_SN_MAX][FIMC_IS_DVFS_I2C]);

	return 0;
}

int fimc_is_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is *pdata;
	struct device *dev;
	struct device_node *dvfs_np = NULL;
	struct device_node *dvfs_table_np = NULL;
	struct device_node *np;
	const char *dvfs_table_desc;
	int i = 0;

	BUG_ON(!pdev);

	dev = &pdev->dev;
	np = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is), GFP_KERNEL);
	if (!pdata) {
		err("no memory for platform data");
		return -ENOMEM;
	}

	pdata->clk_cfg = exynos_fimc_is_cfg_clk;
	pdata->clk_on = exynos_fimc_is_clk_on;
	pdata->clk_off = exynos_fimc_is_clk_off;
	#ifdef FIMC_IS_DBG_CLK
	pdata->print_clk = exynos_fimc_is_print_clk;
	#else
	pdata->print_clk = NULL;
	#endif

	if (parse_gate_info(pdata, np) < 0)
		err("can't parse clock gate info node");

	ret = of_property_read_u32(np, "rear_sensor_id", &pdata->rear_sensor_id);
	if (ret) {
		err("rear_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(np, "front_sensor_id", &pdata->front_sensor_id);
	if (ret) {
		err("front_sensor_id read is fail(%d)", ret);
	}

	pdata->check_sensor_vendor = of_property_read_bool(np, "check_sensor_vendor");
	if (!pdata->check_sensor_vendor) {
		pr_info("check_sensor_vendor not use(%d)", pdata->check_sensor_vendor);
	}

#ifdef CONFIG_OIS_USE
	pdata->use_ois = of_property_read_bool(np, "use_ois");
	if (!pdata->use_ois) {
		err("use_ois not use(%d)", pdata->use_ois);
	}
#endif /* CONFIG_OIS_USE */

	pdata->use_ois_hsi2c = of_property_read_bool(np, "use_ois_hsi2c");
	if (!pdata->use_ois_hsi2c) {
		err("use_ois_hsi2c not use(%d)", pdata->use_ois_hsi2c);
	}

	pdata->use_module_check = of_property_read_bool(np, "use_module_check");
	if (!pdata->use_module_check) {
		err("use_module_check not use(%d)", pdata->use_module_check);
	}

	pdata->skip_cal_loading = of_property_read_bool(np, "skip_cal_loading");
	if (!pdata->skip_cal_loading) {
		pr_info("skip_cal_loading not use(%d)", pdata->skip_cal_loading);
	}

	i = 0;
	dvfs_np = of_find_node_by_name(np, "fimc_is_dvfs");
	if (!dvfs_np) {
		err("can't find fimc_is_dvfs node");
		ret = -ENOENT;
		goto p_err;
	}

	/* Maybe there are two version of DVFS table for 1.0 and 3.2 HAL */
	ret = of_property_read_u32(dvfs_np,
			GET_KEY_FOR_DVFS_TBL_IDX(IS_HAL_VER_1_0),
			&pdata->dvfs_hal_1_0_table_idx);
	if (ret) {
		probe_info("dvfs_hal_1_0_table idx was not existed(%d)", ret);
		pdata->dvfs_hal_1_0_table_idx = 0;
	}

	ret = of_property_read_u32(dvfs_np,
			GET_KEY_FOR_DVFS_TBL_IDX(IS_HAL_VER_3_2),
			&pdata->dvfs_hal_3_2_table_idx);
	if (ret) {
		probe_info("dvfs_hal_3_2_table idx was not existed(%d)", ret);
		pdata->dvfs_hal_3_2_table_idx = 0;
	}

	while ((dvfs_table_np = of_get_next_child(dvfs_np, dvfs_table_np))) {
		of_property_read_string(dvfs_table_np, "desc", &dvfs_table_desc);
		probe_info("dvfs table[%d] %s", i, dvfs_table_desc);
		dvfs_table_desc = NULL;
		parse_dvfs_data(pdata, dvfs_table_np, i);
		i++;
	}
	/* if i is 0, dvfs table is only one */
	if (i == 0) {
		pdata->dvfs_hal_1_0_table_idx = 0;
		pdata->dvfs_hal_3_2_table_idx = 0;
		probe_info("dvfs table is only one");
		parse_dvfs_data(pdata, dvfs_np, 0);
	}
	dev->platform_data = pdata;

	return 0;

p_err:
	kfree(pdata);
	return ret;
}

int fimc_is_sensor_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_sensor), GFP_KERNEL);
	if (!pdata) {
		err("%s: no memory for platform data", __func__);
		return -ENOMEM;
	}

	pdata->iclk_get = exynos_fimc_is_sensor_iclk_get;
	pdata->iclk_cfg = exynos_fimc_is_sensor_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_sensor_iclk_on;
	pdata->iclk_off = exynos_fimc_is_sensor_iclk_off;
	pdata->mclk_on = exynos_fimc_is_sensor_mclk_on;
	pdata->mclk_off = exynos_fimc_is_sensor_mclk_off;

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "csi_ch", &pdata->csi_ch);
	if (ret) {
		err("csi_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "flite_ch", &pdata->flite_ch);
	if (ret) {
		err("flite_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "is_bns", &pdata->is_bns);
	if (ret) {
		err("is_bns read is fail(%d)", ret);
		goto p_err;
	}

#if defined(CONFIG_CAMERA_SUPPORT_PDAF)
	ret = of_property_read_u32(dnode, "pdaf_enable", &pdata->pdaf_enable);
	if (ret) {
		err("pdaf_enable read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "pdaf_ch", &pdata->pdaf_ch);
	if (ret) {
		err("pdaf_ch read is fail(%d)", ret);
		goto p_err;
	}
#endif
	pdev->id = pdata->id;
	dev->platform_data = pdata;

	return ret;

p_err:
	kfree(pdata);
	return ret;
}

static int parse_af_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->af_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->af_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->af_i2c_ch);

	return 0;
}

static int parse_flash_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->flash_product_name);
	DT_READ_U32(dnode, "flash_first_gpio", pdata->flash_first_gpio);
	DT_READ_U32(dnode, "flash_second_gpio", pdata->flash_second_gpio);

	return 0;
}

static int parse_companion_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->companion_product_name);
	DT_READ_U32(dnode, "spi_channel", pdata->companion_spi_channel);
	DT_READ_U32(dnode, "i2c_addr", pdata->companion_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->companion_i2c_ch);

	return 0;
}

static int parse_ois_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->ois_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->ois_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->ois_i2c_ch);

	return 0;
}

int fimc_is_sensor_module_parse_dt(struct platform_device *pdev,
	fimc_is_moudle_dt_callback module_callback)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *pdata;
	struct device_node *dnode;
	struct device_node *af_np;
	struct device_node *flash_np;
	struct device_node *companion_np;
	struct device_node *ois_np;
	struct device *dev;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);
	BUG_ON(!module_callback);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_module), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_fimc_is_module_pins_cfg;
	pdata->gpio_dbg = exynos_fimc_is_module_pins_dbg;

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_ch", &pdata->sensor_i2c_ch);
	if (ret) {
		err("i2c_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_addr", &pdata->sensor_i2c_addr);
	if (ret) {
		err("i2c_addr read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "position", &pdata->position);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	af_np = of_find_node_by_name(dnode, "af");
	if (!af_np) {
		pdata->af_product_name = ACTUATOR_NAME_NOTHING;
	} else {
		parse_af_data(pdata, af_np);
	}

	flash_np = of_find_node_by_name(dnode, "flash");
	if (!flash_np) {
		pdata->flash_product_name = FLADRV_NAME_NOTHING;
	} else {
		parse_flash_data(pdata, flash_np);
	}

	companion_np = of_find_node_by_name(dnode, "companion");
	if (!companion_np) {
		pdata->companion_product_name = COMPANION_NAME_NOTHING;
	} else {
		parse_companion_data(pdata, companion_np);
	}

	ois_np = of_find_node_by_name(dnode, "ois");
	if (!ois_np) {
		pdata->ois_product_name = OIS_NAME_NOTHING;
	} else {
		parse_ois_data(pdata, ois_np);
	}

	ret = module_callback(pdev, pdata);
	if (ret) {
		err("sensor dt callback is fail(%d)", ret);
		goto p_err;
	}

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		err("devm_pinctrl_get is fail");
		goto p_err;
	}

	ret = get_pin_lookup_state(pdata->pinctrl, pdata->pin_ctrls);
	if (ret) {
		err("get_pin_lookup_state is fail(%d)", ret);
		goto p_err;
	}

	dev->platform_data = pdata;

	return ret;

p_err:
	kfree(pdata);
	return ret;
}

int fimc_is_spi_parse_dt(struct fimc_is_spi *spi)
{
	int ret = 0;
	struct device_node *np;
	struct fimc_is_spi_gpio *gpio;

	BUG_ON(!spi);

	gpio = &spi->gpio;

	np = of_find_compatible_node(NULL,NULL, spi->node);
	if(np == NULL) {
		pr_err("compatible: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_spi_clk", (const char **) &gpio->clk);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_spi_ssn",(const char **) &gpio->ssn);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_spi_miso",(const char **) &gpio->miso);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_spi_mosi",(const char **) &gpio->mosi);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_spi_pinname", (const char **) &gpio->pinname);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	info("[SPI] clk = %s, ssn = %s, miso = %s, mosi = %s\n", gpio->clk, gpio->ssn,
		gpio->miso, gpio->mosi);

p_err:
	return ret;
}
#else
struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif
