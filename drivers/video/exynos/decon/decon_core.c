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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/dma-buf.h>
#include <linux/exynos_ion.h>
#include <linux/ion.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/exynos_iovmm.h>
#include <linux/bug.h>
#include <linux/of_address.h>
#include <linux/smc.h>
#include <linux/debugfs.h>
#include <linux/init.h>

#include <linux/list.h>

#include <mach/regs-clock.h>
#include <mach/exynos-pm.h>
#include <media/exynos_mc.h>
#include <video/mipi_display.h>
#include <video/videonode.h>
#include <media/v4l2-subdev.h>

#include "decon.h"
#include "dsim.h"
#include "decon_helper.h"
#include "dpu_reg.h"
#include "regs-dpu.h"

#include "./panels/lcd_ctrl.h"
#include "../../../staging/android/sw_sync.h"
#include "vpp/vpp_core.h"

#if 0	// Used in only EVB for 2lane. kh620.kim@samsung.com
#include "dsim_common.h"
#endif

struct lcdfreq_info *g_info;

enum lcdfreq_level {
	NORMAL,
	LIMIT,
	LEVEL_MAX,
};

struct lcdfreq_t {
	enum lcdfreq_level level;
	u32 hz;
	u32 cmu_div;
};

struct lcdfreq_info {
	struct lcdfreq_t	table[LEVEL_MAX];
	enum lcdfreq_level	level;
	atomic_t		usage;
	struct mutex		lock;
	struct device		*dev;
	unsigned int		enable;
	void __iomem		*lcd_reg;
};

static int set_lcdfreq_div(struct device *dev, enum lcdfreq_level level)
{
	u32 ret;
	u32 reg;
	struct decon_device *decon = get_decon_drvdata(0);
	mutex_lock(&g_info->lock);

	if (!g_info->enable) {
		dev_err(dev, "%s reject. enable flag is %d\n", __func__, g_info->enable);
		ret = -EINVAL;
		goto exit;
	}
	
	g_info->lcd_reg = decon->regs;
	reg=readl(g_info->lcd_reg + VCLKCON1);
	reg&=~0xff;

	if(level)
		{
		reg|=(g_info->table[level].cmu_div);
		writel(reg,(g_info->lcd_reg + VCLKCON1));
	}else{
		reg|=(g_info->table[NORMAL].cmu_div);
		writel(reg,(g_info->lcd_reg + VCLKCON1));
	}
	ret =0;

	g_info->level = level;
exit:
	mutex_unlock(&g_info->lock);

	return ret;
}

static int lcdfreq_lock(struct device *dev, int value)
{
	int ret;

	if (!atomic_read(&g_info->usage))
		ret = set_lcdfreq_div(dev, value);
	else {
		dev_err(dev, "lcd freq is already limit state\n");
		return -EINVAL;
	}

	if (!ret) {
		mutex_lock(&g_info->lock);
		atomic_inc(&g_info->usage);
		mutex_unlock(&g_info->lock);
	}

	return ret;
}

static int lcdfreq_lock_free(struct device *dev)
{
	int ret;

	if (atomic_read(&g_info->usage))
		ret = set_lcdfreq_div(dev, NORMAL);
	else {
#ifdef CONFIG_LCD_DEBUG
		dev_err(dev, "lcd freq is already normal state\n");
#endif
		return -EINVAL;
	}

	if (!ret) {
		mutex_lock(&g_info->lock);
		atomic_dec(&g_info->usage);
		mutex_unlock(&g_info->lock);
	}

	return ret;
}

static ssize_t level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	if (!g_info->enable) {
		dev_err(g_info->dev, "%s reject. enable flag is %d\n", __func__, g_info->enable);
		return -EINVAL;
	}

	return sprintf(buf, "%dhz\n", g_info->table[g_info->level].hz); 
}

static ssize_t level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;
	int ret;
	if (!g_info->enable) {
		dev_err(g_info->dev, "%s reject. enable flag is %d\n", __func__, g_info->enable);
		return -EINVAL;
	}
	ret = kstrtoul(buf, 0, (unsigned long *)&value);
#ifdef CONFIG_LCD_DEBUG
	dev_info(g_info->dev, "%s :: value=%d\n", __func__, value);
#endif

	if (value >= LEVEL_MAX)
		return -EINVAL;

	if (value)
		ret = lcdfreq_lock(g_info->dev, value);
	else
		ret = lcdfreq_lock_free(g_info->dev);

	if (ret) {
#ifdef CONFIG_LCD_DEBUG
		dev_err(g_info->dev, "%s skip\n", __func__);
#endif
		return -EINVAL;
	}

	return count;
}

static ssize_t usage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&g_info->usage));
}

static DEVICE_ATTR(level, S_IRUGO|S_IWUGO|S_IWUSR, level_show, level_store);
static DEVICE_ATTR(usage, S_IRUGO, usage_show, NULL);

static struct attribute *lcdfreq_attributes[] = {
	&dev_attr_level.attr,
	&dev_attr_usage.attr,
	NULL,
};

static struct attribute_group lcdfreq_attr_group = {
	.name = "lcdfreq",
	.attrs = lcdfreq_attributes,
};

//#define CONFIG_DECON_DISPLAY_LOGO
#ifdef CONFIG_DECON_DISPLAY_LOGO
unsigned int decon_bootloader_fb_start = 0;
unsigned int decon_bootloader_fb_size = 0;

static int __init decon_bootloaderfb_arg(char *options)
{
       char *p = options;
	char *s;
	int err;

	for (s=p; *s; s++)
		if (*s == ',')
			break;
	s++;

	decon_bootloader_fb_size = simple_strtoul(s, NULL, 16);	
	decon_bootloader_fb_start = simple_strtoul(p, NULL, 16);
	decon_info("bootloader_fb_start=%08x   bootloader_fb_size=%08x \n", decon_bootloader_fb_start, decon_bootloader_fb_size);

	if(decon_bootloader_fb_start)
	err = memblock_reserve(decon_bootloader_fb_start,
                                decon_bootloader_fb_size);
        if (err)
            pr_warn("failed to reserve old framebuffer location\n");
	
        return 0;
}
__setup("decon.bootloaderfb=", decon_bootloaderfb_arg);
#endif

#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
#include <linux/exynos_touch_daemon.h>
#endif

#define PRINTFPS_DISPLAY
#define SUCCESS_EXYNOS_SMC	2
#define TRACE_VPP_LOG(d, prot) ({	\
	d->vpp_log[d->log_cnt].decon_id = d->id;	\
	d->vpp_log[d->log_cnt].time = sched_clock();	\
	d->vpp_log[d->log_cnt].protection = prot;	\
	d->vpp_log[d->log_cnt].line = __LINE__;		\
	d->log_cnt = ++d->log_cnt >= MAX_VPP_LOG ? 0 : d->log_cnt;	\
	})

#ifdef CONFIG_OF
static const struct of_device_id decon_device_table[] = {
	        { .compatible = "samsung,exynos5-decon_driver" },
		{},
};
MODULE_DEVICE_TABLE(of, decon_device_table);
#endif

#ifdef CONFIG_DECON_DEVFREQ
static struct pm_qos_request exynos7_tv_mif_qos;
static struct pm_qos_request exynos7_tv_int_qos;
static struct pm_qos_request exynos7_tv_disp_qos;
#endif

#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
extern struct exynos_touch_daemon_data exynos_touch_daemon_data;
#endif

int decon_log_level = 6;
module_param(decon_log_level, int, 0644);

struct decon_device *decon_int_drvdata = NULL;
EXPORT_SYMBOL(decon_int_drvdata);
struct decon_device *decon_ext_drvdata = NULL;
EXPORT_SYMBOL(decon_ext_drvdata);

static int decon_runtime_resume(struct device *dev);
static int decon_runtime_suspend(struct device *dev);
static void decon_set_protected_content(struct decon_device *decon,
                struct decon_reg_data *reg);

#if defined(CONFIG_EXYNOS_DECON_DPU)

static int decon_dpu_restore(struct device *dev);
static int decon_dpu_save(struct device *dev);
static int decon_dpu_gamma_init(void);

struct dpu_reg_dump {
	u32	addr;
	u32	val;
};

#define SAVE_ITEM(x)    { .addr = (x), .val = 0x0, }

static struct dpu_reg_dump dpu_dump[] = {
	SAVE_ITEM(DPUCON),
	SAVE_ITEM(DPUAD_BACKLIGHT),
	SAVE_ITEM(DPUAD_AMBIENT_LIGHT),
	SAVE_ITEM(DPUSCR_RED),
	SAVE_ITEM(DPUSCR_GREEN),
	SAVE_ITEM(DPUSCR_BLUE),
	SAVE_ITEM(DPUSCR_CYAN),
	SAVE_ITEM(DPUSCR_MAGENTA),
	SAVE_ITEM(DPUSCR_YELLOW),
	SAVE_ITEM(DPUSCR_WHITE),
	SAVE_ITEM(DPUSCR_BLACK),
	SAVE_ITEM(DPUHSC_CONTROL),
	SAVE_ITEM(DPUHSC_PAIMGAIN2_1_0),
	SAVE_ITEM(DPUHSC_PAIMGAIN5_4_3),
	SAVE_ITEM(DPUHSC_PAIMSCALE_SHIFT),
	SAVE_ITEM(DPUHSC_PPHCGAIN2_1_0),
	SAVE_ITEM(DPUHSC_PPHCGAIN5_4_3),
	SAVE_ITEM(DPUHSC_TSCGAIN_YRATIO),
	SAVE_ITEM(DPUAD_VP_CONTROL0),
	SAVE_ITEM(DPUAD_NVP_CONTROL2),
};

static struct dpu_reg_dump dpu_gamma_dump[DPUGAMMALUT_MAX];
#define AD_SIZE ((0x394 - 0x1D0)/4)
static struct dpu_reg_dump dpu_ad_dump[AD_SIZE] ;
static void decon_init_ad_reg(void)
{
	int i = 0;

	for (i = 0; i < AD_SIZE; i++){
		dpu_ad_dump[i].addr = DPUCON + 0x394 + i *4 ;
		dpu_ad_dump[i].val = 0;
	}
}
#endif

#ifdef PRINTFPS_DISPLAY
static int printfps = 0;
static ssize_t printfps_show(struct device *pdev, struct device_attribute *attr, char *buf) {
	return sprintf(buf, "%d\n", printfps);
}
static ssize_t printfps_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t size) {
	sscanf(buf, "%d", &printfps);
	return size;
}
static DEVICE_ATTR(printfps, S_IRWXUGO, printfps_show, printfps_store);
#endif

#ifdef CONFIG_USE_VSYNC_SKIP
static atomic_t extra_vsync_wait;
#endif /* CCONFIG_USE_VSYNC_SKIP */

void decon_dump(struct decon_device *decon)
{
	dev_err(decon->dev, "=== DECON%d SFR DUMP ===\n", decon->id);

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->regs, 0x718, false);
	dev_err(decon->dev, "=== DECON%d MIC SFR DUMP ===\n", decon->id);

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->regs + 0x2400, 0x20, false);
	dev_err(decon->dev, "=== DECON%d SHADOW SFR DUMP ===\n", decon->id);

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->regs + SHADOW_OFFSET, 0x718, false);

	v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_DUMP, NULL);
}

/* ---------- CHECK FUNCTIONS ----------- */
static void decon_to_regs_param(struct decon_regs_data *win_regs,
		struct decon_reg_data *regs, int idx)
{
	win_regs->wincon = regs->wincon[idx];
	win_regs->winmap = regs->winmap[idx];
	win_regs->vidosd_a = regs->vidosd_a[idx];
	win_regs->vidosd_b = regs->vidosd_b[idx];
	win_regs->vidosd_c = regs->vidosd_c[idx];
	win_regs->vidosd_d = regs->vidosd_d[idx];
	win_regs->vidw_buf_start = regs->buf_start[idx];
	win_regs->vidw_whole_w = regs->whole_w[idx];
	win_regs->vidw_whole_h = regs->whole_h[idx];
	win_regs->vidw_offset_x = regs->offset_x[idx];
	win_regs->vidw_offset_y = regs->offset_y[idx];

	if (idx)
		win_regs->blendeq = regs->blendeq[idx - 1];
	win_regs->type = regs->vpp_config[idx].idma_type;

	decon_dbg("decon idma_type(%d)\n", regs->vpp_config->idma_type);
}

static u16 fb_panstep(u32 res, u32 res_virtual)
{
	return res_virtual > res ? 1 : 0;
}

static u32 vidosd_a(int x, int y)
{
	return VIDOSD_A_TOPLEFT_X(x) |
			VIDOSD_A_TOPLEFT_Y(y);
}

static u32 vidosd_b(int x, int y, u32 xres, u32 yres)
{
	return VIDOSD_B_BOTRIGHT_X(x + xres - 1) |
		VIDOSD_B_BOTRIGHT_Y(y + yres - 1);
}

static u32 vidosd_c(u8 r0, u8 g0, u8 b0)
{
	return VIDOSD_C_ALPHA0_R_F(r0) |
		VIDOSD_C_ALPHA0_G_F(g0) |
		VIDOSD_C_ALPHA0_B_F(b0);
}

static u32 vidosd_d(u8 r1, u8 g1, u8 b1)
{
	return VIDOSD_D_ALPHA1_R_F(r1) |
		VIDOSD_D_ALPHA1_G_F(g1) |
		VIDOSD_D_ALPHA1_B_F(b1);
}

static u32 wincon(u32 bits_per_pixel, u32 transp_length)
{
	u32 data = 0;

	switch (bits_per_pixel) {
	case 16:
		data |= WINCON_BPPMODE_RGB565;
		break;
	case 24:
	case 32:
		if (transp_length > 0) {
			data |= WINCON_BLD_PIX;
			data |= WINCON_BPPMODE_ARGB8888;
		} else {
			data |= WINCON_BPPMODE_XRGB8888;
		}
		break;
	default:
		pr_err("%d bpp doesn't support\n", bits_per_pixel);
		break;
	}

	if (transp_length != 1)
		data |= WINCON_ALPHA_SEL;

	return data;
}

#ifdef CONFIG_USE_VSYNC_SKIP
void decon_extra_vsync_wait_set(int set_count)
{
	atomic_set(&extra_vsync_wait, set_count);
}

int decon_extra_vsync_wait_get(void)
{
	return atomic_read(&extra_vsync_wait);
}

void decon_extra_vsync_wait_add(int skip_count)
{
	atomic_add(skip_count, &extra_vsync_wait);
}
#endif /* CONFIG_USE_VSYNC_SKIP */

static inline u32 blendeq(enum decon_blending blending, u8 transp_length,
		int plane_alpha)
{
	u8 a, b;
	int is_plane_alpha = (plane_alpha < 255 && plane_alpha > 0) ? 1 : 0;

	if (transp_length == 1 && blending == DECON_BLENDING_PREMULT)
		blending = DECON_BLENDING_COVERAGE;

	switch (blending) {
	case DECON_BLENDING_NONE:
		a = BLENDE_COEF_ONE;
		b = BLENDE_COEF_ZERO;
		break;

	case DECON_BLENDING_PREMULT:
		if (!is_plane_alpha) {
			a = BLENDE_COEF_ONE;
			b = BLENDE_COEF_ONE_MINUS_ALPHA_A;
		} else {
			a = BLENDE_COEF_ALPHA0;
			b = BLENDE_COEF_ONE_MINUS_ALPHA_A;
		}
		break;

	case DECON_BLENDING_COVERAGE:
		a = BLENDE_COEF_ALPHA_A;
		b = BLENDE_COEF_ONE_MINUS_ALPHA_A;
		break;

	default:
		return 0;
	}

	return BLENDE_A_FUNC(a) |
			BLENDE_B_FUNC(b) |
			BLENDE_P_FUNC(BLENDE_COEF_ZERO) |
			BLENDE_Q_FUNC(BLENDE_COEF_ZERO);
}

