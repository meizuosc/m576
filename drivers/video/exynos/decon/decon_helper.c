/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Helper file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <asm/cacheflush.h>
#include <asm/page.h>

#include "decon.h"
#include "dsim.h"
#include "./vpp/vpp_core.h"
#include "decon_helper.h"
#include "./panels/lcd_ctrl.h"
#include <video/mipi_display.h>

int decon_clk_set_parent(struct device *dev, const char *child, const char *parent)
{
	struct clk *p;
	struct clk *c;

	p = clk_get(dev, parent);
	if (IS_ERR_OR_NULL(p)) {
		decon_err("%s: couldn't get clock : %s\n", __func__, parent);
		return -ENODEV;
	}

	c = clk_get(dev, child);
	if (IS_ERR_OR_NULL(c)) {
		decon_err("%s: couldn't get clock : %s\n", __func__, child);
		return -ENODEV;
	}

	clk_set_parent(c, p);
	clk_put(p);
	clk_put(c);

	return 0;
}

int decon_clk_set_rate(struct device *dev, const char *conid, unsigned int rate)
{
	struct clk *target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		decon_err("%s: couldn't get clock : %s\n", __func__, conid);
		return -ENODEV;
	}

	clk_set_rate(target, rate);
	clk_put(target);

	return 0;
}

unsigned long decon_clk_get_rate(struct device *dev, const char *clkid)
{
	struct clk *target;
	unsigned long rate;

	target = clk_get(dev, clkid);
	if (IS_ERR_OR_NULL(target)) {
		decon_err("%s: couldn't get clock : %s\n", __func__, clkid);
		return -ENODEV;
	}

	rate = clk_get_rate(target);
	clk_put(target);

	return rate;
}

void decon_to_psr_info(struct decon_device *decon, struct decon_psr_info *psr)
{
	psr->psr_mode = decon->pdata->psr_mode;
	psr->trig_mode = decon->pdata->trig_mode;
	psr->out_type = decon->out_type;
}

void decon_to_init_param(struct decon_device *decon, struct decon_init_param *p)
{
	struct decon_lcd *lcd_info = decon->lcd_info;
	struct v4l2_mbus_framefmt mbus_fmt;
	int ret = 0;

	mbus_fmt.width = 0;
	mbus_fmt.height = 0;
	mbus_fmt.code = 0;
	mbus_fmt.field = 0;
	mbus_fmt.colorspace = 0;

	if (decon->out_type == DECON_OUT_HDMI) {
		ret = v4l2_subdev_call(decon->output_sd, video, g_mbus_fmt,
								&mbus_fmt);
		if (ret)
			decon_warn("failed to get mbus_fmt for hdmi\n");
		p->lcd_info = find_porch(mbus_fmt);
		decon_info("find porch %dx%d@%dHz\n", p->lcd_info->xres,
				p->lcd_info->yres, p->lcd_info->fps);
	} else {
		p->lcd_info = lcd_info;
	}
	decon->lcd_info = p->lcd_info;
	p->psr.psr_mode = decon->pdata->psr_mode;
	p->psr.trig_mode = decon->pdata->trig_mode;
	p->psr.out_type = decon->out_type;
	p->nr_windows = decon->pdata->max_win;
}

/**
* ----- APIs for DISPLAY_SUBSYSTEM_EVENT_LOG -----
*/
/* ===== STATIC APIs ===== */

#ifdef CONFIG_DECON_EVENT_LOG
/* logging a event related with DECON */
static inline void disp_ss_event_log_decon
	(disp_ss_event_t type, struct v4l2_subdev *sd, ktime_t time)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	int idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log = &decon->disp_ss_log[idx];

	if (time.tv64)
		log->time = time;
	else
		log->time = ktime_get();
	log->type = type;

	switch (type) {
	case DISP_EVT_DECON_SUSPEND:
	case DISP_EVT_DECON_RESUME:
	case DISP_EVT_ENTER_LPD:
	case DISP_EVT_EXIT_LPD:
		log->data.pm.pm_status = pm_runtime_active(decon->dev);
		log->data.pm.elapsed = ktime_sub(ktime_get(), log->time);
		break;
	case DISP_EVT_WB_SET_BUFFER:
	case DISP_EVT_WB_SW_TRIGGER:
	case DISP_EVT_WB_TIMELINE_INC:
	case DISP_EVT_WB_FRAME_DONE:
		log->data.frame.timeline = decon->wb_timeline->value;
		log->data.frame.timeline_max = decon->wb_timeline_max;
		break;
	case DISP_EVT_TE_INTERRUPT:
	case DISP_EVT_UNDERRUN:
	case DISP_EVT_LINECNT_ZERO:
		break;
	default:
		/* Any remaining types will be log just time and type */
		break;
	}
}

