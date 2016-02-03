/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/clk-provider.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>
#include <linux/of_address.h>
#include <linux/clk-private.h>

#include <media/v4l2-subdev.h>

#include <mach/regs-clock.h>
#include <plat/cpu.h>

#include "decon.h"
#include "dsim.h"
#include "decon_helper.h"

#define UNDERRUN_FILTER_INTERVAL_MS    100
#define UNDERRUN_FILTER_INIT           0
#define UNDERRUN_FILTER_IDLE           1
static int underrun_filter_status;
static struct delayed_work underrun_filter_work;
static int lpd_enable;

static void underrun_filter_handler(struct work_struct *ws)
{
       msleep(UNDERRUN_FILTER_INTERVAL_MS);
       underrun_filter_status = UNDERRUN_FILTER_IDLE;
}

static void decon_oneshot_underrun_log(struct decon_device *decon)
{
	if (underrun_filter_status++ > UNDERRUN_FILTER_IDLE)
		return;
	pr_err("underrun (level %d), overlap(%d -> %d), mif(%ld), chmap(0x%x), win(0x%lx), aclk(%ld)\n",
		decon->underrun_stat.fifo_level,
		decon->underrun_stat.prev_overlap_cnt,
		decon->underrun_stat.cur_overlap_cnt,
		(decon->underrun_stat.mif_pll >> 1) / MHZ,
		decon->underrun_stat.chmap,
		decon->underrun_stat.used_windows,
		decon->underrun_stat.aclk / MHZ);
	queue_delayed_work(system_freezable_wq, &underrun_filter_work, 0);
}

static void decon_int_get_enabled_win(struct decon_device *decon)
{
	int i;

	decon->underrun_stat.used_windows = 0;

	for (i = 0; i < decon->pdata->max_win; ++i)
		if (decon_read(decon->id, WINCON(i)) & WINCON_ENWIN)
			set_bit(i * 4, &decon->underrun_stat.used_windows);
}

irqreturn_t decon_int_irq_handler(int irq, void *dev_data)
{
	struct decon_device *decon = dev_data;
	u32 irq_sts_reg;
	ktime_t timestamp;
	u32 fifo_level;

	timestamp = ktime_get();

	spin_lock(&decon->slock);
	if ((decon->state == DECON_STATE_OFF) ||
		(decon->state == DECON_STATE_LPD)) {
		goto irq_end;
	}

	irq_sts_reg = decon_read(decon->id, VIDINTCON1);
	if (irq_sts_reg & VIDINTCON1_INT_FRAME) {
		/* VSYNC interrupt, accept it */
		decon_write_mask(decon->id, VIDINTCON1, ~0, VIDINTCON1_INT_FRAME);
		decon->vsync_info.timestamp = timestamp;
		wake_up_interruptible_all(&decon->vsync_info.wait);
	}
	if (irq_sts_reg & VIDINTCON1_INT_FIFO) {
		/* TODO: false underrun check only for EVT0. This will be removed in EVT1 */
		fifo_level = FRAMEFIFO_FIFO0_VALID_SIZE_GET(decon_read(decon->id, FRAMEFIFO_REG7));
		decon->underrun_stat.fifo_level = fifo_level;
		decon->underrun_stat.prev_overlap_cnt = decon->prev_overlap_cnt;
		decon->underrun_stat.cur_overlap_cnt = decon->cur_overlap_cnt;
		decon->underrun_stat.chmap = decon_read(0, WINCHMAP0);
		decon->underrun_stat.aclk = decon->res.aclk->rate;
		decon->underrun_stat.mif_pll = decon->res.mif_pll->rate;
		decon_int_get_enabled_win(decon);
		decon_oneshot_underrun_log(decon);
		decon_write_mask(decon->id, VIDINTCON1, ~0, VIDINTCON1_INT_FIFO);
		/* TODO: underrun function */
		/* s3c_fb_log_fifo_underflow_locked(decon, timestamp); */
	}
	if (irq_sts_reg & VIDINTCON1_INT_I80) {
		decon_write_mask(decon->id, VIDINTCON1, ~0, VIDINTCON1_INT_I80);
	}

irq_end:
	spin_unlock(&decon->slock);

	return IRQ_HANDLED;
}