static u32 decon_red_length(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 5;

	case DECON_PIXEL_FORMAT_RGB_565:
		return 5;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_red_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 0;

	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGB_565:
		return 11;

	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 16;

	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return 24;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_green_length(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 5;

	case DECON_PIXEL_FORMAT_RGB_565:
		return 6;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_green_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return 16;

	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_RGB_565:
		return 5;
	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_blue_length(int format)
{
	return decon_red_length(format);
}

static u32 decon_blue_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
		return 16;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 10;

	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return 8;

	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return 24;

	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_transp_length(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 1;

	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_transp_offset(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return 24;

	case DECON_PIXEL_FORMAT_RGBA_5551:
		return 15;

	case DECON_PIXEL_FORMAT_RGBX_8888:
		return decon_blue_offset(format);

	case DECON_PIXEL_FORMAT_BGRX_8888:
		return decon_red_offset(format);

	case DECON_PIXEL_FORMAT_RGB_565:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

static u32 decon_padding(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return 8;

	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return 0;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}

}

/* DECON_PIXEL_FORMAT_RGBA_8888 and WINCON_BPPMODE_ABGR8888 are same format
 * A[31:24] : B[23:16] : G[15:8] : R[7:0] */
static u32 decon_rgborder(int format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_RGBA_8888:
		return WINCON_BPPMODE_ABGR8888;
	case DECON_PIXEL_FORMAT_RGBX_8888:
		return WINCON_BPPMODE_XBGR8888;
	case DECON_PIXEL_FORMAT_RGB_565:
		return WINCON_BPPMODE_RGB565;
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return WINCON_BPPMODE_ARGB8888;
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return WINCON_BPPMODE_XRGB8888;
	case DECON_PIXEL_FORMAT_ARGB_8888:
		return WINCON_BPPMODE_BGRA8888;
	case DECON_PIXEL_FORMAT_ABGR_8888:
		return WINCON_BPPMODE_RGBA8888;
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return WINCON_BPPMODE_BGRX8888;
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return WINCON_BPPMODE_RGBX8888;

	default:
		decon_warn("unrecognized pixel format %u\n", format);
		return 0;
	}
}

bool decon_validate_x_alignment(struct decon_device *decon, int x, u32 w,
		u32 bits_per_pixel)
{
	uint8_t pixel_alignment = 32 / bits_per_pixel;

	if (x % pixel_alignment) {
		decon_err("left X coordinate not properly aligned to %u-pixel boundary (bpp = %u, x = %u)\n",
				pixel_alignment, bits_per_pixel, x);
		return 0;
	}
	if ((x + w) % pixel_alignment) {
		decon_err("right X coordinate not properly aligned to %u-pixel boundary (bpp = %u, x = %u, w = %u)\n",
				pixel_alignment, bits_per_pixel, x, w);
		return 0;
	}

	return 1;
}

static unsigned int decon_calc_bandwidth(u32 w, u32 h, u32 bits_per_pixel, int fps)
{
	unsigned int bw = w * h;

	bw *= DIV_ROUND_UP(bits_per_pixel, 8);
	bw *= fps;

	return bw;
}

static bool is_vpp_type(enum decon_idma_type idma_type)
{
	switch (idma_type) {
#ifdef CONFIG_EXYNOS_VPP
	case IDMA_VG0:
	case IDMA_VG1:
	case IDMA_VGR0:
	case IDMA_VGR1:
		return true;
#endif
	default:
		return false;
	}
}

#ifdef CONFIG_CPU_IDLE
static int exynos_decon_lpc_event(struct notifier_block *notifier,
		unsigned long pm_event, void *v)
{
	struct decon_device *decon = get_decon_drvdata(0);
	int err = NOTIFY_DONE;

	switch (pm_event) {
	case LPC_PREPARE:
		if (decon->state != DECON_STATE_LPD)
			err = -EBUSY;
		break;
	}

	return notifier_from_errno(err);
}

static struct notifier_block exynos_decon_lpc_nb = {
	.notifier_call = exynos_decon_lpc_event,
};
#endif

/* ---------- OVERLAP COUNT CALCULATION ----------- */
static inline int rect_width(struct decon_rect *r)
{
	return	r->right - r->left;
}

static inline int rect_height(struct decon_rect *r)
{
	return	r->bottom - r->top;
}

static bool decon_intersect(struct decon_rect *r1, struct decon_rect *r2)
{
	return !(r1->left > r2->right || r1->right < r2->left ||
		r1->top > r2->bottom || r1->bottom < r2->top);
}

static int decon_intersection(struct decon_rect *r1,
			struct decon_rect *r2, struct decon_rect *r3)
{
	r3->top = max(r1->top, r2->top);
	r3->bottom = min(r1->bottom, r2->bottom);
	r3->left = max(r1->left, r2->left);
	r3->right = min(r1->right, r2->right);
	return 0;
}

#if defined(CONFIG_FB_WINDOW_UPDATE) || defined(CONFIG_DECON_DEVFREQ)
static int decon_union(struct decon_rect *r1,
		struct decon_rect *r2, struct decon_rect *r3)
{
	r3->top = min(r1->top, r2->top);
	r3->bottom = max(r1->bottom, r2->bottom);
	r3->left = min(r1->left, r2->left);
	r3->right = max(r1->right, r2->right);
	return 0;
}
#endif

#ifdef CONFIG_FB_WINDOW_UPDATE
static bool is_decon_rect_differ(struct decon_rect *r1,
                struct decon_rect *r2)
{
	return ((r1->left != r2->left) || (r1->top != r2->top) ||
		(r1->right != r2->right) || (r1->bottom != r2->bottom));
}

static inline bool does_layer_need_scale(struct decon_win_config *config)
{
	return (config->dst.w != config->src.w) || (config->dst.h != config->src.h);
}

static int decon_set_win_blocking_mode(struct decon_device *decon, struct decon_win *win,
		struct decon_win_config *win_config, struct decon_reg_data *regs)
{
	struct decon_rect r1, r2, overlap_rect, block_rect;
	unsigned int overlap_size, blocking_size = 0;
	int j;
	bool enabled = false;

	r1.left = win_config[win->index].dst.x;
	r1.top = win_config[win->index].dst.y;
	r1.right = r1.left + win_config[win->index].dst.w - 1;
	r1.bottom = r1.top + win_config[win->index].dst.h - 1;

	memset(&block_rect, 0, sizeof(struct decon_rect));
	for (j = win->index + 1; j < decon->pdata->max_win; j++) {
		struct decon_win_config *config = &win_config[j];
		if (config->state != DECON_WIN_STATE_BUFFER)
			continue;

		/* Support only XRGB */
		if ((config->format == DECON_PIXEL_FORMAT_ARGB_8888) ||
				(config->format == DECON_PIXEL_FORMAT_ABGR_8888) ||
				(config->format == DECON_PIXEL_FORMAT_RGBA_8888) ||
				(config->format == DECON_PIXEL_FORMAT_BGRA_8888) ||
				(config->format == DECON_PIXEL_FORMAT_RGBA_5551) ||
				((config->plane_alpha < 255) && (config->plane_alpha > 0)))
			continue;

		if (is_vpp_type(config->idma_type) && (is_rotation(config) ||
			(config->src.w != config->dst.w) ||
			(config->src.h != config->dst.h)))
			continue;

		r2.left = config->dst.x;
		r2.top = config->dst.y;
		r2.right = r2.left + config->dst.w - 1;
		r2.bottom = r2.top + config->dst.h - 1;
		/* overlaps or not */
		if (decon_intersect(&r1, &r2)) {
			decon_intersection(&r1, &r2, &overlap_rect);
			if (!is_decon_rect_differ(&r1, &overlap_rect)) {
				/* window rect and blocking rect is same. */
				win_config[win->index].state = DECON_WIN_STATE_DISABLED;
				return 1;
			}

			if (overlap_rect.right - overlap_rect.left + 1 <
					MIN_BLK_MODE_WIDTH ||
				overlap_rect.bottom - overlap_rect.top + 1 <
					MIN_BLK_MODE_HEIGHT)
				continue;

			overlap_size = (overlap_rect.right - overlap_rect.left) *
					(overlap_rect.bottom - overlap_rect.top);

			if (overlap_size > blocking_size) {
				memcpy(&block_rect, &overlap_rect,
						sizeof(struct decon_rect));
				blocking_size = (block_rect.right - block_rect.left) *
						(block_rect.bottom - block_rect.top);
				enabled = true;
			}
		}
	}

	if (enabled) {
		regs->block_rect[win->index].w = block_rect.right - block_rect.left + 1;
		regs->block_rect[win->index].h = block_rect.bottom - block_rect.top + 1;
		regs->block_rect[win->index].x = block_rect.left - win_config[win->index].dst.x;
		regs->block_rect[win->index].y = block_rect.top -  win_config[win->index].dst.y;
		memcpy(&win_config->block_area, &regs->block_rect[win->index],
				sizeof(struct decon_win_rect));
	}
	return 0;
}
#else

static int decon_set_win_blocking_mode(struct decon_device *decon, struct decon_win *win,
		struct decon_win_config *win_config, struct decon_reg_data *regs)
{
	return 0;
}
#endif

static void decon_enable_blocking_mode(struct decon_device *decon,
		struct decon_reg_data *regs, u32 win_idx)
{
	struct decon_win_rect rect = regs->block_rect[win_idx];
	bool enable = false;

	/* TODO: Check a DECON H/W limitation */
	enable = (rect.w * rect.h) ? true : false;

	if (enable) {
		decon_reg_set_block_mode(decon->id, win_idx, rect.x, rect.y,
						rect.w, rect.h, true);
		decon_dbg("win[%d] blocking_mode:(%d,%d,%d,%d)\n", win_idx,
				rect.x, rect.y, rect.w, rect.h);
	} else {
		decon_reg_set_block_mode(decon->id, win_idx, 0, 0, 0, 0, false);
	}
}

#if defined(CONFIG_DECON_DEVFREQ)
static void decon_get_overlap_region(struct decon_reg_data *regs,
				struct decon_rect *overlaps,
				int cnt)
{
	int i;

	memcpy(&regs->overlap_rect, &overlaps[0], sizeof(struct decon_rect));
	for (i = 1; i < cnt; i++)
		decon_union(&overlaps[i],
			&regs->overlap_rect, &regs->overlap_rect);

	decon_dbg("overlap_rect[%d %d %d %d]\n", regs->overlap_rect.left,
		regs->overlap_rect.top, regs->overlap_rect.right,
		regs->overlap_rect.bottom);
}
static u32 decon_get_bw(struct decon_device *decon,
			struct decon_win_config *win_config,
			struct decon_reg_data *regs)
{
	struct decon_lcd *lcd_info = decon->lcd_info;
	struct decon_win *win;
	u32 vclk_rate = clk_get_rate(decon->res.vclk) / KHZ;
	struct decon_win_config *win_cfg;
	u32 format_in_bytes = 4;
	u32 bw, dma_bw, dma0_bw = 0, dma1_bw = 0;
	struct decon_rect dma1_r1, dma1_r2;
	int dma0_overlap_cnt = 0;
	int dma1_overlap_cnt = 0;
	int i;

	for (i = 0; i < decon->pdata->max_win; i++) {
		win_cfg = &win_config[i];
		/* TODO: move the VPP min_lock code to decon driver */
		if ((win_cfg->state != DECON_WIN_STATE_BUFFER) ||
			(is_vpp_type(win_cfg->idma_type)))
			continue;
		win = decon->windows[i];
		switch (win_cfg->idma_type) {
		case IDMA_G0:
			dma0_overlap_cnt++;
		/* TODO: if decon_ext is enabled, add one to overlap_cnt*/
			break;
		case IDMA_G1:
		case IDMA_G2:
			if (!dma1_overlap_cnt) {
				dma1_r1.left = win_cfg->dst.x;
				dma1_r1.top = win_cfg->dst.y;
				dma1_r1.right =
					dma1_r1.left + win_cfg->dst.w - 1;
				dma1_r1.bottom =
					dma1_r1.top + win_cfg->dst.h - 1;
				dma1_overlap_cnt++;
			} else {
				dma1_r2.left = win_cfg->dst.x;
				dma1_r2.top = win_cfg->dst.y;
				dma1_r2.right =
					dma1_r2.left + win_cfg->dst.w - 1;
				dma1_r2.bottom =
					dma1_r2.top + win_cfg->dst.h - 1;
				if (decon_intersect(&dma1_r1, &dma1_r2)) {
					decon_intersection(&dma1_r1,
							&dma1_r2, &dma1_r2);
					dma1_overlap_cnt++;
				}
			}
			break;
		default:
			break;
		}
	}

	if (!decon->id) {
		/* TODO: MIC ratio should be get from DT */
		if (regs->win_overlap_cnt > 1) {
			if (rect_width(&regs->overlap_rect) * NO_CNT_TH < lcd_info->xres) {
				decon_dbg("overlap_cnt is modulated\n");
				regs->win_overlap_cnt--;
			}
		}

		if (dma1_overlap_cnt > 1) {
			if (rect_width(&dma1_r2) * NO_CNT_TH < lcd_info->xres)
				dma1_overlap_cnt--;
		}

		bw = vclk_rate * 2 * format_in_bytes * regs->win_overlap_cnt;
		dma0_bw = vclk_rate * 2 * format_in_bytes * dma0_overlap_cnt;
		dma1_bw = vclk_rate * 2 * format_in_bytes * dma1_overlap_cnt;
		dma_bw = max(dma0_bw, dma1_bw);
		regs->int_bw = (dma_bw * 100) / (DECON_INT_UTIL * 16);
		regs->disp_bw = (dma_bw * 100) / (DISP_UTIL * 16);
		regs->disp_bw = max(regs->disp_bw, (u32)167000);
		decon_dbg("vclk_rate %d bw %d format_in_bytes %d,"
			"win_overlap_cnt %d,"
			"dma0_bw %d dma1_bw %d int_bw %d disp_bw %d\n",
			vclk_rate, bw, format_in_bytes, regs->win_overlap_cnt,
				dma0_bw, dma1_bw, regs->int_bw, regs->disp_bw);
	} else {
		bw = vclk_rate * format_in_bytes;
	}

	return bw;
}

static int decon_get_overlap_cnt(struct decon_device *decon,
				struct decon_win_config *win_config,
				struct decon_reg_data *regs)
{
	struct decon_rect overlaps2[21];
	struct decon_rect overlaps3[15];
	struct decon_rect overlaps4[10];
	struct decon_rect overlaps5[6];
	struct decon_rect overlaps6[3];
	struct decon_rect r1, r2;
	struct decon_win_config *win_cfg1, *win_cfg2;
	int overlaps2_cnt = 0;
	int overlaps3_cnt = 0;
	int overlaps4_cnt = 0;
	int overlaps5_cnt = 0;
	int overlaps6_cnt = 0;
	int i, j;

	int overlap_max_cnt = 1;

	for (i = 1; i < decon->pdata->max_win; i++) {
		win_cfg1 = &win_config[i];
		/* TODO: move the VPP min_lock code to decon driver */
		if ((win_cfg1->state != DECON_WIN_STATE_BUFFER) ||
			(is_vpp_type(win_cfg1->idma_type)))
			continue;
		r1.left = win_cfg1->dst.x;
		r1.top = win_cfg1->dst.y;
		r1.right = r1.left + win_cfg1->dst.w - 1;
		r1.bottom = r1.top + win_cfg1->dst.h - 1;
		for (j = 0; j < overlaps6_cnt; j++) {
			/* 7 window overlaps */
			if (decon_intersect(&r1, &overlaps6[j])) {
				overlap_max_cnt = 7;
				break;
			}
		}
		for (j = 0; (j < overlaps5_cnt) && (overlaps6_cnt < 3); j++) {
			/* 6 window overlaps */
			if (decon_intersect(&r1, &overlaps5[j])) {
				decon_intersection(&r1, &overlaps5[j],
						&overlaps6[overlaps6_cnt]);
				overlaps6_cnt++;
			}
		}
		for (j = 0; (j < overlaps4_cnt) && (overlaps5_cnt < 6); j++) {
			/* 5 window overlaps */
			if (decon_intersect(&r1, &overlaps4[j])) {
				decon_intersection(&r1, &overlaps4[j],
						&overlaps5[overlaps5_cnt]);
				overlaps5_cnt++;
			}
		}
		for (j = 0; (j < overlaps3_cnt) && (overlaps4_cnt < 10); j++) {
			/* 4 window overlaps */
			if (decon_intersect(&r1, &overlaps3[j])) {
				decon_intersection(&r1, &overlaps3[j],
						&overlaps4[overlaps4_cnt]);
				overlaps4_cnt++;
			}
		}
		for (j = 0; (j < overlaps2_cnt) && (overlaps3_cnt < 15); j++) {
			/* 3 window overlaps */
			if (decon_intersect(&r1, &overlaps2[j])) {
				decon_intersection(&r1, &overlaps2[j],
						&overlaps3[overlaps3_cnt]);
				overlaps3_cnt++;
			}
		}
		for (j = 0; (j < i) && (overlaps2_cnt < 21); j++) {
			win_cfg2 = &win_config[j];
			if ((win_cfg2->state != DECON_WIN_STATE_BUFFER) ||
				(is_vpp_type(win_cfg2->idma_type)))
				continue;
			r2.left = win_cfg2->dst.x;
			r2.top = win_cfg2->dst.y;
			r2.right = r2.left + win_cfg2->dst.w - 1;
			r2.bottom = r2.top + win_cfg2->dst.h - 1;
			/* 2 window overlaps */
			if (decon_intersect(&r1, &r2)) {
				decon_intersection(&r1, &r2,
					&overlaps2[overlaps2_cnt]);
				overlaps2_cnt++;
			}
		}
	}

	if (overlaps6_cnt > 0) {
		overlap_max_cnt = max(overlap_max_cnt, 6);
		decon_get_overlap_region(regs, &overlaps6[0], overlaps6_cnt);
	} else if (overlaps5_cnt > 0) {
		overlap_max_cnt = max(overlap_max_cnt, 5);
		decon_get_overlap_region(regs, &overlaps5[0], overlaps5_cnt);
	} else if (overlaps4_cnt > 0) {
		overlap_max_cnt = max(overlap_max_cnt, 4);
		decon_get_overlap_region(regs, &overlaps4[0], overlaps4_cnt);
	} else if (overlaps3_cnt > 0) {
		overlap_max_cnt = max(overlap_max_cnt, 3);
		decon_get_overlap_region(regs, &overlaps3[0], overlaps3_cnt);
	} else if (overlaps2_cnt > 0) {
		overlap_max_cnt = max(overlap_max_cnt, 2);
		decon_get_overlap_region(regs, &overlaps2[0], overlaps2_cnt);
	}

	return overlap_max_cnt;
}
#endif

#if 0
static void vpp_dump(struct decon_device *decon)
{
	int i;

	for (i = 0; i < MAX_VPP_SUBDEV; i++) {
		if (decon->vpp_used[i]) {
			struct v4l2_subdev *sd = NULL;
			sd = decon->mdev->vpp_sd[i];
			BUG_ON(!sd);
			v4l2_subdev_call(sd, core, ioctl, VPP_DUMP, NULL);
		}
	}
}
#endif

static void decon_vpp_stop(struct decon_device *decon, bool do_reset)
{
	int i;
	unsigned long state = (unsigned long)do_reset;

	for (i = 0; i < MAX_VPP_SUBDEV; i++) {
		if (decon->vpp_used[i] && (!(decon->vpp_usage_bitmask & (1 << i)))) {
			struct v4l2_subdev *sd = NULL;
			sd = decon->mdev->vpp_sd[i];
			BUG_ON(!sd);
			if (decon->vpp_err_stat[i])
				state = VPP_STOP_ERR;
			v4l2_subdev_call(sd, core, ioctl, VPP_STOP,
						(unsigned long *)state);
			decon->vpp_used[i] = false;
			decon->vpp_err_stat[i] = false;
		}
	}
}

/* ---------- SYSTEM REGISTER CONTROL ----------- */
static void decon_set_sysreg_disp_cfg(struct decon_device *decon)
{
	u32 val = readl(decon->sys_regs + DISP_CFG);

	if (decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY)
		val |= DISP_CFG_TV_MIPI_EN;

	val |= DISP_CFG_UNMASK_GLOBAL;

	writel(val, decon->sys_regs + DISP_CFG);
}

static void decon_set_sysreg_dsd_cfg(struct decon_device *decon,
				enum decon_idma_type idma_type)
{
	u32 val = 0;
	u32 en = decon->id ? ~0 : 0;
	u32 old = readl(decon->sys_regs + DSD_CFG);

	switch (idma_type) {
	case IDMA_VG0:
		val = (en & DSD_CFG_IDMA_VG0) |
			(old & ~DSD_CFG_IDMA_VG0);
		break;
	case IDMA_VG1:
		val = (en & DSD_CFG_IDMA_VG1) |
			(old & ~DSD_CFG_IDMA_VG1);
		break;
	case IDMA_VGR0:
		val = (en & DSD_CFG_IDMA_VGR0) |
			(old & ~DSD_CFG_IDMA_VGR0);
		break;
	case IDMA_VGR1:
		val = (en & DSD_CFG_IDMA_VGR1) |
			(old & ~DSD_CFG_IDMA_VGR1);
		break;
	default:
		val = old;
		decon_err("not supported vpp type\n");
	}

	writel(val, decon->sys_regs + DSD_CFG);
}

#ifdef CONFIG_FB_WINDOW_UPDATE
inline static void decon_win_update_rect_reset(struct decon_device *decon)
{
	decon->update_win.x = 0;
	decon->update_win.y = 0;
	decon->update_win.w = 0;
	decon->update_win.h = 0;
	decon->need_update = true;
}

static void decon_wait_for_framedone(struct decon_device *decon)
{
	int ret;
	s64 time_ms = ktime_to_ms(ktime_get()) - ktime_to_ms(decon->trig_mask_timestamp);

	if (time_ms < 17) {
		DISP_SS_EVENT_LOG(DISP_EVT_DECON_FRAMEDONE_WAIT, &decon->sd, ktime_set(0, 0));
		ret = wait_event_interruptible_timeout(decon->wait_frmdone,
				(decon->frame_done_cnt_target <= decon->frame_done_cnt_cur),
				msecs_to_jiffies(17 - time_ms));
	}
}

static int decon_reg_ddi_partial_cmd(struct decon_device *decon, struct decon_win_rect *rect)
{
	struct decon_win_rect win_rect;
	int ret;

	decon_wait_for_framedone(decon);
	/* TODO: need to set DSI_IDX */
	decon_reg_wait_linecnt_is_zero_timeout(decon->id, 0, 35 * 1000);
	DISP_SS_EVENT_LOG(DISP_EVT_LINECNT_ZERO, &decon->sd, ktime_set(0, 0));
	/* Partial Command */
	win_rect.x = rect->x;
	win_rect.y = rect->y;
	/* w is right & h is bottom */
	win_rect.w = rect->x + rect->w - 1;
	win_rect.h = rect->y + rect->h - 1;
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_DISABLE, NULL);
	if (ret)
		decon_err("Failed to disable Packet-go in %s\n", __func__);
#endif
	ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
			DSIM_IOC_PARTIAL_CMD, &win_rect);
	if (ret) {
		decon_win_update_rect_reset(decon);
		decon_err("%s: partial_area CMD is failed  %s [%d %d %d %d]\n",
				__func__, decon->output_sd->name, rect->x,
				rect->y, rect->w, rect->h);
	}
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL); /* Don't care failure or success */
#endif

	return ret;
}

static int decon_win_update_disp_config(struct decon_device *decon,
					struct decon_win_rect *win_rect)
{
	struct decon_lcd lcd_info;
	int ret = 0;

	memcpy(&lcd_info, decon->lcd_info, sizeof(struct decon_lcd));
	lcd_info.xres = win_rect->w;
	lcd_info.yres = win_rect->h;

	lcd_info.hfp = decon->lcd_info->hfp + ((decon->lcd_info->xres - win_rect->w) >> 1);
	lcd_info.vfp = decon->lcd_info->vfp + decon->lcd_info->yres - win_rect->h;

	v4l2_set_subdev_hostdata(decon->output_sd, &lcd_info);
	ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_SET_PORCH, NULL);
	if (ret) {
		decon_win_update_rect_reset(decon);
		decon_err("failed to set porch values of DSIM [%d %d %d %d]\n",
				win_rect->x, win_rect->y,
				win_rect->w, win_rect->h);
	}

	if (lcd_info.mic_enabled)
		decon_reg_config_mic(decon->id, 0, &lcd_info);
	decon_reg_set_porch(decon->id, 0, &lcd_info);
	decon_win_update_dbg("[WIN_UPDATE]%s : vfp %d vbp %d vsa %d hfp %d hbp %d hsa %d w %d h %d\n",
			__func__,
			lcd_info.vfp, lcd_info.vbp, lcd_info.vsa,
			lcd_info.hfp, lcd_info.hbp, lcd_info.hsa,
			win_rect->w, win_rect->h);

	return ret;
}
#endif

int decon_tui_protection(struct decon_device *decon, bool tui_en)
{
	int ret = 0;
	int i;
	struct decon_psr_info psr;

	if (tui_en) {
		/* Blocking LPD */
		decon_lpd_block_exit(decon);
		mutex_lock(&decon->output_lock);
		flush_kthread_worker(&decon->update_regs_worker);

		/* disable all the windows */
		for (i = 0; i < decon->pdata->max_win; i++)
			decon_write(decon->id, WINCON(i), 0);
#ifdef CONFIG_FB_WINDOW_UPDATE
		/* Restore window_partial_update */
		if (decon->need_update) {
			decon->update_win.x = 0;
			decon->update_win.y = 0;
			decon->update_win.w = decon->lcd_info->xres;
			decon->update_win.h = decon->lcd_info->yres;
			decon_reg_ddi_partial_cmd(decon, &decon->update_win);
			decon_win_update_disp_config(decon, &decon->update_win);
			decon->need_update = false;
		}
#endif
		decon_to_psr_info(decon, &psr);
		decon_reg_start(decon->id, decon->pdata->dsi_mode, &psr);
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		if (!decon->id && decon->pdata->trig_mode == DECON_HW_TRIG)
			decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
					decon->pdata->trig_mode, DECON_TRIG_DISABLE);
		/*
		 * TODO: stop the VPP
		decon->vpp_usage_bitmask = 0;
		decon_vpp_stop(decon, false);
		*/
		decon->out_type = DECON_OUT_TUI;
		mutex_unlock(&decon->output_lock);
		/* set qos for only single window */
		exynos7_update_media_scenario(TYPE_DECON_INT,
						decon->default_bw, 0);
		pm_qos_update_request(&decon->disp_qos, 167000);
		pm_qos_update_request(&decon->int_qos, 167000);
	}
	else {
		mutex_lock(&decon->output_lock);
		decon->out_type = DECON_OUT_DSI;
		mutex_unlock(&decon->output_lock);
		/* UnBlocking LPD */
		decon_lpd_unblock(decon);
	}

	return ret;
}

static void decon_free_dma_buf(struct decon_device *decon,
		struct decon_dma_buf_data *dma)
{
	if (!dma->dma_addr)
		return;

	if (dma->fence)
		sync_fence_put(dma->fence);

	ion_iovmm_unmap(dma->attachment, dma->dma_addr);

	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);

	dma_buf_detach(dma->dma_buf, dma->attachment);
	dma_buf_put(dma->dma_buf);
	ion_free(decon->ion_client, dma->ion_handle);
	memset(dma, 0, sizeof(struct decon_dma_buf_data));
}

/* ---------- FB_BLANK INTERFACE ----------- */
int decon_enable(struct decon_device *decon)
{
	struct decon_psr_info psr;
	struct decon_init_param p;
	int state = decon->state;
	int ret = 0;

	decon_dbg("enable decon-%s\n", decon->id ? "ext" : "int");
	exynos_ss_printk("%s:state %d: active %d:+\n", __func__,
				decon->state, pm_runtime_active(decon->dev));

	if (decon->state != DECON_STATE_LPD_EXIT_REQ)
		mutex_lock(&decon->output_lock);

	if ((decon->out_type == DECON_OUT_DSI) && (decon->state == DECON_STATE_INIT)) {
		decon_info("decon%d init state\n", decon->id);
		decon->state = DECON_STATE_ON;
		goto err;
	}

	if (decon->state == DECON_STATE_ON) {
		decon_warn("decon%d already enabled\n", decon->id);
		goto err;
	}

	if(decon->out_type != DECON_OUT_HDMI){
#if defined(CONFIG_PM_RUNTIME)
	if (decon->state != DECON_STATE_LPD_EXIT_REQ)
		pm_runtime_get_sync(decon->dev);
	else
		decon_runtime_resume(decon->dev);
#else
		decon_runtime_resume(decon->dev);
#endif
	}

#if defined(CONFIG_DECON_DEVFREQ)
	if (!decon->id) {
		exynos7_update_media_scenario(TYPE_DECON_INT,
						decon->default_bw, 0);
		pm_qos_update_request(&decon->disp_qos, 167000);
		pm_qos_update_request(&decon->int_qos, 167000);
	} else {
		exynos7_update_media_scenario(TYPE_DECON_EXT,
						decon->default_bw, 0);
	}
	decon->prev_bw = decon->default_bw;
	decon->prev_int_bw = 167000;
	decon->prev_disp_bw = 167000;
	if ((decon->out_type != DECON_OUT_DSI) &&
		(decon->out_type != DECON_OUT_TUI)) {
		pm_qos_add_request(&exynos7_tv_mif_qos, PM_QOS_BUS_THROUGHPUT, 828000);
		pm_qos_add_request(&exynos7_tv_int_qos, PM_QOS_DEVICE_THROUGHPUT, 468000);
		pm_qos_add_request(&exynos7_tv_disp_qos, PM_QOS_DISPLAY_THROUGHPUT, 400000);
	}
#endif

	if (decon->state == DECON_STATE_LPD_EXIT_REQ) {
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
				DSIM_IOC_ENTER_ULPS, (unsigned long *)0);
		if (ret) {
			decon_err("%s: failed to exit ULPS state for %s\n",
					__func__, decon->output_sd->name);
			goto err;
		}
	} else {
		/* start output device (mipi-dsi or hdmi) */
		if (decon->out_type == DECON_OUT_HDMI) {
			ret = v4l2_subdev_call(decon->output_sd, core, s_power, 1);
			if (ret) {
				decon_err("failed to get power for %s\n",
						decon->output_sd->name);
				goto err;
			}
		} else if (decon->out_type == DECON_OUT_DSI) {
			pm_stay_awake(decon->dev);
			dev_warn(decon->dev, "pm_stay_awake");
			ret = v4l2_subdev_call(decon->output_sd, video, s_stream, 1);
			if (ret) {
				decon_err("starting stream failed for %s\n",
						decon->output_sd->name);
				goto err;
			}
		}
	}

	if(decon->out_type == DECON_OUT_HDMI){
#if defined(CONFIG_PM_RUNTIME)
		pm_runtime_get_sync(decon->dev);
#else
		decon_runtime_resume(decon->dev);
#endif
	}

	ret = iovmm_activate(decon->dev);
	if (ret < 0) {
		decon_err("failed to reactivate vmm\n");
		goto err;
	}
	ret = 0;

	if (!decon->id)
		decon_set_sysreg_disp_cfg(decon);

	decon_to_init_param(decon, &p);
	decon_reg_init(decon->id, decon->pdata->dsi_mode, &p);

#if defined(CONFIG_EXYNOS_DECON_DPU)
	decon_reg_enable_apb_clk(decon->id, 1);
	if (!decon->id && decon->state != DECON_STATE_LPD_EXIT_REQ) {
		dpu_reg_start(decon->lcd_info->xres, decon->lcd_info->yres);
		decon_dpu_restore(decon->dev);
	}
#endif

	decon_to_psr_info(decon, &psr);
	if (decon->state != DECON_STATE_LPD_EXIT_REQ) {
	/* In case of resume*/
		if (decon->out_type != DECON_OUT_WB)
			decon_reg_start(decon->id, decon->pdata->dsi_mode, &psr);
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		if (!decon->id) {
			ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL);
			if (ret)
				decon_err("Failed to call DSIM packet go enable!\n");
		}
#endif
	}
	if (decon->out_type == DECON_OUT_HDMI) {
		ret = v4l2_subdev_call(decon->output_sd, video, s_stream, 1);
		if (ret) {
			decon_err("starting stream failed for %s\n",
					decon->output_sd->name);
			goto err;
		}
	}

#ifdef CONFIG_FB_WINDOW_UPDATE
	if ((decon->out_type == DECON_OUT_DSI) && (decon->need_update)) {
		if (decon->state != DECON_STATE_LPD_EXIT_REQ) {
			decon->need_update = false;
			decon->update_win.x = 0;
			decon->update_win.y = 0;
			decon->update_win.w = decon->lcd_info->xres;
			decon->update_win.h = decon->lcd_info->yres;
		} else {
			decon_win_update_disp_config(decon, &decon->update_win);
		}
	}
#endif
	if (!decon->id && !decon->eint_status) {
		enable_irq(decon->irq);
		decon->eint_status = 1;
	}

	decon->state = DECON_STATE_ON;
	g_info->enable = true;
	lcdfreq_lock_free(NULL);

	if(decon->out_type == DECON_OUT_DSI) {
#if !defined(CONFIG_EXYNOS_DECON_LCD_S6E8AA0) && !defined(CONFIG_EXYNOS_DECON_LCD_S6E8FA0) && !defined(CONFIG_MEIZU_DECON_LCD_S6E3FA3)
		decon_reg_clear_int(decon->id);
		decon_reg_set_int(decon->id, &psr, DSI_MODE_SINGLE, 1);
#endif
	}

err:
	exynos_ss_printk("%s:state %d: active %d:-\n", __func__,
				decon->state, pm_runtime_active(decon->dev));
	if (state != DECON_STATE_LPD_EXIT_REQ)
		mutex_unlock(&decon->output_lock);
	return ret;
}