/* logging a event related with DSIM */
static inline void disp_ss_event_log_dsim
	(disp_ss_event_t type, struct v4l2_subdev *sd, ktime_t time)
{
	struct dsim_device *dsim = container_of(sd, struct dsim_device, sd);
	struct decon_device *decon = get_decon_drvdata(dsim->id);
	int idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log = &decon->disp_ss_log[idx];

	if (time.tv64)
		log->time = time;
	else
		log->time = ktime_get();
	log->type = type;

	switch (type) {
	case DISP_EVT_DSIM_SUSPEND:
	case DISP_EVT_DSIM_RESUME:
	case DISP_EVT_ENTER_ULPS:
	case DISP_EVT_EXIT_ULPS:
		log->data.pm.pm_status = pm_runtime_active(dsim->dev);
		log->data.pm.elapsed = ktime_sub(ktime_get(), log->time);
		break;
	default:
		/* Any remaining types will be log just time and type */
		break;
	}
}

/* get decon's id used by vpp */
static int __get_decon_id_for_vpp(struct v4l2_subdev *sd)
{
	struct decon_device *decon = get_decon_drvdata(0);
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);

	return decon->vpp_used[vpp->id]? 0 : 1;
}

/* logging a event related with VPP */
static inline void disp_ss_event_log_vpp
	(disp_ss_event_t type, struct v4l2_subdev *sd, ktime_t time)
{
	struct decon_device *decon = get_decon_drvdata(__get_decon_id_for_vpp(sd));
	int idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log = &decon->disp_ss_log[idx];
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);

	if (time.tv64)
		log->time = time;
	else
		log->time = ktime_get();
	log->type = type;

	switch (type) {
	case DISP_EVT_VPP_SUSPEND:
	case DISP_EVT_VPP_RESUME:
		log->data.pm.pm_status = pm_runtime_active(&vpp->pdev->dev);
		log->data.pm.elapsed = ktime_sub(ktime_get(), log->time);
		break;
	case DISP_EVT_VPP_FRAMEDONE:
	case DISP_EVT_VPP_STOP:
	case DISP_EVT_VPP_WINCON:
		log->data.vpp.id = vpp->id;
		log->data.vpp.start_cnt = vpp->start_count;
		log->data.vpp.done_cnt = vpp->done_count;
		break;
	default:
		log->data.vpp.id = vpp->id;
		break;
	}

	return;
}