int decon_int_get_clocks(struct decon_device *decon)
{
	decon->res.pclk = clk_get(decon->dev, "pclk_decon0");
	if (IS_ERR_OR_NULL(decon->res.pclk)) {
		decon_err("failed to get pclk_decon0\n");
		return -ENODEV;
	}

	decon->res.aclk = clk_get(decon->dev, "aclk_decon0");
	if (IS_ERR_OR_NULL(decon->res.aclk)) {
		decon_err("failed to get aclk_decon0\n");
		return -ENODEV;
	}

	decon->res.eclk = clk_get(decon->dev, "decon0_eclk");
	if (IS_ERR_OR_NULL(decon->res.eclk)) {
		decon_err("failed to get decon0_eclk\n");
		return -ENODEV;
	}

	decon->res.vclk = clk_get(decon->dev, "decon0_vclk");
	if (IS_ERR_OR_NULL(decon->res.vclk)) {
		decon_err("failed to get decon0_vclk\n");
		return -ENODEV;
	}

	decon->res.aclk_disp = clk_get(decon->dev, "aclk_disp");
	if (IS_ERR_OR_NULL(decon->res.aclk_disp)) {
		decon_err("failed to get aclk_disp\n");
		return -ENODEV;
	}

	decon->res.mif_pll = clk_get(decon->dev, "mif_pll");
	if (IS_ERR_OR_NULL(decon->res.mif_pll)) {
		decon_err("failed to get mif_pll\n");
		return -ENODEV;
	}

	decon->res.aclk_disp_200 = clk_get(decon->dev, "aclk_disp_200");
	if (IS_ERR_OR_NULL(decon->res.aclk_disp_200)) {
		decon_err("failed to get aclk_disp_200\n");
		return -ENODEV;
	}

	return 0;
}

void decon_int_set_clocks(struct decon_device *decon)
{
	struct device *dev = decon->dev;
	u32 clk_val_dpll = decon->pdata->disp_pll_clk;
	u32 clk_val_vclk, clk_val_pclk, clk_val_eclk, clk_val_aclk;

	if (!clk_val_dpll) {
		decon_info("setting predefined clock values\n");

		/* predefined values */
		decon_clk_set_rate(dev, "disp_pll", 267 * MHZ);
		decon_clk_set_rate(dev, "d_pclk_disp", 134 * MHZ);

		decon_clk_set_parent(dev, "m_decon0_eclk", "disp_pll");
		decon_clk_set_rate(dev, "d_decon0_eclk", 134 * MHZ);

		decon_clk_set_parent(dev, "m_decon0_vclk", "disp_pll");
		decon_clk_set_rate(dev, "d_decon0_vclk", 134 * MHZ);
	} else {
		clk_val_vclk = clk_val_dpll>>1;
		clk_val_eclk = (clk_val_vclk * 12) / 10;
		clk_val_aclk = clk_get_rate(decon->res.aclk_disp_200);
		clk_val_pclk = clk_val_aclk>>1;

		decon_info("setting pll to %dMHz\n", clk_val_dpll/MHZ);
		decon_info("setting clk_val_vclk %dMHz\n", clk_val_vclk/MHZ);
		decon_info("setting clk_val_eclk %dMHz\n", clk_val_eclk/MHZ);
		decon_info("setting clk_val_aclk %dMHz\n", clk_val_aclk/MHZ);
		decon_info("setting clk_val_pclk %dMHz\n", clk_val_pclk/MHZ);

		/* values based on DT input */
		decon_clk_set_rate(dev, "disp_pll", clk_val_dpll);
		decon_clk_set_rate(dev, "d_pclk_disp", clk_val_pclk);

		decon_clk_set_parent(dev, "m_decon0_eclk", "m_eclk_user");
		decon_clk_set_parent(dev, "m_eclk_user_a",
				"m_bus_pll_top_user");

		decon_clk_set_rate(dev, "d_sclk_decon0_eclk", clk_val_eclk);
		decon_clk_set_rate(dev, "d_decon0_eclk", clk_val_eclk);

		decon_clk_set_parent(dev, "m_decon0_vclk", "disp_pll");
		decon_clk_set_rate(dev, "d_decon0_vclk", clk_val_vclk);
	}
}

