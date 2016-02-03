/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.

 * Alternatively, this program is free software in case of open source projec;
 * you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 */

#include <linux/of_gpio.h>
#include <linux/clk-provider.h>
#include <mach/regs-clock.h>

#include "decon.h"
#include "decon_helper.h"

irqreturn_t decon_ext_irq_handler(int irq, void *dev_data)
{
	struct decon_device *decon = dev_data;
	ktime_t timestamp = ktime_get();
	u32 irq_sts_reg;
	u32 wb_irq_sts_reg;

	spin_lock(&decon->slock);

	irq_sts_reg = decon_read(decon->id, VIDINTCON1);
	wb_irq_sts_reg = decon_read(decon->id, VIDINTCON3);
	if (irq_sts_reg & VIDINTCON1_INT_FRAME) {
		/* VSYNC interrupt, accept it */
		decon_write_mask(decon->id, VIDINTCON1, ~0, VIDINTCON1_INT_FRAME);
		decon->vsync_info.timestamp = timestamp;
		wake_up_interruptible_all(&decon->vsync_info.wait);
	}
	if (irq_sts_reg & VIDINTCON1_INT_FIFO) {
		decon_err("DECON-ext FIFO underrun\n");
		decon_write_mask(decon->id, VIDINTCON1, ~0, VIDINTCON1_INT_FIFO);
	}
	if (irq_sts_reg & VIDINTCON1_INT_I80) {
		decon_warn("DECON-ext frame done interrupt shouldn't happen\n");
		decon_write_mask(decon->id, VIDINTCON1, ~0, VIDINTCON1_INT_I80);
	}
	if (wb_irq_sts_reg & VIDINTCON3_WB_FRAME_DONE) {
		decon_dbg("write-back frame done\n");
		DISP_SS_EVENT_LOG(DISP_EVT_WB_FRAME_DONE, &decon->sd, ktime_set(0, 0));
		decon_write_mask(decon->id, VIDINTCON3, ~0, VIDINTCON3_WB_FRAME_DONE);
		atomic_set(&decon->wb_done, STATE_DONE);
		wake_up_interruptible_all(&decon->wait_frmdone);
		decon_reg_per_frame_off(decon->id);
		decon_reg_update_standalone(decon->id);
		decon_reg_wb_swtrigger(decon->id);
		decon_reg_wait_stop_status_timeout(decon->id, 20 * 1000);
	}

	spin_unlock(&decon->slock);
	return IRQ_HANDLED;
}

int decon_ext_get_clocks(struct decon_device *decon)
{
	decon->res.pclk = clk_get(decon->dev, "pclk_decon1");
	if (IS_ERR_OR_NULL(decon->res.pclk)) {
		decon_err("failed to get pclk_decon1\n");
		return -ENODEV;
	}

	decon->res.aclk = clk_get(decon->dev, "aclk_decon1");
	if (IS_ERR_OR_NULL(decon->res.aclk)) {
		decon_err("failed to get aclk_decon1\n");
		return -ENODEV;
	}

	decon->res.eclk = clk_get(decon->dev, "decon1_eclk");
	if (IS_ERR_OR_NULL(decon->res.eclk)) {
		decon_err("failed to get decon1_eclk\n");
		return -ENODEV;
	}

	decon->res.vclk = clk_get(decon->dev, "decon1_vclk");
	if (IS_ERR_OR_NULL(decon->res.vclk)) {
		decon_err("failed to get decon1_vclk\n");
		return -ENODEV;
	}

	decon->res.dsd = clk_get(decon->dev, "sclk_dsd");
	if (IS_ERR_OR_NULL(decon->res.dsd)) {
		decon_err("failed to get sclk_dsd\n");
		return -ENODEV;
	}

	decon->res.lh_disp1 = clk_get(decon->dev, "aclk_lh_disp1");
	if (IS_ERR_OR_NULL(decon->res.lh_disp1)) {
		decon_err("failed to get aclk_lh_disp1\n");
		return -ENODEV;
	}

	decon->res.aclk_disp = clk_get(decon->dev, "aclk_disp");
	if (IS_ERR_OR_NULL(decon->res.aclk_disp)) {
		decon_err("failed to get aclk_disp\n");
		return -ENODEV;
	}

	decon->res.pclk_disp = clk_get(decon->dev, "pclk_disp");
	if (IS_ERR_OR_NULL(decon->res.pclk_disp)) {
		decon_err("failed to get pclk_disp\n");
		return -ENODEV;
	}

	return 0;
}