int decon_disable(struct decon_device *decon)
{
	struct decon_psr_info psr;
	int ret = 0;
	int i, j;
	unsigned long irq_flags;
	int state = decon->state;
	int old_plane_cnt[MAX_DECON_EXT_WIN];
	g_info->enable = false;
	exynos_ss_printk("disable decon-%s, state(%d) cnt %d\n", decon->id ? "ext" : "int",
				decon->state, pm_runtime_active(decon->dev));

	if (decon->state != DECON_STATE_LPD_ENT_REQ)
		mutex_lock(&decon->output_lock);

	if (decon->state == DECON_STATE_OFF) {
		decon_info("decon%d already disabled\n", decon->id);
		goto err;
	} else if (decon->state == DECON_STATE_LPD) {
#ifdef DECON_LPD_OPT
		decon_lcd_off(decon);
		decon_info("decon is LPD state. only lcd is off\n");
#endif
		goto err;
	}

	flush_kthread_worker(&decon->update_regs_worker);


	if(decon->out_type == DECON_OUT_DSI) {
#if !defined(CONFIG_EXYNOS_DECON_LCD_S6E8AA0) && !defined(CONFIG_EXYNOS_DECON_LCD_S6E8FA0) && !defined(CONFIG_MEIZU_DECON_LCD_S6E3FA3)
		decon_reg_set_int(decon->id, &psr, DSI_MODE_SINGLE, 0);
#endif
	}

	if (!decon->id && (decon->vsync_info.irq_refcount <= 0) &&
			decon->eint_status) {
		disable_irq(decon->irq);
		decon->eint_status = 0;
	}

	if (decon->out_type == DECON_OUT_WB) {
		if (decon->wb_timeline_max > decon->wb_timeline->value) {
			if (wait_event_interruptible_timeout(decon->wait_frmdone,
					atomic_read(&decon->wb_done) == STATE_DONE,
					msecs_to_jiffies(VSYNC_TIMEOUT_MSEC)) == 0) {
				decon_warn("write-back timeout\n");
			} else {
				decon->frame_done_cnt_cur++;
				atomic_set(&decon->wb_done, STATE_IDLE);
			}
			sw_sync_timeline_inc(decon->wb_timeline, 1);
		}

		for (i = 0; i < MAX_DECON_EXT_WIN; i++) {
			old_plane_cnt[i] = decon->windows[i]->plane_cnt;
			for (j = 0; j < old_plane_cnt[i]; ++j) {
				decon_free_dma_buf(decon,
					&decon->windows[i]->dma_buf_data[j]);
			}
		}

		decon->frame_done_cnt_target += decon->frame_done_cnt_cur;
		decon_info("write-back cur:%d/total:%d, timeline:%d/max:%d\n",
			decon->frame_done_cnt_cur, decon->frame_done_cnt_target,
			decon->wb_timeline->value, decon->wb_timeline_max);
		decon->frame_done_cnt_cur = 0;
	}

#if defined(CONFIG_EXYNOS_DECON_DPU)
	decon_reg_enable_apb_clk(decon->id, 1);
	if (!decon->id  && decon->state != DECON_STATE_LPD_ENT_REQ)
		decon_dpu_save(decon->dev);
#endif

	decon_to_psr_info(decon, &psr);
	decon_reg_stop(decon->id, decon->pdata->dsi_mode, &psr);
	decon_reg_clear_int(decon->id);

	if (decon->out_type != DECON_OUT_WB) {
		/* DMA protection disable must be happen on vpp domain is alive */
		decon_set_protected_content(decon, NULL);
		decon->vpp_usage_bitmask = 0;
		decon_vpp_stop(decon, true);
		iovmm_deactivate(decon->dev);
	}

#if defined(CONFIG_EXYNOS_DECON_DPU)
	decon_reg_enable_apb_clk(decon->id, 1);
	if (!decon->id && decon->state != DECON_STATE_LPD_ENT_REQ)
		dpu_reg_stop();
#endif

	/* Synchronize the decon->state with irq_handler */
	spin_lock_irqsave(&decon->slock, irq_flags);
	if (state == DECON_STATE_LPD_ENT_REQ)
		decon->state = DECON_STATE_LPD;
	spin_unlock_irqrestore(&decon->slock, irq_flags);
#if defined(CONFIG_DECON_DEVFREQ)
	if (!decon->id) {
		exynos7_update_media_scenario(TYPE_DECON_INT, 0, 0);
		pm_qos_update_request(&decon->disp_qos, 0);
		pm_qos_update_request(&decon->int_qos, 0);
		if (decon->prev_frame_has_yuv)
			exynos7_update_media_scenario(TYPE_YUV, 0, 0);
	} else {
		exynos7_update_media_scenario(TYPE_DECON_EXT, 0, 0);
	}
	decon->prev_bw = 0;
	decon->prev_int_bw = 0;
	decon->prev_disp_bw = 0;
	decon->prev_frame_has_yuv = 0;
	if ((decon->out_type != DECON_OUT_DSI) &&
		(decon->out_type != DECON_OUT_TUI)) {
		pm_qos_remove_request(&exynos7_tv_mif_qos);
		pm_qos_remove_request(&exynos7_tv_int_qos);
		pm_qos_remove_request(&exynos7_tv_disp_qos);
	}
#endif
#if defined(CONFIG_PM_RUNTIME)
	if (state != DECON_STATE_LPD_ENT_REQ)
		pm_runtime_put_sync(decon->dev);
	else
		decon_runtime_suspend(decon->dev);
#else
	decon_runtime_suspend(decon->dev);
#endif

	if (state == DECON_STATE_LPD_ENT_REQ) {
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl,
				DSIM_IOC_ENTER_ULPS, (unsigned long *)1);
		if (ret) {
			decon_err("%s: failed to enter ULPS state for %s\n",
					__func__, decon->output_sd->name);
			goto err;
		}
		decon->state = DECON_STATE_LPD;
	} else {
		if (decon->out_type != DECON_OUT_WB) {
			/* stop output device (mipi-dsi or hdmi) */
			ret = v4l2_subdev_call(decon->output_sd, video, s_stream, 0);
			if (ret) {
				decon_err("stopping stream failed for %s\n",
						decon->output_sd->name);
				goto err;
			}
		}
		if (decon->out_type == DECON_OUT_HDMI) {
			ret = v4l2_subdev_call(decon->output_sd, core, s_power, 0);
			if (ret) {
				decon_err("failed to put power for %s\n",
						decon->output_sd->name);
				goto err;
			}
		} else if (decon->out_type == DECON_OUT_DSI) {
			pm_relax(decon->dev);
			dev_warn(decon->dev, "pm_relax");
		}

		decon->state = DECON_STATE_OFF;
	}

err:
	exynos_ss_printk("%s:state %d: active%d:-\n", __func__,
				decon->state, pm_runtime_active(decon->dev));
	if (state != DECON_STATE_LPD_ENT_REQ)
		mutex_unlock(&decon->output_lock);
	return ret;
}

static int decon_blank(int blank_mode, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	int ret = 0;

#ifdef CONFIG_DECON_LPD_DISPLAY
	if (decon->b_enable != 1)
		return ret;
#endif

	decon_info("decon-%s %s mode: %dtype (0: DSI, 1: HDMI, 2:WB)\n",
			decon->id ? "ext" : "int",
			blank_mode == FB_BLANK_UNBLANK ? "UNBLANK" : "POWERDOWN",
			decon->out_type);

	decon_lpd_block_exit(decon);

#ifdef CONFIG_USE_VSYNC_SKIP
	decon_extra_vsync_wait_set(ERANGE);
#endif /* CONFIG_USE_VSYNC_SKIP */

	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
		DISP_SS_EVENT_LOG(DISP_EVT_BLANK, &decon->sd, ktime_set(0, 0));
		ret = decon_disable(decon);
		if (ret) {
			decon_err("failed to disable decon\n");
			goto blank_exit;
		}
		break;
	case FB_BLANK_UNBLANK:
		DISP_SS_EVENT_LOG(DISP_EVT_UNBLANK, &decon->sd, ktime_set(0, 0));
		ret = decon_enable(decon);
		if (ret) {
			decon_err("failed to enable decon\n");
			goto blank_exit;
		}
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	default:
		ret = -EINVAL;
	}

blank_exit:
	decon_lpd_unblock(decon);
	decon_info("%s -\n", __func__);
	return ret;
}

/* ---------- FB_IOCTL INTERFACE ----------- */
static void decon_activate_vsync(struct decon_device *decon)
{
#if defined(CONFIG_EXYNOS_DECON_LCD_S6E8AA0) || defined(CONFIG_EXYNOS_DECON_LCD_S6E8FA0)
	|| defined(CONFIG_MEIZU_DECON_LCD_S6E3FA3)
	struct decon_psr_info psr;
#endif
	int prev_refcount;

	mutex_lock(&decon->vsync_info.irq_lock);

	prev_refcount = decon->vsync_info.irq_refcount++;
	if (!prev_refcount) {
		if(decon->out_type == DECON_OUT_DSI) {
#if defined(CONFIG_EXYNOS_DECON_LCD_S6E8AA0) || defined(CONFIG_EXYNOS_DECON_LCD_S6E8FA0)
	|| defined(CONFIG_MEIZU_DECON_LCD_S6E3FA3)
			decon_to_psr_info(decon, &psr);
			decon_reg_set_int(decon->id, &psr, DSI_MODE_SINGLE, 1);
#endif
		}
		DISP_SS_EVENT_LOG(DISP_EVT_ACT_VSYNC, &decon->sd, ktime_set(0, 0));
	}

	mutex_unlock(&decon->vsync_info.irq_lock);
}

static void decon_deactivate_vsync(struct decon_device *decon)
{
#if defined(CONFIG_EXYNOS_DECON_LCD_S6E8AA0) || defined(CONFIG_EXYNOS_DECON_LCD_S6E8FA0)
	|| defined(CONFIG_MEIZU_DECON_LCD_S6E3FA3)
	struct decon_psr_info psr;
#endif
	int new_refcount;

	mutex_lock(&decon->vsync_info.irq_lock);

	new_refcount = --decon->vsync_info.irq_refcount;
	WARN_ON(new_refcount < 0);
	if (!new_refcount) {
		if(decon->out_type == DECON_OUT_DSI) {
#if defined(CONFIG_EXYNOS_DECON_LCD_S6E8AA0) || defined(CONFIG_EXYNOS_DECON_LCD_S6E8FA0)
	|| defined(CONFIG_MEIZU_DECON_LCD_S6E3FA3)
			decon_to_psr_info(decon, &psr);
			decon_reg_set_int(decon->id, &psr, DSI_MODE_SINGLE, 0);
#endif
		}
		DISP_SS_EVENT_LOG(DISP_EVT_DEACT_VSYNC, &decon->sd, ktime_set(0, 0));
	}

	mutex_unlock(&decon->vsync_info.irq_lock);
}

int decon_wait_for_vsync(struct decon_device *decon, u32 timeout)
{
	ktime_t timestamp;
	int ret;

	if (decon->out_type == DECON_OUT_TUI)
		return 0;

	timestamp = decon->vsync_info.timestamp;
	if (decon->pdata->trig_mode == DECON_SW_TRIG)
		decon_activate_vsync(decon);

	if (timeout) {
		ret = wait_event_interruptible_timeout(decon->vsync_info.wait,
				!ktime_equal(timestamp,
						decon->vsync_info.timestamp),
				msecs_to_jiffies(timeout));
	} else {
		ret = wait_event_interruptible(decon->vsync_info.wait,
				!ktime_equal(timestamp,
						decon->vsync_info.timestamp));
	}

	if (decon->pdata->trig_mode == DECON_SW_TRIG)
		decon_deactivate_vsync(decon);

	if (timeout && ret == 0) {
		if (decon->eint_pend && decon->eint_mask) {
			decon_err("decon%d wait for vsync timeout(p:0x%x, m:0x%x)\n",
				decon->id, readl(decon->eint_pend),
				readl(decon->eint_mask));
		} else {
			decon_err("decon%d wait for vsync timeout\n", decon->id);
		}

		return -ETIMEDOUT;
	}

	return 0;
}

int decon_set_window_position(struct fb_info *info,
				struct decon_user_window user_window)
{
	return 0;
}

int decon_set_plane_alpha_blending(struct fb_info *info,
				struct s3c_fb_user_plane_alpha user_alpha)
{
	return 0;
}

int decon_set_chroma_key(struct fb_info *info,
			struct s3c_fb_user_chroma user_chroma)
{
	return 0;
}

int decon_set_vsync_int(struct fb_info *info, bool active)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	bool prev_active = decon->vsync_info.active;

	decon->vsync_info.active = active;
	smp_wmb();

	if (active && !prev_active)
		decon_activate_vsync(decon);
	else if (!active && prev_active)
		decon_deactivate_vsync(decon);

	return 0;
}

static unsigned int decon_map_ion_handle(struct decon_device *decon,
		struct device *dev, struct decon_dma_buf_data *dma,
		struct ion_handle *ion_handle, struct dma_buf *buf, int win_no)
{
	dma->fence = NULL;
	dma->dma_buf = buf;

	dma->attachment = dma_buf_attach(dma->dma_buf, dev);
	if (IS_ERR_OR_NULL(dma->attachment)) {
		decon_err("dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma->attachment));
		goto err_buf_map_attach;
	}

	dma->sg_table = dma_buf_map_attachment(dma->attachment,
			DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dma->sg_table)) {
		decon_err("dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma->sg_table));
		goto err_buf_map_attachment;
	}

	dma->dma_addr = ion_iovmm_map(dma->attachment, 0,
			dma->dma_buf->size, DMA_TO_DEVICE, win_no);
	if (!dma->dma_addr || IS_ERR_VALUE(dma->dma_addr)) {
		decon_err("iovmm_map() failed: %pa\n", &dma->dma_addr);
		goto err_iovmm_map;
	}

	exynos_ion_sync_dmabuf_for_device(dev, dma->dma_buf, dma->dma_buf->size,
			DMA_TO_DEVICE);

	dma->ion_handle = ion_handle;

	return dma->dma_buf->size;

err_iovmm_map:
	dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
			DMA_TO_DEVICE);
err_buf_map_attachment:
	dma_buf_detach(dma->dma_buf, dma->attachment);
err_buf_map_attach:
	return 0;
}



static int decon_get_plane_cnt(enum decon_pixel_format format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
	case DECON_PIXEL_FORMAT_RGBA_5551:
	case DECON_PIXEL_FORMAT_RGB_565:
		return 1;

	case DECON_PIXEL_FORMAT_NV16:
	case DECON_PIXEL_FORMAT_NV61:
	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
		return 2;

	case DECON_PIXEL_FORMAT_YVU422_3P:
	case DECON_PIXEL_FORMAT_YUV420:
	case DECON_PIXEL_FORMAT_YVU420:
	case DECON_PIXEL_FORMAT_YUV420M:
	case DECON_PIXEL_FORMAT_YVU420M:
		return 3;

	default:
		decon_err("invalid format(%d)\n", format);
		return 1;
		//return -EINVAL;
	}

}

inline static u32 get_vpp_src_format_opaque(u32 format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return DECON_PIXEL_FORMAT_BGRX_8888;
	case DECON_PIXEL_FORMAT_RGBA_8888:
		return DECON_PIXEL_FORMAT_RGBX_8888;
	case DECON_PIXEL_FORMAT_ABGR_8888:
		return DECON_PIXEL_FORMAT_XBGR_8888;
	case DECON_PIXEL_FORMAT_ARGB_8888:
		return DECON_PIXEL_FORMAT_XRGB_8888;
	default:
		return format;
	}
}

static u32 get_vpp_src_format(u32 format, int id)
{
	switch (id) {
        case IDMA_VG0:
        case IDMA_VG1:
                format = get_vpp_src_format_opaque(format);
		break;
        default:
                break;
        }

	switch (format) {
	case DECON_PIXEL_FORMAT_NV12:
		return DECON_PIXEL_FORMAT_NV21;
	case DECON_PIXEL_FORMAT_NV21:
		return DECON_PIXEL_FORMAT_NV12;
	case DECON_PIXEL_FORMAT_NV12M:
		return DECON_PIXEL_FORMAT_NV21M;
	case DECON_PIXEL_FORMAT_NV21M:
		return DECON_PIXEL_FORMAT_NV12M;
	case DECON_PIXEL_FORMAT_YUV420:
		return DECON_PIXEL_FORMAT_YVU420;
	case DECON_PIXEL_FORMAT_YVU420:
		return DECON_PIXEL_FORMAT_YUV420;
	case DECON_PIXEL_FORMAT_YUV420M:
		return DECON_PIXEL_FORMAT_YVU420M;
	case DECON_PIXEL_FORMAT_YVU420M:
		return DECON_PIXEL_FORMAT_YUV420M;
	case DECON_PIXEL_FORMAT_ARGB_8888:
		return DECON_PIXEL_FORMAT_BGRA_8888;
	case DECON_PIXEL_FORMAT_ABGR_8888:
		return DECON_PIXEL_FORMAT_RGBA_8888;
	case DECON_PIXEL_FORMAT_RGBA_8888:
		return DECON_PIXEL_FORMAT_ABGR_8888;
	case DECON_PIXEL_FORMAT_BGRA_8888:
		return DECON_PIXEL_FORMAT_ARGB_8888;
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return DECON_PIXEL_FORMAT_BGRX_8888;
	case DECON_PIXEL_FORMAT_XBGR_8888:
		return DECON_PIXEL_FORMAT_RGBX_8888;
	case DECON_PIXEL_FORMAT_RGBX_8888:
		return DECON_PIXEL_FORMAT_XBGR_8888;
	case DECON_PIXEL_FORMAT_BGRX_8888:
		return DECON_PIXEL_FORMAT_XRGB_8888;
	default:
		return format;
	}
}

static u32 get_vpp_out_format(u32 format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21M:
	case DECON_PIXEL_FORMAT_YUV420:
	case DECON_PIXEL_FORMAT_YVU420:
	case DECON_PIXEL_FORMAT_YUV420M:
	case DECON_PIXEL_FORMAT_YVU420M:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
		return DECON_PIXEL_FORMAT_BGRA_8888;
	default:
		return format;
	}
}

static bool decon_get_protected_idma(struct decon_device *decon, u32 prot_bits)
{
	int i;
	u32 used_idma = 0;

	for (i = 0; i < MAX_DMA_TYPE - 1; i++)
		if ((prot_bits & (1 << i)) >> i)
			used_idma++;

	if (used_idma)
		return true;
	else
		return false;
}

static void decon_set_protected_content(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	bool prot;
	int type, i, ret = 0;
	u32 change = 0;
	u32 cur_protect_bits = 0;

	/**
	 * get requested DMA protection configs
	 * 0:G0 1:G1 2:VG0 3:VG1 4:VGR0 5:VGR1 6:G2 7:G3 8:WB1
	 */
	/* IDMA protection configs (G0,G1,VG0,VG1,VGR0,VGR1,G2,G3) */
	for (i = 0; i < decon->pdata->max_win; i++) {
		if (!regs)
			break;

		cur_protect_bits |=
			(regs->protection[i] << regs->vpp_config[i].idma_type);
	}

	/* ODMA(for writeback) protection config (WB1)*/
	if (decon->out_type == DECON_OUT_WB)
		if (decon_get_protected_idma(decon, cur_protect_bits))
			cur_protect_bits |= (1 << DECON_ODMA_WB);

	decon->cur_protection_bitmask = cur_protect_bits;
	if (decon->prev_protection_bitmask != cur_protect_bits) {
		decon_reg_wait_linecnt_is_zero_timeout(decon->id, 0, 35 * 1000);

		/* apply protection configs for each DMA */
		for (type = 0; type < MAX_DMA_TYPE; type++) {
			prot = cur_protect_bits & (1 << type);

			change = (cur_protect_bits & (1 << type)) ^
				(decon->prev_protection_bitmask & (1 << type));

			if (change) {
				if (is_vpp_type(type)) {
					struct v4l2_subdev *sd = NULL;
					sd = decon->mdev->vpp_sd[type - 2];
					v4l2_subdev_call(sd, core, ioctl,
						VPP_WAIT_IDLE, NULL);
					TRACE_VPP_LOG(decon, prot);
				}

				ret = exynos_smc(SMC_PROTECTION_SET, 0,
						type + DECON_TZPC_OFFSET, prot);
				if (ret != SUCCESS_EXYNOS_SMC) {
					WARN(1, "decon%d DMA-%d smc call fail\n",
							decon->id, type);
				} else {
					if (is_vpp_type(type))
						TRACE_VPP_LOG(decon, prot);
					decon_info("decon%d DMA-%d protection %s\n",
							decon->id, type, prot ? "enabled" : "disabled");
				}
			}
		}
	}

	/* save current portection configs */
	decon->prev_protection_bitmask = cur_protect_bits;
}

static inline int decon_set_alpha_blending(struct decon_win_config *win_config,
		struct decon_reg_data *regs, int win_no, int transp_length)
{
	u8 alpha0, alpha1;

	if ((win_config->plane_alpha > 0) && (win_config->plane_alpha < 0xFF)) {
		alpha0 = win_config->plane_alpha;
		alpha1 = 0;
	} else if (transp_length == 1 &&
			win_config->blending == DECON_BLENDING_NONE) {
		alpha0 = 0xff;
		alpha1 = 0xff;
	} else {
		alpha0 = 0;
		alpha1 = 0xff;
	}
	regs->vidosd_c[win_no] = vidosd_c(alpha0, alpha0, alpha0);
	regs->vidosd_d[win_no] = vidosd_d(alpha1, alpha1, alpha1);

	if (win_no) {
		if ((win_config->plane_alpha > 0) && (win_config->plane_alpha < 0xFF)) {
			if (transp_length) {
				if (win_config->blending != DECON_BLENDING_NONE)
					regs->wincon[win_no] |= WINCON_ALPHA_MUL;
			} else {
				regs->wincon[win_no] &= (~WINCON_ALPHA_SEL);
				if (win_config->blending == DECON_BLENDING_PREMULT)
					win_config->blending = DECON_BLENDING_COVERAGE;
			}
		}
		regs->blendeq[win_no - 1] = blendeq(win_config->blending,
					transp_length, win_config->plane_alpha);
	}

	return 0;
}

static int decon_set_win_buffer(struct decon_device *decon, struct decon_win *win,
		struct decon_win_config *win_config, struct decon_reg_data *regs)
{
	struct ion_handle *handle;
	struct fb_var_screeninfo prev_var = win->fbinfo->var;
	struct dma_buf *buf[MAX_BUF_PLANE_CNT];
	struct decon_dma_buf_data dma_buf_data[MAX_BUF_PLANE_CNT];
	unsigned short win_no = win->index;
	int ret, i;
	size_t buf_size = 0, window_size;
	int plane_cnt;
	u32 format;

	if (win_config->format >= DECON_PIXEL_FORMAT_MAX) {
		decon_err("unknown pixel format %u\n", win_config->format);
		return -EINVAL;
	}

	if (win_config->blending >= DECON_BLENDING_MAX) {
		decon_err("unknown blending %u\n", win_config->blending);
		return -EINVAL;
	}

	if (win_no == 0 && win_config->blending != DECON_BLENDING_NONE) {
		decon_err("blending not allowed on window 0\n");
		return -EINVAL;
	}

	if (win_config->dst.w == 0 || win_config->dst.h == 0 ||
			win_config->dst.x < 0 || win_config->dst.y < 0) {
		decon_err("win[%d] size is abnormal (w:%d, h:%d, x:%d, y:%d)\n",
				win_no, win_config->dst.w, win_config->dst.h,
				win_config->dst.x, win_config->dst.y);
		return -EINVAL;
	}

	format = win_config->format;
	if (is_vpp_type(win_config->idma_type))
		format = get_vpp_out_format(format);

	win->fbinfo->var.red.length = decon_red_length(format);
	win->fbinfo->var.red.offset = decon_red_offset(format);
	win->fbinfo->var.green.length = decon_green_length(format);
	win->fbinfo->var.green.offset = decon_green_offset(format);
	win->fbinfo->var.blue.length = decon_blue_length(format);
	win->fbinfo->var.blue.offset = decon_blue_offset(format);
	win->fbinfo->var.transp.length =
			decon_transp_length(format);
	win->fbinfo->var.transp.offset =
			decon_transp_offset(format);
	win->fbinfo->var.bits_per_pixel = win->fbinfo->var.red.length +
			win->fbinfo->var.green.length +
			win->fbinfo->var.blue.length +
			win->fbinfo->var.transp.length +
			decon_padding(format);

	if (win_config->dst.w * win->fbinfo->var.bits_per_pixel / 8 < 128) {
		decon_err("window wide < 128bytes, width = %u, bpp = %u)\n",
				win_config->dst.w,
				win->fbinfo->var.bits_per_pixel);
		ret = -EINVAL;
		goto err_invalid;
	}