int find_subdev_mipi(struct decon_device *decon)
{
	struct exynos_md *md;

	if (decon->id && decon->pdata->dsi_mode == DSI_MODE_SINGLE) {
		decon_err("failed to get subdev of dsim\n");
		return -EINVAL;
	}

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		decon_err("failed to get mdev device(%d)\n", decon->id);
		return -ENODEV;
	}

	decon->output_sd = md->dsim_sd[decon->id];
	decon->out_type = DECON_OUT_DSI;

	if (IS_ERR_OR_NULL(decon->output_sd))
		decon_warn("couldn't find dsim%d subdev\n", decon->id);

	v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_GET_LCD_INFO, NULL);
	decon->lcd_info = (struct decon_lcd *)v4l2_get_subdev_hostdata(decon->output_sd);
	if (IS_ERR_OR_NULL(decon->lcd_info)) {
		decon_err("failed to get lcd information\n");
		return -EINVAL;
	}

	return 0;
}

int create_link_mipi(struct decon_device *decon)
{
	int i, ret = 0;
	int n_pad = decon->n_sink_pad + decon->n_src_pad;
	int flags = 0;
	char err[80];
	struct exynos_md *md = decon->mdev;

	if (IS_ERR_OR_NULL(md->dsim_sd[decon->id])) {
		decon_err("failed to get subdev of dsim%d\n", decon->id);
		return -EINVAL;
	}

	flags = MEDIA_LNK_FL_ENABLED;
	for (i = decon->n_sink_pad; i < n_pad ; i++) {
		ret = media_entity_create_link(&decon->sd.entity, i,
				&md->dsim_sd[decon->id]->entity, 0, flags);
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

static u32 fb_visual(u32 bits_per_pixel, unsigned short palette_sz)
{
	switch (bits_per_pixel) {
	case 32:
	case 24:
	case 16:
	case 12:
		return FB_VISUAL_TRUECOLOR;
	case 8:
		if (palette_sz >= 256)
			return FB_VISUAL_PSEUDOCOLOR;
		else
			return FB_VISUAL_TRUECOLOR;
	case 1:
		return FB_VISUAL_MONO01;
	default:
		return FB_VISUAL_PSEUDOCOLOR;
	}
}

static inline u32 fb_linelength(u32 xres_virtual, u32 bits_per_pixel)
{
	return (xres_virtual * bits_per_pixel) / 8;
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
	case 24:
		data |= WINCON_BPPMODE_RGB565;
		break;
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

int decon_set_par(struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	struct decon_regs_data win_regs;
	int win_no = win->index;

	memset(&win_regs, 0, sizeof(struct decon_regs_data));
	dev_info(decon->dev, "setting framebuffer parameters\n");

	if (decon->state == DECON_STATE_OFF)
		return 0;

	decon_lpd_block_exit(decon);

	decon_reg_shadow_protect_win(decon->id, win->index, 1);

	info->fix.visual = fb_visual(var->bits_per_pixel, 0);

	info->fix.line_length = fb_linelength(var->xres_virtual,
			var->bits_per_pixel);
	info->fix.xpanstep = fb_panstep(var->xres, var->xres_virtual);
	info->fix.ypanstep = fb_panstep(var->yres, var->yres_virtual);

	if (decon_reg_is_win_enabled(decon->id, win_no))
		win_regs.wincon = WINCON_ENWIN;
	else
		win_regs.wincon = 0;

	win_regs.wincon |= wincon(var->bits_per_pixel, var->transp.length);
	win_regs.winmap = 0x0;
	win_regs.vidosd_a = vidosd_a(0, 0);
	win_regs.vidosd_b = vidosd_b(0, 0, var->xres, var->yres);
	win_regs.vidosd_c = vidosd_c(0x0, 0x0, 0x0);
	win_regs.vidosd_d = vidosd_d(0xff, 0xff, 0xff);
	win_regs.vidw_buf_start = info->fix.smem_start;
	win_regs.vidw_whole_w = var->xoffset + var->width;
	win_regs.vidw_whole_h = var->yoffset + var->height;
	win_regs.vidw_offset_x = var->xoffset;
	win_regs.vidw_offset_y = var->yoffset;
	if (win_no)
		win_regs.blendeq = BLENDE_A_FUNC(BLENDE_COEF_ONE) |
			BLENDE_B_FUNC(BLENDE_COEF_ZERO) |
			BLENDE_P_FUNC(BLENDE_COEF_ZERO) |
			BLENDE_Q_FUNC(BLENDE_COEF_ZERO);
	win_regs.type = IDMA_G0;
	decon_reg_set_regs_data(decon->id, win_no, &win_regs);

	decon_reg_shadow_protect_win(decon->id, win->index, 0);

	decon_reg_update_standalone(decon->id);
	decon_lpd_unblock(decon);

	return 0;
}
EXPORT_SYMBOL(decon_set_par);

int decon_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	int x, y;
	unsigned long long hz;

	var->xres_virtual = max(var->xres_virtual, var->xres);
	var->yres_virtual = max(var->yres_virtual, var->yres);

	if (!decon_validate_x_alignment(decon, 0, var->xres,
			var->bits_per_pixel))
		return -EINVAL;

	/* always ensure these are zero, for drop through cases below */
	var->transp.offset = 0;
	var->transp.length = 0;

	switch (var->bits_per_pixel) {
	case 1:
	case 2:
	case 4:
	case 8:
		var->red.offset		= 4;
		var->green.offset	= 2;
		var->blue.offset	= 0;
		var->red.length		= 5;
		var->green.length	= 3;
		var->blue.length	= 2;
		var->transp.offset	= 7;
		var->transp.length	= 1;
		break;

	case 19:
		/* 666 with one bit alpha/transparency */
		var->transp.offset	= 18;
		var->transp.length	= 1;
	case 18:
		var->bits_per_pixel	= 32;

		/* 666 format */
		var->red.offset		= 12;
		var->green.offset	= 6;
		var->blue.offset	= 0;
		var->red.length		= 6;
		var->green.length	= 6;
		var->blue.length	= 6;
		break;

	case 16:
		/* 16 bpp, 565 format */
		var->red.offset		= 11;
		var->green.offset	= 5;
		var->blue.offset	= 0;
		var->red.length		= 5;
		var->green.length	= 6;
		var->blue.length	= 5;
		break;

	case 32:
	case 28:
	case 25:
		var->transp.length	= var->bits_per_pixel - 24;
		var->transp.offset	= 24;
		/* drop through */
	case 24:
		/* our 24bpp is unpacked, so 32bpp */
		var->bits_per_pixel	= 32;
		var->red.offset		= 16;
		var->red.length		= 8;
		var->green.offset	= 8;
		var->green.length	= 8;
		var->blue.offset	= 0;
		var->blue.length	= 8;
		break;

	default:
		decon_err("invalid bpp %d\n", var->bits_per_pixel);
		return -EINVAL;
	}

	if (decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE) {
		x = var->xres;
		y = var->yres;
	} else {
		x = var->xres + var->left_margin + var->right_margin + var->hsync_len;
		y = var->yres + var->upper_margin + var->lower_margin + var->vsync_len;
	}
	hz = 1000000000000ULL;		/* 1e12 picoseconds per second */

	hz += (x * y) / 2;
	do_div(hz, x * y);		/* divide by x * y with rounding */

	hz += var->pixclock / 2;
	do_div(hz, var->pixclock);	/* divide by pixclock with rounding */

	win->fps = hz;
	decon_dbg("xres:%d, yres:%d, v_xres:%d, v_yres:%d, bpp:%d, %lldhz\n",
			var->xres, var->yres, var->xres_virtual,
			var->yres_virtual, var->bits_per_pixel, hz);

	return 0;
}

static inline unsigned int chan_to_field(unsigned int chan,
					 struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

int decon_setcolreg(unsigned regno,
			    unsigned red, unsigned green, unsigned blue,
			    unsigned transp, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	unsigned int val;

	dev_dbg(decon->dev, "%s: win %d: %d => rgb=%d/%d/%d\n",
		__func__, win->index, regno, red, green, blue);

	if (decon->state == DECON_STATE_OFF)
		return 0;

	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/* true-colour, use pseudo-palette */

		if (regno < 16) {
			u32 *pal = info->pseudo_palette;

			val  = chan_to_field(red,   &info->var.red);
			val |= chan_to_field(green, &info->var.green);
			val |= chan_to_field(blue,  &info->var.blue);

			pal[regno] = val;
		}
		break;
	default:
		return 1;	/* unknown type */
	}

	return 0;
}
EXPORT_SYMBOL(decon_setcolreg);

static void decon_activate_window_dma(struct decon_device *decon, unsigned int index)
{
	decon_reg_direct_on_off(decon->id, 1);
	decon_reg_update_standalone(decon->id);
}

int decon_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	unsigned int start_boff, end_boff;
	int ret = 0;

	decon_lpd_block_exit(decon);

	decon_set_par(info);

	/* Offset in bytes to the start of the displayed area */
	start_boff = var->yoffset * info->fix.line_length;
	/* X offset depends on the current bpp */
	if (info->var.bits_per_pixel >= 8) {
		start_boff += var->xoffset * (info->var.bits_per_pixel >> 3);
	} else {
		switch (info->var.bits_per_pixel) {
		case 4:
			start_boff += var->xoffset >> 1;
			break;
		case 2:
			start_boff += var->xoffset >> 2;
			break;
		case 1:
			start_boff += var->xoffset >> 3;
			break;
		default:
			dev_err(decon->dev, "invalid bpp\n");
			ret = -EINVAL;
			goto pan_display_exit;
		}
	}
	/* Offset in bytes to the end of the displayed area */
	end_boff = start_boff + info->var.yres * info->fix.line_length;

	/* Temporarily turn off per-vsync update from shadow registers until
	 * both start and end addresses are updated to prevent corruption */
	decon_reg_shadow_protect_win(decon->id, win->index, 1);

	decon_activate_window_dma(decon, win->index);
	decon_reg_activate_window(decon->id, win->index);

	writel(info->fix.smem_start + start_boff, decon->regs + VIDW_ADD0(win->index));
	/* writel(info->fix.smem_start + end_boff, buf + 	sfb->variant.buf_end); */

	decon_reg_shadow_protect_win(decon->id, win->index, 0);

#ifdef CONFIG_FB_I80_COMMAND_MODE
	decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
			decon->pdata->trig_mode, DECON_TRIG_ENABLE);
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO

	if (v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL))
		decon_err("Failed to call DSIM packet go enable!\n");