void decon_ext_set_clocks(struct decon_device *decon)
{
	struct device *dev = decon->dev;

	decon_clk_set_parent(dev, "m_decon1_eclk", "um_decon1_eclk");
	decon_clk_set_rate(dev, "d_decon1_eclk", 200 * MHZ);

	if (decon->out_type == DECON_OUT_HDMI) {
		decon_clk_set_parent(dev, "m_decon1_vclk", "hdmi_pixel");
	} else {
		decon_clk_set_parent(dev, "um_decon1_vclk", "disp_pll");
#if defined(CONFIG_EXYNOS_DECON_LCD_S6E8AA0)
		decon_clk_set_rate(dev, "d_decon1_vclk", 67 * MHZ);
#else
		decon_clk_set_rate(dev, "d_decon1_vclk", 134   * MHZ);
#endif
		decon_clk_set_parent(dev, "m_decon1_vclk", "d_decon1_vclk");
	}
}

int find_subdev_hdmi(struct decon_device *decon)
{
	struct v4l2_subdev *output_sd;

	output_sd = (struct v4l2_subdev *)module_name_to_driver_data("s5p-hdmi");
	if (!output_sd) {
		decon_err("failed to get hdmi device\n");
		return -ENODEV;
	}

	decon->output_sd = output_sd;
	decon->out_type = DECON_OUT_HDMI;
	decon_info("%s entity get successfully\n", output_sd->name);

	return 0;
}

int create_link_hdmi(struct decon_device *decon)
{
	int i, ret = 0;
	int n_pad = decon->n_sink_pad + decon->n_src_pad;
	int flags = 0;
	char err[80];

	if (!strcmp(decon->output_sd->name, "s5p-hdmi-sd"))
		flags = MEDIA_LNK_FL_ENABLED;
	for (i = decon->n_sink_pad; i < n_pad ; i++) {
		ret = media_entity_create_link(&decon->sd.entity, i,
				&decon->output_sd->entity, 0, flags);
		if (ret) {
			snprintf(err, sizeof(err), "%s --> %s",
					decon->sd.entity.name,
					decon->output_sd->entity.name);
			return ret;
		}
		decon_info("%s[%d] --> [0]%s link is created successfully\n",
				decon->sd.entity.name, i,
				decon->output_sd->entity.name);
	}

	return ret;
}

static struct decon_lcd decon_int_porchs[] =
{
	/*    mode	 vfp vbp hfp hbp  vsa hsa xres yres  width height hs esc fps mic */
	{V4L2_FIELD_NONE, 1, 42, 8, 94,   1, 36,  720, 480,   720, 480,   0, 0, 60, 0, 0},
	{V4L2_FIELD_NONE, 1, 46, 8, 94,   1, 42,  720, 576,   720, 576,   0, 0, 50, 0, 0},
	{V4L2_FIELD_NONE, 1, 27, 8, 194,  1, 168, 1280, 720,  1280, 720,  0, 0, 60, 0, 0},
	{V4L2_FIELD_NONE, 1, 27, 8, 94,   1, 598, 1280, 720,  1280, 720,  0, 0, 50, 0, 0},
	{V4L2_FIELD_NONE, 1, 42, 8, 94,   1, 178, 1920, 1080, 1920, 1080, 0, 0, 60, 0, 0},
	{V4L2_FIELD_NONE, 1, 42, 8, 94,   1, 618, 1920, 1080, 1920, 1080, 0, 0, 50, 0, 0},
	{V4L2_FIELD_NONE, 1, 42, 8, 94,   1, 178, 1920, 1080, 1920, 1080, 0, 0, 30, 0, 0},
	{V4L2_FIELD_NONE, 1, 42, 8, 94,   1, 618, 1920, 1080, 1920, 1080, 0, 0, 25, 0, 0},
	{V4L2_FIELD_NONE, 1, 42, 8, 94,   1, 728, 1920, 1080, 1920, 1080, 0, 0, 24, 0, 0},
	{V4L2_FIELD_NONE, 1, 87, 8, 460,  1, 92,  3840, 2160, 3840, 2160, 0, 0, 30, 0, 0},
	{V4L2_FIELD_NONE, 1, 87, 8, 1340, 1, 92,  3840, 2160, 3840, 2160, 0, 0, 25, 0, 0},
	{V4L2_FIELD_NONE, 1, 87, 8, 1560, 1, 92,  3840, 2160, 3840, 2160, 0, 0, 24, 0, 0},
	{V4L2_FIELD_NONE, 1, 87, 8, 1304, 1, 92,  4096, 2160, 4096, 2160, 0, 0, 24, 0, 0},
};