	if (!is_vpp_type(win_config->idma_type)) {
		if (win_config->src.f_w < win_config->dst.w) {
			decon_err("f_width(%u) < width(%u),\
				bpp = %u\n", win_config->src.f_w,
				win_config->dst.w,
				win->fbinfo->var.bits_per_pixel);
			ret = -EINVAL;
			goto err_invalid;
		}
	}

	if (!decon_validate_x_alignment(decon, win_config->dst.x,
				win_config->dst.w,
				win->fbinfo->var.bits_per_pixel)) {
		ret = -EINVAL;
		goto err_invalid;
	}

	plane_cnt = decon_get_plane_cnt(win_config->format);
	for (i = 0; i < plane_cnt; ++i) {
		handle = ion_import_dma_buf(decon->ion_client, win_config->fd_idma[i]);
		if (IS_ERR(handle)) {
			decon_err("failed to import fd\n");
			ret = PTR_ERR(handle);
			goto err_invalid;
		}

		buf[i] = dma_buf_get(win_config->fd_idma[i]);
		if (IS_ERR_OR_NULL(buf[i])) {
			decon_err("dma_buf_get() failed: %ld\n", PTR_ERR(buf[i]));
			ret = PTR_ERR(buf[i]);
			goto err_buf_get;
		}

		if (is_vpp_type(win_config->idma_type)) {
			int vpp_id = win_config->idma_type - 2;
			struct device *dev =
				decon->mdev->vpp_dev[vpp_id];
			buf_size = decon_map_ion_handle(decon, dev, &dma_buf_data[i],
					handle, buf[i], win_no);
		} else {
			buf_size = decon_map_ion_handle(decon, decon->dev,
					&dma_buf_data[i], handle, buf[i], win_no);
		}

		if (!buf_size) {
			ret = -ENOMEM;
			goto err_map;
		}
		win_config->vpp_parm.addr[i] = dma_buf_data[i].dma_addr;
		handle = NULL;
		buf[i] = NULL;
	}
	if (win_config->fence_fd >= 0) {
		dma_buf_data[0].fence = sync_fence_fdget(win_config->fence_fd);
		if (!dma_buf_data[0].fence) {
			decon_err("failed to import fence fd\n");
			ret = -EINVAL;
			goto err_offset;
		}
		decon_dbg("%s(%d): fence_fd(%d), fence(%lx)\n", __func__, __LINE__,
				win_config->fence_fd, (ulong)dma_buf_data[0].fence);
	}

	if (!is_vpp_type(win_config->idma_type)) {
		window_size = win_config->dst.w * win_config->dst.h *
			win->fbinfo->var.bits_per_pixel / 8;
		if (window_size > buf_size) {
			decon_err("window size(%zu) > buffer size(%zu)\n",
					window_size, buf_size);
			ret = -EINVAL;
			goto err_offset;
		}
	}

	win->fbinfo->fix.smem_start = dma_buf_data[0].dma_addr;
	win->fbinfo->fix.smem_len = buf_size;
	win->fbinfo->var.xres = win_config->dst.w;
	win->fbinfo->var.xres_virtual = win_config->dst.f_w;
	win->fbinfo->var.yres = win_config->dst.h;
	win->fbinfo->var.yres_virtual = win_config->dst.f_h;
	win->fbinfo->var.xoffset = win_config->src.x;
	win->fbinfo->var.yoffset = win_config->src.y;

	win->fbinfo->fix.line_length = win_config->src.f_w *
			win->fbinfo->var.bits_per_pixel / 8;
	win->fbinfo->fix.xpanstep = fb_panstep(win_config->dst.w,
			win->fbinfo->var.xres_virtual);
	win->fbinfo->fix.ypanstep = fb_panstep(win_config->dst.h,
			win->fbinfo->var.yres_virtual);

	plane_cnt = decon_get_plane_cnt(win_config->format);
	for (i = 0; i < plane_cnt; ++i)
		regs->dma_buf_data[win_no][i] = dma_buf_data[i];
	regs->buf_start[win_no] = win->fbinfo->fix.smem_start;

	regs->vidosd_a[win_no] = vidosd_a(win_config->dst.x, win_config->dst.y);
	regs->vidosd_b[win_no] = vidosd_b(win_config->dst.x, win_config->dst.y,
			win_config->dst.w, win_config->dst.h);
	if (!is_vpp_type(win_config->idma_type)) {
		regs->whole_w[win_no] = win_config->src.f_w;
		regs->whole_h[win_no] = win_config->src.f_h;
		regs->offset_x[win_no] = win_config->src.x;
		regs->offset_y[win_no] = win_config->src.y;
	} else {
		regs->whole_w[win_no] = win_config->dst.f_w;
		regs->whole_h[win_no] = win_config->dst.f_h;
		regs->offset_x[win_no] = win_config->dst.x;
		regs->offset_y[win_no] = win_config->dst.y;
	}

	regs->wincon[win_no] = wincon(win->fbinfo->var.bits_per_pixel,
			win->fbinfo->var.transp.length);
	regs->wincon[win_no] |= decon_rgborder(format);
	regs->protection[win_no] = win_config->protection;
	decon_set_alpha_blending(win_config, regs, win_no,
				win->fbinfo->var.transp.length);
	decon_dbg("win[%d] SRC:(%d,%d) %dx%d  DST:(%d,%d) %dx%d\n", win_no,
			win_config->src.x, win_config->src.y,
			win_config->src.f_w, win_config->src.f_h,
			win_config->dst.x, win_config->dst.y,
			win_config->dst.w, win_config->dst.h);

	return 0;

err_offset:
	for (i = 0; i < plane_cnt; ++i)
		decon_free_dma_buf(decon, &dma_buf_data[i]);
err_map:
	for (i = 0; i < plane_cnt; ++i)
		dma_buf_put(buf[i]);
err_buf_get:
	if (handle)
		ion_free(decon->ion_client, handle);
err_invalid:
	win->fbinfo->var = prev_var;
	return ret;
}

static int decon_set_wb_buffer(struct decon_device *decon, int fd,
				struct decon_reg_data *regs)
{
	struct ion_handle *handle;
	struct dma_buf *buf;
	struct decon_dma_buf_data wb_dma_buf_data;
	int ret = 0;
	size_t buf_size = 0, window_size = 0;
	struct decon_win_config_data *win_data;
	struct sync_fence *fence;
	struct sync_pt *pt;

	DISP_SS_EVENT_LOG(DISP_EVT_WB_SET_BUFFER, &decon->sd, ktime_set(0, 0));
	win_data = &decon->ioctl_data.win_data;
	handle = ion_import_dma_buf(decon->ion_client, win_data->fd_odma);
	if (IS_ERR(handle)) {
		decon_err("failed to import fd_odma\n");
		ret = PTR_ERR(handle);
		goto fail;
	}

	buf = dma_buf_get(win_data->fd_odma);
	if (IS_ERR_OR_NULL(buf)) {
		decon_err("dma_buf_get() failed: %ld\n", PTR_ERR(buf));
		ret = PTR_ERR(buf);
		goto fail_buf;
	}

	window_size = decon->lcd_info->width * decon->lcd_info->height *
		WB_DEFAULT_BPP / 8;
	buf_size = decon_map_ion_handle(decon, decon->dev, &wb_dma_buf_data,
				handle, buf, 0);
	if (!buf_size) {
		decon_err("failed to mapping buffer\n");
		ret = -ENOMEM;
		goto fail_map;
	} else if (buf_size < window_size) {
		decon_err("writeback output size(%zu) > buffer size(%zu)\n",
				window_size, buf_size);
		ret = -EINVAL;
		goto fail_pt;
	}

	decon->wb_timeline_max++;
	pt = sw_sync_pt_create(decon->wb_timeline, decon->wb_timeline_max);
	if (!pt) {
		decon_err("failed to create sync_pt\n");
		ret = -ENOMEM;
		goto fail_pt;
	}

	fence = sync_fence_create("wb_display", pt);
	if (!fence) {
		decon_err("failed to craete fence\n");
		ret = -ENOMEM;
		goto fail_fence;
	}

	sync_fence_install(fence, fd);
	win_data->fence = fd;
	handle = NULL;
	buf = NULL;
	decon->wb_dma_buf_data = wb_dma_buf_data;
	regs->wb_dma_buf_data = wb_dma_buf_data;
	regs->wb_whole_w = decon->lcd_info->width;
	regs->wb_whole_h = decon->lcd_info->height;

	return ret;

fail_fence:
	sync_pt_free(pt);

fail_pt:
	decon_free_dma_buf(decon, &wb_dma_buf_data);

fail_map:
	if (buf)
		dma_buf_put(buf);

fail_buf:
	if (handle)
		ion_free(decon->ion_client, handle);

fail:
	decon_err("failed save write back buffer configuration\n");
	return ret;
}

#ifdef CONFIG_FB_WINDOW_UPDATE
static void decon_modulate_overlap_cnt(struct decon_device *decon,
		struct decon_win_config *win_config,
		struct decon_reg_data *regs)
{
	struct decon_lcd *lcd_info = decon->lcd_info;
	int i;
	int overlap_cnt = regs->win_overlap_cnt;

	/*
	* If VPP layer is present and the layer's width is over 10%
	* of LCD_WIDTH, original overlap count must be used.
	* - If overlap count is 1:
	* => width < 0.8 * LCD_WIDTH  ===> overlap count 1 ¡æ 0.
	* => (width > 0.8 * LCD_WIDTH) && (height <= 60) => overlap count 1 ¡æ 0
	* - If overlap count is 2:
	* => width < 0.4 * LCD_WIDTH  ===> overlap count 2 ¡æ 0.
	* => (width < 0.6 * LCD_WIDTH) && (height <= 30) => overlap count 2 ¡æ 0
	* => (width < 0.8 * LCD_WIDTH) && (height <= 10) => overlap count 2 ¡æ 0
	* - If overlap count is 3:
	* => width < 0.2 * LCD_WIDTH  ===> overlap count 3 ¡æ 0.
	* => (width < 0.4 * LCD_WIDTH) && (height <= 15) => overlap count 3 ¡æ 0
	* => (width < 0.6 * LCD_WIDTH) && (height <= 8) => overlap count 3 ¡æ 0
	*/

	if (regs->need_update) {
		for (i = 0; i < decon->pdata->max_win; i++) {
			struct decon_win_config *config = &win_config[i];
			if ((config->state != DECON_WIN_STATE_DISABLED) &&
					(is_vpp_type(config->idma_type))) {
				/* VPP layer dst width is over 10% */
				if (config->dst.w * 10 > lcd_info->xres)
					return;
			}
		}

		switch(regs->win_overlap_cnt) {
		case 1:
			if ((regs->update_win.w * 10 < lcd_info->xres * 8) ||
				(regs->update_win.h <= 60)) {
				regs->win_overlap_cnt = 0;
			}
			break;
		case 2:
			if ((regs->update_win.w * 10 < lcd_info->xres * 4) ||
			((regs->update_win.w * 10 < lcd_info->xres * 6) &&
			 (regs->update_win.h <= 30)) ||
			((regs->update_win.w * 10 < lcd_info->xres * 8) &&
			 (regs->update_win.h <= 10))) {
				regs->win_overlap_cnt = 0;
			}
			break;
		case 3:
			if ((regs->update_win.w * 10 < lcd_info->xres * 2) ||
			((regs->update_win.w * 10 < lcd_info->xres * 4) &&
			 (regs->update_win.h <= 15)) ||
			((regs->update_win.w * 10 < lcd_info->xres * 6) &&
			 (regs->update_win.h <= 8))) {
				regs->win_overlap_cnt = 0;
			}
			break;
		default:
			break;
		}
		decon_dbg("WIN_UPDATE[%d %d] overlap_cnt[%d -> %d] \n", regs->update_win.w,
					 regs->update_win.h, overlap_cnt, regs->win_overlap_cnt);
	}
	return;
}

static inline void decon_update_2_full(struct decon_device *decon,
			struct decon_reg_data *regs,
			struct decon_lcd *lcd_info,
			int flag)
{
	if (flag)
		regs->need_update = true;

	decon->need_update = false;
	decon->update_win.x = 0;
	decon->update_win.y = 0;
	decon->update_win.w = lcd_info->xres;
	decon->update_win.h = lcd_info->yres;
	regs->update_win.w = lcd_info->xres;
	regs->update_win.h = lcd_info->yres;
	decon_win_update_dbg("[WIN_UPDATE]update2org: [%d %d %d %d]\n",
			decon->update_win.x, decon->update_win.y, decon->update_win.w, decon->update_win.h);
	return;

}

static void decon_calibrate_win_update_size(struct decon_device *decon,
		struct decon_win_config *win_config,
		struct decon_win_config *update_config)
{
	int i;
	struct decon_rect r1, r2;
	bool rect_changed = false;

	if (update_config->state != DECON_WIN_STATE_UPDATE)
		return;

	r1.left = update_config->dst.x;
	r1.top = update_config->dst.y;
	r1.right = r1.left + update_config->dst.w - 1;
	r1.bottom = r1.top + update_config->dst.h - 1;
	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win_config *config = &win_config[i];
		if (config->state != DECON_WIN_STATE_DISABLED) {
			if (is_vpp_type(config->idma_type) &&
				(is_rotation(config) || does_layer_need_scale(config))) {
				update_config->state = DECON_WIN_STATE_DISABLED;
				return;
			}
			if ((config->src.w != config->dst.w) ||
					(config->src.h != config->dst.h)){
				r2.left = config->dst.x;
				r2.top = config->dst.y;
				r2.right = r2.left + config->dst.w - 1;
				r2.bottom = r2.top + config->dst.h - 1;
				if (decon_intersect(&r1, &r2) &&
					is_decon_rect_differ(&r1, &r2)) {
					decon_union(&r1, &r2, &r1);
					rect_changed = true;
				}
			}
		}
	}

	if (rect_changed) {
		decon_win_update_dbg("update [%d %d %d %d] -> [%d %d %d %d]\n",
			update_config->dst.x, update_config->dst.y,
			update_config->dst.w, update_config->dst.h,
			r1.left, r1.top, r1.right - r1.left + 1,
			r1.bottom - r1.top + 1);
		update_config->dst.x = r1.left;
		update_config->dst.y = r1.top;
		update_config->dst.w = r1.right - r1.left + 1;
		update_config->dst.h = r1.bottom - r1.top + 1;
	}

	if (update_config->dst.x & 0x7) {
		update_config->dst.w += update_config->dst.x & 0x7;
		update_config->dst.x = update_config->dst.x & (~0x7);
	}
	update_config->dst.w = ((update_config->dst.w + 7) & (~0x7));
	if (update_config->dst.x + update_config->dst.w > decon->lcd_info->xres) {
		update_config->dst.w = decon->lcd_info->xres;
		update_config->dst.x = 0;
	}

	return;
}

static void decon_set_win_update_config(struct decon_device *decon,
		struct decon_win_config *win_config,
		struct decon_reg_data *regs)
{
	int i;
	struct decon_win_config *update_config = &win_config[DECON_WIN_UPDATE_IDX];
	struct decon_win_config temp_config;
	struct decon_rect r1, r2;
	struct decon_lcd *lcd_info = decon->lcd_info;
	int need_update = decon->need_update;
	bool is_scale_layer;

	decon_calibrate_win_update_size(decon, win_config, update_config);

	/* if the current mode is not WINDOW_UPDATE, set the config as WINDOW_UPDATE */
	if ((update_config->state == DECON_WIN_STATE_UPDATE) &&
			((update_config->dst.x != decon->update_win.x) ||
			 (update_config->dst.y != decon->update_win.y) ||
			 (update_config->dst.w != decon->update_win.w) ||
			 (update_config->dst.h != decon->update_win.h))) {
		decon->update_win.x = update_config->dst.x;
		decon->update_win.y = update_config->dst.y;
		decon->update_win.w = update_config->dst.w;
		decon->update_win.h = update_config->dst.h;
		decon->need_update = true;
		regs->need_update = true;
		regs->update_win.x = update_config->dst.x;
		regs->update_win.y = update_config->dst.y;
		regs->update_win.w = update_config->dst.w;
		regs->update_win.h = update_config->dst.h;

		decon_win_update_dbg("[WIN_UPDATE]need_update_1: [%d %d %d %d]\n",
				update_config->dst.x, update_config->dst.y, update_config->dst.w, update_config->dst.h);
	} else if (decon->need_update &&
			(update_config->state != DECON_WIN_STATE_UPDATE)) {
		/* Platform requested for normal mode, switch to normal mode from WINDOW_UPDATE */
		decon_update_2_full(decon, regs, lcd_info, true);
		return;
	} else if (decon->need_update) {
		/* It is just for debugging info */
		regs->update_win.x = update_config->dst.x;
		regs->update_win.y = update_config->dst.y;
		regs->update_win.w = update_config->dst.w;
		regs->update_win.h = update_config->dst.h;
	}

	if (update_config->state != DECON_WIN_STATE_UPDATE)
		return;

	r1.left = update_config->dst.x;
	r1.top = update_config->dst.y;
	r1.right = r1.left + update_config->dst.w - 1;
	r1.bottom = r1.top + update_config->dst.h - 1;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win_config *config = &win_config[i];
		if ((config->state == DECON_WIN_STATE_DISABLED) ||
			(!is_vpp_type(config->idma_type)))
			continue;
		r2.left = config->dst.x;
		r2.top = config->dst.y;
		r2.right = r2.left + config->dst.w - 1;
		r2.bottom = r2.top + config->dst.h - 1;
		if (!decon_intersect(&r1, &r2))
			continue;
		decon_intersection(&r1, &r2, &r2);
		if (((r2.right - r2.left) != 0) ||
			((r2.bottom - r2.top) != 0)) {
			if (decon_get_plane_cnt(config->format) == 1) {
			/*
			 * Platform requested for win_update mode. But, the win_update is
			 * smaller than the VPP min size. So, change the mode to normal mode
			 */
				if (((r2.right - r2.left) < 32) ||
					((r2.bottom - r2.top) < 16)) {
					decon_update_2_full(decon, regs, lcd_info, need_update);
					return;
				}
			} else {
				if (((r2.right - r2.left) < 64) ||
					((r2.bottom - r2.top) < 32)) {
					decon_update_2_full(decon, regs, lcd_info, need_update);
					return;
				}
			}
		}
	}

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win_config *config = &win_config[i];
		if (config->state == DECON_WIN_STATE_DISABLED)
			continue;
		r2.left = config->dst.x;
		r2.top = config->dst.y;
		r2.right = r2.left + config->dst.w - 1;
		r2.bottom = r2.top + config->dst.h - 1;
		if (!decon_intersect(&r1, &r2)) {
			config->state = DECON_WIN_STATE_DISABLED;
			continue;
		}
		memcpy(&temp_config, config, sizeof(struct decon_win_config));
		is_scale_layer = does_layer_need_scale(config);
		if (update_config->dst.x > config->dst.x)
			config->dst.w = min(update_config->dst.w,
					config->dst.x + config->dst.w - update_config->dst.x);
		else if(update_config->dst.x + update_config->dst.w < config->dst.x + config->dst.w)
			config->dst.w = min(config->dst.w,
					update_config->dst.w + update_config->dst.x - config->dst.x);

		if (update_config->dst.y > config->dst.y)
			config->dst.h = min(update_config->dst.h,
					config->dst.y + config->dst.h - update_config->dst.y);
		else if(update_config->dst.y + update_config->dst.h < config->dst.y + config->dst.h)
			config->dst.h = min(config->dst.h,
					update_config->dst.h + update_config->dst.y - config->dst.y);

		config->dst.x = max(config->dst.x - update_config->dst.x, 0);
		config->dst.y = max(config->dst.y - update_config->dst.y, 0);

		if (!is_scale_layer) {
			if (update_config->dst.y > temp_config.dst.y) {
				config->src.y += (update_config->dst.y - temp_config.dst.y);
			}

			if (update_config->dst.x > temp_config.dst.x) {
				config->src.x += (update_config->dst.x - temp_config.dst.x);
			}
			config->src.w = config->dst.w;
			config->src.h = config->dst.h;
		}

		if (regs->need_update == true)
			decon_win_update_dbg("[WIN_UPDATE]win_idx %d: idma_type %d:,"
				"dst[%d %d %d %d] -> [%d %d %d %d], src[%d %d %d %d] -> [%d %d %d %d]\n",
				i, temp_config.idma_type,
				temp_config.dst.x, temp_config.dst.y, temp_config.dst.w, temp_config.dst.h,
				config->dst.x, config->dst.y, config->dst.w, config->dst.h,
				temp_config.src.x, temp_config.src.y, temp_config.src.w, temp_config.src.h,
				config->src.x, config->src.y, config->src.w, config->src.h);
	}

	return;
}
#endif

void decon_reg_chmap_validate(struct decon_device *decon, struct decon_reg_data *regs)
{
	unsigned short i, bitmap = 0;

	for (i = 0; i < decon->pdata->max_win; i++) {
		if ((regs->wincon[i] & WINCON_ENWIN) &&
			!(regs->winmap[i] & WIN_MAP_MAP)) {
			if (bitmap & (1 << regs->vpp_config[i].idma_type)) {
				decon_warn("Channel-%d is mapped to multiple windows\n",
					regs->vpp_config[i].idma_type);
				regs->wincon[i] &= (~WINCON_ENWIN);
			}
			bitmap |= 1 << regs->vpp_config[i].idma_type;
		}
	}
}

#ifdef CONFIG_FB_WINDOW_UPDATE
static int decon_reg_set_win_update_config(struct decon_device *decon, struct decon_reg_data *regs)
{
	int ret = 0;

	if (regs->need_update) {
		decon_reg_ddi_partial_cmd(decon, &regs->update_win);
		ret = decon_win_update_disp_config(decon, &regs->update_win);
	}
	return ret;
}
#endif

static void decon_wait_until_vpp_stop_from_deconx(struct decon_device *decon, int vpp_id)
{
	struct decon_device *decon2;
	int cnt = 1000;

	if (!decon->id)
		decon2 = decon_ext_drvdata;
	else
		decon2 = decon_int_drvdata;

	if (!decon2)
		return;

	while (decon2->vpp_used[vpp_id] && cnt--)
		udelay(50);

	if (cnt <= 0)
		decon_warn("decon-%d: both decons are trying to use same VPP-%d\n",
								decon->id, vpp_id);
}

static void decon_check_vpp_used(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	decon->vpp_usage_bitmask = 0;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];
		if (is_vpp_type(regs->vpp_config[i].idma_type)) {
			win->vpp_id = regs->vpp_config[i].idma_type - 2;
			if ((regs->wincon[i] & WINCON_ENWIN) &&
					!(regs->winmap[i] & WIN_MAP_MAP)) {
				decon_wait_until_vpp_stop_from_deconx(decon,
						win->vpp_id);
				decon_set_sysreg_dsd_cfg(decon,
						regs->vpp_config[i].idma_type);
				decon->vpp_usage_bitmask |= (1 << win->vpp_id);
				decon->vpp_used[win->vpp_id] = true;
			}
		}
	}
}

static void decon_vpp_wait_for_update(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	struct v4l2_subdev *sd = NULL;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];
		if (is_vpp_type(regs->vpp_config[i].idma_type)) {
			if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
				sd = decon->mdev->vpp_sd[win->vpp_id];
				v4l2_subdev_call(sd, core, ioctl,
				VPP_WAIT_FOR_UPDATE, NULL);
			}
		}
	}
}

static void decon_get_vpp_min_lock(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	struct v4l2_subdev *sd = NULL;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];
		if (is_vpp_type(regs->vpp_config[i].idma_type)) {
			if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
				sd = decon->mdev->vpp_sd[win->vpp_id];
				v4l2_subdev_call(sd, core, ioctl,
				VPP_GET_BTS_VAL, &regs->vpp_config[i]);
			}
		}
	}
}

static void decon_set_vpp_min_lock_early(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	struct v4l2_subdev *sd = NULL;

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];
		if (is_vpp_type(regs->vpp_config[i].idma_type)) {
			if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
				struct vpp_dev *vpp;
				sd = decon->mdev->vpp_sd[win->vpp_id];
				vpp = v4l2_get_subdevdata(sd);
				if (vpp->cur_bw > vpp->prev_bw) {
					v4l2_subdev_call(sd, core, ioctl,
						VPP_SET_BW,
						&regs->vpp_config[i]);
				}
				if (vpp->cur_int > vpp->prev_int) {
					v4l2_subdev_call(sd, core, ioctl,
						VPP_SET_MIN_INT,
						&regs->vpp_config[i]);
				}
			}
		}
	}
}