#endif
	decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
			decon->pdata->trig_mode, DECON_TRIG_DISABLE);
#endif

pan_display_exit:
	decon_lpd_unblock(decon);
	return ret;
}
EXPORT_SYMBOL(decon_pan_display);

int decon_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
#ifdef CONFIG_ION_EXYNOS
	int ret;
	struct decon_win *win = info->par;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = dma_buf_mmap(win->dma_buf_data[0].dma_buf, vma, 0);

	return ret;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(decon_mmap);

static void decon_fb_missing_pixclock(struct decon_fb_videomode *win_mode,
		enum decon_psr_mode mode)
{
	u64 pixclk = 1000000000000ULL;
	u32 div;
	u32 width, height;

	if (mode == DECON_MIPI_COMMAND_MODE) {
		width = win_mode->videomode.xres;
		height = win_mode->videomode.yres;
	} else {
		width  = win_mode->videomode.left_margin + win_mode->videomode.hsync_len +
			win_mode->videomode.right_margin + win_mode->videomode.xres;
		height = win_mode->videomode.upper_margin + win_mode->videomode.vsync_len +
			win_mode->videomode.lower_margin + win_mode->videomode.yres;
	}

	div = width * height * (win_mode->videomode.refresh ? : 60);

	do_div(pixclk, div);
	win_mode->videomode.pixclock = pixclk;
}

