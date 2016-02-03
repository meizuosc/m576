/* linux/drivers/video/decon_display/decon_reg_5430.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Sewoon Park <seuni.park@samsung.com>
 *
 * Sewoon Park <seuni.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "decon_reg.h"
#include "decon_mipi_dsi.h"

/******************* CAL raw functions implementation *************************/
int decon_reg_reset(void)
{
	int tries;

	decon_write(VIDCON0, VIDCON0_SWRESET);
	for (tries = 2000; tries; --tries) {
		if (~decon_read(VIDCON0) & VIDCON0_SWRESET)
			break;
		udelay(10);
	}

	if (!tries) {
		decon_err("failed to reset Decon\n");
		return -EBUSY;
	}

	return 0;
}

void decon_reg_set_clkgate_mode(u32 en)
{
	u32 val = en ? ~0 : 0;

	decon_write_mask(DECON_CMU, val, DECON_CMU_ALL_CLKGATE_ENABLE);
}

void decon_reg_blend_alpha_bits(u32 alpha_bits)
{
	decon_write(BLENDCON, alpha_bits);
}

void decon_reg_set_vidout(enum s3c_fb_psr_mode mode, u32 en)
{
	if (mode == S3C_FB_MIPI_COMMAND_MODE)
		decon_write_mask(VIDOUTCON0, VIDOUTCON0_I80IF_F, VIDOUTCON0_IF_MASK);
	else
		decon_write_mask(VIDOUTCON0, VIDOUTCON0_RGBIF_F, VIDOUTCON0_IF_MASK);

	decon_write_mask(VIDOUTCON0, en ? ~0 : 0, VIDOUTCON0_LCD_ON_F);
}

void decon_reg_set_crc(u32 en)
{
	u32 val = en ? ~0 : 0;

	decon_write_mask(CRCCTRL, val, CRCCTRL_CRCCLKEN | CRCCTRL_CRCEN);
}

void decon_reg_set_fixvclk(enum decon_hold_scheme mode)
{
	u32 val = VIDCON1_VCLK_HOLD;

	switch (mode) {
	case DECON_VCLK_HOLD:
		val = VIDCON1_VCLK_HOLD;
		break;
	case DECON_VCLK_RUNNING:
		val = VIDCON1_VCLK_RUN;
		break;
	case DECON_VCLK_RUN_VDEN_DISABLE:
		val = VIDCON1_VCLK_RUN_VDEN_DISABLE;
		break;
	}

	decon_write_mask(VIDCON1, val, VIDCON1_VCLK_MASK);
}

void decon_reg_clear_win(int win)
{
	decon_write(WINCON(win), 0);
	decon_write(VIDOSD_A(win), 0);
	decon_write(VIDOSD_B(win), 0);
	decon_write(VIDOSD_C(win), 0);
	decon_write(VIDOSD_D(win), 0);
}

void decon_reg_set_rgb_order(enum decon_rgb_order order)
{
	u32 val = VIDCON2_RGB_ORDER_O_RGB;

	switch (order) {
	case DECON_RGB:
		val = VIDCON2_RGB_ORDER_O_RGB;
		break;
	case DECON_GBR:
		val = VIDCON2_RGB_ORDER_O_GBR;
		break;
	case DECON_BRG:
		val = VIDCON2_RGB_ORDER_O_BRG;
		break;
	case DECON_BGR:
		val = VIDCON2_RGB_ORDER_O_BGR;
		break;
	case DECON_RBG:
		val = VIDCON2_RGB_ORDER_O_RBG;
		break;
	case DECON_GRB:
		val = VIDCON2_RGB_ORDER_O_GRB;
		break;
	}

	decon_write_mask(VIDCON2, val, VIDCON2_RGB_ORDER_O_MASK);
}

void decon_reg_set_porch(struct decon_lcd *info)
{
	u32 val = 0;

#if defined(CONFIG_SOC_EXYNOS5433)
	val = VIDTCON00_VBPD(info->vbp - 1) | VIDTCON00_VFPD(info->vfp - 1);
	decon_write(VIDTCON00, val);

	val = VIDTCON01_VSPW(info->vsa - 1);
	decon_write(VIDTCON01, val);

	val = VIDTCON10_HBPD(info->hbp - 1) | VIDTCON10_HFPD(info->hfp - 1);
	decon_write(VIDTCON10, val);

	val = VIDTCON11_HSPW(info->hsa - 1);
	decon_write(VIDTCON11, val);
#else
	val = VIDTCON0_VBPD(info->vbp - 1) | VIDTCON0_VFPD(info->vfp - 1) |
		VIDTCON0_VSPW(info->vsa - 1);
	decon_write(VIDTCON0, val);

	val = VIDTCON1_HBPD(info->hbp - 1) | VIDTCON1_HFPD(info->hfp - 1) |
		VIDTCON1_HSPW(info->hsa - 1);
	decon_write(VIDTCON1, val);
#endif

	val = VIDTCON2_LINEVAL(info->yres - 1) | VIDTCON2_HOZVAL(info->xres - 1);
	decon_write(VIDTCON2, val);
}