static void decon_set_vpp_min_lock_lately(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	struct v4l2_subdev *sd = NULL;
	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];
		if (is_vpp_type(regs->vpp_config[i].idma_type)) {
			if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
				struct vpp_dev *vpp;
				sd = decon->mdev->vpp_sd[win->vpp_id];
				vpp = v4l2_get_subdevdata(sd);
				if (vpp->cur_bw < vpp->prev_bw) {
					v4l2_subdev_call(sd, core, ioctl,
						VPP_SET_BW,
						&regs->vpp_config[i]);
				}
				if (vpp->cur_int < vpp->prev_int) {
					v4l2_subdev_call(sd, core, ioctl,
						VPP_SET_MIN_INT,
						&regs->vpp_config[i]);
				}

				vpp->prev_bw = vpp->cur_bw;
				vpp->prev_int = vpp->cur_int;
			}
		}
	}
}

#if defined(CONFIG_DECON_DEVFREQ)
static void decon_set_qos(struct decon_device *decon,
			struct decon_reg_data *regs, int is_after)
{
	int mif_do;
	int int_dma_do;
	int disp_do;
	int yuv_do;
	int plane_cnt;
	int i;

	if (decon->id)
		return;

	if (!is_after) {
		for (i = 0; i < decon->pdata->max_win; i++) {
			if (is_vpp_type(regs->vpp_config[i].idma_type)) {
				plane_cnt = decon_get_plane_cnt(regs->vpp_config[i].format);
				if (plane_cnt > 1) {
					decon->cur_frame_has_yuv = 1;
					break;
				}
			}
		}
		mif_do = (decon->prev_bw < regs->bw) ? 1 : 0;
		int_dma_do = (decon->prev_int_bw < regs->int_bw) ? 1 : 0;
		disp_do = (decon->prev_disp_bw < regs->disp_bw) ? 1 : 0;
		yuv_do = (decon->prev_frame_has_yuv < decon->cur_frame_has_yuv) ? 1 : 0;
	} else {
		mif_do = (decon->prev_bw > regs->bw) ? 1 : 0;
		int_dma_do = (decon->prev_int_bw > regs->int_bw) ? 1 : 0;
		disp_do = (decon->prev_disp_bw > regs->disp_bw) ? 1 : 0;
		yuv_do = (decon->prev_frame_has_yuv > decon->cur_frame_has_yuv) ? 1 : 0;
	}

	if (mif_do) {
		if (!decon->id)
			exynos7_update_media_scenario(TYPE_DECON_INT,
								regs->bw, 0);
		else
			exynos7_update_media_scenario(TYPE_DECON_EXT,
								regs->bw, 0);
	}

	if (!decon->id) {
		if (disp_do)
			pm_qos_update_request(&decon->disp_qos, regs->disp_bw);

		if (int_dma_do)
			pm_qos_update_request(&decon->int_qos, regs->int_bw);
	}

	if (yuv_do)
		exynos7_update_media_scenario(TYPE_YUV, decon->cur_frame_has_yuv, 0);

	if (is_after) {
		decon->prev_bw = regs->bw;
		decon->prev_int_bw = regs->int_bw;
		decon->prev_disp_bw = regs->disp_bw;
		decon->prev_frame_has_yuv = decon->cur_frame_has_yuv;
	}

	if (mif_do || int_dma_do || disp_do)
		decon_dbg("[is_after %d] tot_bw %d, disp_bw %d, int_bw %d\n",
			is_after, regs->bw, regs->disp_bw, regs->int_bw);
	return;
}
#else
static void decon_set_qos(struct decon_device *decon,
			struct decon_reg_data *regs, int is_after)
{
	return;
}
#endif

static void __decon_update_regs(struct decon_device *decon, struct decon_reg_data *regs)
{
	unsigned short i, j;
	struct decon_regs_data win_regs;
	struct decon_psr_info psr;
	struct v4l2_subdev *sd = NULL;
	int plane_cnt;
#ifdef CONFIG_FB_WINDOW_UPDATE
	int ret = 0;
#endif
	int vpp_ret = 0;

	memset(&win_regs, 0, sizeof(struct decon_regs_data));

	if (!decon->id && decon->pdata->trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
				decon->pdata->trig_mode, DECON_TRIG_DISABLE);

	for (i = 0; i < decon->pdata->max_win; i++)
		decon_reg_shadow_protect_win(decon->id, decon->windows[i]->index, 1);

	decon_reg_chmap_validate(decon, regs);

#ifdef CONFIG_FB_WINDOW_UPDATE
	/* if any error in config, skip the trigger UNAMSK. keep others as it is*/
	if (decon->out_type == DECON_OUT_DSI)
		ret = decon_reg_set_win_update_config(decon, regs);
#endif

	for (i = 0; i < decon->pdata->max_win; i++) {
		struct decon_win *win = decon->windows[i];

		/* set decon registers for each window */
		decon_to_regs_param(&win_regs, regs, i);
		decon_reg_set_regs_data(decon->id, i, &win_regs);

		/* set plane data */
		plane_cnt = decon_get_plane_cnt(regs->vpp_config[i].format);
		for (j = 0; j < plane_cnt; ++j)
			decon->windows[i]->dma_buf_data[j] = regs->dma_buf_data[i][j];

		decon->windows[i]->plane_cnt = plane_cnt;
		if (is_vpp_type(regs->vpp_config[i].idma_type)) {
			if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
				sd = decon->mdev->vpp_sd[win->vpp_id];
				vpp_ret = v4l2_subdev_call(sd, core, ioctl,
					VPP_WIN_CONFIG, &regs->vpp_config[i]);
				if (vpp_ret) {
					decon_err("Failed to config VPP-%d\n", win->vpp_id);
					win_regs.wincon &= (~WINCON_ENWIN);
					decon_write(decon->id, WINCON(i), win_regs.wincon);
					decon->vpp_usage_bitmask &= ~(1 << win->vpp_id);
					decon->vpp_err_stat[win->vpp_id] = true;
				}
			}
		}
		/* video mode: 0, dp: 1 mipi command mode: 2 */
		if(decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE){
			if (!is_vpp_type(regs->vpp_config[i].idma_type)) {
				if (IS_ENABLED(CONFIG_DECON_BLOCKING_MODE))
					decon_enable_blocking_mode(decon, regs, i);
			}
		}
	}

	for (i = 0; i < decon->pdata->max_win; i++)
		decon_reg_shadow_protect_win(decon->id, decon->windows[i]->index, 0);

	/* DMA protection(SFW) enable must be happen on vpp domain is alive */
	decon_set_protected_content(decon, regs);

	if (decon->out_type == DECON_OUT_WB)
		decon_reg_set_wb_frame(decon->id, regs->wb_whole_w, regs->wb_whole_h,
				regs->wb_dma_buf_data.dma_addr);

	decon_to_psr_info(decon, &psr);
	decon_reg_start(decon->id, decon->pdata->dsi_mode, &psr);
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	if (!decon->id) {
		ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL);
		if (ret)
			decon_err("Failed to call DSIM packet go enable in %s!\n", __func__);
	}
#endif
}

static void decon_fence_wait(struct sync_fence *fence)
{
	int err = sync_fence_wait(fence, 900);
	if (err >= 0)
		return;

	if (err < 0)
		decon_warn("error waiting on acquire fence: %d\n", err);
}

static int decon_prevent_size_mismatch
	(struct decon_device *decon, int dsi_idx, unsigned long timeout)
{
	unsigned long delay_time = 100;
	unsigned long cnt = timeout / delay_time;
	u32 decon_line, dsim_line;
	u32 decon_hoz, dsim_hoz;
	u32 need_save = true;
	struct disp_ss_size_info info;

	if (decon->pdata->psr_mode == DECON_VIDEO_MODE)
		return 0;

	while (decon_reg_get_vstatus(decon->id, dsi_idx) == VIDCON1_VSTATUS_IDLE && --cnt)
	{
		/* Check a DECON and DSIM size mismatch */
		decon_line = decon_reg_get_lineval(decon->id, dsi_idx, decon->lcd_info);
		dsim_line = dsim_reg_get_lineval(dsi_idx);

		decon_hoz = decon_reg_get_hozval(decon->id, dsi_idx, decon->lcd_info);
		dsim_hoz = dsim_reg_get_hozval(dsi_idx);

		if (decon_line == dsim_line && decon_hoz == dsim_hoz)
			goto wait_done;

		if (need_save) {
			/* TODO: Save a err data */
			info.w_in = decon_hoz;
			info.h_in = decon_line;
			info.w_out = dsim_hoz;
			info.h_out = dsim_line;
			DISP_SS_EVENT_SIZE_ERR_LOG(&decon->sd, &info);
			need_save = false;
		}

		udelay(delay_time);
	}

	if (!cnt) {
		decon_err("size mis-match, TRIGCON:0x%x decon_line:%d,	\
				dsim_line:%d, decon_hoz:%d, dsim_hoz:%d\n",
				decon_read(decon->id, TRIGCON),
				decon_line, dsim_line, decon_hoz, dsim_hoz);
	}
wait_done:
	return 0;
}

#ifdef PRINTFPS_DISPLAY
ktime_t basicstamp;
int update_fps;
#endif

#if defined(CONFIG_EXYNOS_DECON_DPU)
int ad_set_state = 0;
static struct delayed_work update_work;

static void decon_update_frames(struct work_struct *work)
{
        struct decon_device *decon = decon_int_drvdata;
//	printk("lpd_cnt:%d\n", atomic_read(&decon->lpd_block_cnt));
        decon_lpd_unblock(decon);
//	printk("lpd_cnt:%d\n", atomic_read(&decon->lpd_block_cnt));
	ad_set_state = 0;
}

void decon_start_update_frames_state(void)
{
        struct decon_device *decon = decon_int_drvdata;
        decon_lpd_block(decon);
	if (ad_set_state == 1)
		return;
        if (decon->pdata->trig_mode == DECON_HW_TRIG)
                decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
                                decon->pdata->trig_mode, DECON_TRIG_ENABLE);
	ad_set_state = 1;
        schedule_delayed_work(&update_work, msecs_to_jiffies(100));
}


void decon_init_update_timer(void)
{
	INIT_DELAYED_WORK(&update_work, decon_update_frames);
}

void decon_exit_lpd_state(void)
{
	struct decon_device *decon = decon_int_drvdata;
	decon_lpd_block_exit(decon);
}

u32 decon_ad_onoff_state(void)
{
	return decon_int_drvdata->dpu_save.ad_onoff;
}

void decon_lpd_unblock_state(void)
{
	struct decon_device *decon = decon_int_drvdata;
	decon_lpd_unblock(decon);
}

#endif
static void decon_restore_work_handler(struct work_struct *work)
{
	struct delayed_work *decon_restart_delay_work = (struct delayed_work *)work;
	struct decon_device *decon = container_of(decon_restart_delay_work, struct decon_device, restore_delay_work);

	decon_disable(decon);
	decon_enable(decon);
	printk("@@@@@@ [WARN] Succeed to restore decon [%s]!\r\n",
		list_empty(&decon->update_regs_list) ? "empty" : "not empty");

	decon_lpd_unblock(decon);
}

static ssize_t decon_restart_cnt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", decon->restore_cnt);
}

static DEVICE_ATTR(restore_cnt, S_IRUGO, decon_restart_cnt_show, NULL);

static void decon_update_regs(struct decon_device *decon, struct decon_reg_data *regs)
{
	struct decon_dma_buf_data old_dma_bufs[decon->pdata->max_win][MAX_BUF_PLANE_CNT];
	int old_plane_cnt[MAX_DECON_WIN];
	int i, j;
#ifdef CONFIG_USE_VSYNC_SKIP
	int vsync_wait_cnt = 0;
#endif /* CONFIG_USE_VSYNC_SKIP */
#ifdef PRINTFPS_DISPLAY
	ktime_t timestamp;
#endif
#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
	struct timespec ts;
#endif

#ifdef CONFIG_DECON_SIMULATE_DISPLAY
	if (decon->isConnectLcd != 1) {
		if (decon->state == DECON_STATE_LPD)
			decon_exit_lpd(decon);

		sw_sync_timeline_inc(decon->timeline, 1);
		return ;
	}
#endif

	if (decon->state == DECON_STATE_RESTORE) {
		sw_sync_timeline_inc(decon->timeline, 1);
		return ;
	}

	decon->cur_frame_has_yuv = 0;

	if (decon->state == DECON_STATE_LPD)
		decon_exit_lpd(decon);

	for (i = 0; i < decon->pdata->max_win; i++) {
		for (j = 0; j < MAX_BUF_PLANE_CNT; ++j)
			memset(&old_dma_bufs[i][j], 0, sizeof(struct decon_dma_buf_data));
		old_plane_cnt[i] = 0;
	}

	for (i = 0; i < decon->pdata->max_win; i++) {
		old_plane_cnt[i] = decon->windows[i]->plane_cnt;
		for (j = 0; j < old_plane_cnt[i]; ++j) {
			old_dma_bufs[i][j] = decon->windows[i]->dma_buf_data[j];
		}
		if (regs->dma_buf_data[i][0].fence) {
			decon_fence_wait(regs->dma_buf_data[i][0].fence);
		}
	}

	decon_set_qos(decon, regs, 0);
	decon_check_vpp_used(decon, regs);
	decon_get_vpp_min_lock(decon, regs);
	decon_set_vpp_min_lock_early(decon, regs);

	DISP_SS_EVENT_LOG_WINCON(&decon->sd, regs);

#ifdef CONFIG_USE_VSYNC_SKIP
	if (decon->out_type == DECON_OUT_DSI) {
		vsync_wait_cnt = decon_extra_vsync_wait_get();
		decon_extra_vsync_wait_set(0);

		if (vsync_wait_cnt < ERANGE && regs->num_of_window <= 2) {
			while ((vsync_wait_cnt--) > 0) {
				if((decon_extra_vsync_wait_get() >= ERANGE)) {
					decon_extra_vsync_wait_set(0);
					break;
				}

				decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
			}
		}
	}
#endif /* CONFIG_USE_VSYNC_SKIP */

	__decon_update_regs(decon, regs);
	if (decon->out_type == DECON_OUT_WB) {
		decon_reg_wb_swtrigger(decon->id);
		DISP_SS_EVENT_LOG(DISP_EVT_WB_SW_TRIGGER, &decon->sd, ktime_set(0, 0));
		if (wait_event_interruptible_timeout(decon->wait_frmdone,
					atomic_read(&decon->wb_done) == STATE_DONE,
					msecs_to_jiffies(VSYNC_TIMEOUT_MSEC)) == 0) {
			decon_err("write-back timeout, but continue progress\n");
		} else {
			decon->frame_done_cnt_cur++;
			atomic_set(&decon->wb_done, STATE_IDLE);
			decon->vpp_usage_bitmask = 0;
			decon_set_protected_content(decon, NULL);
		}
	} else {
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);

		if (decon_reg_wait_for_update_timeout(decon->id, 300 * 1000) < 0) {
			decon->state = DECON_STATE_RESTORE;
			decon_lpd_block(decon);
			decon->restore_cnt++;
			sw_sync_timeline_inc(decon->timeline, 1);
			queue_delayed_work(decon->restore_wq, &decon->restore_delay_work, msecs_to_jiffies(2000));
			return ;
		}

		/* prevent size mis-matching after decon update clear */
		decon_prevent_size_mismatch(decon, 0, 50 * 1000); /* 50ms */

		if (!decon->id) {
			/* clear I80 Framedone pending interrupt */
			decon_write_mask(decon->id, VIDINTCON1, ~0, VIDINTCON1_INT_I80);
			decon->frame_done_cnt_target = decon->frame_done_cnt_cur + 1;
		}

		decon_vpp_wait_for_update(decon, regs);
	}

	if (!decon->id && decon->pdata->trig_mode == DECON_HW_TRIG)
		decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
				decon->pdata->trig_mode, DECON_TRIG_DISABLE);

	DISP_SS_EVENT_LOG(DISP_EVT_TRIG_MASK, &decon->sd, ktime_set(0, 0));
	decon->trig_mask_timestamp =  ktime_get();
	for (i = 0; i < decon->pdata->max_win; i++) {
		for (j = 0; j < old_plane_cnt[i]; ++j)
			decon_free_dma_buf(decon, &old_dma_bufs[i][j]);
	}
	if (decon->out_type == DECON_OUT_WB) {
		decon_free_dma_buf(decon, &regs->wb_dma_buf_data);
		sw_sync_timeline_inc(decon->wb_timeline, 1);
		DISP_SS_EVENT_LOG(DISP_EVT_WB_TIMELINE_INC, &decon->sd, ktime_set(0, 0));
	} else {
		sw_sync_timeline_inc(decon->timeline, 1);
	}

#ifdef PRINTFPS_DISPLAY
	if(printfps == 1) {
		update_fps++;
		timestamp = ktime_get();

		if(1000000<(unsigned int)ktime_to_us(ktime_sub(timestamp, basicstamp))) {
			basicstamp = timestamp;
			printk("------fps : %d------\n", update_fps);
			update_fps = 0;
		}
	}
#endif
#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
	if(exynos_touch_daemon_data.start) {
		if(exynos_touch_daemon_data.response_time == 0) {
			ktime_get_real_ts(&ts);
			exynos_touch_daemon_data.response_time = (unsigned int)(timespec_to_ns(&ts) - exynos_touch_daemon_data.start_time)/1000;
		}
		exynos_touch_daemon_data.frame++;
	}
#endif
	decon_set_vpp_min_lock_lately(decon, regs);
	decon_vpp_stop(decon, false);

	decon_set_qos(decon, regs, 1);
}

static void decon_update_regs_handler(struct kthread_work *work)
{
	struct decon_device *decon =
			container_of(work, struct decon_device, update_regs_work);
	struct decon_reg_data *data, *next;
	struct list_head saved_list;

	if (decon->state == DECON_STATE_LPD)
		decon_warn("%s: LPD state: %d\n", __func__, decon_get_lpd_block_cnt(decon));

	mutex_lock(&decon->update_regs_list_lock);
	saved_list = decon->update_regs_list;
	list_replace_init(&decon->update_regs_list, &saved_list);
	mutex_unlock(&decon->update_regs_list_lock);

	list_for_each_entry_safe(data, next, &saved_list, list) {
		decon_update_regs(decon, data);
		decon_lpd_unblock(decon);
		list_del(&data->list);
		kfree(data);
	}
}

static int decon_set_win_config(struct decon_device *decon,
		struct decon_win_config_data *win_data)
{
	struct decon_win_config *win_config = win_data->config;
	int ret = 0;
	unsigned short i, j;
	struct decon_reg_data *regs;
	struct sync_fence *fence;
	struct sync_pt *pt;
	int fd;
	unsigned int bw = 0;
	int plane_cnt = 0;
	int isData = 0;

	fd = get_unused_fd();
	if (fd < 0)
		return fd;

	mutex_lock(&decon->output_lock);

	if ((decon->state == DECON_STATE_OFF) ||
		(decon->state == DECON_STATE_RESTORE) ||
		(decon->out_type == DECON_OUT_TUI)) {
		decon->timeline_max++;
		pt = sw_sync_pt_create(decon->timeline, decon->timeline_max);
		fence = sync_fence_create("display", pt);
		sync_fence_install(fence, fd);
		win_data->fence = fd;

		sw_sync_timeline_inc(decon->timeline, 1);
		goto err;
	}

	for (i = 0; i < decon->pdata->max_win; i++) {
		if (win_config[i].state != DECON_WIN_STATE_DISABLED) {
			isData = 1;
			break;
		}
	}
	if (isData != 1) {
		decon->timeline_max++;
		pt = sw_sync_pt_create(decon->timeline, decon->timeline_max);
		fence = sync_fence_create("display", pt);
		sync_fence_install(fence, fd);
		win_data->fence = fd;

		sw_sync_timeline_inc(decon->timeline, 1);
		goto err;
	}

	regs = kzalloc(sizeof(struct decon_reg_data), GFP_KERNEL);
	if (!regs) {
		decon_err("could not allocate decon_reg_data\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < decon->pdata->max_win; i++) {
		decon->windows[i]->prev_fix =
			decon->windows[i]->fbinfo->fix;
		decon->windows[i]->prev_var =
			decon->windows[i]->fbinfo->var;

	}

#ifdef CONFIG_FB_WINDOW_UPDATE
	if (decon->out_type == DECON_OUT_DSI)
		decon_set_win_update_config(decon, win_config, regs);
#endif

	for (i = 0; i < decon->pdata->max_win && !ret; i++) {
		struct decon_win_config *config = &win_config[i];
		struct decon_win *win = decon->windows[i];

		bool enabled = 0;
		u32 color_map = WIN_MAP_MAP | WIN_MAP_MAP_COLOUR(0);

		switch (config->state) {
		case DECON_WIN_STATE_DISABLED:
			break;
		case DECON_WIN_STATE_COLOR:
			enabled = 1;
			color_map |= WIN_MAP_MAP_COLOUR(config->color);
			regs->vidosd_a[win->index] = vidosd_a(config->dst.x, config->dst.y);
			regs->vidosd_b[win->index] = vidosd_b(config->dst.x, config->dst.y,
					config->dst.w, config->dst.h);
			decon_set_alpha_blending(config, regs, win->index, 0);
			break;
		case DECON_WIN_STATE_BUFFER:
			if (decon->id && i == DECON_BACKGROUND) {
				decon_warn("decon-ext win[0]: background\n");
				break;
			}

		/* video mode: 0, dp: 1 mipi command mode: 2 */
		if(decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE){
			if (IS_ENABLED(CONFIG_DECON_BLOCKING_MODE))
				if (decon_set_win_blocking_mode(decon, win, win_config, regs))
					break;
		}

			ret = decon_set_win_buffer(decon, win, config, regs);
			if (!ret) {
				enabled = 1;
				color_map = 0;
			}

#ifdef CONFIG_DECON_LPD_DISPLAY
			decon->b_enable = 1;
#endif
			break;
		default:
			decon_warn("unrecognized window state %u",
					config->state);
			ret = -EINVAL;
			break;
		}
		if (enabled)
			regs->wincon[i] |= WINCON_ENWIN;
		else
			regs->wincon[i] &= ~WINCON_ENWIN;

		/*
		 * Because BURSTLEN field does not have shadow register,
		 * this bit field should be retain always.
		 * exynos7420 must be set 16 burst
		 */
		regs->wincon[i] |= WINCON_BURSTLEN_16WORD;

		regs->winmap[i] = color_map;

		if (enabled && config->state == DECON_WIN_STATE_BUFFER) {
			bw += decon_calc_bandwidth(config->dst.w, config->dst.h,
					win->fbinfo->var.bits_per_pixel,
					win->fps);
			regs->num_of_window++;
		}
	}

	for (i = 0; i < decon->pdata->max_win; i++) {
		memcpy(&regs->vpp_config[i], &win_config[i],
				sizeof(struct decon_win_config));
		regs->vpp_config[i].format =
			get_vpp_src_format(regs->vpp_config[i].format, win_config[i].idma_type);
	}
	regs->bandwidth = bw;

	decon_dbg("Total BW = %d Mbits, Max BW per window = %d Mbits\n",
			bw / (1024 * 1024), MAX_BW_PER_WINDOW / (1024 * 1024));

#ifdef CONFIG_DECON_DEVFREQ
	regs->win_overlap_cnt = decon_get_overlap_cnt(decon, win_config, regs);
#ifdef CONFIG_FB_WINDOW_UPDATE
	/* video mode: 0, dp: 1 mipi command mode: 2 */
	if(decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE){
		decon_modulate_overlap_cnt(decon, win_config, regs);
	}
#endif
	regs->bw = decon_get_bw(decon, win_config, regs);
#endif
	if (decon->out_type == DECON_OUT_WB)
		ret = decon_set_wb_buffer(decon, fd, regs);
	if (ret) {
		for (i = 0; i < decon->pdata->max_win; i++) {
			decon->windows[i]->fbinfo->fix = decon->windows[i]->prev_fix;
			decon->windows[i]->fbinfo->var = decon->windows[i]->prev_var;

			plane_cnt = decon_get_plane_cnt(regs->vpp_config[i].format);
			for (j = 0; j < plane_cnt; ++j)
				decon_free_dma_buf(decon, &regs->dma_buf_data[i][j]);
		}
		if (decon->out_type == DECON_OUT_WB) {
			decon_free_dma_buf(decon, &regs->wb_dma_buf_data);
			decon_err("failed to set buffer for wb\n");
		}
		put_unused_fd(fd);
		kfree(regs);
	} else {
		if (decon->out_type != DECON_OUT_WB) {
			decon_lpd_block(decon);
			decon->timeline_max++;
			pt = sw_sync_pt_create(decon->timeline, decon->timeline_max);
			fence = sync_fence_create("display", pt);
			sync_fence_install(fence, fd);
			win_data->fence = fd;
		}
		mutex_lock(&decon->update_regs_list_lock);
		list_add_tail(&regs->list, &decon->update_regs_list);
		mutex_unlock(&decon->update_regs_list_lock);
		queue_kthread_work(&decon->update_regs_worker,
				&decon->update_regs_work);
	}
err:
	mutex_unlock(&decon->output_lock);
	return ret;
}

static int decon_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	int ret;
	u32 crtc;