static void decon_parse_lcd_info(struct decon_device *decon)
{
	int i;
	struct decon_lcd *lcd_info = decon->lcd_info;

	for (i = 0; i < decon->pdata->max_win; i++) {
		decon->windows[i]->win_mode.videomode.left_margin = lcd_info->hbp;
		decon->windows[i]->win_mode.videomode.right_margin = lcd_info->hfp;
		decon->windows[i]->win_mode.videomode.upper_margin = lcd_info->vbp;
		decon->windows[i]->win_mode.videomode.lower_margin = lcd_info->vfp;
		decon->windows[i]->win_mode.videomode.hsync_len = lcd_info->hsa;
		decon->windows[i]->win_mode.videomode.vsync_len = lcd_info->vsa;
		decon->windows[i]->win_mode.videomode.xres = lcd_info->xres;
		decon->windows[i]->win_mode.videomode.yres = lcd_info->yres;
		decon->windows[i]->win_mode.width = lcd_info->width;
		decon->windows[i]->win_mode.height = lcd_info->height;
	}
}

int decon_int_set_lcd_config(struct decon_device *decon)
{
	struct fb_info *fbinfo;
	struct decon_fb_videomode *win_mode;
	int i, ret = 0;

	decon_parse_lcd_info(decon);
	for (i = 0; i < decon->pdata->max_win; i++) {
		if (!decon->windows[i])
			continue;

		win_mode = &decon->windows[i]->win_mode;
		if (!win_mode->videomode.pixclock)
			decon_fb_missing_pixclock(win_mode, decon->pdata->psr_mode);

		fbinfo = decon->windows[i]->fbinfo;
		fb_videomode_to_var(&fbinfo->var, &win_mode->videomode);

		fbinfo->fix.type	= FB_TYPE_PACKED_PIXELS;
		fbinfo->fix.accel	= FB_ACCEL_NONE;
		fbinfo->fix.line_length = fb_linelength(fbinfo->var.xres_virtual,
					fbinfo->var.bits_per_pixel);
		fbinfo->var.activate	= FB_ACTIVATE_NOW;
		fbinfo->var.vmode	= FB_VMODE_NONINTERLACED;
		fbinfo->var.bits_per_pixel = LCD_DEFAULT_BPP;
		fbinfo->var.width	= win_mode->width;
		fbinfo->var.height	= win_mode->height;

		ret = decon_check_var(&fbinfo->var, fbinfo);
		if (ret)
			return ret;
		decon_dbg("window[%d] verified parameters\n", i);
	}

	return ret;
}