void decon_reg_set_linecnt_op_threshold(u32 th)
{
	decon_write(LINECNT_OP_THRESHOLD, th);
}

void decon_reg_set_clkval(u32 clkdiv)
{
	u32 val;

	decon_write_mask(VIDCON0, ~0, VIDCON0_CLKVALUP);

	val = (clkdiv >= 1) ? VIDCON0_CLKVAL_F(clkdiv - 1) : 0;
	decon_write_mask(VIDCON0, val, VIDCON0_CLKVAL_F_MASK);
}

void decon_reg_direct_on_off(u32 en)
{
	u32 val = en ? ~0 : 0;

	decon_write_mask(VIDCON0, val, VIDCON0_ENVID_F | VIDCON0_ENVID);
}

void decon_reg_per_frame_off(void)
{
	decon_write_mask(VIDCON0, 0, VIDCON0_ENVID_F);
}

void decon_reg_set_freerun_mode(u32 en)
{
	decon_write_mask(VIDCON0, en ? ~0 : 0, VIDCON0_VLCKFREE);
}

void decon_reg_update_standalone(void)
{
	decon_write_mask(DECON_UPDATE, ~0, DECON_UPDATE_STANDALONE_F);
}

void decon_reg_configure_lcd(struct decon_lcd *lcd_info)
{
	decon_reg_set_rgb_order(DECON_RGB);
	decon_reg_set_porch(lcd_info);

	if (lcd_info->mode == VIDEO_MODE)
		decon_reg_set_linecnt_op_threshold(lcd_info->yres - 1);

	decon_reg_set_clkval(0);

	decon_reg_set_freerun_mode(1);
	decon_reg_direct_on_off(0);
	decon_reg_update_standalone();
}

void decon_reg_configure_trigger(enum decon_trig_mode mode)
{
	u32 val, mask;

	mask = TRIGCON_SWTRIGEN_I80_RGB | TRIGCON_HWTRIGEN_I80_RGB |
		TRIGCON_HWTRIGMASK_I80_RGB | TRIGCON_TRIGEN_PER_I80_RGB_F |
		TRIGCON_TRIGEN_I80_RGB_F;

	val = TRIGCON_TRIGEN_PER_I80_RGB_F | TRIGCON_TRIGEN_I80_RGB_F;

	if (mode == DECON_SW_TRIG)
		val |= TRIGCON_SWTRIGEN_I80_RGB;
	else
		val |= TRIGCON_HWTRIGEN_I80_RGB;

	decon_write_mask(TRIGCON, val, mask);
}

void decon_reg_set_winmap(u32 idx, u32 color, u32 en)
{
	u32 val = en ? WINxMAP_MAP : 0;

	val |= WINxMAP_MAP_COLOUR(color);
	decon_write_mask(WINxMAP(idx), val, WINxMAP_MAP | WINxMAP_MAP_COLOUR_MASK);
}

u32 decon_reg_get_linecnt(void)
{
	return VIDCON1_LINECNT_GET(decon_read(VIDCON1));
}

u32 decon_reg_get_vstatus(void)
{
	return decon_read(VIDCON1) & VIDCON1_VSTATUS_MASK;
}

/* timeout : usec */
int decon_reg_wait_linecnt_is_zero_timeout(unsigned long timeout)
{
	unsigned long delay_time = 10;
	unsigned long cnt = timeout / delay_time;
	u32 linecnt, vstatus;

	do {
		linecnt = decon_reg_get_linecnt();
		if (!linecnt) {
			vstatus = decon_reg_get_vstatus();
			if ((vstatus != VIDCON1_VSTATUS_BACKPORCH) && (vstatus != VIDCON1_VSTATUS_ACTIVE))
				break;
		}
		cnt--;
		udelay(delay_time);
	} while (cnt);

	if (!cnt) {
		decon_err("wait timeout linecount is zero(%u)\n", linecnt);
		return -EBUSY;
	}

	return 0;
}