	decon_lpd_block_exit(decon);

	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		if (get_user(crtc, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		if (crtc == 0)
			ret = decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		else
			ret = -ENODEV;

		break;

	case S3CFB_WIN_POSITION:
		if (copy_from_user(&decon->ioctl_data.user_window,
				(struct decon_user_window __user *)arg,
				sizeof(decon->ioctl_data.user_window))) {
			ret = -EFAULT;
			break;
		}

		if (decon->ioctl_data.user_window.x < 0)
			decon->ioctl_data.user_window.x = 0;
		if (decon->ioctl_data.user_window.y < 0)
			decon->ioctl_data.user_window.y = 0;

		ret = decon_set_window_position(info, decon->ioctl_data.user_window);
		break;

	case S3CFB_WIN_SET_PLANE_ALPHA:
		if (copy_from_user(&decon->ioctl_data.user_alpha,
				(struct s3c_fb_user_plane_alpha __user *)arg,
				sizeof(decon->ioctl_data.user_alpha))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_plane_alpha_blending(info, decon->ioctl_data.user_alpha);
		break;

	case S3CFB_WIN_SET_CHROMA:
		if (copy_from_user(&decon->ioctl_data.user_chroma,
				   (struct s3c_fb_user_chroma __user *)arg,
				   sizeof(decon->ioctl_data.user_chroma))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_chroma_key(info, decon->ioctl_data.user_chroma);
		break;

	case S3CFB_SET_VSYNC_INT:
		if (get_user(decon->ioctl_data.vsync, (int __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_vsync_int(info, decon->ioctl_data.vsync);
		break;

	case S3CFB_WIN_CONFIG:
		DISP_SS_EVENT_LOG(DISP_EVT_WIN_CONFIG, &decon->sd, ktime_set(0, 0));
		if (copy_from_user(&decon->ioctl_data.win_data,
				   (struct decon_win_config_data __user *)arg,
				   sizeof(decon->ioctl_data.win_data))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_win_config(decon, &decon->ioctl_data.win_data);
		if (ret)
			break;

		if (copy_to_user(&((struct decon_win_config_data __user *)arg)->fence,
				 &decon->ioctl_data.win_data.fence,
				 sizeof(decon->ioctl_data.win_data.fence))) {
			ret = -EFAULT;
			break;
		}
		break;

	case EXYNOS_GET_HDMI_CONFIG:
		if (copy_from_user(&decon->ioctl_data.hdmi_data,
				   (struct exynos_hdmi_data __user *)arg,
				   sizeof(decon->ioctl_data.hdmi_data))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_get_hdmi_config(decon, &decon->ioctl_data.hdmi_data);
		if (ret)
			break;

		if (copy_to_user((struct exynos_hdmi_data __user *)arg,
				&decon->ioctl_data.hdmi_data, sizeof(decon->ioctl_data.hdmi_data))) {
			ret = -EFAULT;
			break;
		}
		break;

	case EXYNOS_SET_HDMI_CONFIG:
		if (copy_from_user(&decon->ioctl_data.hdmi_data,
				   (struct exynos_hdmi_data __user *)arg,
				   sizeof(decon->ioctl_data.hdmi_data))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_hdmi_config(decon, &decon->ioctl_data.hdmi_data);
		break;

	default:
		ret = -ENOTTY;
	}

	decon_lpd_unblock(decon);
	return ret;
}

int decon_release(struct fb_info *info, int user)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;

	decon_info("%s +\n", __func__);

	if (decon->id && decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY) {
		find_subdev_mipi(decon);
		decon_info("output device of decon%d is changed to %s\n",
				decon->id, decon->output_sd->name);
	}

	decon_info("%s -\n", __func__);

	return 0;
}

extern int decon_set_par(struct fb_info *info);
extern int decon_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);
extern int decon_setcolreg(unsigned regno,
			    unsigned red, unsigned green, unsigned blue,
			    unsigned transp, struct fb_info *info);
extern int decon_mmap(struct fb_info *info, struct vm_area_struct *vma);

/* ---------- FREAMBUFFER INTERFACE ----------- */
static struct fb_ops decon_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= decon_check_var,
	.fb_set_par	= decon_set_par,
	.fb_blank	= decon_blank,
	.fb_setcolreg	= decon_setcolreg,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
	.fb_ioctl	= decon_ioctl,
	.fb_pan_display	= decon_pan_display,
	.fb_mmap	= decon_mmap,
	.fb_release	= decon_release,
};

/* ---------- POWER MANAGEMENT ----------- */
void decon_clocks_info(struct decon_device *decon)
{
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.pclk),
				clk_get_rate(decon->res.pclk) / MHZ);
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.aclk),
				clk_get_rate(decon->res.aclk) / MHZ);
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.eclk),
				clk_get_rate(decon->res.eclk) / MHZ);
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.vclk),
				clk_get_rate(decon->res.vclk) / MHZ);
	decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.aclk_disp),
				clk_get_rate(decon->res.aclk_disp) / MHZ);
	if (decon->pdata->ip_ver & IP_VER_DECON_7I) {
		decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.dsd),
				clk_get_rate(decon->res.dsd) / MHZ);
		decon_info("%s: %ld Mhz\n", __clk_get_name(decon->res.lh_disp1),
				clk_get_rate(decon->res.lh_disp1) / MHZ);
		if (!decon->id) {
			decon_info("%s: %ld Mhz\n",
				__clk_get_name(decon->res.lh_disp0),
				clk_get_rate(decon->res.lh_disp0) / MHZ);
		} else {
			decon_info("%s: %ld Mhz\n",
				__clk_get_name(decon->res.pclk_disp),
				clk_get_rate(decon->res.pclk_disp) / MHZ);
		}
	}
}

void decon_put_clocks(struct decon_device *decon)
{
	clk_put(decon->res.pclk);
	clk_put(decon->res.aclk);
	clk_put(decon->res.eclk);
	clk_put(decon->res.vclk);
	clk_put(decon->res.aclk_disp);

	if (decon->pdata->ip_ver & IP_VER_DECON_7I) {
		clk_put(decon->res.dsd);
		clk_put(decon->res.lh_disp1);

		if (!decon->id)
			clk_put(decon->res.lh_disp0);
		else
			clk_put(decon->res.pclk_disp);
	}

	if (!decon->id) {
		clk_put(decon->res.mif_pll);
	}
}

static int decon_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct decon_device *decon = platform_get_drvdata(pdev);

	DISP_SS_EVENT_LOG(DISP_EVT_DECON_RESUME, &decon->sd, ktime_set(0, 0));
	decon_dbg("decon%d %s +\n", decon->id, __func__);
	mutex_lock(&decon->mutex);

	if (!decon->id)
		decon_int_set_clocks(decon);
	else
		decon_ext_set_clocks(decon);

	clk_prepare_enable(decon->res.pclk);
	clk_prepare_enable(decon->res.aclk);
	clk_prepare_enable(decon->res.eclk);
	clk_prepare_enable(decon->res.vclk);
	clk_prepare_enable(decon->res.aclk_disp);

	if (decon->pdata->ip_ver & IP_VER_DECON_7I) {
		clk_prepare_enable(decon->res.dsd);
		clk_prepare_enable(decon->res.lh_disp1);

		if (!decon->id)
			clk_prepare_enable(decon->res.lh_disp0);
		else
			clk_prepare_enable(decon->res.pclk_disp);
	}

	if (decon->state == DECON_STATE_INIT)
		decon_clocks_info(decon);

	mutex_unlock(&decon->mutex);
	decon_dbg("decon%d %s -\n", decon->id, __func__);

	return 0;
}

static int decon_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct decon_device *decon = platform_get_drvdata(pdev);

	DISP_SS_EVENT_LOG(DISP_EVT_DECON_SUSPEND, &decon->sd, ktime_set(0, 0));
	decon_dbg("decon%d %s +\n", decon->id, __func__);
	mutex_lock(&decon->mutex);

	clk_disable_unprepare(decon->res.pclk);
	clk_disable_unprepare(decon->res.aclk);
	clk_disable_unprepare(decon->res.eclk);
	clk_disable_unprepare(decon->res.vclk);
	clk_disable_unprepare(decon->res.aclk_disp);

	if (decon->pdata->ip_ver & IP_VER_DECON_7I) {
		clk_disable_unprepare(decon->res.dsd);
		clk_disable_unprepare(decon->res.lh_disp1);

		if (!decon->id)
			clk_disable_unprepare(decon->res.lh_disp0);
		else
			clk_disable_unprepare(decon->res.pclk_disp);
	}

	mutex_unlock(&decon->mutex);
	decon_dbg("decon%d %s -\n", decon->id, __func__);

	return 0;
}

static const struct dev_pm_ops decon_pm_ops = {
	.runtime_suspend = decon_runtime_suspend,
	.runtime_resume	 = decon_runtime_resume,
};

/* ---------- MEDIA CONTROLLER MANAGEMENT ----------- */
static long decon_sd_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	long ret = 0;

	switch (cmd) {
	case DECON_IOC_LPD_EXIT_LOCK:
		decon_lpd_block_exit(decon);
		break;
	case DECON_IOC_LPD_UNLOCK:
		decon_lpd_unblock(decon);
		break;
	default:
		dev_err(decon->dev, "unsupported ioctl");
		ret = -EINVAL;
	}
	return ret;
}

static int decon_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int decon_s_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *format)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	struct decon_lcd *porch;

	DISP_SS_EVENT_LOG_S_FMT(&decon->sd, format);

	if (format->pad != DECON_PAD_WB) {
		decon_err("it is possible only output format setting\n");
		return -EINVAL;
	}

	if (format->format.width * format->format.height > 3840 * 2160) {
		decon_err("size is bigger than 3840x2160\n");
		return -EINVAL;
	}

	porch = kzalloc(sizeof(struct decon_lcd), GFP_KERNEL);
	if (!porch) {
		decon_err("could not allocate decon_lcd for wb\n");
		return -ENOMEM;
	}

	decon->lcd_info = porch;
	decon->lcd_info->width = format->format.width;
	decon->lcd_info->height = format->format.height;
	decon->lcd_info->xres = format->format.width;
	decon->lcd_info->yres = format->format.height;
	decon->lcd_info->vfp = 2;
	decon->lcd_info->vbp = 20;
	decon->lcd_info->hfp = 20;
	decon->lcd_info->hbp = 20;
	decon->lcd_info->vsa = 2;
	decon->lcd_info->hsa = 20;
	decon->lcd_info->fps = 60;
	decon->out_type = DECON_OUT_WB;

	decon_info("decon-ext output size for writeback %dx%d\n",
			decon->lcd_info->width, decon->lcd_info->height);

	return 0;
}

static const struct v4l2_subdev_core_ops decon_sd_core_ops = {
	.ioctl = decon_sd_ioctl,
};

static const struct v4l2_subdev_video_ops decon_sd_video_ops = {
	.s_stream = decon_s_stream,
};

static const struct v4l2_subdev_pad_ops	decon_sd_pad_ops = {
	.set_fmt = decon_s_fmt,
};

static const struct v4l2_subdev_ops decon_sd_ops = {
	.video = &decon_sd_video_ops,
	.core = &decon_sd_core_ops,
	.pad = &decon_sd_pad_ops,
};

static int decon_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations decon_entity_ops = {
	.link_setup = decon_link_setup,
};

static int decon_register_subdev_nodes(struct decon_device *decon,
					struct exynos_md *md)
{
	int ret = v4l2_device_register_subdev_nodes(&md->v4l2_dev);
	if (ret) {
		decon_err("failed to make nodes for subdev\n");
		return ret;
	}

	decon_info("Register V4L2 subdev nodes for DECON\n");

	return 0;

}

static int decon_create_links(struct decon_device *decon,
					struct exynos_md *md)
{
	int ret;
	char err[80];
#ifdef CONFIG_EXYNOS_VPP
	int i,j;
	u32 flags = MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED;
#endif

	decon_info("decon%d create links\n", decon->id);
	memset(err, 0, sizeof(err));

	/*
	 * Link creation : vpp -> decon-int and ext.
	 * All windos of decon should be connected to all
	 * VPP each other.
	 */
#ifdef CONFIG_EXYNOS_VPP
	for (i = 0; i < MAX_VPP_SUBDEV; ++i) {
		for (j = 0; j < decon->n_sink_pad; j++) {
			ret = media_entity_create_link(&md->vpp_sd[i]->entity,
					VPP_PAD_SOURCE, &decon->sd.entity,
					j, flags);
			if (ret) {
				snprintf(err, sizeof(err), "%s --> %s",
						md->vpp_sd[i]->entity.name,
						decon->sd.entity.name);
				return ret;
			}
		}
	}
	decon_info("vpp <-> decon links are created successfully\n");
#endif

	/* link creation: decon <-> output */
	if (!decon->id) {
		ret = create_link_mipi(decon);
	} else {
		if (decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY)
			ret = create_link_mipi(decon);
#ifdef CONFIG_VIDEO_EXYNOS_HDMI
		else
			ret = create_link_hdmi(decon);
#endif
	}

	return ret;
}

static void decon_unregister_entity(struct decon_device *decon)
{
	v4l2_device_unregister_subdev(&decon->sd);
}

static int decon_register_entity(struct decon_device *decon)
{
	struct v4l2_subdev *sd = &decon->sd;
	struct media_pad *pads = decon->pads;
	struct media_entity *me = &sd->entity;
	struct exynos_md *md;
	int i, n_pad, ret = 0;

	/* init DECON sub-device */
	v4l2_subdev_init(sd, &decon_sd_ops);
	sd->owner = THIS_MODULE;
	snprintf(sd->name, sizeof(sd->name), "exynos-decon%d", decon->id);

	/* DECON sub-device can be opened in user space */
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	/* init DECON sub-device as entity */
	n_pad = decon->n_sink_pad + decon->n_src_pad;
	for (i = 0; i < decon->n_sink_pad; i++)
		pads[i].flags = MEDIA_PAD_FL_SINK;
	for (i = decon->n_sink_pad; i < n_pad; i++)
		pads[i].flags = MEDIA_PAD_FL_SOURCE;

	me->ops = &decon_entity_ops;
	ret = media_entity_init(me, n_pad, pads, 0);
	if (ret) {
		decon_err("failed to initialize media entity\n");
		return ret;
	}

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		decon_err("failed to get output media device\n");
		return -ENODEV;
	}

	ret = v4l2_device_register_subdev(&md->v4l2_dev, sd);
	if (ret) {
		decon_err("failed to register DECON subdev\n");
		return ret;
	}
	decon_info("%s entity init\n", sd->name);

	if (!decon->id) {
		ret = find_subdev_mipi(decon);
	} else {
		if (decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY)
			ret = find_subdev_mipi(decon);
#ifdef CONFIG_VIDEO_EXYNOS_HDMI
		else
			ret = find_subdev_hdmi(decon);
#endif
	}

	return ret;
}

static void decon_release_windows(struct decon_win *win)
{
	if (win->fbinfo)
		framebuffer_release(win->fbinfo);
}

#ifdef CONFIG_DECON_DISPLAY_LOGO
#if 1
#include "mz_logo.h"
static void s3c_fb_copy_logo(unsigned int xres, unsigned int yres, char *screen_base)
{
        int row = 0;
        unsigned int line_offset = ((xres < PIC_WIDTH) ? 0 : ((xres - PIC_WIDTH) >> 1));
        unsigned int row_offset = ((yres < PIC_HEIGHT) ? 0 : ((yres - PIC_HEIGHT) >> 2));
        char *curBufBase = screen_base;

        for (row = 0; row < PIC_HEIGHT; row++)
        {
                curBufBase = screen_base + ((row_offset + row) * xres + line_offset) * 4;
                memcpy(curBufBase, (mz_logo + row * PIC_WIDTH * 4), PIC_WIDTH * 4);
        }
}
#else
#include "ms_bmp.h"
static int __devinit s3c_fb_copy_bootloader_fb(unsigned int xres, unsigned int yres, char *screen_base)
{
	int ret = 0;
	size_t pos;
	unsigned int i, j, sx, sy;
	unsigned int fbsize = xres*yres*4;

	void *curBufBase = vmalloc(fbsize);
	unsigned char *pboot_logo_data=curBufBase;
	unsigned int LCD_WIDTH_CHG = xres;
	unsigned int LCD_HEIGHT_CHG = yres;
	unsigned int* pBuffer = (unsigned int*)screen_base;
//	unsigned int* pBufferEnd = pBuffer + (LCD_HEIGHT_CHG*LCD_WIDTH_CHG);
	BITMAPFILEHEADER *pBithead = (BITMAPFILEHEADER *)pboot_logo_data;
	BITMAPINFOHEADER* pBitinfo = (BITMAPINFOHEADER *)(pboot_logo_data+(char)sizeof(BITMAPFILEHEADER));
	unsigned char* pBitmap = (unsigned char*)pboot_logo_data+(char)sizeof(BITMAPFILEHEADER)+(char)sizeof(BITMAPINFOHEADER);
	unsigned int iBitmapData;
	unsigned int *pcur_fb;
	int width, height;
	
	if (!curBufBase)
		return -ENOMEM;
	
	if(!decon_bootloader_fb_start){
		ret = -1;
		goto Buf_free;
	}
	
	for (pos = 0; pos < fbsize; pos += PAGE_SIZE) {
		void *page = phys_to_page(decon_bootloader_fb_start + pos);
		void *from_virt = kmap(page);
		void *to_virt = curBufBase+pos;
		memcpy(to_virt, from_virt, PAGE_SIZE);
		kunmap(page);
	}
	
	memset(pBuffer, 0xff, fbsize);

	decon_info("------------------\n");
	decon_info("size FILEHEADER:%d, INFOHEADER:%d\n", (int)sizeof(BITMAPFILEHEADER), (int)sizeof(BITMAPINFOHEADER));
	decon_info("type:0x%x,size:%d:offset:%d\n", pBithead->bfType, pBithead->bfSize, pBithead->bfOffBits);
	decon_info("bitcount:%d\n", pBitinfo->biBitCount);
	decon_info("logo width:%d, height:%d\n", (pBitinfo->biWidth), pBitinfo->biHeight );
	decon_info("LCD_WIDTH:%d, LCD_HEIGHT:%d\n", LCD_WIDTH_CHG, LCD_HEIGHT_CHG);
	if(pBithead->bfType!=0x4d42 || pBitinfo->biBitCount != 24)
	{
		decon_info("bfType:0x%x, biBitCount:%d\n", pBithead->bfType, pBitinfo->biBitCount);
		decon_info("Please run \"fastboot flash charger sd_fuse/battery_low.bmp\"\n");
		ret = -1;
		goto Buf_free;
	}
	width = pBitinfo->biWidth;
	height = pBitinfo->biHeight;
	if(width<0 || width>LCD_WIDTH_CHG){
		ret = -1;
		goto Buf_free;
	}
	if(height<0 || height>LCD_HEIGHT_CHG){
		ret = -1;
		goto Buf_free;
	}
	sx = (LCD_WIDTH_CHG - width)>>1;
	sy = (LCD_HEIGHT_CHG - height)>>1;
	//BMP is flush from bottom to top! 14+50+(width*3+pad_to_4)*height
	for(i=sy+height;i>sy;i--)
	{
		for(j=sx;j<sx+width;j++)
		{
			pcur_fb = pBuffer + (i*LCD_WIDTH_CHG +j);
			iBitmapData = (pBitmap[0]<<0) | (pBitmap[1]<<8) | (pBitmap[2]<<16) | 0xff<<24;
			*pcur_fb = iBitmapData;
			pBitmap += 3;
		}
		j = (((unsigned long)((width*3+3)/4))*4-width*3);
		//printf("padding:%d\n",j);
		pBitmap += j;
	}

Buf_free:
	vfree(curBufBase);
	
	return ret;
}
#endif
#endif

static int decon_fb_alloc_memory(struct decon_device *decon, struct decon_win *win)
{
	struct decon_fb_pd_win *windata = &win->windata;
	unsigned int real_size, virt_size, size;
	struct fb_info *fbi = win->fbinfo;
	struct ion_handle *handle;
	dma_addr_t map_dma;
	struct dma_buf *buf;
	void *vaddr;
	unsigned int ret;

	dev_info(decon->dev, "allocating memory for display\n");

	real_size = windata->win_mode.videomode.xres * windata->win_mode.videomode.yres;
	virt_size = windata->virtual_x * windata->virtual_y;

	dev_info(decon->dev, "real_size=%u (%u.%u), virt_size=%u (%u.%u)\n",
		real_size, windata->win_mode.videomode.xres, windata->win_mode.videomode.yres,
		virt_size, windata->virtual_x, windata->virtual_y);

	size = (real_size > virt_size) ? real_size : virt_size;
	size *= (windata->max_bpp > 16) ? 32 : windata->max_bpp;
	size /= 8;

	fbi->fix.smem_len = size;
	size = PAGE_ALIGN(size);

	dev_info(decon->dev, "want %u bytes for window[%d]\n", size, win->index);

#if defined(CONFIG_ION_EXYNOS)
	handle = ion_alloc(decon->ion_client, (size_t)size, 0,
					EXYNOS_ION_HEAP_SYSTEM_MASK, 0);
	if (IS_ERR(handle)) {
		dev_err(decon->dev, "failed to ion_alloc\n");
		return -ENOMEM;
	}

	buf = ion_share_dma_buf(decon->ion_client, handle);
	if (IS_ERR_OR_NULL(buf)) {
		dev_err(decon->dev, "ion_share_dma_buf() failed\n");
		goto err_share_dma_buf;
	}

	vaddr = ion_map_kernel(decon->ion_client, handle);

	fbi->screen_base = vaddr;

	win->dma_buf_data[1].fence = NULL;
	win->dma_buf_data[2].fence = NULL;
	win->plane_cnt = 1;
	ret = decon_map_ion_handle(decon, decon->dev, &win->dma_buf_data[0],
			handle, buf, win->index);
	if (!ret)
		goto err_map;
	map_dma = win->dma_buf_data[0].dma_addr;

	dev_info(decon->dev, "alloated memory\n");
#else
	fbi->screen_base = dma_alloc_writecombine(decon->dev, size,
						  &map_dma, GFP_KERNEL);
	if (!fbi->screen_base)
		return -ENOMEM;

	dev_dbg(decon->dev, "mapped %x to %p\n",
		(unsigned int)map_dma, fbi->screen_base);

	memset(fbi->screen_base, 0x0, size);
#endif
	fbi->fix.smem_start = map_dma;
#ifdef CONFIG_DECON_DISPLAY_LOGO
	//s3c_fb_copy_bootloader_fb(windata->win_mode.videomode.xres, windata->win_mode.videomode.yres, fbi->screen_base);
	s3c_fb_copy_logo(windata->win_mode.videomode.xres, windata->win_mode.videomode.yres, fbi->screen_base);
#endif
	dev_info(decon->dev, "fb start addr = 0x%x\n", (u32)fbi->fix.smem_start);

	return 0;

#ifdef CONFIG_ION_EXYNOS
err_map:
	dma_buf_put(buf);
err_share_dma_buf:
	ion_free(decon->ion_client, handle);
	return -ENOMEM;
#endif
}

static void decon_missing_pixclock(struct decon_fb_videomode *win_mode)
{
	u64 pixclk = 1000000000000ULL;
	u32 div;
	u32 width, height;

	width = win_mode->videomode.xres;
	height = win_mode->videomode.yres;

	div = width * height * (win_mode->videomode.refresh ? : 60);

	do_div(pixclk, div);
	win_mode->videomode.pixclock = pixclk;
}