/* If event are happend continuously, then ignore */
static bool disp_ss_event_ignore
	(disp_ss_event_t type, struct decon_device *decon)
{
	int latest = atomic_read(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log;
	int idx;

	/* Seek a oldest from current index */
	idx = (latest + DISP_EVENT_LOG_MAX - DECON_ENTER_LPD_CNT) % DISP_EVENT_LOG_MAX;
	do {
		if (++idx >= DISP_EVENT_LOG_MAX)
			idx = 0;

		log = &decon->disp_ss_log[idx];
		if (log->type != type)
			return false;
	} while (latest != idx);

	return true;
}

/* ===== EXTERN APIs ===== */
/* Common API to log a event related with DECON/DSIM/VPP */
void DISP_SS_EVENT_LOG(disp_ss_event_t type, struct v4l2_subdev *sd, ktime_t time)
{
	struct decon_device *decon = get_decon_drvdata(0);

	if (!decon || IS_ERR_OR_NULL(decon->debug_event))
		return;

	/* log a eventy softly */
	switch (type) {
	case DISP_EVT_TE_INTERRUPT:
	case DISP_EVT_UNDERRUN:
		/* If occurs continuously, skipped. It is a burden */
		if (disp_ss_event_ignore(type, decon))
			break;
	case DISP_EVT_BLANK:
	case DISP_EVT_UNBLANK:
	case DISP_EVT_ENTER_LPD:
	case DISP_EVT_EXIT_LPD:
	case DISP_EVT_DECON_SUSPEND:
	case DISP_EVT_DECON_RESUME:
	case DISP_EVT_LINECNT_ZERO:
	case DISP_EVT_TRIG_MASK:
	case DISP_EVT_DECON_FRAMEDONE:
	case DISP_EVT_DECON_FRAMEDONE_WAIT:
	case DISP_EVT_WB_SET_BUFFER:
	case DISP_EVT_WB_SW_TRIGGER:
	case DISP_EVT_WB_TIMELINE_INC:
	case DISP_EVT_WB_FRAME_DONE:
		disp_ss_event_log_decon(type, sd, time);
		break;
	case DISP_EVT_DSIM_FRAMEDONE:
	case DISP_EVT_ENTER_ULPS:
	case DISP_EVT_EXIT_ULPS:
		disp_ss_event_log_dsim(type, sd, time);
		break;
	case DISP_EVT_VPP_FRAMEDONE:
	case DISP_EVT_VPP_STOP:
	case DISP_EVT_VPP_WINCON:
	case DISP_EVT_VPP_UPDATE_DONE:
	case DISP_EVT_VPP_SHADOW_UPDATE:
		disp_ss_event_log_vpp(type, sd, time);
		break;
	default:
		break;
	}

	if (decon->disp_ss_log_level == DISP_EVENT_LEVEL_LOW)
		return;

	/* additionally logging hardly */
	switch (type) {
	case DISP_EVT_ACT_VSYNC:
	case DISP_EVT_DEACT_VSYNC:
	case DISP_EVT_WIN_CONFIG:
		disp_ss_event_log_decon(type, sd, time);
		break;
	case DISP_EVT_DSIM_SUSPEND:
	case DISP_EVT_DSIM_RESUME:
		disp_ss_event_log_dsim(type, sd, time);
		break;
	case DISP_EVT_VPP_SUSPEND:
	case DISP_EVT_VPP_RESUME:
		disp_ss_event_log_vpp(type, sd, time);
	default:
		break;
	}
}

void DISP_SS_EVENT_LOG_WINCON(struct v4l2_subdev *sd, struct decon_reg_data *regs)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	int idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log = &decon->disp_ss_log[idx];
	int win = 0;

	log->time = ktime_get();
	log->type = DISP_EVT_UPDATE_HANDLER;

	memset(&log->data.reg, 0, sizeof(struct decon_update_reg_data));
	log->data.reg.bw = regs->bw;
	log->data.reg.int_bw = regs->int_bw;
	log->data.reg.disp_bw = regs->disp_bw;

	for (win = 0; win < MAX_DECON_WIN; win++) {
		if (regs->wincon[win] & WINCON_ENWIN) {
			log->data.reg.wincon[win] = regs->wincon[win];
			log->data.reg.offset_x[win] = regs->offset_x[win];
			log->data.reg.offset_y[win] = regs->offset_y[win];
			log->data.reg.whole_w[win] = regs->whole_w[win];
			log->data.reg.whole_h[win] = regs->whole_h[win];
			log->data.reg.vidosd_a[win] = regs->vidosd_a[win];
			log->data.reg.vidosd_b[win] = regs->vidosd_b[win];
			memcpy(&log->data.reg.win_config[win], &regs->vpp_config[win],
				sizeof(struct decon_win_config));
		}
	}

#ifdef CONFIG_FB_WINDOW_UPDATE
	if ((regs->need_update) ||
		(decon->need_update && regs->update_win.w)) {
		memcpy(&log->data.reg.win, &regs->update_win,
				sizeof(struct decon_rect));
	} else {
		log->data.reg.win.x = 0;
		log->data.reg.win.y = 0;
		log->data.reg.win.w = decon->lcd_info->xres;
		log->data.reg.win.h = decon->lcd_info->yres;
	}
#else
	memset(&log->data.reg.win, 0, sizeof(struct decon_rect));
#endif
}

void DISP_SS_EVENT_LOG_S_FMT(struct v4l2_subdev *sd, struct v4l2_subdev_format *fmt)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	int idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log = &decon->disp_ss_log[idx];

	log->time = ktime_get();
	log->type = DISP_EVT_WB_SET_FORMAT;

	memset(&log->data.fmt, 0, sizeof(struct v4l2_subdev_format));
	log->data.fmt.which = fmt->which;
	log->data.fmt.pad = fmt->pad;
	log->data.fmt.format.width = fmt->format.width;
	log->data.fmt.format.height = fmt->format.height;
	log->data.fmt.format.code = fmt->format.code;
	log->data.fmt.format.field = fmt->format.field;
	log->data.fmt.format.colorspace = fmt->format.colorspace;
}