irqreturn_t decon_fb_isr_for_eint(int irq, void *dev_id)
{
	struct decon_device *decon = dev_id;
	ktime_t timestamp = ktime_get();

	spin_lock(&decon->slock);

	if (decon->pdata->trig_mode == DECON_SW_TRIG) {
		decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
				decon->pdata->trig_mode, DECON_TRIG_ENABLE);
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
		if (v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_ENABLE, NULL))
			decon_err("Failed to call DSIM packet go enable!\n");
#endif
	}

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
#ifdef CONFIG_DECON_LPD_DISPLAY
	if (decon->state == DECON_STATE_ON)
#endif
		if (v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_PKT_GO_READY, NULL))
			decon_err("Failed to call DSIM packet go ready!\n");
#endif

	decon->vsync_info.timestamp = timestamp;
	wake_up_interruptible_all(&decon->vsync_info.wait);

#ifdef CONFIG_DECON_LPD_DISPLAY
	if (decon->state == DECON_STATE_ON) {
		if (decon_lpd_enter_cond(decon) && lpd_enable) {
			decon_lpd_trig_reset(decon);
			queue_work(decon->lpd_wq, &decon->lpd_work);
		}
	}
#endif

	spin_unlock(&decon->slock);


	return IRQ_HANDLED;
}