static int decon_acquire_windows(struct decon_device *decon, int idx)
{
	struct decon_win *win;
	struct fb_info *fbinfo;
	struct fb_var_screeninfo *var;
	struct decon_lcd *lcd_info = NULL;
	int ret, i;

	decon_dbg("acquire DECON window%d\n", idx);

	fbinfo = framebuffer_alloc(sizeof(struct decon_win), decon->dev);
	if (!fbinfo) {
		decon_err("failed to allocate framebuffer\n");
		return -ENOENT;
	}

	win = fbinfo->par;
	decon->windows[idx] = win;
	var = &fbinfo->var;
	win->fbinfo = fbinfo;
	//fbinfo->fbops = &decon_fb_ops;
	//fbinfo->flags = FBINFO_FLAG_DEFAULT;
	win->decon = decon;
	win->index = idx;

	win->windata.default_bpp = 32;
	win->windata.max_bpp = 32;
	if (!decon->id || decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY) {
		lcd_info = decon->lcd_info;
		win->windata.virtual_x = lcd_info->xres;
		win->windata.virtual_y = lcd_info->yres * 2;
		win->windata.width = lcd_info->xres;
		win->windata.height = lcd_info->yres;
		win->windata.win_mode.videomode.left_margin = lcd_info->hbp;
		win->windata.win_mode.videomode.right_margin = lcd_info->hfp;
		win->windata.win_mode.videomode.upper_margin = lcd_info->vbp;
		win->windata.win_mode.videomode.lower_margin = lcd_info->vfp;
		win->windata.win_mode.videomode.hsync_len = lcd_info->hsa;
		win->windata.win_mode.videomode.vsync_len = lcd_info->vsa;
		win->windata.win_mode.videomode.xres = lcd_info->xres;
		win->windata.win_mode.videomode.yres = lcd_info->yres;
		decon_missing_pixclock(&win->windata.win_mode);
	}

	for (i = 0; i < MAX_BUF_PLANE_CNT; ++i)
		memset(&win->dma_buf_data[i], 0, sizeof(win->dma_buf_data));

	if ((!decon->id || decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY)
			&& win->index == decon->pdata->default_win) {
		ret = decon_fb_alloc_memory(decon, win);
		if (ret) {
			dev_err(decon->dev, "failed to allocate display memory\n");
			return ret;
		}
	}

	fb_videomode_to_var(&fbinfo->var, &win->windata.win_mode.videomode);

	fbinfo->fix.type	= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.accel	= FB_ACCEL_NONE;
	fbinfo->var.activate	= FB_ACTIVATE_NOW;
	fbinfo->var.vmode	= FB_VMODE_NONINTERLACED;
	fbinfo->var.bits_per_pixel = win->windata.default_bpp;
	fbinfo->var.width	= win->windata.width;
	fbinfo->var.height	= win->windata.height;
	fbinfo->fbops		= &decon_fb_ops;
	fbinfo->flags		= FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette  = &win->pseudo_palette;

	ret = decon_check_var(&fbinfo->var, fbinfo);
	if (ret < 0) {
		dev_err(decon->dev, "check_var failed on initial video params\n");
		return ret;
	}

	ret = fb_alloc_cmap(&fbinfo->cmap, 256 /* palette size */, 1);
	if (ret == 0)
		fb_set_cmap(&fbinfo->cmap, fbinfo);
	else
		dev_err(decon->dev, "failed to allocate fb cmap\n");

	decon_info("decon%d window[%d] create\n", decon->id, idx);
	return 0;
}

static int decon_acquire_window(struct decon_device *decon)
{
	int i, ret;

	for (i = 0; i < decon->n_sink_pad; i++) {
		ret = decon_acquire_windows(decon, i);
		if (ret < 0) {
			decon_err("failed to create decon-int window[%d]\n", i);
			for (; i >= 0; i--)
				decon_release_windows(decon->windows[i]);
			return ret;
		}
	}

	return 0;
}

int decon_get_sysreg_addr(struct decon_device *decon)
{
	if (of_have_populated_dt()) {
		struct device_node *nd;

		nd = of_find_compatible_node(NULL, NULL,
				"samsung,exynos5-sysreg_disp");
		if (!nd) {
			decon_err("failed find compatible node(sysreg-disp)");
			return -ENODEV;
		}

		decon->sys_regs = of_iomap(nd, 0);
		if (!decon->sys_regs) {
			decon_err("Failed to get sysreg-disp address.");
			return -ENOMEM;
		}
	} else {
		decon_err("failed have populated device tree");
		return -EIO;
	}

	return 0;
}

static void decon_parse_pdata(struct decon_device *decon, struct device *dev)
{
	struct device_node *te_eint;

	if (dev->of_node) {
		decon->id = of_alias_get_id(dev->of_node, "decon");
		of_property_read_u32(dev->of_node, "ip_ver",
					&decon->pdata->ip_ver);
		of_property_read_u32(dev->of_node, "n_sink_pad",
					&decon->n_sink_pad);
		of_property_read_u32(dev->of_node, "n_src_pad",
					&decon->n_src_pad);
		of_property_read_u32(dev->of_node, "max_win",
					&decon->pdata->max_win);
		of_property_read_u32(dev->of_node, "default_win",
					&decon->pdata->default_win);
		/* video mode: 0, dp: 1 mipi command mode: 2 */
		of_property_read_u32(dev->of_node, "psr_mode",
					&decon->pdata->psr_mode);
		/* H/W trigger: 0, S/W trigger: 1 */
		of_property_read_u32(dev->of_node, "trig_mode",
					&decon->pdata->trig_mode);
		decon_info("decon-%s: ver%d, max win%d, %s mode, %s trigger\n",
			decon->id ? "ext" : "int", decon->pdata->ip_ver,
			decon->pdata->max_win,
			decon->pdata->psr_mode ? "command" : "video",
			decon->pdata->trig_mode ? "sw" : "hw");

		/* single DSI: 0, dual DSI: 1 */
		of_property_read_u32(dev->of_node, "dsi_mode",
				&decon->pdata->dsi_mode);
		decon_info("dsi mode(%d). 0: single 1: dual dsi 2: dual display\n",
				decon->pdata->dsi_mode);
		te_eint = of_get_child_by_name(decon->dev->of_node, "te_eint");
		if (!te_eint) {
			decon_info("No DT node for te_eint\n");
		} else {
			decon->eint_pend = of_iomap(te_eint, 0);
			if (!decon->eint_pend)
				decon_info("Failed to get te eint pend\n");

			decon->eint_mask = of_iomap(te_eint, 1);
			if (!decon->eint_mask)
				decon_info("Failed to get te eint mask\n");
		}
	} else {
		decon_warn("no device tree information\n");
	}
}

#ifdef CONFIG_DECON_EVENT_LOG
static int decon_debug_event_show(struct seq_file *s, void *unused)
{
	struct decon_device *decon = s->private;
	DISP_SS_EVENT_SHOW(s, decon);
	return 0;
}

static int decon_debug_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, decon_debug_event_show, inode->i_private);
}

static struct file_operations decon_event_fops = {
	.open = decon_debug_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

#if defined(CONFIG_EXYNOS_DECON_DPU)

static int decon_dpu_save(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dpu_dump); i++) {
		dpu_dump[i].val = dpu_read(dpu_dump[i].addr);
	}
	for (i = 0; i < ARRAY_SIZE(dpu_gamma_dump); i++) {
		dpu_gamma_dump[i].val = dpu_read(dpu_gamma_dump[i].addr);
	}
	for (i = 0; i < AD_SIZE; i++ ){
		dpu_ad_dump[i].val = dpu_read(dpu_ad_dump[i].addr);
	}
	return 0;
}

/* Restore registers after hibernation */
static int decon_dpu_restore(struct device *dev)
{
	int i;

	dpu_write_mask(DPU_MASK_CTRL, ~0, DPU_MASK_CTRL_MASK);
	for (i = 0; i < ARRAY_SIZE(dpu_dump); i++) {
		dpu_write(dpu_dump[i].addr, dpu_dump[i].val);
	}
	for (i = 0; i < ARRAY_SIZE(dpu_gamma_dump); i++) {
		dpu_write(dpu_gamma_dump[i].addr, dpu_gamma_dump[i].val);
	}
	for (i = 0; i < AD_SIZE; i++) {
		dpu_write(dpu_ad_dump[i].addr, dpu_ad_dump[i].val);
	}
	dpu_write_mask(DPU_MASK_CTRL, 0, DPU_MASK_CTRL_MASK);

	return 0;
}

static int decon_dpu_gamma_init(void)
{
	int i;

	for(i = 0; i < DPUGAMMALUT_MAX; i++)
	{
		dpu_gamma_dump[i].addr = DPUGAMMALUT_X_Y_BASE + (i*4);
		dpu_gamma_dump[i].val = 0;
	}

	return 0;
}

static u32 cur_gamma_table[195] = {0};
static ssize_t decon_dpu_cont_store(struct device *dev, struct device_attribute *attr,
	const char *buffer, size_t count)
{
	int i = 0, j = 0, val = 0, ret = 0;
	u32 offset_in = 0, offset_ex = 0, gamma = 0, mask = 0, length = 0;
	const char *str_fomat = NULL;
	const char *str_num = NULL;
	char *head = NULL, *ptr = NULL;

	if (count > 1024 || count == 0)
		return -EINVAL;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -EBUSY;
			goto cont_err;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		return -ENODEV;
	}

	if (buffer) {
		str_fomat = buffer;
		str_num = buffer+2;
		length = count - 2;

		switch (str_fomat[0]) {
		case 'O':
			mask = TSC_ON;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for cont!\n");
				ret = -EINVAL;
				goto cont_err;
			}
			dpu_reg_set_contrast_onoff(val);
			decon_int_drvdata->dpu_save.contrast_onoff = val;
			ret = count;
			goto cont_done;
		default:
			break;
		}
	} else {
		ret = -EINVAL;
		goto cont_err;
	}

	head = (char *)buffer;
	while (*head != 0 && i < 195) {
		ptr = strchr(head, ',');
		if (ptr == NULL) {
			ret = -EFAULT;
			goto cont_err;
		}
		*ptr = 0;
		ret = kstrtou32(head, 0, &cur_gamma_table[i]);
		if (ret) {
			ret = -EFAULT;
			goto cont_err;
		}

		head = ptr + 1;
		i++;
	}

	for(j = 0; j < 3; j++) {
		for(i = 0; i < 65; i++) {
			if ((i%2) == 0) {
				if ( i >= 2)
					offset_in += 4;
				mask = DPU_GAMMA_LUT_Y_MASK;
				gamma = DPU_GAMMA_LUT_Y(cur_gamma_table[j * 65 + i]);
			} else {
				mask = DPU_GAMMA_LUT_X_MASK;
				gamma = DPU_GAMMA_LUT_X(cur_gamma_table[j * 65 + i]);
			}
			dpu_reg_set_contrast(offset_in + offset_ex, mask, gamma);
			mask = 0;
			gamma = 0;
			if (i == 64)
				offset_in = 0;
		}
		offset_ex += DPU_GAMMA_OFFSET;
	}

cont_done:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return count;

cont_err:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}

static DEVICE_ATTR(cont, S_IWUGO, NULL, decon_dpu_cont_store);

static ssize_t decon_dpu_scr_store(struct device *dev, struct device_attribute *attr,
	const char *buffer, size_t count)
{
	int val, ret = 0;
	u32 length = 0;
	const char *str_fomat = NULL;
	const char *str_num = NULL;

	if (count > 15 || count == 0)
		return -EINVAL;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -EBUSY;
			goto scr_err;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		return -ENODEV;
	}

	if (buffer) {
		str_fomat = buffer;
		str_num = buffer+2;
		length = count - 2;

		switch (str_fomat[0]) {
		case 'O':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for scr!\n");
				ret = -EINVAL;
				goto scr_err;
			}
			dpu_reg_set_scr_onoff(val);
			break;
		case 'R':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for scr!\n");
				ret = -EINVAL;
				goto scr_err;
			}
			dpu_reg_set_scr(DPUSCR_RED, val);
			break;
		case 'G':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for scr!\n");
				ret = -EINVAL;
				goto scr_err;
			}
			dpu_reg_set_scr(DPUSCR_GREEN, val);
			break;
		case 'B':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for scr!\n");
				ret = -EINVAL;
				goto scr_err;
			}
			dpu_reg_set_scr(DPUSCR_BLUE, val);
			break;
		case 'C':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for scr!\n");
				ret = -EINVAL;
				goto scr_err;
			}
			dpu_reg_set_scr(DPUSCR_CYAN, val);
			break;
		case 'M':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for scr!\n");
				ret = -EINVAL;
				goto scr_err;
			}
			dpu_reg_set_scr(DPUSCR_MAGENTA, val);
			break;
		case 'Y':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for scr!\n");
				ret = -EINVAL;
				goto scr_err;
			}
			dpu_reg_set_scr(DPUSCR_YELLOW, val);
			break;
		case 'W':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for scr!\n");
				ret = -EINVAL;
				goto scr_err;
			}
			dpu_reg_set_scr(DPUSCR_WHITE, val);
			break;
		case 'A':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for scr!\n");
				ret = -EINVAL;
				goto scr_err;
			}
			dpu_reg_set_scr(DPUSCR_BLACK, val);
			break;
		default:
			ret = -EINVAL;
			goto scr_err;
		}
	} else {
		ret = -EINVAL;
		goto scr_err;
	}

	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return count;

scr_err:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}

static DEVICE_ATTR(scr, S_IWUGO, NULL, decon_dpu_scr_store);

static ssize_t decon_dpu_sat_store(struct device *dev, struct device_attribute *attr,
	const char *buffer, size_t count)
{
	int val, ret = 0;
	u32 mask, length = 0;
	const char *str_fomat = NULL;
	const char *str_num = NULL;

	if (count > 10 || count == 0)
		return -EINVAL;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -EBUSY;
			goto sat_err;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		return -ENODEV;
	}

	if (buffer) {
		str_fomat = buffer;
		str_num = buffer+2;
		length = count - 2;

		switch (str_fomat[0]) {
		case 'R':
			mask = PAIM_GAIN0;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for sat!\n");
				ret = -EINVAL;
				goto sat_err;
			}
			dpu_reg_set_saturation_rgb(mask, DPU_TSC_RED(val));
			decon_int_drvdata->dpu_save.saturation_red = val;
			break;
		case 'G':
			mask = PAIM_GAIN1;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for sat!\n");
				ret = -EINVAL;
				goto sat_err;
			}
			dpu_reg_set_saturation_rgb(mask, DPU_TSC_GREEN(val));
			decon_int_drvdata->dpu_save.saturation_green = val;
			break;
		case 'B':
			mask = PAIM_GAIN2;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for sat!\n");
				ret = -EINVAL;
				goto sat_err;
			}
			dpu_reg_set_saturation_rgb(mask, DPU_TSC_BLUE(val));
			decon_int_drvdata->dpu_save.saturation_blue = val;
			break;
		case 'M':
			mask = PAIM_GAIN3;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for sat!\n");
				ret = -EINVAL;
				goto sat_err;
			}
			dpu_reg_set_saturation_cmy(mask, DPU_TSC_MAGENTA(val));
			decon_int_drvdata->dpu_save.saturation_magenta = val;
			break;
		case 'Y':
			mask = PAIM_GAIN4;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for sat!\n");
				ret = -EINVAL;
				goto sat_err;
			}
			dpu_reg_set_saturation_cmy(mask, DPU_TSC_YELLOW(val));
			decon_int_drvdata->dpu_save.saturation_yellow = val;
			break;
		case 'O':
			mask = TSC_ON;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for sat!\n");
				ret = -EINVAL;
				goto sat_err;
			}
			dpu_reg_set_saturation_onoff(val);
			decon_int_drvdata->dpu_save.saturation_onoff = val;
			break;
		case 'T':
			mask = TSC_GAIN;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for sat!\n");
				ret = -EINVAL;
				goto sat_err;
			}
			dpu_reg_set_saturation_tscgain(DPU_TSC_GAIN(val));
			decon_int_drvdata->dpu_save.saturation_total = val;
			break;
		case 'S':
			mask = PAIM_SHIFT;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for sat!\n");
				ret = -EINVAL;
				goto sat_err;
			}
			dpu_reg_set_saturation_shift(mask, DPU_TSC_SHIFT(val));
			decon_int_drvdata->dpu_save.saturation_shift = val;
			break;
		case 'C':
			mask = PAIM_SCALE;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for sat!\n");
				ret = -EINVAL;
				goto sat_err;
			}
			dpu_reg_set_saturation_shift(mask, DPU_TSC_SCALE(val));
			decon_int_drvdata->dpu_save.saturation_scale = val;
			break;
		default:
			ret = -EINVAL;
			goto sat_err;
		}
	} else {
		ret = -EINVAL;
		goto sat_err;
	}

	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return count;

sat_err:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}

static DEVICE_ATTR(sat, S_IWUGO, NULL, decon_dpu_sat_store);

static ssize_t decon_dpu_hue_store(struct device *dev, struct device_attribute *attr,
	const char *buffer, size_t count)
{
	int val, ret = 0;
	u32 mask, length = 0;
	const char *str_fomat = NULL;
	const char *str_num = NULL;

	if (count > 10 || count == 0)
		return -EINVAL;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -EBUSY;
			goto hue_err;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		return -ENODEV;
	}

	if (buffer) {
		str_fomat = buffer;
		str_num = buffer+2;
		length = count - 2;

		switch (str_fomat[0]) {
		case 'R':
			mask = DPU_PPHC_GAIN0_MASK;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for hue!\n");
				ret = -EINVAL;
				goto hue_err;
			}
			dpu_reg_set_hue_rgb(mask, DPU_HUE_RED(val));
			decon_int_drvdata->dpu_save.hue_red = val;
			break;
		case 'G':
			mask = DPU_PPHC_GAIN1_MASK;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for hue!\n");
				ret = -EINVAL;
				goto hue_err;
			}
			dpu_reg_set_hue_rgb(mask, DPU_HUE_GREEN(val));
			decon_int_drvdata->dpu_save.hue_green = val;
			break;
		case 'B':
			mask = DPU_PPHC_GAIN2_MASK;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for hue!\n");
				ret = -EINVAL;
				goto hue_err;
			}
			dpu_reg_set_hue_rgb(mask, DPU_HUE_BLUE(val));
			decon_int_drvdata->dpu_save.hue_blue = val;
			break;
		case 'C':
			mask = DPU_PPHC_GAIN3_MASK;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for hue!\n");
				ret = -EINVAL;
				goto hue_err;
			}
			dpu_reg_set_hue_cmy(mask, DPU_HUE_CYAN(val));
			decon_int_drvdata->dpu_save.hue_cyan = val;
			break;
		case 'M':
			mask = DPU_PPHC_GAIN4_MASK;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for hue!\n");
				ret = -EINVAL;
				goto hue_err;
			}
			dpu_reg_set_hue_cmy(mask, DPU_HUE_MAGENTA(val));
			decon_int_drvdata->dpu_save.hue_magenta = val;
			break;
		case 'Y':
			mask = DPU_PPHC_GAIN5_MASK;
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for hue!\n");
				ret = -EINVAL;
				goto hue_err;
			}
			dpu_reg_set_hue_cmy(mask, DPU_HUE_YELLOW(val));
			decon_int_drvdata->dpu_save.hue_yellow = val;
			break;
		case 'O':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for hue!\n");
				ret = -EINVAL;
				goto hue_err;
			}
			dpu_reg_set_hue_onoff(val);
			decon_int_drvdata->dpu_save.hue_onoff = val;
			break;
		default:
			ret = -EINVAL;
			goto hue_err;
		}
	} else {
		ret = -EINVAL;
		goto hue_err;
	}

	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return count;

hue_err:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}

static DEVICE_ATTR(hue, S_IWUGO, NULL, decon_dpu_hue_store);

static ssize_t decon_dpu_ad_store(struct device *dev, struct device_attribute *attr,
	const char *buffer, size_t count)
{
	int val, ret = 0;
	u32 length = 0;
	const char *str_fomat = NULL;
	const char *str_num = NULL;

	if (count > 32 || count == 0)
		return -EINVAL;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -EBUSY;
			goto ad_err;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		return -ENODEV;
	}

	if (buffer) {
		str_fomat = buffer;
		str_num = buffer+2;
		length = count - 2;

		switch (str_fomat[0]) {
		case 'B':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for ad!\n");
				ret = -EINVAL;
				goto ad_err;
			}
			dpu_reg_set_ad_backlight(DPU_AD_BACKLIGHT(val));
			decon_int_drvdata->dpu_save.ad_blacklight = val;
			break;
		case 'A':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for ad!\n");
				ret = -EINVAL;
				goto ad_err;
			}
			dpu_reg_set_ad_ambient(DPU_AD_AMBIENT(val));
			decon_int_drvdata->dpu_save.ad_ambient = val;
			break;
		case 'O':
			if (kstrtoint(str_num, 16, &val)) {
				printk(KERN_ERR " invalid parameter for ad!\n");
				ret = -EINVAL;
				goto ad_err;
			}
			dpu_reg_set_ad_onoff(val);
			decon_int_drvdata->dpu_save.ad_onoff = val;
			break;
		default:
			ret = -EINVAL;
			goto ad_err;
		}
	} else {
		ret = -EINVAL;
		goto ad_err;
	}

	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return count;

ad_err:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}

static DEVICE_ATTR(ad, S_IWUGO, NULL, decon_dpu_ad_store);

static ssize_t decon_dpu_con_reg_show(struct device *dev,
	struct device_attribute *attr, char *buffer)
{
	u32 dpu_con;
	int ret = 0;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -EBUSY;
			goto con_reg_show_done;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		return -ENODEV;
	}

	dpu_con = dpu_read(DPUCON);
	ret = sprintf(buffer, "0x%x\n", dpu_con);

con_reg_show_done:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}

static DEVICE_ATTR(con_reg, S_IRUGO, decon_dpu_con_reg_show, NULL);

static ssize_t decon_dpu_bri_store(struct device *dev, struct device_attribute *attr,
	const char *buffer, size_t count)
{
	u32 value = 0;
	const char *ptr = NULL;
	int ret = count;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -EBUSY;
			goto bri_store_done;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		return -ENODEV;
	}

	if (buffer) {
		ptr = buffer;

		if (kstrtoint(ptr, 10, &value)) {
			printk(KERN_ERR " invalid parameter for bri!\n");
			ret = -EINVAL;
			goto bri_store_done;
		}

		dpu_reg_set_bri_gain(DPU_BRI_GAIN(value));
	}

bri_store_done:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}

static DEVICE_ATTR(bri, S_IWUGO, NULL, decon_dpu_bri_store);

static ssize_t decon_dpu_ctrl_reg_show(struct device *dev,
	struct device_attribute *attr, char *buffer)
{
	u32 dpu_ctrl;
	int ret = 0;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -EBUSY;
			goto ctrl_reg_show_done;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		return -ENODEV;
	}

	dpu_ctrl = dpu_read(DPUHSC_CONTROL);
	ret = sprintf(buffer, "0x%x\n", dpu_ctrl);

ctrl_reg_show_done:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}

static ssize_t decon_dpu_ctrl_reg_store(struct device *dev, struct device_attribute *attr,
	const char *buffer, size_t count)
{
	u32 dpu_ctrl = 0, value = 0;
	int type = -1;
	int ret = count;
	const char *tptr = NULL, *ptr = NULL;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -EBUSY;
			goto ctrl_reg_store_done;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		return -ENODEV;
	}

	if (buffer) {
		tptr = buffer;
		ptr = buffer + 2;

		switch (tptr[0]) {
		case 'P':
			type = 5;
			break;
		case 'Y':
			type = 4;
			break;
		case 'T':
			type = 3;
			break;
		case 'G':
			type = 2;
			break;
		case 'H':
			type = 1;
			break;
		case 'S':
			type = 0;
			break;
		default:
			ret = -EINVAL;
			goto ctrl_reg_store_done;
		}

		if (kstrtoint(ptr, 10, &value)) {
			printk(KERN_ERR " invalid parameter for ctrl reg!\n");
			ret = -EINVAL;
			goto ctrl_reg_store_done;
		}

		dpu_ctrl = dpu_read(DPUHSC_CONTROL);
		if (value == 1) {
			dpu_ctrl |= (1 << type);
		} else if (value == 0) {
			dpu_ctrl &= ~(1 << type);
		} else {
			printk(KERN_ERR " invalid parameter for ctrl reg!\n");
			ret = -EINVAL;
			goto ctrl_reg_store_done;
		}

		dpu_reg_set_control(dpu_ctrl);
	}

ctrl_reg_store_done:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}

static DEVICE_ATTR(ctrl_reg, 0666, decon_dpu_ctrl_reg_show, decon_dpu_ctrl_reg_store);

#if 0
static ssize_t decon_dpu_preset_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *ppos)
{
	int val, ret = 0;

	if (count > 32 || count == 0)
		return -1;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -1;
			goto preset_err;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		ret = -1;
		goto preset_err;
	}

	if (kstrtoint_from_user(buffer, count, 16, &val)) {
		printk(KERN_ERR " copy_from_user() failed!\n");
		ret = -EFAULT;
		goto preset_err;
	}

	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return count;
preset_err:

	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}
#endif

typedef struct _reg_ops{
	const char *reg_name;
	void (*func)(char *value[], int count);
}reg_ops;

static void decon_dpu_lut_fi_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
			return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_lut_fi(reg_val, count);
}

static void decon_dpu_lut_cc_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_lut_cc(reg_val, count);
}
static void decon_dpu_iridix_control_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_iridix_control(reg_val[0]);
}

static void decon_dpu_black_level_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_black_level(reg_val[0]);
}

static void decon_dpu_white_level_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_white_level(reg_val[0]);
}

static void decon_dpu_variance_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_variance(reg_val[0]);
}

static void decon_dpu_limit_ampl_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_limit_ampl(reg_val[0]);
}

static void decon_dpu_iridix_dither_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_iridix_dither(reg_val[0]);
}

static void decon_dpu_slope_max_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_slope_max(reg_val[0]);
}

static void decon_dpu_slope_min_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_slope_min(reg_val[0]);
}

static void decon_dpu_dither_control_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	//dpu_reg_en_dither(reg_val[0]);
	// no exposed to user
}

static void decon_dpu_frame_width_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_frame_width(reg_val[0]);
}