/* Common API to log a event related with DSIM COMMAND */
void DISP_SS_EVENT_LOG_CMD(struct v4l2_subdev *sd, u32 cmd_id, unsigned long data)
{
	struct dsim_device *dsim = container_of(sd, struct dsim_device, sd);
	struct decon_device *decon = get_decon_drvdata(dsim->id);
	int idx;
	struct disp_ss_log *log;

	if (!decon || IS_ERR_OR_NULL(decon->debug_event))
		return;

	idx = atomic_inc_return(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	log = &decon->disp_ss_log[idx];

	log->time = ktime_get();
	log->type = DISP_EVT_DSIM_COMMAND;
	log->data.cmd_buf.id = cmd_id;
	if (cmd_id == MIPI_DSI_DCS_LONG_WRITE)
		log->data.cmd_buf.buf = *(u8 *)(data);
	else
		log->data.cmd_buf.buf = (u8)data;
}

/* display logged events related with DECON */
void DISP_SS_EVENT_SHOW(struct seq_file *s, struct decon_device *decon)
{
	int idx = atomic_read(&decon->disp_ss_log_idx) % DISP_EVENT_LOG_MAX;
	struct disp_ss_log *log;
	int latest = idx;
	struct timeval tv;

	/* TITLE */
	seq_printf(s, "-------------------DECON%d EVENT LOGGER ----------------------\n",
			decon->id);
	seq_printf(s, "-- STATUS: LPD(%s) ", IS_ENABLED(CONFIG_DECON_LPD_DISPLAY)? "on":"off");
	seq_printf(s, "PKTGO(%s) ", IS_ENABLED(CONFIG_DECON_MIPI_DSI_PKTGO)? "on":"off");
	seq_printf(s, "BlockMode(%s) ", IS_ENABLED(CONFIG_DECON_BLOCKING_MODE)? "on":"off");
	seq_printf(s, "Window_Update(%s)\n", IS_ENABLED(CONFIG_FB_WINDOW_UPDATE)? "on":"off");
	seq_printf(s, "-------------------------------------------------------------\n");
	seq_printf(s, "%14s  %20s  %20s\n",
		"Time", "Event ID", "Remarks");
	seq_printf(s, "-------------------------------------------------------------\n");

	/* return if there is no event log */
	if (idx < 0)
		return;
	/* Seek a oldest from current index */
	idx = (idx + DISP_EVENT_LOG_MAX - DISP_EVENT_PRINT_MAX) % DISP_EVENT_LOG_MAX;

	do {
		if (++idx >= DISP_EVENT_LOG_MAX)
			idx = 0;

		/* Seek a index */
		log = &decon->disp_ss_log[idx];

		/* TIME */
		tv = ktime_to_timeval(log->time);
		seq_printf(s, "[%6ld.%06ld] ", tv.tv_sec, tv.tv_usec);

		/* If there is no timestamp, then exit directly */
		if (!tv.tv_sec)
			break;

		/* EVETN ID + Information */
		switch (log->type) {
		case DISP_EVT_BLANK:
			seq_printf(s, "%20s  %20s", "FB_BLANK", "-\n");
			break;
		case DISP_EVT_UNBLANK:
			seq_printf(s, "%20s  %20s", "FB_UNBLANK", "-\n");
			break;
		case DISP_EVT_ACT_VSYNC:
			seq_printf(s, "%20s  %20s", "ACT_VSYNC", "-\n");
			break;
		case DISP_EVT_DEACT_VSYNC:
			seq_printf(s, "%20s  %20s", "DEACT_VSYNC", "-\n");
			break;
		case DISP_EVT_WIN_CONFIG:
			seq_printf(s, "%20s  %20s", "WIN_CONFIG", "-\n");
			break;
		case DISP_EVT_TE_INTERRUPT:
			seq_printf(s, "%20s  %20s", "TE_INTERRUPT", "-\n");
			break;
		case DISP_EVT_UNDERRUN:
			seq_printf(s, "%20s  %20s", "UNDER_RUN", "-\n");
			break;
		case DISP_EVT_DSIM_FRAMEDONE:
			seq_printf(s, "%20s  %20s", "FRAME_DONE", "-\n");
			break;
		case DISP_EVT_UPDATE_HANDLER:
			seq_printf(s, "%20s  ", "UPDATE_HANDLER");
			seq_printf(s, "bw(tot=%d,int=%d,disp=%d), (%d,%d,%d,%d)\n",
					log->data.reg.bw,
					log->data.reg.int_bw,
					log->data.reg.disp_bw,
					log->data.reg.win.x,
					log->data.reg.win.y,
					log->data.reg.win.w,
					log->data.reg.win.h);
			break;
		case DISP_EVT_DSIM_COMMAND:
			seq_printf(s, "%20s  ", "DSIM_COMMAND");
			seq_printf(s, "id=0x%x, command=0x%x\n",
					log->data.cmd_buf.id,
					log->data.cmd_buf.buf);
			break;
		case DISP_EVT_VPP_WINCON:
			seq_printf(s, "%20s  ", "VPP_WINCON");
			seq_printf(s, "(id:%d)\n", log->data.vpp.id);
			break;
		case DISP_EVT_VPP_FRAMEDONE:
			seq_printf(s, "%20s  ", "VPP_FRAMEDONE");
			seq_printf(s, "(id:%d)Num of start=%d, framedone=%d\n",
					log->data.vpp.id,
					log->data.vpp.start_cnt,
					log->data.vpp.done_cnt);
			break;
		case DISP_EVT_VPP_STOP:
			seq_printf(s, "%20s  ", "VPP_STOP");
			seq_printf(s, "(id:%d)\n", log->data.vpp.id);
			break;
		case DISP_EVT_VPP_SUSPEND:
			seq_printf(s, "%20s  %20s", "VPP_SUSPEND", "-\n");
			break;
		case DISP_EVT_VPP_RESUME:
			seq_printf(s, "%20s  %20s", "VPP_RESUME", "-\n");
			break;
		case DISP_EVT_DECON_SUSPEND:
			seq_printf(s, "%20s  %20s", "DECON_SUSPEND", "-\n");
			break;
		case DISP_EVT_DECON_RESUME:
			seq_printf(s, "%20s  %20s", "DECON_RESUME", "-\n");
			break;
		case DISP_EVT_ENTER_LPD:
			seq_printf(s, "%20s  ", "ENTER_LPD");
			tv = ktime_to_timeval(log->data.pm.elapsed);
			seq_printf(s, "pm=%s, elapsed=[%ld.%03lds]\n",
					log->data.pm.pm_status ? "active ":"suspend",
					tv.tv_sec, tv.tv_usec/1000);
			break;
		case DISP_EVT_EXIT_LPD:
			seq_printf(s, "%20s  ", "EXIT_LPD");
			tv = ktime_to_timeval(log->data.pm.elapsed);
			seq_printf(s, "pm=%s, elapsed=[%ld.%03lds]\n",
					log->data.pm.pm_status ? "active ":"suspend",
					tv.tv_sec, tv.tv_usec/1000);
			break;
		case DISP_EVT_DSIM_SUSPEND:
			seq_printf(s, "%20s  %20s", "DSIM_SUSPEND", "-\n");
			break;
		case DISP_EVT_DSIM_RESUME:
			seq_printf(s, "%20s  %20s", "DSIM_RESUME", "-\n");
			break;
		case DISP_EVT_ENTER_ULPS:
			seq_printf(s, "%20s  ", "ENTER_ULPS");
			tv = ktime_to_timeval(log->data.pm.elapsed);
			seq_printf(s, "pm=%s, elapsed=[%ld.%03lds]\n",
					log->data.pm.pm_status ? "active ":"suspend",
					tv.tv_sec, tv.tv_usec/1000);
			break;
		case DISP_EVT_EXIT_ULPS:
			seq_printf(s, "%20s  ", "EXIT_ULPS");
			tv = ktime_to_timeval(log->data.pm.elapsed);
			seq_printf(s, "pm=%s, elapsed=[%ld.%03lds]\n",
					log->data.pm.pm_status ? "active ":"suspend",
					tv.tv_sec, tv.tv_usec/1000);
			break;
		default:
			seq_printf(s, "%20s  (%2d)\n", "NO_DEFINED", log->type);
			break;
		}
	} while (latest != idx);

	seq_printf(s, "-------------------------------------------------------------\n");

	return;
}

void DISP_SS_EVENT_SIZE_ERR_LOG(struct v4l2_subdev *sd, struct disp_ss_size_info *info)
{
	struct decon_device *decon = container_of(sd, struct decon_device, sd);
	int idx = (decon->disp_ss_size_log_idx++) % DISP_EVENT_SIZE_ERR_MAX;
	struct disp_ss_size_err_info *log = &decon->disp_ss_size_log[idx];

	if (!decon)
		return;

	log->time = ktime_get();
	memcpy(&log->info, info, sizeof(struct disp_ss_size_info));
}
#endif