int decon_int_register_irq(struct platform_device *pdev, struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct resource *res;
	int ret = 0;

	/* Get IRQ resource and register IRQ handler. */
	/* 0: FIFO irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	ret = devm_request_irq(dev, res->start, decon_int_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	if ((decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE) &&
			(decon->pdata->trig_mode == DECON_SW_TRIG)) {
		/* 1: frame irq */
		res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
		ret = devm_request_irq(dev, res->start, decon_int_irq_handler,
				0, pdev->name, decon);
		if (ret) {
			decon_err("failed to install irq\n");
			return ret;
		}
	} else if (decon->pdata->psr_mode == DECON_VIDEO_MODE) {
		/* 2: i80 irq */
		res = platform_get_resource(pdev, IORESOURCE_IRQ, 2);
		ret = devm_request_irq(dev, res->start, decon_int_irq_handler,
				0, pdev->name, decon);
		if (ret) {
			decon_err("failed to install irq\n");
			return ret;
		}
	}

	if (underrun_filter_status++ == UNDERRUN_FILTER_INIT)
		INIT_DELAYED_WORK(&underrun_filter_work,
				underrun_filter_handler);

	return ret;
}

int decon_fb_config_eint_for_te(struct platform_device *pdev, struct decon_device *decon)
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
	decon->irq = gpio;
	ret = devm_request_irq(dev, gpio, decon_fb_isr_for_eint,
			  IRQF_TRIGGER_RISING, pdev->name, decon);

	return ret;
}

static int decon_wait_for_vsync_thread(void *data)
{
	struct decon_device *decon = data;

	while (!kthread_should_stop()) {
		ktime_t timestamp = decon->vsync_info.timestamp;
		int ret = wait_event_interruptible(decon->vsync_info.wait,
			!ktime_equal(timestamp, decon->vsync_info.timestamp) &&
			decon->vsync_info.active);

		if (!ret) {
			sysfs_notify(&decon->dev->kobj, NULL, "vsync");
		}
	}

	return 0;
}

static ssize_t decon_vsync_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%llu\n",
			ktime_to_ns(decon->vsync_info.timestamp));
}

static DEVICE_ATTR(vsync, S_IRUGO, decon_vsync_show, NULL);

static ssize_t decon_psr_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%d\n", decon->pdata->psr_mode);
}

static DEVICE_ATTR(psr_info, S_IRUGO, decon_psr_info, NULL);

int decon_int_create_vsync_thread(struct decon_device *decon)
{
	int ret = 0;

	ret = device_create_file(decon->dev, &dev_attr_vsync);
	if (ret) {
		decon_err("failed to create vsync file\n");
		return ret;
	}

	decon->vsync_info.thread = kthread_run(decon_wait_for_vsync_thread,
			decon, "s3c-fb-vsync");
	if (decon->vsync_info.thread == ERR_PTR(-ENOMEM)) {
		decon_err("failed to run vsync thread\n");
		decon->vsync_info.thread = NULL;
	}

	return ret;
}

int decon_int_create_psr_thread(struct decon_device *decon)
{
	int ret = 0;

	ret = device_create_file(decon->dev, &dev_attr_psr_info);
	if (ret) {
		decon_err("failed to create psr info file\n");
		return ret;
	}

	return ret;
}

void decon_int_destroy_vsync_thread(struct decon_device *decon)
{
	device_remove_file(decon->dev, &dev_attr_vsync);
}

void decon_int_destroy_psr_thread(struct decon_device *decon)
{
	device_remove_file(decon->dev, &dev_attr_psr_info);
}

/************* LPD funtions ************************/
u32 decon_reg_get_cam_status(void __iomem *cam_status)
{
	if (cam_status)
		return readl(cam_status);
	else
		return 0xF;
}