static void decon_dpu_frame_height_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_frame_height(reg_val[0]);
}

static void decon_dpu_logo_top_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_logo_top(reg_val[0]);
}

static void decon_dpu_logo_left_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_logo_left(reg_val[0]);
}

static void decon_dpu_mode_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_mode(reg_val[0]);
}

static void decon_dpu_al_calib_lut_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_al_calib_lut(reg_val, count);
}

static void decon_dpu_backlight_min_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_backlight_min(reg_val[0]);
}

static void decon_dpu_backlight_max_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_backlight_max(reg_val[0]);
}

static void decon_dpu_backlight_scale_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_backlight_scale(reg_val[0]);
}

static void decon_dpu_ambient_light_min_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_ambient_light_min(reg_val[0]);
}

static void decon_dpu_filter_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_ambient_filter_a(reg_val[0]);
	dpu_reg_set_ambient_filter_b(reg_val[1]);
}

static void decon_dpu_calibration_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++){
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	}
	dpu_reg_set_calibration_a(reg_val[0]);
	dpu_reg_set_calibration_b(reg_val[1]);
	dpu_reg_set_calibration_c(reg_val[2]);
	dpu_reg_set_calibration_d(reg_val[3]);
}

static void decon_dpu_strength_limit_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_strength_limit(reg_val[0]);
}

static void decon_dpu_t_filter_control_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_t_filter_control(reg_val[0]);
}

static void decon_dpu_backlight_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_ad_backlight(reg_val[0]);
}

static void decon_dpu_ambientlight_set(char *value[], int count)
{
	int i = 0, ret = 0;
	int reg_val[40] = {0};

	if (count > 40)
		return;
	for (i = 0; i < count; i++)
		ret = kstrtoint(value[i], 0, &reg_val[i]);
	dpu_reg_set_ad_ambient(reg_val[0]);
//#ifdef CONFIG_DTS_CMD_LCD
	decon_start_update_frames_state();
//#endif
}

static  reg_ops reg_op[] = {
	{"lut_fi", decon_dpu_lut_fi_set},
	{"lut_cc", decon_dpu_lut_cc_set},
	{"iridix_control", decon_dpu_iridix_control_set},
	{"black_level", decon_dpu_black_level_set},
	{"white_level", decon_dpu_white_level_set},
	{"variance", decon_dpu_variance_set},
	{"limit_ampl", decon_dpu_limit_ampl_set},
	{"iridix_dither", decon_dpu_iridix_dither_set},
	{"slope_max", decon_dpu_slope_max_set},
	{"slope_min", decon_dpu_slope_min_set},
	{"dither_control", decon_dpu_dither_control_set},
	{"frame_width", decon_dpu_frame_width_set},
	{"frame_height", decon_dpu_frame_height_set},
	{"logo_top", decon_dpu_logo_top_set},
	{"logo_left", decon_dpu_logo_left_set},
	{"mode", decon_dpu_mode_set},
	{"al_calib_lut", decon_dpu_al_calib_lut_set},
	{"backlight_min", decon_dpu_backlight_min_set},
	{"backlight_max", decon_dpu_backlight_max_set},
	{"backlight_scale", decon_dpu_backlight_scale_set},
	{"ambient_light_min", decon_dpu_ambient_light_min_set},
	{"filter", decon_dpu_filter_set},
	{"calibration", decon_dpu_calibration_set},
	{"strength_limit", decon_dpu_strength_limit_set},
	{"t_filter_control", decon_dpu_t_filter_control_set},
	{"backlight", decon_dpu_backlight_set},
	{"ambientlight", decon_dpu_ambientlight_set},
};

static void decon_dpu_adreg_command_set(const char *buf)
{
#define CMDLEN 1024
#define VALUECOUNT 40

	char str[CMDLEN] = {0};
	char *space = ",";
	char *p = NULL;
	char *cmd[VALUECOUNT];
	int i = 0;
	int value_count = 0;
	int ret = 0;
	if (buf == NULL)
		return;
	if (strlen(buf) >= CMDLEN){
		printk(KERN_ERR "command too long!\n");
		return;
	}
	strcpy(str, buf);
	str[strlen(buf)] = ',';
	//printk( "%s\n ", str);
	p =  strstr(str, space);
	cmd[i] = str;
	while( p != NULL )
	{
		p[0] ='\0';
		//printk( "%s\n ", cmd[i]);
		if (i++ > VALUECOUNT){
			printk(KERN_ERR "too many value count!\n");
			return;
		}
		cmd[i] = p + strlen(space);
		p = strstr(cmd[i], space);
	}
	if (i < 3)
		return;
	ret = kstrtoint(cmd[1], 0, &value_count);
	for(i = 0; i < sizeof(reg_op)/sizeof(reg_ops); i++){
		if (!strcmp(cmd[0], reg_op[i].reg_name)){
			(reg_op[i].func)(&cmd[2], value_count);
			break;
		}
	}

	return;
}

static ssize_t decon_dpu_adreg_store(struct device *dev, struct device_attribute *attr,
	const char *buffer, size_t count)
{
	int  ret = 0;

	if (count > 1024 || count == 0)
		return -EINVAL;

	if (decon_int_drvdata) {
		decon_lpd_block_exit(decon_int_drvdata);
		if(decon_int_drvdata->state != DECON_STATE_ON) {
			printk(KERN_ERR " decon is not enabled!\n");
			ret = -EBUSY;
			goto preset_err;
		}
	} else {
		printk(KERN_ERR " decon_int_drvdata is NULL!\n");
		return -ENODEV;
	}
	if (buffer) {
		decon_dpu_adreg_command_set(buffer);
	}

	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return count;

preset_err:
	if (decon_int_drvdata)
		decon_lpd_unblock(decon_int_drvdata);

	return ret;
}

static DEVICE_ATTR(adreg, S_IWUGO, NULL, decon_dpu_adreg_store);

#if 0
void AL_change_detect(struct decon_device * decon, unsigned int input_ambient_light_val)
{
	float tolerance = 0.5;
	float AL_offset = 100;
	static int log_AL_discrete_prev = 0;
	static int log_AL_discrete = 0;
	//double N = 60;

	int log_AL = __ilog2_u32(input_ambient_light_val+ AL_offset);
	if (abs(log_AL_discrete_prev - log_AL) < tolerance){
		log_AL_discrete = log_AL_discrete_prev;
	}else{
		log_AL_discrete = tolerance * ((log_AL/tolerance + ((log_AL_discrete_prev > log_AL)? 1:0)));
		decon_exit_lpd(decon);
		msleep(1000);
	}
	log_AL_discrete_prev = log_AL_discrete;
}
#endif
#endif

static ssize_t decon_enterHBM_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	struct dsim_device *dsim;
	unsigned int value;
	int ret = 0;

	if (decon->id)
		goto out;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		goto out;

	if (decon->HBM_status == value)
		goto out;

	if (decon->state != DECON_STATE_ON && decon->state != DECON_STATE_LPD)
		goto out;

	decon->HBM_status = value;

	decon_lpd_block(decon);

	if (decon->state == DECON_STATE_LPD)
		decon_exit_lpd(decon);

	dsim = container_of(decon->output_sd, struct dsim_device, sd);
	if (dsim != NULL)
		call_panel_ops(dsim, enterHBM, dsim, value);

	decon_lpd_unblock(decon);

out:
	return count;
}

static ssize_t decon_enterHBM_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", decon->HBM_status);
}

static DEVICE_ATTR(enterHBM, S_IRUGO | S_IWUSR, decon_enterHBM_show, decon_enterHBM_store);

/* --------- DRIVER INITIALIZATION ---------- */
static int decon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct decon_device *decon;
	struct resource *res;
	struct fb_info *fbinfo;
	int ret = 0;
	char device_name[MAX_NAME_SIZE], debug_name[MAX_NAME_SIZE];
	struct decon_psr_info psr;
	struct decon_init_param p;
	struct decon_regs_data win_regs;
	struct dsim_device *dsim;
	struct exynos_md *md;
	struct device_node *cam_stat;
	struct lcdfreq_info *info = NULL;
	u32 reg,clkval;

	int i = 0;
#ifdef CONFIG_DECON_DISPLAY_LOGO
	int err;
#endif
	int win_idx = 0;
	u32 vclk_rate;

	dev_info(dev, "%s start\n", __func__);

	decon = devm_kzalloc(dev, sizeof(struct decon_device), GFP_KERNEL);
	if (!decon) {
		decon_err("no memory for decon device\n");
		return -ENOMEM;
	}

	/* setup pointer to master device */
	decon->dev = dev;
	decon->pdata = devm_kzalloc(dev, sizeof(struct exynos_decon_platdata),
						GFP_KERNEL);
	if (!decon->pdata) {
		decon_err("no memory for DECON platdata\n");
		return -ENOMEM;
	}

	/* store platform data ptr to decon_tv context */
	decon_parse_pdata(decon, dev);
	win_idx = decon->pdata->default_win;

	/* init clock setting for decon */
	if (!decon->id) {
		decon_int_drvdata = decon;
		decon_int_get_clocks(decon);
	} else {
		decon_ext_drvdata = decon;
		decon_ext_get_clocks(decon);
	}

	/* Get memory resource and map SFR region. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	decon->regs = devm_request_and_ioremap(dev, res);
	if (decon->regs == NULL) {
		decon_err("failed to claim register region\n");
		return -ENOENT;
	}

	spin_lock_init(&decon->slock);
	init_waitqueue_head(&decon->vsync_info.wait);
	init_waitqueue_head(&decon->wait_frmdone);
	mutex_init(&decon->vsync_info.irq_lock);
	snprintf(device_name, MAX_NAME_SIZE, "decon%d", decon->id);
	decon->timeline = sw_sync_timeline_create(device_name);
	decon->timeline_max = 1;

	/* Get IRQ resource and register IRQ, create thread */
	if (!decon->id) {
		ret = decon_int_register_irq(pdev, decon);
		if (ret)
			goto fail;
		ret = decon_int_create_vsync_thread(decon);
		if (ret)
			goto fail;
		ret = decon_int_create_psr_thread(decon);
		if (ret)
			goto fail_vsync_thread;
		ret = decon_fb_config_eint_for_te(pdev, decon);
		if (ret)
			goto fail_psr_thread;
		ret = decon_int_register_lpd_work(decon);
		if (ret)
			goto fail_psr_thread;

		snprintf(debug_name, MAX_NAME_SIZE, "decon");
		decon->debug_root = debugfs_create_dir(debug_name, NULL);
		if (!decon->debug_root) {
			decon_err("failed to create debugfs root directory.\n");
			goto fail_psr_thread;
		}
	} else {
		decon->debug_root = get_decon_drvdata(0)->debug_root;
		ret = decon_ext_register_irq(pdev, decon);
		if (ret)
			goto fail;
	}

	decon->ion_client = ion_client_create(ion_exynos, device_name);
	if (IS_ERR(decon->ion_client)) {
		decon_err("failed to ion_client_create\n");
		goto fail_thread;
	}

#ifdef CONFIG_DECON_EVENT_LOG
	snprintf(debug_name, MAX_NAME_SIZE, "event%d", decon->id);
	atomic_set(&decon->disp_ss_log_idx, -1);
	decon->debug_event = debugfs_create_file(debug_name, 0444,
						decon->debug_root,
						decon, &decon_event_fops);
#endif

#if defined(CONFIG_EXYNOS_DECON_DPU)
	device_create_file(decon->dev, &dev_attr_cont);
	device_create_file(decon->dev, &dev_attr_scr);
	device_create_file(decon->dev, &dev_attr_sat);
	device_create_file(decon->dev, &dev_attr_hue);
	device_create_file(decon->dev, &dev_attr_ad);
	device_create_file(decon->dev, &dev_attr_adreg);
	device_create_file(decon->dev, &dev_attr_con_reg);
	device_create_file(decon->dev, &dev_attr_bri);
	device_create_file(decon->dev, &dev_attr_ctrl_reg);

	decon_init_update_timer();
	decon_init_ad_reg();
#endif

	/* exynos_create_iovmm(dev, decon->n_sink_pad, 0); */ /* It doesn't need any more */
	ret = iovmm_activate(decon->dev);
	if (ret < 0) {
		decon_err("failed to reactivate vmm\n");
		goto fail_iovmm;
	}

	/* register internal and external DECON as entity */
	ret = decon_register_entity(decon);
	if (ret)
		goto fail_psr_thread;

	/* configure windows */
	ret = decon_acquire_window(decon);
	if (ret)
		goto fail_entity;

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		decon_err("failed to get output media device\n");
		goto fail_entity;
	}

	decon->mdev = md;

	/* mapping SYSTEM registers */
	ret = decon_get_sysreg_addr(decon);
	if (ret)
		goto fail_entity;

	/* link creation: vpp <-> decon / decon <-> output */
	ret = decon_create_links(decon, md);
	if (ret)
		goto fail_entity;

	ret = decon_register_subdev_nodes(decon, md);
	if (ret)
		goto fail_entity;


	/* register framebuffer */
	fbinfo = decon->windows[decon->pdata->default_win]->fbinfo;
	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		decon_err("failed to register framebuffer\n");
		goto fail_entity;
	}

	/* mutex mechanism */
	mutex_init(&decon->output_lock);
	mutex_init(&decon->mutex);

	/* init work thread for update registers */
	INIT_LIST_HEAD(&decon->update_regs_list);
	mutex_init(&decon->update_regs_list_lock);
	init_kthread_worker(&decon->update_regs_worker);

	decon->update_regs_thread = kthread_run(kthread_worker_fn,
			&decon->update_regs_worker, device_name);
	if (IS_ERR(decon->update_regs_thread)) {
		ret = PTR_ERR(decon->update_regs_thread);
		decon->update_regs_thread = NULL;
		decon_err("failed to run update_regs thread\n");
		goto fail_entity;
	}
	init_kthread_work(&decon->update_regs_work, decon_update_regs_handler);

	snprintf(device_name, MAX_NAME_SIZE, "decon%d-wb", decon->id);
	decon->wb_timeline = sw_sync_timeline_create(device_name);
	decon->wb_timeline_max = 0;

	if (!decon->id || decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY) {
		ret = decon_int_set_lcd_config(decon);
		if (ret) {
			decon_err("failed to set lcd information\n");
			goto fail_iovmm;
		}
	}
	platform_set_drvdata(pdev, decon);
	pm_runtime_enable(dev);

	if (!decon->id || decon->pdata->dsi_mode == DSI_MODE_DUAL_DISPLAY) {
#if defined(CONFIG_DECON_DEVFREQ)
		if (!decon->id) {
			decon->default_bw = decon_calc_bandwidth(fbinfo->var.xres,
					fbinfo->var.yres, fbinfo->var.bits_per_pixel,
					decon->windows[decon->pdata->default_win]->fps);
			exynos7_update_media_layers(TYPE_RESOLUTION, decon->default_bw);
		}
#endif

#if defined(CONFIG_PM_RUNTIME)
		pm_runtime_get_sync(decon->dev);
#else
		decon_runtime_resume(decon->dev);
#endif

		decon_to_init_param(decon, &p);

		/* DECON does not need to start, if DECON is already
		 * running(enabled in LCD_ON_UBOOT) */
		if (decon_reg_get_stop_status(decon->id)) {
			decon_reg_init_probe(decon->id, decon->pdata->dsi_mode, &p);
			if (decon->pdata->trig_mode == DECON_HW_TRIG)
				decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
						decon->pdata->trig_mode, DECON_TRIG_DISABLE);

#if defined(CONFIG_EXYNOS_DECON_DPU)
			if (!decon->id && decon->state != DECON_STATE_LPD_EXIT_REQ) {
				dpu_reg_start(decon->lcd_info->xres, decon->lcd_info->yres);
				decon_dpu_gamma_init();
			}
#endif

#ifdef CONFIG_DECON_SIMULATE_DISPLAY
			dsim = container_of(decon->output_sd, struct dsim_device, sd);
			if (!decon->id) {
				decon->isConnectLcd = dsim->isConnectLcd;
			}
#endif

			goto decon_init_done;
		}

		if (decon->pdata->ip_ver & IP_VER_DECON_7I)
			decon_set_sysreg_disp_cfg(decon);

		decon_reg_shadow_protect_win(decon->id, win_idx, 1);

		decon_reg_init(decon->id, decon->pdata->dsi_mode, &p);

		win_regs.wincon = WINCON_BPPMODE_ARGB8888;
		win_regs.winmap = 0x0;
		win_regs.vidosd_a = vidosd_a(0, 0);
		win_regs.vidosd_b = vidosd_b(0, 0, fbinfo->var.xres, fbinfo->var.yres);
		win_regs.vidosd_c = vidosd_c(0x0, 0x0, 0x0);
		win_regs.vidosd_d = vidosd_d(0xff, 0xff, 0xff);
		win_regs.vidw_buf_start = fbinfo->fix.smem_start;
		win_regs.vidw_whole_w = fbinfo->var.xres_virtual;
		win_regs.vidw_whole_h = fbinfo->var.yres_virtual;
		win_regs.vidw_offset_x = fbinfo->var.xoffset;
		win_regs.vidw_offset_y = fbinfo->var.yoffset;
		win_regs.type = IDMA_G0;

		decon_reg_set_regs_data(decon->id, win_idx, &win_regs);

		decon_reg_shadow_protect_win(decon->id, win_idx, 0);

#if defined(CONFIG_EXYNOS_DECON_DPU)
		if (!decon->id && decon->state != DECON_STATE_LPD_EXIT_REQ) {
			dpu_reg_start(decon->lcd_info->xres, decon->lcd_info->yres);
			decon_dpu_gamma_init();
		}
#endif

		decon_to_psr_info(decon, &psr);
		decon_reg_start(decon->id, decon->pdata->dsi_mode, &psr);

		decon_reg_activate_window(decon->id, win_idx);

		decon_reg_set_winmap(decon->id, win_idx, 0x000000 /* black */, 1);

		if (decon->pdata->trig_mode == DECON_HW_TRIG)
			decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
					decon->pdata->trig_mode, DECON_TRIG_ENABLE);

		dsim = container_of(decon->output_sd, struct dsim_device, sd);
#ifdef CONFIG_DECON_SIMULATE_DISPLAY
		if (!decon->id) {
			decon->isConnectLcd = dsim->isConnectLcd;
		}
#endif
#if 0	// Used in only EVB for 2lane. kh620.kim@samsung.com
		dsim_reg_set_data_transfer_mode(0, 1);
		call_panel_ops(dsim, displayon, dsim);
		dsim_reg_set_data_transfer_mode(0, 0);
#else
		call_panel_ops(dsim, displayon, dsim);
#endif

decon_init_done:
		if (!decon->id) {
			decon->HBM_status = 0;
			device_create_file(decon->dev, &dev_attr_enterHBM);
		}

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		if (!decon->id) {
			ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL);
			if (ret)
				decon_err("Failed to call DSIM packet go enable in %s!\n", __func__);
		}
#endif
		decon->state = DECON_STATE_ON;

		pm_stay_awake(decon->dev);
		dev_warn(decon->dev, "pm_stay_awake");
		cam_stat = of_get_child_by_name(decon->dev->of_node, "cam-stat");
		if (!cam_stat) {
			decon_info("No DT node for cam-stat\n");
		} else {
			decon->cam_status[0] = of_iomap(cam_stat, 0);
			if (!decon->cam_status[0])
				decon_info("Failed to get CAM0-STAT Reg\n");

			decon->cam_status[1] = of_iomap(cam_stat, 1);
			if (!decon->cam_status[1])
				decon_info("Failed to get CAM1-STAT Reg\n");
		}

#if defined(CONFIG_DECON_DEVFREQ)
		vclk_rate = clk_get_rate(decon->res.vclk) / KHZ;
		vclk_rate = (vclk_rate <= 0) ? 133000 : vclk_rate;
		if (!decon->id) {
			/* MIC_FACT = 2, PIX_BYTES = 4 */
			decon->default_bw = vclk_rate * 2 * 4;
			exynos7_update_media_scenario(TYPE_DECON_INT,
							decon->default_bw, 0);
			pm_qos_add_request(&decon->int_qos,
						PM_QOS_DEVICE_THROUGHPUT, 0);
			pm_qos_add_request(&decon->disp_qos,
						PM_QOS_DISPLAY_THROUGHPUT, 0);
		} else {
			/*PIX_BYTES = 4 */
			decon->default_bw = vclk_rate * 4;
			exynos7_update_media_scenario(TYPE_DECON_EXT,
							decon->default_bw, 0);
		}
		decon->prev_bw = decon->default_bw;
#endif
		if (!decon->id) {
			decon->restore_cnt = 0;
			INIT_DELAYED_WORK(&decon->restore_delay_work, decon_restore_work_handler);
			decon->restore_wq = create_singlethread_workqueue("decon_restore_wq");
			if (!decon->restore_wq) {
				dev_err(dev, "can't create decon restore workqueue\n");
			}
			device_create_file(decon->dev, &dev_attr_restore_cnt);
		}
	} else { /* DECON-EXT(only single-dsi) is off at probe */
		decon->state = DECON_STATE_INIT;
	}

	for (i = 0; i < MAX_VPP_SUBDEV; i++)
		decon->vpp_used[i] = false;

#ifdef CONFIG_CPU_IDLE
	decon->lpc_nb = exynos_decon_lpc_nb;
	exynos_pm_register_notifier(&decon->lpc_nb);
#endif
//#ifdef CONFIG_DECON_DISPLAY_LOGO
	decon_set_par(fbinfo);
//#endif

#ifdef CONFIG_DECON_DISPLAY_LOGO
        if (decon_bootloader_fb_start) {
                err = memblock_free(decon_bootloader_fb_start,
                                decon_bootloader_fb_size);
                if (err){
				decon_info("failed to free bootloader fb\n");
        		} else {
                		decon_info("free bootloader fb\n");
        		}
	}
#endif
		
	decon->log_cnt = 0;

#ifdef PRINTFPS_DISPLAY
	device_create_file(dev, &dev_attr_printfps);
#endif

	decon_info("decon%d registered successfully", decon->id);

	info = kzalloc(sizeof(struct lcdfreq_info), GFP_KERNEL);
	if (!info) {
		pr_err("fail to allocate for lcdfreq\n");
		ret = -ENOMEM;
		goto err_1;
	}

	g_info = info;
	
	info->lcd_reg = decon->regs;
	info->level = NORMAL;
	info->table[NORMAL].level = NORMAL;
	info->table[NORMAL].hz = 60;
	info->table[LIMIT].level = LIMIT;
	info->table[LIMIT].hz = 30;
	
	for(i=NORMAL;i<LEVEL_MAX;i++){
		reg = readl(g_info->lcd_reg + VCLKCON1);
		clkval = (reg&0xff) + 0x1;
		g_info->table[i].cmu_div = clkval*(g_info->table[NORMAL].hz)/(g_info->table[i].hz)-0x1;
	}
	
	atomic_set(&info->usage, 0);
	mutex_init(&info->lock);
	ret = sysfs_create_group(&dev->kobj, &lcdfreq_attr_group);
	if (ret < 0) {
		pr_err("%s skip: sysfs_create_group\n", __func__);
		goto err_2;
	}

	info->enable = true;

	return 0;

fail_iovmm:
	iovmm_deactivate(dev);

fail_thread:
	if (decon->update_regs_thread)
		kthread_stop(decon->update_regs_thread);

fail_entity:
	decon_unregister_entity(decon);

fail_psr_thread:
	decon_int_destroy_psr_thread(decon);

fail_vsync_thread:
	decon_int_destroy_vsync_thread(decon);

fail:
	decon_err("decon probe fail");
	return ret;

err_2:
	sysfs_remove_group(&dev->kobj, &lcdfreq_attr_group);
err_1:
	kfree(info);
	return ret;	
}

static int decon_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct decon_device *decon = platform_get_drvdata(pdev);
	int i;

	pm_runtime_disable(dev);
	decon_put_clocks(decon);

	iovmm_deactivate(dev);
	unregister_framebuffer(decon->windows[0]->fbinfo);

	if (decon->update_regs_thread)
		kthread_stop(decon->update_regs_thread);

	for (i = 0; i < decon->pdata->max_win; i++)
		decon_release_windows(decon->windows[i]);

	debugfs_remove_recursive(decon->debug_root);
	kfree(decon);

	decon_info("remove sucessful\n");
	return 0;
}

static struct platform_driver decon_driver __refdata = {
	.probe		= decon_probe,
	.remove		= decon_remove,
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm	= &decon_pm_ops,
		.of_match_table = of_match_ptr(decon_device_table),
	}
};

static int exynos_decon_register(void)
{
	platform_driver_register(&decon_driver);

	return 0;
}

static void exynos_decon_unregister(void)
{
	platform_driver_unregister(&decon_driver);
}
module_init(exynos_decon_register);
module_exit(exynos_decon_unregister);

MODULE_AUTHOR("Ayoung Sim <a.sim@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS Soc DECON driver");
MODULE_LICENSE("GPL");