struct decon_lcd *find_porch(struct v4l2_mbus_framefmt mbus_fmt)
{
	struct decon_lcd *porch;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(decon_int_porchs); ++i) {
		porch = &decon_int_porchs[i];
		if ((mbus_fmt.field == V4L2_FIELD_INTERLACED)
			&& (mbus_fmt.field == porch->mode))
			return porch;
		if ((mbus_fmt.width == porch->xres)
			&& (mbus_fmt.height == porch->yres)
			&& (mbus_fmt.field == porch->mode)
			&& (mbus_fmt.reserved[0] == porch->fps))
			return porch;
	}

	return &decon_int_porchs[0];

}

int decon_get_hdmi_config(struct decon_device *decon,
               struct exynos_hdmi_data *hdmi_data)
{
	struct v4l2_control ctrl;
	int ret = 0;

	ctrl.id = 0;
	ctrl.value = 0;
	decon_dbg("state : %d\n", hdmi_data->state);

	switch (hdmi_data->state) {
	case EXYNOS_HDMI_STATE_PRESET:
		ret = v4l2_subdev_call(decon->output_sd, video, g_dv_timings, &hdmi_data->timings);
		if (ret)
			decon_err("failed to get current timings\n");
		else
			ret = find_subdev_hdmi(decon);
		decon_dbg("%dx%d@%s %lldHz %s(%#x)\n", hdmi_data->timings.bt.width,
			hdmi_data->timings.bt.height,
			hdmi_data->timings.bt.interlaced ? "I" : "P",
			hdmi_data->timings.bt.pixelclock,
			hdmi_data->timings.type ? "S3D" : "2D",
			hdmi_data->timings.type);
		break;
	case EXYNOS_HDMI_STATE_ENUM_PRESET:
		ret = v4l2_subdev_call(decon->output_sd, video, enum_dv_timings, &hdmi_data->etimings);
		if (ret)
			decon_err("failed to enumerate timings\n");
		break;
	case EXYNOS_HDMI_STATE_CEC_ADDR:
		ctrl.id = V4L2_CID_TV_SOURCE_PHY_ADDR;
		ret = v4l2_subdev_call(decon->output_sd, core, g_ctrl, &ctrl);
		if (ret)
			decon_err("failed to get physical address for CEC\n");
		hdmi_data->cec_addr = ctrl.value;
		decon_dbg("get physical address for CEC: %#x\n",
					hdmi_data->cec_addr);
		break;
	case EXYNOS_HDMI_STATE_AUDIO:
		ctrl.id = V4L2_CID_TV_MAX_AUDIO_CHANNELS;
		ret = v4l2_subdev_call(decon->output_sd, core, g_ctrl, &ctrl);
		if (ret)
			decon_err("failed to get hdmi audio information\n");
		hdmi_data->audio_info = ctrl.value;
		break;
	default:
		decon_warn("unrecongnized state %u", hdmi_data->state);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int decon_set_hdmi_config(struct decon_device *decon,
               struct exynos_hdmi_data *hdmi_data)
{
	struct v4l2_control ctrl;
	int ret = 0;

	decon_dbg("state : %d\n", hdmi_data->state);

	switch (hdmi_data->state) {
	case EXYNOS_HDMI_STATE_PRESET:
		ret = v4l2_subdev_call(decon->output_sd, video, s_dv_timings, &hdmi_data->timings);
		if (ret)
			decon_err("failed to set timings newly\n");
		else
			ret = find_subdev_hdmi(decon);
		decon_dbg("%dx%d@%s %lldHz %s(%#x)\n", hdmi_data->timings.bt.width,
			hdmi_data->timings.bt.height,
			hdmi_data->timings.bt.interlaced ? "I" : "P",
			hdmi_data->timings.bt.pixelclock,
			hdmi_data->timings.type ? "S3D" : "2D",
			hdmi_data->timings.type);
		break;
	case EXYNOS_HDMI_STATE_HDCP:
		ctrl.id = V4L2_CID_TV_HDCP_ENABLE;
		ctrl.value = hdmi_data->hdcp;
		ret = v4l2_subdev_call(decon->output_sd, core, s_ctrl, &ctrl);
		if (ret)
			decon_err("failed to enable HDCP\n");
		decon_dbg("HDCP %s\n", ctrl.value ? "enabled" : "disabled");
		break;
	case EXYNOS_HDMI_STATE_AUDIO:
		ctrl.id = V4L2_CID_TV_SET_NUM_CHANNELS;
		ctrl.value = hdmi_data->audio_info;
		ret = v4l2_subdev_call(decon->output_sd, core, s_ctrl, &ctrl);
		if (ret)
			decon_err("failed to set hdmi audio information\n");
		break;
	default:
		decon_warn("unrecongnized state %u", hdmi_data->state);
		ret = -EINVAL;
		break;
	}

	return ret;
}

irqreturn_t decon_ext_isr_for_eint(int irq, void *dev_id)
{
	struct decon_device *decon = dev_id;
	ktime_t timestamp = ktime_get();

	spin_lock(&decon->slock);

	decon->vsync_info.timestamp = timestamp;
	wake_up_interruptible_all(&decon->vsync_info.wait);

	spin_unlock(&decon->slock);

	return IRQ_HANDLED;
}

int decon_ext_config_eint_for_te(struct platform_device *pdev, struct decon_device *decon)
{
	struct device *dev = decon->dev;
	int gpio;
	int ret = 0;

	/* Get IRQ resource and register IRQ handler. */
	gpio = of_get_gpio(dev->of_node, 0);
	if (gpio < 0) {
		decon_err("failed to get proper gpio number\n");
		return -EINVAL;
	}

	gpio = gpio_to_irq(gpio);
	ret = devm_request_irq(dev, gpio, decon_ext_isr_for_eint,
			  IRQF_TRIGGER_RISING, pdev->name, decon);

	return ret;
}

int decon_ext_register_irq(struct platform_device *pdev, struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct resource *res;
	int ret = 0;

	/* Get IRQ resource and register IRQ handler. */
	/* 0: FIFO irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	ret = devm_request_irq(dev, res->start, decon_ext_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	/* 1: frame irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	ret = devm_request_irq(dev, res->start, decon_ext_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	/* 2: i80 irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 2);
	ret = devm_request_irq(dev, res->start, decon_ext_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	/* 3: wb frame done irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 3);
	ret = devm_request_irq(dev, res->start, decon_ext_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}
#ifndef CONFIG_EXYNOS_DUAL_MIPI_DISPLAY
	/* 4: external irq for te */
	if (decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY) {
		ret = decon_ext_config_eint_for_te(pdev, decon);
		if (ret) {
			decon_err("failed to config external irq\n");
			return ret;
		}
	}
#endif
	return ret;
}