static int decon_enter_lpd(struct decon_device *decon)
{
	int ret = 0;

#ifdef CONFIG_DECON_LPD_DISPLAY
	if (decon->id)
		return ret;

	mutex_lock(&decon->lpd_lock);

	if (decon_is_lpd_blocked(decon))
		goto err2;

	decon_lpd_block(decon);
	if ((decon->state == DECON_STATE_LPD) ||
		(decon->state != DECON_STATE_ON)) {
		goto err;
	}

#ifdef CONFIG_DECON_PM_QOS_REQUESTS
	decon_update_qos_requests(decon, NULL, true);
#endif

	exynos_ss_printk("%s +\n", __func__);
	disable_irq(decon->irq);
	decon->state = DECON_STATE_LPD_ENT_REQ;
	decon_disable(decon);
	decon->state = DECON_STATE_LPD;
	exynos_ss_printk("%s -\n", __func__);

err:
	decon_lpd_unblock(decon);
err2:
	decon_lpd_trig_reset(decon);
	mutex_unlock(&decon->lpd_lock);
#endif
	return ret;
}

int decon_exit_lpd(struct decon_device *decon)
{
	int ret = 0;

#ifdef CONFIG_DECON_LPD_DISPLAY
	if (decon->id)
		return ret;

	decon_lpd_block(decon);
	flush_workqueue(decon->lpd_wq);
	mutex_lock(&decon->lpd_lock);

	if (decon->state != DECON_STATE_LPD) {
		goto err;
	}

	exynos_ss_printk("%s +\n", __func__);
	decon->state = DECON_STATE_LPD_EXIT_REQ;
	decon_enable(decon);
	enable_irq(decon->irq);
	decon->state = DECON_STATE_ON;
	exynos_ss_printk("%s -\n", __func__);
err:
	decon_lpd_unblock(decon);
	decon_lpd_trig_reset(decon);
	mutex_unlock(&decon->lpd_lock);
#endif
	return ret;
}

/*
 * This function will be called only when system is ready to interact
 * with driver. Without this check, driver goes to LPD mode before kernel
 * boot completes and CCF (Common Clock Framework) will try to access the
 * unused clock gate registers. Accessing the DISP CMU registers while the
 * pd_disp is off makes the system crash.
 */
void decon_lpd_enable(void)
{
	lpd_enable = true;
}

int decon_lpd_block_exit(struct decon_device *decon)
{
	int ret = 0;

	if (decon->id)
		return ret;

	decon_lpd_trig_reset(decon);
	decon_lpd_block(decon);
	ret = decon_exit_lpd(decon);

	return ret;
}

#ifdef DECON_LPD_OPT
int decon_lcd_off(struct decon_device *decon)
{
	/* It cann't be used incase of PACKET_GO mode */
	int ret;

	decon_info("%s +\n", __func__);

	decon_lpd_block(decon);
	flush_workqueue(decon->lpd_wq);

	mutex_lock(&decon->lpd_lock);

	ret = v4l2_subdev_call(decon->output_sd, core, ioctl, DSIM_IOC_LCD_OFF, NULL);
	if (ret < 0) {
		decon_err("failed to turn off LCD\n");
	}

	decon->state = DECON_STATE_OFF;

	mutex_unlock(&decon->lpd_lock);
	decon_lpd_unblock(decon);

	decon_info("%s -\n", __func__);

	return 0;
}
#endif

static void decon_int_lpd_handler(struct work_struct *work)
{
	struct decon_device *decon =
			container_of(work, struct decon_device, lpd_work);

	decon_enter_lpd(decon);
}

int decon_int_register_lpd_work(struct decon_device *decon)
{
	mutex_init(&decon->lpd_lock);

	atomic_set(&decon->lpd_trig_cnt, 0);
	atomic_set(&decon->lpd_block_cnt, 0);

	decon->lpd_wq = create_singlethread_workqueue("decon_lpd");
	if (decon->lpd_wq == NULL) {
		decon_err("%s:failed to create workqueue for LPD\n", __func__);
		return -ENOMEM;
	}

	INIT_WORK(&decon->lpd_work, decon_int_lpd_handler);

	return 0;
}