u32 decon_reg_get_stop_status(void)
{
	u32 val;

	val = decon_read(VIDCON0);
	if (val & VIDCON0_DECON_STOP_STATUS)
		return 1;

	return 0;
}

int decon_reg_wait_stop_status_timeout(unsigned long timeout)
{
	unsigned long delay_time = 10;
	unsigned long cnt = timeout / delay_time;
	u32 status;

	do {
		status = decon_reg_get_stop_status();
		cnt--;
		udelay(delay_time);
	} while (status && cnt);

	if (!cnt) {
		decon_err("wait timeout decon stop status(%u)\n", status);
		return -EBUSY;
	}

	return 0;
}

int decon_reg_is_win_enabled(int idx)
{
	if (decon_read(WINCON(idx)) & WINCONx_ENWIN)
		return 1;

	return 0;
}

/***************** CAL APIs implementation *******************/

void decon_reg_init(struct decon_init_param *p)
{
	int win_no;
	struct decon_lcd *lcd_info = p->lcd_info;
	struct decon_psr_info *psr = &p->psr;

	decon_reg_reset();
	decon_reg_set_clkgate_mode(1);
	decon_reg_blend_alpha_bits(BLENDCON_NEW_8BIT_ALPHA_VALUE);
	decon_reg_set_vidout(psr->psr_mode, 1);
	decon_reg_set_crc(1);

	if (psr->psr_mode == S3C_FB_MIPI_COMMAND_MODE)
		decon_reg_set_fixvclk(DECON_VCLK_RUN_VDEN_DISABLE);
	else
		decon_reg_set_fixvclk(DECON_VCLK_HOLD);

	for (win_no = 0; win_no < p->nr_windows; win_no++)
		decon_reg_clear_win(win_no);

	/* RGB order -> porch values -> LINECNT_OP_THRESHOLD -> clock divider
	 * -> freerun mode --> stop DECON */
	decon_reg_configure_lcd(lcd_info);

	if (psr->psr_mode == S3C_FB_MIPI_COMMAND_MODE)
		decon_reg_configure_trigger(psr->trig_mode);

	decon_reg_set_sys_reg();
}

void decon_reg_init_probe(struct decon_init_param *p)
{
	struct decon_lcd *lcd_info = p->lcd_info;
	struct decon_psr_info *psr = &p->psr;

	decon_reg_set_clkgate_mode(1);
	decon_reg_blend_alpha_bits(BLENDCON_NEW_8BIT_ALPHA_VALUE);
	decon_reg_set_vidout(psr->psr_mode, 1);

	if (psr->psr_mode == S3C_FB_MIPI_COMMAND_MODE)
		decon_reg_set_fixvclk(DECON_VCLK_RUN_VDEN_DISABLE);
	else
		decon_reg_set_fixvclk(DECON_VCLK_HOLD);

	/* RGB order -> porch values -> LINECNT_OP_THRESHOLD -> clock divider
	 * -> freerun mode --> stop DECON */
	decon_reg_set_rgb_order(DECON_RGB);
	decon_reg_set_porch(lcd_info);

	if (lcd_info->mode == VIDEO_MODE)
		decon_reg_set_linecnt_op_threshold(lcd_info->yres - 1);

	decon_reg_set_clkval(0);

	decon_reg_set_freerun_mode(1);
	decon_reg_update_standalone();

	if (psr->psr_mode == S3C_FB_MIPI_COMMAND_MODE)
		decon_reg_configure_trigger(psr->trig_mode);
}

void decon_reg_start(struct decon_psr_info *psr)
{
	decon_reg_direct_on_off(1);

	decon_reg_update_standalone();

	if ((psr->psr_mode == S3C_FB_MIPI_COMMAND_MODE) &&
			(psr->trig_mode == DECON_HW_TRIG))
		decon_reg_set_trigger(psr->trig_mode, DECON_TRIG_ENABLE);
}

int decon_reg_stop(struct decon_psr_info *psr)
{
	int ret = 0;

	if ((psr->psr_mode == S3C_FB_MIPI_COMMAND_MODE) &&
			(psr->trig_mode == DECON_HW_TRIG)) {
		decon_reg_set_trigger(psr->trig_mode, DECON_TRIG_DISABLE);
	}

	/* timeout : 200ms */
	ret = decon_reg_wait_linecnt_is_zero_timeout(2000 * 100);
	if (ret)
		goto err;

	if (psr->psr_mode == S3C_FB_MIPI_COMMAND_MODE)
		decon_reg_direct_on_off(0);
	else
		decon_reg_per_frame_off();

	/* timeout : 20ms */
	ret = decon_reg_wait_stop_status_timeout(2000 * 10);
	if (ret)
		goto err;

err:
	ret = decon_reg_reset();

	return ret;
}

void decon_reg_set_regs_data(int idx, struct decon_regs_data *regs)
{
	decon_write(WINCON(idx), regs->wincon);
	decon_write(WINxMAP(idx), regs->winmap);

	decon_write(VIDOSD_A(idx), regs->vidosd_a);
	decon_write(VIDOSD_B(idx), regs->vidosd_b);
	decon_write(VIDOSD_C(idx), regs->vidosd_c);
	decon_write(VIDOSD_D(idx), regs->vidosd_d);

	decon_write(VIDW_BUF_START(idx), regs->vidw_buf_start);
	decon_write(VIDW_BUF_END(idx), regs->vidw_buf_end);
	decon_write(VIDW_BUF_SIZE(idx), regs->vidw_buf_size);

	if (idx)
		decon_write(BLENDEQ(idx - 1), regs->blendeq);
}

/* set DECON's interrupt. It depends on mode(video mode or command mode) */
void decon_reg_set_int(struct decon_psr_info *psr, u32 en)
{
	u32 val;

	val = VIDINTCON0_INT_ENABLE | VIDINTCON0_FIFOLEVEL_EMPTY |
		VIDINTCON0_INT_FIFO | VIDINTCON0_FIFOSEL_MAIN_EN;

	if (psr->psr_mode == S3C_FB_MIPI_COMMAND_MODE)
		val |= VIDINTCON0_INT_I80_EN;
	else
		val |= VIDINTCON0_INT_FRAME | VIDINTCON0_FRAMESEL0_VSYNC;

	if (en) {
		if (psr->psr_mode == S3C_FB_MIPI_COMMAND_MODE)
			decon_write_mask(VIDINTCON1, ~0, VIDINTCON1_INT_I80);

		decon_write_mask(VIDINTCON0, val, ~0);
	} else {
		decon_write_mask(VIDINTCON0, 0, VIDINTCON0_INT_ENABLE);
	}
}

/* enable(unmask) / disable(mask) hw trigger */
void decon_reg_set_trigger(enum decon_trig_mode trig, enum decon_set_trig en)
{
	u32 val = (en == DECON_TRIG_ENABLE) ? ~0 : 0;
	u32 reg_id = TRIGCON_HWTRIGMASK_I80_RGB;

	if (trig == DECON_SW_TRIG)
		reg_id = TRIGCON_SWTRIGCMD_I80_RGB;

	decon_write_mask(TRIGCON, val, reg_id);
}

/* wait until shadow update is finished */
int decon_reg_wait_for_update_timeout(unsigned long timeout)
{
	unsigned long delay_time = 100;
	unsigned long cnt = timeout / delay_time;

	while ((decon_read(DECON_UPDATE) & DECON_UPDATE_STANDALONE_F) && cnt--)
		udelay(delay_time);

	if (!cnt) {
		decon_err("timeout of updating decon registers\n");
		return -EBUSY;
	}

	return 0;
}

/* prohibit shadow update during writing something to SFR */
void decon_reg_shadow_protect_win(u32 idx, u32 protect)
{
	u32 val = protect ? ~0 : 0;

	decon_write_mask(SHADOWCON, val, SHADOWCON_WINx_PROTECT(idx));
}

/* enable each window */
void decon_reg_activate_window(u32 index)
{
	decon_write_mask(WINCON(index), ~0, WINCONx_ENWIN);
	decon_reg_update_standalone();
}

void decon_reg_set_sys_reg(void)
{
#if defined(CONFIG_FB_I80_HW_TRIGGER) && defined(CONFIG_SOC_EXYNOS5433)
	u32 data;
	struct display_sysreg *sysreg_va;

	sysreg_va = decon_get_sysreg();

	if (sysreg_va && sysreg_va->disp_sysreg[0]) {
		data = readl(sysreg_va->disp_sysreg[0]);
		data |= (1 << 13);
		writel(data, sysreg_va->disp_sysreg[0]);
	}

	return;
#endif
}
