/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
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
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include <mach/map.h>
#include <mach/regs-clock.h>

#include "fimc-is-type.h"
#include "fimc-is-core.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-groupmgr.h"
#include "fimc-is-cmd.h"
#include "fimc-is-dvfs.h"
#include "fimc-is-debug.h"
#include "fimc-is-hw.h"

/* sysfs variable for debug */
extern struct fimc_is_sysfs_debug sysfs_debug;
static int vendor_aeflashMode;

static inline void smp_shot_init(struct fimc_is_group *group, u32 value)
{
	sema_init(&group->smp_shot, value);
	atomic_set(&group->smp_shot_count, value);
}

static inline int smp_shot_get(struct fimc_is_group *group)
{
	return atomic_read(&group->smp_shot_count);
}

static inline int smp_shot_inc(struct fimc_is_group *group)
{
	atomic_inc(&group->smp_shot_count);
	up(&group->smp_shot);
	return 0;
}

static inline int smp_shot_dec(struct fimc_is_group *group)
{
	int ret = 0;

	ret = down_interruptible(&group->smp_shot);
	if (ret) {
		mgerr(" down fail(%d)", group, group, ret);
		goto p_err;
	}

	atomic_dec(&group->smp_shot_count);

p_err:
	return ret;
}

static void fimc_is_gframe_s_info(struct fimc_is_group_frame *gframe,
	u32 slot, struct fimc_is_frame *frame)
{
	BUG_ON(!gframe);
	BUG_ON(!frame);
	BUG_ON(!frame->shot_ext);
	BUG_ON(slot >= GROUP_SLOT_MAX);

	memcpy(&gframe->group_cfg[slot], &frame->shot_ext->node_group,
		sizeof(struct camera2_node_group));
}

static void fimc_is_gframe_free_head(struct fimc_is_group_framemgr *gframemgr,
	struct fimc_is_group_frame **gframe)
{
	if (gframemgr->gframe_cnt)
		*gframe = container_of(gframemgr->gframe_head.next, struct fimc_is_group_frame, list);
	else
		*gframe = NULL;
}

static void fimc_is_gframe_s_free(struct fimc_is_group_framemgr *gframemgr,
	struct fimc_is_group_frame *gframe)
{
	BUG_ON(!gframemgr);
	BUG_ON(!gframe);

	list_add_tail(&gframe->list, &gframemgr->gframe_head);
	gframemgr->gframe_cnt++;
}

static void fimc_is_gframe_print_free(struct fimc_is_group_framemgr *gframemgr)
{
	struct fimc_is_group_frame *gframe, *temp;

	BUG_ON(!gframemgr);

	printk(KERN_ERR "[GFM] fre(%d) :", gframemgr->gframe_cnt);

	list_for_each_entry_safe(gframe, temp, &gframemgr->gframe_head, list) {
		printk(KERN_CONT "%d->", gframe->fcount);
	}

	printk(KERN_CONT "X\n");
}

static void fimc_is_gframe_group_head(struct fimc_is_group *group,
	struct fimc_is_group_frame **gframe)
{
	if (group->gframe_cnt)
		*gframe = container_of(group->gframe_head.next, struct fimc_is_group_frame, list);
	else
		*gframe = NULL;
}

static void fimc_is_gframe_s_group(struct fimc_is_group *group,
	struct fimc_is_group_frame *gframe)
{
	BUG_ON(!group);
	BUG_ON(!gframe);

	list_add_tail(&gframe->list, &group->gframe_head);
	group->gframe_cnt++;
}

static void fimc_is_gframe_print_group(struct fimc_is_group *group)
{
	struct fimc_is_group_frame *gframe, *temp;

	while (group) {
		printk(KERN_ERR "[GP%d] req(%d) :", group->id, group->gframe_cnt);

		list_for_each_entry_safe(gframe, temp, &group->gframe_head, list) {
			printk(KERN_CONT "%d->", gframe->fcount);
		}

		printk(KERN_CONT "X\n");

		group = group->gnext;
	}
}

static int fimc_is_gframe_check(struct fimc_is_group *gprev,
	struct fimc_is_group *group,
	struct fimc_is_group_frame *gframe)
{
	int ret = 0;
	u32 capture_id;
	struct fimc_is_device_ischain *device;
	struct fimc_is_crop *incrop, *otcrop;
	struct fimc_is_subdev *subdev, *junction;
	struct camera2_node *node;

	BUG_ON(!group);
	BUG_ON(!group->device);
	BUG_ON(group->slot >= GROUP_SLOT_MAX);
	BUG_ON(gprev && (gprev->slot >= GROUP_SLOT_MAX));

	device = group->device;

	/*
	 * perframe check
	 * 1. perframe size can't exceed s_format size
	 */
	incrop = (struct fimc_is_crop *)gframe->group_cfg[group->slot].leader.input.cropRegion;
	subdev = &group->leader;
	if ((incrop->w * incrop->h) > (subdev->input.width * subdev->input.height)) {
		mrwarn("the input size is invalid(%dx%d > %dx%d)", group, gframe,
			incrop->w, incrop->h, subdev->input.width, subdev->input.height);
		incrop->w = subdev->input.width;
		incrop->h = subdev->input.height;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		node = &gframe->group_cfg[group->slot].capture[capture_id];
		if (node->vid == 0) /* no effect */
			continue;

		otcrop = (struct fimc_is_crop *)node->output.cropRegion;
		subdev = video2subdev(device, node->vid);
		if (!subdev) {
			mgerr("subdev is NULL", group, group);
			ret = -EINVAL;
			node->request = 0;
			node->vid = 0;
			goto p_err;
		}

		if ((otcrop->w * otcrop->h) > (subdev->output.width * subdev->output.height)) {
			mrwarn("the output size is invalid(%dx%d > %dx%d)", group, gframe,
				otcrop->w, otcrop->h, subdev->output.width, subdev->output.height);
			otcrop->w = subdev->output.width;
			otcrop->h = subdev->output.height;
		}

		subdev->cid = capture_id;
	}

	/*
	 * junction check
	 * 1. skip if previous is empty
	 * 2. previous capture size should be same with current leader szie
	 */
	if (!gprev)
		goto p_err;

	junction = gprev->junction;
	if (!junction) {
		mgerr("junction is NULL", gprev, gprev);
		ret = -EINVAL;
		goto p_err;
	}

	if (junction->cid >= CAPTURE_NODE_MAX) {
		mgerr("capture id(%d) is invalid", gprev, gprev, junction->cid);
		ret = -EFAULT;
		goto p_err;
	}

	otcrop = (struct fimc_is_crop *)gframe->group_cfg[gprev->slot].capture[junction->cid].output.cropRegion;
	incrop = (struct fimc_is_crop *)gframe->group_cfg[group->slot].leader.input.cropRegion;

	if (!COMPARE_CROP(otcrop, incrop)) {
		mrwarn("the size is incoincidence(GP%d(%d,%d,%d,%d) != GP%d(%d,%d,%d,%d))", group, gframe,
			gprev->id, otcrop->x, otcrop->y, otcrop->w, otcrop->h,
			group->id, incrop->x, incrop->y, incrop->w, incrop->h);
		*incrop = *otcrop;
	}

p_err:
	return ret;
}

static int fimc_is_gframe_trans_fre_to_grp(struct fimc_is_group_framemgr *gframemgr,
	struct fimc_is_group_frame *gframe,
	struct fimc_is_group *group,
	struct fimc_is_group *gnext)
{
	int ret = 0;

	BUG_ON(!gframemgr);
	BUG_ON(!gframe);
	BUG_ON(!group);
	BUG_ON(!gnext);
	BUG_ON(!group->tail);
	BUG_ON(!group->tail->junction);

	if (unlikely(!gframemgr->gframe_cnt)) {
		merr("gframe_cnt is zero", group);
		ret = -EFAULT;
		goto p_err;
	}

	if (gframe->group_cfg[group->slot].capture[group->tail->junction->cid].request) {
		list_del(&gframe->list);
		gframemgr->gframe_cnt--;
		fimc_is_gframe_s_group(gnext, gframe);
	}

p_err:
	return ret;
}

static int fimc_is_gframe_trans_grp_to_grp(struct fimc_is_group_framemgr *gframemgr,
	struct fimc_is_group_frame *gframe,
	struct fimc_is_group *group,
	struct fimc_is_group *gnext)
{
	int ret = 0;

	BUG_ON(!gframemgr);
	BUG_ON(!gframe);
	BUG_ON(!group);
	BUG_ON(!gnext);
	BUG_ON(!group->tail);
	BUG_ON(!group->tail->junction);

	if (unlikely(!group->gframe_cnt)) {
		merr("gframe_cnt is zero", group);
		ret = -EFAULT;
		goto p_err;
	}

	if (gframe->group_cfg[group->slot].capture[group->tail->junction->cid].request) {
		list_del(&gframe->list);
		group->gframe_cnt--;
		fimc_is_gframe_s_group(gnext, gframe);
	} else {
		list_del(&gframe->list);
		group->gframe_cnt--;
		fimc_is_gframe_s_free(gframemgr, gframe);
	}

p_err:
	return ret;
}

static int fimc_is_gframe_trans_grp_to_fre(struct fimc_is_group_framemgr *gframemgr,
	struct fimc_is_group_frame *gframe,
	struct fimc_is_group *group)
{
	int ret = 0;

	BUG_ON(!gframemgr);
	BUG_ON(!gframe);
	BUG_ON(!group);

	if (!group->gframe_cnt) {
		merr("gframe_cnt is zero", group);
		ret = -EFAULT;
		goto p_err;
	}

	list_del(&gframe->list);
	group->gframe_cnt--;
	fimc_is_gframe_s_free(gframemgr, gframe);

p_err:
	return ret;
}

int fimc_is_gframe_cancel(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 target_fcount)
{
	int ret = -EINVAL;
	struct fimc_is_group_framemgr *gframemgr;
	struct fimc_is_group_frame *gframe, *temp;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);

	gframemgr = &groupmgr->gframemgr[group->instance];

	spin_lock_irq(&gframemgr->gframe_slock);

	list_for_each_entry_safe(gframe, temp, &group->gframe_head, list) {
		if (gframe->fcount == target_fcount) {
			list_del(&gframe->list);
			group->gframe_cnt--;
			mwarn("gframe%d is cancelled", group, target_fcount);
			fimc_is_gframe_s_free(gframemgr, gframe);
			ret = 0;
			break;
		}
	}

	spin_unlock_irq(&gframemgr->gframe_slock);

	return ret;
}

void * fimc_is_gframe_rewind(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 target_fcount)
{
	struct fimc_is_group_framemgr *gframemgr;
	struct fimc_is_group_frame *gframe, *temp;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);

	gframemgr = &groupmgr->gframemgr[group->instance];

	list_for_each_entry_safe(gframe, temp, &group->gframe_head, list) {
		if (gframe->fcount == target_fcount)
			break;

		if (gframe->fcount > target_fcount) {
			mgwarn("qbuf fcount(%d) is smaller than expect fcount(%d)", group, group,
				target_fcount, gframe->fcount);
			break;
		}

		list_del(&gframe->list);
		group->gframe_cnt--;
		mgwarn("gframe%d is cancel(count : %d)", group, group, gframe->fcount, group->gframe_cnt);
		fimc_is_gframe_s_free(gframemgr, gframe);
	}

	if (!group->gframe_cnt) {
		merr("gframe%d can't be found", group, target_fcount);
		gframe = NULL;
	}

	return gframe;
}

int fimc_is_gframe_flush(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group)
{
	int ret = 0;
	unsigned long flag;
	struct fimc_is_group_framemgr *gframemgr;
	struct fimc_is_group_frame *gframe, *temp;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);

	gframemgr = &groupmgr->gframemgr[group->instance];

	spin_lock_irqsave(&gframemgr->gframe_slock, flag);

	list_for_each_entry_safe(gframe, temp, &group->gframe_head, list) {
		list_del(&gframe->list);
		group->gframe_cnt--;
		fimc_is_gframe_s_free(gframemgr, gframe);
	}

	spin_unlock_irqrestore(&gframemgr->gframe_slock, flag);

	return ret;
}

static void fimc_is_group_subdev_cancel(struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame)
{
	u32 entry;
	unsigned long flags;
	struct fimc_is_video_ctx *sub_vctx;
	struct fimc_is_framemgr *sub_framemgr;
	struct fimc_is_frame *sub_frame;
	struct fimc_is_subdev *subdev;

	while (group) {
		for (entry = ENTRY_3AA; entry < ENTRY_END; ++entry) {
			subdev = group->subdev[entry];
			if (subdev && subdev->vctx && test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
				sub_vctx = subdev->vctx;
				sub_framemgr = GET_FRAMEMGR(sub_vctx);
				if (!sub_framemgr) {
					mserr("sub_framemgr is NULL", group, subdev);
					BUG();
				}

				framemgr_e_barrier_irqs(sub_framemgr, FMGR_IDX_19, flags);

				fimc_is_frame_request_head(sub_framemgr, &sub_frame);
				if (sub_frame) {
					clear_bit(subdev->id, &ldr_frame->out_flag);

					sub_frame->stream->fvalid = 0;
					sub_frame->stream->fcount = ldr_frame->fcount;
					sub_frame->stream->rcount = ldr_frame->rcount;
					clear_bit(REQ_FRAME, &sub_frame->req_flag);
					fimc_is_frame_trans_req_to_com(sub_framemgr, sub_frame);
					msrinfo("[ERR] group subdev CANCEL(%d)\n", group, subdev, sub_frame, sub_frame->index);
					buffer_done(sub_vctx, sub_frame->index, VB2_BUF_STATE_ERROR);
				}

				framemgr_x_barrier_irqr(sub_framemgr, FMGR_IDX_19, flags);
			}
		}

		group = group->child;
	}
}

static void fimc_is_group_cancel(struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame)
{
	unsigned long flags;
	struct fimc_is_video_ctx *ldr_vctx;
	struct fimc_is_framemgr *ldr_framemgr;

	BUG_ON(!group);
	BUG_ON(!ldr_frame);

	ldr_vctx = group->leader.vctx;
	ldr_framemgr = GET_FRAMEMGR(ldr_vctx);
	if (!ldr_framemgr) {
		mgerr("ldr_framemgr is NULL", group, group);
		BUG();
	}

	framemgr_e_barrier_irqs(ldr_framemgr, FMGR_IDX_20, flags);

	fimc_is_group_subdev_cancel(group, ldr_frame);

	fimc_is_frame_trans_req_to_com(ldr_framemgr, ldr_frame);
	mgrinfo("[ERR] group CANCEL(%d)\n", group, group, ldr_frame, ldr_frame->index);
	buffer_done(ldr_vctx, ldr_frame->index, VB2_BUF_STATE_ERROR);

	framemgr_x_barrier_irqr(ldr_framemgr, FMGR_IDX_20, flags);
}

static void fimc_is_group_s_leader(struct fimc_is_group *group,
	struct fimc_is_subdev *leader)
{
	u32 entry;
	struct fimc_is_subdev *subdev;

	BUG_ON(!group);
	BUG_ON(!leader);

	subdev = &group->leader;
	subdev->leader = leader;

	for (entry = ENTRY_3AA; entry < ENTRY_END; ++entry) {
		subdev = group->subdev[entry];
		if (subdev)
			subdev->leader = leader;
	}
}

static void fimc_is_stream_status(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group)
{
	struct fimc_is_queue *queue;
	struct fimc_is_framemgr *framemgr;

	while (group) {
		BUG_ON(!group->leader.vctx);

		queue = GET_SUBDEV_QUEUE(&group->leader);
		framemgr = &queue->framemgr;

		mginfo(" ginfo(res %d, rcnt %d, pos %d)\n", group, group,
			groupmgr->group_smp_res[group->slot].count,
			atomic_read(&group->rcount),
			group->pcount);
		mginfo(" vinfo(req %d, pre %d, que %d, com %d, dqe %d)\n", group, group,
			queue->buf_req,
			queue->buf_pre,
			queue->buf_que,
			queue->buf_com,
			queue->buf_dqe);
		fimc_is_frame_print_all(framemgr);

		group = group->gnext;
	}
}

#ifdef CONFIG_USE_VENDER_FEATURE
/* Flash Mode Control */
#ifdef CONFIG_LEDS_LM3560
extern int lm3560_reg_update_export(u8 reg, u8 mask, u8 data);
#endif
#ifdef CONFIG_LEDS_SKY81296
extern int sky81296_torch_ctrl(int state);
#endif

static void fimc_is_group_set_torch(struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame)
{
	if (group->prev)
		return;

#ifdef CONFIG_ENABLE_HAL3_2_META_INTERFACE
	if (group->aeflashMode != ldr_frame->shot->ctl.aa.vendor_aeflashMode) {
		group->aeflashMode = ldr_frame->shot->ctl.aa.vendor_aeflashMode;
#else
	if (group->aeflashMode != ldr_frame->shot->ctl.aa.aeflashMode) {
		group->aeflashMode = ldr_frame->shot->ctl.aa.aeflashMode;
#endif
		switch (group->aeflashMode) {
		case AA_FLASHMODE_ON_ALWAYS: /*TORCH mode*/
#ifdef CONFIG_LEDS_LM3560
			lm3560_reg_update_export(0xE0, 0xFF, 0xEF);
#elif defined(CONFIG_LEDS_SKY81296)
			sky81296_torch_ctrl(1);
#endif
			break;
		case AA_FLASHMODE_START: /*Pre flash mode*/
#ifdef CONFIG_LEDS_LM3560
			lm3560_reg_update_export(0xE0, 0xFF, 0xEF);
#elif defined(CONFIG_LEDS_SKY81296)
			sky81296_torch_ctrl(1);
#endif
			break;
		case AA_FLASHMODE_CAPTURE: /*Main flash mode*/
			break;
		case AA_FLASHMODE_OFF: /*OFF mode*/
#ifdef CONFIG_LEDS_SKY81296
			sky81296_torch_ctrl(0);
#endif
			break;
		default:
			break;
		}
	}
	return;
}
#endif

#ifdef DEBUG_AA
static void fimc_is_group_debug_aa_shot(struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame)
{
	if (group->prev)
		return;

#ifdef DEBUG_FLASH
#ifdef CONFIG_ENABLE_HAL3_2_META_INTERFACE
	if (ldr_frame->shot->ctl.aa.vendor_aeflashMode != group->flashmode) {
		group->flashmode = ldr_frame->shot->ctl.aa.vendor_aeflashMode;
#else
	if (ldr_frame->shot->ctl.aa.aeflashMode != group->flashmode) {
		group->flashmode = ldr_frame->shot->ctl.aa.aeflashMode;
#endif
		info("flash ctl : %d(%d)\n", group->flashmode, ldr_frame->fcount);
	}
#endif
}

static void fimc_is_group_debug_aa_done(struct fimc_is_group *group,
	struct fimc_is_frame *ldr_frame)
{
	if (group->prev)
		return;

#ifdef DEBUG_FLASH
	if (ldr_frame->shot->dm.flash.firingStable != group->flash.firingStable) {
		group->flash.firingStable = ldr_frame->shot->dm.flash.firingStable;
		info("flash stable : %d(%d)\n", group->flash.firingStable, ldr_frame->fcount);
	}

	if (ldr_frame->shot->dm.flash.flashReady!= group->flash.flashReady) {
		group->flash.flashReady = ldr_frame->shot->dm.flash.flashReady;
		info("flash ready : %d(%d)\n", group->flash.flashReady, ldr_frame->fcount);
	}

	if (ldr_frame->shot->dm.flash.flashOffReady!= group->flash.flashOffReady) {
		group->flash.flashOffReady = ldr_frame->shot->dm.flash.flashOffReady;
		info("flash off : %d(%d)\n", group->flash.flashOffReady, ldr_frame->fcount);
	}
#endif
}
#endif

static void fimc_is_group_start_trigger(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame)
{
	BUG_ON(!group);
	BUG_ON(!frame);

	atomic_inc(&group->rcount);
	queue_kthread_work(group->worker, &frame->work);
}

static void fimc_is_group_pump(struct kthread_work *work)
{
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_frame *frame;

	frame = container_of(work, struct fimc_is_frame, work);
	groupmgr = frame->work_data1;
	group = frame->work_data2;

	fimc_is_group_shot(groupmgr, group, frame);
}

int fimc_is_groupmgr_probe(struct fimc_is_groupmgr *groupmgr)
{
	int ret = 0;
	u32 stream, slot, gframe;
	struct fimc_is_group_framemgr *gframemgr;

	for (stream = 0; stream < FIMC_IS_STREAM_COUNT; ++stream) {
		gframemgr = &groupmgr->gframemgr[stream];
		spin_lock_init(&groupmgr->gframemgr[stream].gframe_slock);
		INIT_LIST_HEAD(&groupmgr->gframemgr[stream].gframe_head);
		groupmgr->gframemgr[stream].gframe_cnt = 0;

		for (gframe = 0; gframe < FIMC_IS_MAX_GFRAME; ++gframe) {
			groupmgr->gframemgr[stream].gframe[gframe].fcount = 0;
			fimc_is_gframe_s_free(&groupmgr->gframemgr[stream],
				&groupmgr->gframemgr[stream].gframe[gframe]);
		}

		groupmgr->leader[stream] = NULL;
		for (slot = GROUP_SLOT_3AA; slot < GROUP_SLOT_MAX; ++slot)
			groupmgr->group[stream][slot] = NULL;
	}

	for (slot = GROUP_SLOT_3AA; slot < GROUP_SLOT_MAX; ++slot) {
		atomic_set(&groupmgr->group_refcount[slot], 0);
		clear_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[slot]);
		clear_bit(FIMC_IS_GGROUP_START, &groupmgr->group_state[slot]);
		clear_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[slot]);
	}

	return ret;
}

int fimc_is_groupmgr_init(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_device_ischain *device)
{
	int ret = 0;
	struct fimc_is_path_info *path;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_group *group, *prev, *next, *sibling;
	struct fimc_is_group *leader_group;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_video *video;
	u32 slot, source_vid;
	u32 instance;
	u32 entry;

	BUG_ON(!groupmgr);
	BUG_ON(!device);

	group = NULL;
	prev = NULL;
	next = NULL;
	instance = device->instance;
	path = &device->path;

	path->group[GROUP_SLOT_3AA] = GROUP_ID_INVALID;
	path->group[GROUP_SLOT_ISP] = GROUP_ID_INVALID;
	path->group[GROUP_SLOT_DIS] = GROUP_ID_INVALID;
	path->group[GROUP_SLOT_MAX] = GROUP_ID_INVALID;

	leader_group = groupmgr->leader[instance];
	if (!leader_group) {
		err("stream leader is not selected");
		ret = -EINVAL;
		goto p_err;
	}

	for (slot = leader_group->slot; slot < GROUP_SLOT_MAX; ++slot) {
		group = groupmgr->group[instance][slot];
		if (!group)
			continue;

		group->prev = NULL;
		group->next = NULL;
		group->gprev = NULL;
		group->gnext = NULL;
		group->parent = NULL;
		group->child = NULL;
		group->tail = group;
		group->junction = NULL;

		source_vid = group->source_vid;
		mdbgd_group("source vid : %02d\n", group, source_vid);
		if (source_vid) {
			leader = &group->leader;
			fimc_is_group_s_leader(group, leader);

			if (prev) {
				group->prev = prev;
				prev->next = group;
			}

			prev = group;
		}
	}

	group = leader_group;
	sibling = leader_group;
	next = group->next;
	while (next) {
		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &next->state)) {
			group->child = next;
			next->parent = sibling;
			sibling->tail = next;

			leader = &sibling->leader;
			fimc_is_group_s_leader(next, leader);
		} else {
			sibling->gnext = next;
			next->gprev = sibling;
			sibling = next;
		}

		group = next;
		next = group->next;
	}

	fimc_is_dmsg_init();

	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		fimc_is_dmsg_concate("STM(R) PH:");
	else
		fimc_is_dmsg_concate("STM(N) PH:");

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &leader_group->state))
		fimc_is_dmsg_concate(" %02d -> ", leader_group->source_vid);
	else
		fimc_is_dmsg_concate(" %02d => ", leader_group->source_vid);

	group = leader_group;
	while(group) {
		next = group->next;
		if (next) {
			source_vid = next->source_vid;
			BUG_ON(group->slot >= GROUP_SLOT_MAX);
			BUG_ON(next->slot >= GROUP_SLOT_MAX);
		} else {
			source_vid = 0;
			path->group[group->slot] = group->id;
		}

		fimc_is_dmsg_concate("GP%d ( ", group->id);
		for (entry = ENTRY_3AA; entry < ENTRY_END; ++entry) {
			subdev = group->subdev[entry];
			if (!subdev)
				continue;

			vctx = subdev->vctx;
			if (!vctx)
				continue;

			video = vctx->video;
			if (!video) {
				merr("video is NULL", device);
				BUG();
			}

			/* groupping check */
			switch (group->id) {
			case GROUP_ID_3AA0:
				if ((video->id >= FIMC_IS_VIDEO_31S_NUM) &&
					(video->id <= FIMC_IS_VIDEO_31P_NUM)) {
					merr("invalid video group(G0 -> V%02d)", device, video->id);
					BUG();
				}
				break;
			case GROUP_ID_3AA1:
				if ((video->id >= FIMC_IS_VIDEO_30S_NUM) &&
					(video->id <= FIMC_IS_VIDEO_30P_NUM)) {
					merr("invalid video group(G1 -> V%02d)", device, video->id);
					BUG();
				}
				break;
			case GROUP_ID_ISP0:
				if ((video->id >= FIMC_IS_VIDEO_I1S_NUM) &&
					(video->id <= FIMC_IS_VIDEO_I1P_NUM)) {
					merr("invalid video group(G2 -> V%02d)", device, video->id);
					BUG();
				}

				if ((video->id >= FIMC_IS_VIDEO_30S_NUM) &&
					(video->id <= FIMC_IS_VIDEO_31P_NUM)) {
					merr("invalid video group(G2 -> V%02d)", device, video->id);
					BUG();
				}
				break;
			case GROUP_ID_ISP1:
				if ((video->id >= FIMC_IS_VIDEO_I0S_NUM) &&
					(video->id <= FIMC_IS_VIDEO_I0P_NUM)) {
					merr("invalid video group(G3 -> V%02d)", device, video->id);
					BUG();
				}

				if ((video->id >= FIMC_IS_VIDEO_30S_NUM) &&
					(video->id <= FIMC_IS_VIDEO_31P_NUM)) {
					merr("invalid video group(G2 -> V%02d)", device, video->id);
					BUG();
				}
				break;
			case GROUP_ID_DIS0:
				if ((video->id >= FIMC_IS_VIDEO_30S_NUM) &&
					(video->id <= FIMC_IS_VIDEO_I1P_NUM)) {
					merr("invalid video group(G4 -> V%02d)", device, video->id);
					BUG();
				}
				break;
			default:
				merr("invalid group(%d)", device, group->id);
				BUG();
				break;
			}

			/* connection check */
			if (video->id == source_vid) {
				fimc_is_dmsg_concate("*%02d ", video->id);
				group->junction = group->subdev[entry];
				path->group[group->slot] = group->id;
				path->group[next->slot] = next->id;
			} else {
				fimc_is_dmsg_concate("%02d ", video->id);
			}
		}
		fimc_is_dmsg_concate(")");

		if (next && !group->junction) {
			mgerr("junction subdev can NOT be found", device, group);
			ret = -EINVAL;
			goto p_err;
		}

		if (next) {
			if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &next->state)) {
				set_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state);
				fimc_is_dmsg_concate(" -> ");
			} else {
				clear_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state);
				fimc_is_dmsg_concate(" => ");
			}
		}

		group = next;
	}
	fimc_is_dmsg_concate("\n");

p_err:
	minfo(" =STM CFG===============\n", device);
	minfo(" %s", device, fimc_is_dmsg_print());
	minfo(" DEVICE GRAPH : %X %X %X\n", device, path->group[0], path->group[1], path->group[2]);
	minfo(" =======================\n", device);
	return ret;
}

int fimc_is_groupmgr_start(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 subdev_id;
	u32 instance;
	u32 width, height;
	u32 lindex, hindex, indexes;
	struct fimc_is_group *group, *prev;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_crop incrop, otcrop;

	BUG_ON(!groupmgr);
	BUG_ON(!device);

	instance = device->instance;
	group = groupmgr->leader[instance];
	if (!group) {
		merr("stream leader is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	minfo(" =GRP CFG===============\n", device);
	while(group) {
		leader = &group->leader;
		prev = group->prev;

		if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state) &&
			!test_bit(FIMC_IS_GROUP_START, &group->state)) {
			merr("GP%d is NOT started", device, group->id);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
			if (group->slot == GROUP_SLOT_3AA) {
				width = fimc_is_sensor_g_bns_width(device->sensor);
				height = fimc_is_sensor_g_bns_height(device->sensor);
				leader->input.width = width;
				leader->input.height = height;
				width -= device->margin_width;
				height -= device->margin_height;
			} else {
				if (prev && prev->junction) {
					/* HACK, Max size constrains */
					if (prev->slot == GROUP_SLOT_3AA) {
						if (width > 2560)
							width = 2560;

						if (height > 1440)
							height = 1440;
					}

					leader->input.width = width;
					leader->input.height = height;
					prev->junction->output.width = width;
					prev->junction->output.height = height;
				} else {
					mgerr("previous group is NULL", group, group);
					BUG();
				}
			}
		} else {
			if (group->slot == GROUP_SLOT_3AA) {
				width = leader->input.width;
				height = leader->input.height;
				width -= device->margin_width;
				height -= device->margin_height;
			} else {
				width = leader->input.width;
				height = leader->input.height;
			}
		}

		mginfo(" SRC%02d:%04dx%04d\n", device, group, leader->vid,
			leader->input.width, leader->input.height);
		for (subdev_id = ENTRY_3AA; subdev_id < ENTRY_END; ++subdev_id) {
			subdev = group->subdev[subdev_id];
			if (subdev && subdev->vctx && test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
				mginfo(" CAP%2d:%04dx%04d\n", device, group, subdev->vid,
					subdev->output.width, subdev->output.height);
			}
		}

		if (prev && !test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
			if (!prev->junction) {
				mgerr("prev group is existed but junction is NULL", device, group);
				ret = -EINVAL;
				goto p_err;
			}

			if ((prev->junction->output.width != group->leader.input.width) ||
				(prev->junction->output.height != group->leader.input.height)) {
				merr("%s(%d x %d) != %s(%d x %d)", device,
					prev->junction->name,
					prev->junction->output.width,
					prev->junction->output.height,
					group->leader.name,
					group->leader.input.width,
					group->leader.input.height);
				ret = -EINVAL;
				goto p_err;
			}
		}

		incrop.x = 0;
		incrop.y = 0;
		incrop.w = width;
		incrop.h = height;

		otcrop.x = 0;
		otcrop.y = 0;
		otcrop.w = width;
		otcrop.h = height;

		ret = group->cfg_callback(device, NULL, &incrop, &otcrop, &lindex, &hindex, &indexes);
		if (ret) {
			mgerr("tag callback is fail(%d)", group, group, ret);
			goto p_err;
		}

		group = group->next;
	}
	minfo(" =======================\n", device);

p_err:
	return ret;
}

int fimc_is_groupmgr_stop(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 instance;
	struct fimc_is_group *group;

	BUG_ON(!groupmgr);
	BUG_ON(!device);

	instance = device->instance;
	group = groupmgr->leader[instance];
	if (!group) {
		merr("stream leader is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(FIMC_IS_GROUP_START, &group->state)) {
		merr("stream leader is NOT stopped", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_group_probe(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_device_ischain *device,
	fimc_is_shot_callback shot_callback,
	fimc_is_cfg_callback cfg_callback,
	u32 slot,
	u32 id,
	char *name)
{
	int ret = 0;
	struct fimc_is_subdev *leader;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!device);

	leader = &group->leader;
	group->id = GROUP_ID_INVALID;
	group->slot = slot;
	group->shot_callback = shot_callback;
	group->cfg_callback = cfg_callback;
	group->device = device;
	group->instance = device->instance;

	ret = fimc_is_hw_group_cfg(group);
	if (ret) {
		merr("fimc_is_hw_group_cfg is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_GROUP_OPEN, &group->state);
	clear_bit(FIMC_IS_GROUP_INIT, &group->state);
	clear_bit(FIMC_IS_GROUP_START, &group->state);
	clear_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state);
	clear_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state);
	clear_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state);

	fimc_is_subdev_probe(leader, device->instance, id, name);

p_err:
	return ret;
}

int fimc_is_group_open(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 id,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0, i;
	char name[30];
	u32 slot;
	struct fimc_is_framemgr *framemgr;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!vctx);
	BUG_ON(id >= GROUP_ID_MAX);

	mdbgd_ischain("%s(id %d)\n", device, __func__, id);

	group->id = id;
	slot = group->slot;

	if (test_bit(FIMC_IS_GROUP_OPEN, &group->state)) {
		mgerr("already open", group, group);
		ret = -EMFILE;
		goto p_err;
	}

	if(id == GROUP_ID_3AA0) {
		vendor_aeflashMode = AA_FLASHMODE_OFF;
	}

	framemgr = GET_FRAMEMGR(vctx);
	if (!framemgr) {
		mgerr("framemgr is NULL", group, group);
		ret = -EINVAL;
		goto p_err;
	}

	/* 1. start kthread */
	if (!test_bit(FIMC_IS_GGROUP_START, &groupmgr->group_state[slot])) {
		init_kthread_worker(&groupmgr->group_worker[slot]);
		snprintf(name, sizeof(name), "fimc_is_gworker%d", id);
		groupmgr->group_task[slot] = kthread_run(kthread_worker_fn,
			&groupmgr->group_worker[slot], name);
		if (IS_ERR(groupmgr->group_task[slot])) {
			err("failed to create group_task%d\n", slot);
			ret = -ENOMEM;
			goto p_err;
		}

		ret = sched_setscheduler_nocheck(groupmgr->group_task[slot], SCHED_FIFO, &param);
		if (ret) {
			merr("sched_setscheduler_nocheck is fail(%d)", group, ret);
			goto p_err;
		}

		set_bit(FIMC_IS_GGROUP_START, &groupmgr->group_state[slot]);
	}

	group->worker = &groupmgr->group_worker[slot];
	for (i = 0; i < FRAMEMGR_MAX_REQUEST; ++i)
		init_kthread_work(&framemgr->frame[i].work, fimc_is_group_pump);

	/* 2. Init Group */
	clear_bit(FIMC_IS_GROUP_INIT, &group->state);
	clear_bit(FIMC_IS_GROUP_START, &group->state);
	clear_bit(FIMC_IS_GROUP_SHOT, &group->state);
	clear_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state);
	clear_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state);
	clear_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state);

	group->prev = NULL;
	group->next = NULL;
	group->gprev = NULL;
	group->gnext = NULL;
	group->parent = NULL;
	group->child = NULL;
	group->tail = NULL;
	group->junction = NULL;
	group->source_vid = 0;
	group->fcount = 0;
	group->pcount = 0;
	group->aeflashMode = 0; /* Flash Mode Control */
	atomic_set(&group->scount, 0);
	atomic_set(&group->rcount, 0);
	atomic_set(&group->backup_fcount, 0);
	atomic_set(&group->sensor_fcount, 1);
	sema_init(&group->smp_trigger, 0);

	INIT_LIST_HEAD(&group->gframe_head);
	group->gframe_cnt = 0;

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
	monitor_init(&group->time);
#endif
#endif

	/* 3. Subdev Init */
	fimc_is_subdev_open(&group->leader, vctx, NULL);

	/* 4. Update Group Manager */
	groupmgr->group[group->instance][slot] = group;
	atomic_inc(&groupmgr->group_refcount[slot]);
	set_bit(FIMC_IS_GROUP_OPEN, &group->state);

p_err:
	mdbgd_group("%s(%d)\n", group, __func__, ret);
	return ret;
}

int fimc_is_group_close(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group)
{
	int ret = 0;
	u32 stream, slot, refcount, i;
	struct fimc_is_group *group_slot;
	struct fimc_is_group_framemgr *gframemgr;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);
	BUG_ON(group->id >= GROUP_ID_MAX);

	slot = group->slot;
	refcount = atomic_read(&groupmgr->group_refcount[slot]);
	stream = group->instance;

	if (!test_bit(FIMC_IS_GROUP_OPEN, &group->state)) {
		merr("group%d already close", group, group->id);
		ret = -EMFILE;
		goto p_err;
	}

	if ((refcount == 1) &&
		test_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[slot]) &&
		groupmgr->group_task[slot]) {

		set_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[slot]);

		for (i = 0; i < FIMC_IS_STREAM_COUNT; ++i) {
			group_slot = groupmgr->group[i][slot];
			if (group_slot && test_bit(FIMC_IS_GROUP_SHOT, &group_slot->state)) {
				smp_shot_inc(group_slot);
				up(&groupmgr->group_smp_res[slot]);
				up(&group->smp_trigger);
			}
		}

		/*
		 * flush kthread wait until all work is complete
		 * it's dangerous if all is not finished
		 * so it's commented currently
		 * flush_kthread_worker(&groupmgr->group_worker[slot]);
		 */
		kthread_stop(groupmgr->group_task[slot]);

		clear_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[slot]);
		clear_bit(FIMC_IS_GGROUP_START, &groupmgr->group_state[slot]);
		clear_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[slot]);
	}

	fimc_is_subdev_close(&group->leader);

	group->prev = NULL;
	group->next = NULL;
	group->gprev = NULL;
	group->gnext = NULL;
	group->parent = NULL;
	group->child = NULL;
	group->tail = NULL;
	group->junction = NULL;
	group->id = GROUP_ID_INVALID;
	clear_bit(FIMC_IS_GROUP_INIT, &group->state);
	clear_bit(FIMC_IS_GROUP_OPEN, &group->state);
	atomic_dec(&groupmgr->group_refcount[slot]);
	groupmgr->group[stream][slot] = NULL;

	if (!groupmgr->group[stream][GROUP_SLOT_3AA] &&
		!groupmgr->group[stream][GROUP_SLOT_ISP] &&
		!groupmgr->group[stream][GROUP_SLOT_DIS]) {
		gframemgr = &groupmgr->gframemgr[stream];

		if (gframemgr->gframe_cnt != FIMC_IS_MAX_GFRAME) {
			mwarn("gframemgr free count is invalid(%d)", group, gframemgr->gframe_cnt);
			INIT_LIST_HEAD(&gframemgr->gframe_head);
			gframemgr->gframe_cnt = 0;
			for (i = 0; i < FIMC_IS_MAX_GFRAME; ++i) {
				gframemgr->gframe[i].fcount = 0;
				fimc_is_gframe_s_free(gframemgr, &gframemgr->gframe[i]);
			}
		}
	}

p_err:
	mdbgd_group("%s(ref %d, %d)", group, __func__, (refcount - 1), ret);
	return ret;
}

int fimc_is_group_init(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	u32 otf_input,
	u32 video_id,
	u32 stream_leader)
{
	int ret = 0;
	u32 slot;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);
	BUG_ON(group->id >= GROUP_ID_MAX);
	BUG_ON(video_id >= FIMC_IS_VIDEO_MAX_NUM);

	if (!test_bit(FIMC_IS_GROUP_OPEN, &group->state)) {
		merr("group is NOT open", group);
		ret = -EINVAL;
		goto p_err;
	}

	slot = group->slot;
	group->source_vid = video_id;
	clear_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state);

	if (stream_leader)
		groupmgr->leader[group->instance] = group;

	if (!test_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[slot])) {
		sema_init(&groupmgr->group_smp_res[slot], MIN_OF_SHOT_RSC);
		set_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[slot]);
	}

	if (otf_input) {
		smp_shot_init(group, MIN_OF_SHOT_RSC);
		set_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state);
		group->async_shots = MIN_OF_ASYNC_SHOTS;
		group->sync_shots = MIN_OF_SYNC_SHOTS;
	} else {
		smp_shot_init(group, 1);
		clear_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state);
		group->async_shots = 0;
		group->sync_shots = 1;
	}

	set_bit(FIMC_IS_GROUP_INIT, &group->state);

p_err:
	mdbgd_group("%s(otf : %d):%d\n", group, __func__, otf_input, ret);
	return ret;
}

int fimc_is_group_start(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group)
{
	int ret = 0;
	struct fimc_is_device_ischain *device;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_framemgr *framemgr = NULL;
	u32 shot_resource = 1;
	u32 sensor_fcount;
	u32 framerate;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!group->device);
	BUG_ON(!group->device->sensor);
	BUG_ON(!group->leader.vctx);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);
	BUG_ON(group->id >= GROUP_ID_MAX);
	BUG_ON(!test_bit(FIMC_IS_GGROUP_INIT, &groupmgr->group_state[group->slot]));
	BUG_ON(!test_bit(FIMC_IS_GROUP_INIT, &group->state));

	device = group->device;
	sensor = device->sensor;

	if (!test_bit(FIMC_IS_GROUP_INIT, &group->state)) {
		merr("group is NOT initialized", group);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(FIMC_IS_GROUP_START, &group->state)) {
		warn("already group start");
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		group->async_shots = 1;
		group->sync_shots = 0;
		shot_resource = group->async_shots + group->sync_shots;
	} else {
		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
			framemgr = GET_SUBDEV_FRAMEMGR(&group->leader);

			/* async & sync shots */
			framerate = fimc_is_sensor_g_framerate(sensor);
			if (framerate <= 30)
				group->async_shots = MIN_OF_ASYNC_SHOTS + 0;
			else if (framerate <= 60)
				group->async_shots = MIN_OF_ASYNC_SHOTS + 1;
			else if (framerate <= 120)
				group->async_shots = MIN_OF_ASYNC_SHOTS + 2;
			else if (framerate <= 240)
				group->async_shots = MIN_OF_ASYNC_SHOTS + 2;
			else /* 300fps */
				group->async_shots = MIN_OF_ASYNC_SHOTS + 3;

			group->sync_shots = max_t(int, MIN_OF_SYNC_SHOTS,
					framemgr->frame_cnt - group->async_shots);

			/* shot resource */
			shot_resource = group->async_shots + MIN_OF_SYNC_SHOTS;
			sema_init(&groupmgr->group_smp_res[group->slot], shot_resource);

			/* frame count */
			sensor_fcount = fimc_is_sensor_g_fcount(sensor) + 1;
			atomic_set(&group->sensor_fcount, sensor_fcount);
			atomic_set(&group->backup_fcount, sensor_fcount - 1);
			group->fcount = sensor_fcount - 1;

			memset(&group->intent_ctl, 0, sizeof(struct camera2_ctl));
		} else {
			if (fimc_is_sensor_g_framerate(sensor) > 120)
				group->async_shots = MIN_OF_ASYNC_SHOTS;
			else
				group->async_shots = 1;
			group->sync_shots = 0;

			shot_resource = group->async_shots + group->sync_shots;
		}
	}

	smp_shot_init(group, shot_resource);
	atomic_set(&group->scount, 0);
	atomic_set(&group->rcount, 0);
	sema_init(&group->smp_trigger, 0);

	set_bit(FIMC_IS_SUBDEV_START, &group->leader.state);
	set_bit(FIMC_IS_GROUP_START, &group->state);

p_err:
	mginfo("async: %d, shot resource: %d\n", group, group, group->async_shots, shot_resource);
	return ret;
}

int fimc_is_group_stop(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group)
{
	int ret = 0;
	int stop_ret = 0;
	int retry;
	u32 rcount, pcount;
	unsigned long flags;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_device_ischain *device;
	struct fimc_is_device_sensor *sensor;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!group->device);
	BUG_ON(!group->leader.vctx);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);
	BUG_ON(group->id >= GROUP_ID_MAX);

	device = group->device;
	sensor = device->sensor;
	framemgr = GET_SUBDEV_FRAMEMGR(&group->leader);

	if (!test_bit(FIMC_IS_GROUP_START, &group->state)) {
		mwarn("already group stop", group);
		goto p_err;
	}

	if (test_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state)) {
		set_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state);
		clear_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state);

		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state) &&
			!list_empty(&group->smp_trigger.wait_list)) {
			pcount = group->pcount;

			if (!sensor) {
				mwarn(" sensor is NULL, forcely trigger(pc %d)", device, pcount);
				up(&group->smp_trigger);
				goto check_completion;
			}

			if (!test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state)) {
				mwarn(" sensor is closed, forcely trigger(pc %d)", device, pcount);
				up(&group->smp_trigger);
				goto check_completion;
			}

			if (!test_bit(FIMC_IS_SENSOR_FRONT_START, &sensor->state)) {
				mwarn(" front is stopped, forcely trigger(pc %d)", device, pcount);
				up(&group->smp_trigger);
				goto check_completion;
			}

			if (!test_bit(FIMC_IS_SENSOR_BACK_START, &sensor->state)) {
				mwarn(" back is stopped, forcely trigger(pc %d)", device, pcount);
				up(&group->smp_trigger);
				goto check_completion;
			}
		}
	}

check_completion:
	retry = 150;
	while (--retry && framemgr->frame_req_cnt) {
		mgwarn(" %d reqs waiting...", device, group, framemgr->frame_req_cnt);
		msleep(20);
	}

	if (!retry) {
		mgerr(" waiting(until request empty) is fail", device, group);
		set_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state);
		up(&group->smp_trigger);
		ret = -EINVAL;
	}

	/* ensure that request cancel work is complete fully */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_21, flags);
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_21, flags);

	if (test_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state)) {
		stop_ret = fimc_is_itf_force_stop(device, GROUP_ID(group->id));
		if (stop_ret) {
			mgerr(" fimc_is_itf_force_stop is fail(%d)", device, group, ret);
			ret = -EINVAL;
		}
	} else {
		stop_ret = fimc_is_itf_process_stop(device, GROUP_ID(group->id));
		if (stop_ret) {
			mgerr(" fimc_is_itf_process_stop is fail(%d)", device, group, ret);
			ret = -EINVAL;
		}
	}

	retry = 150;
	while (--retry && framemgr->frame_pro_cnt) {
		mgwarn(" %d pros waiting...", device, group, framemgr->frame_pro_cnt);
		msleep(20);
	}

	if (!retry) {
		mgerr(" waiting(until process empty) is fail", device, group);
		ret = -EINVAL;
	}

	rcount = atomic_read(&group->rcount);
	if (rcount) {
		mgerr(" request is NOT empty(%d)", device, group, rcount);
		ret = -EINVAL;
	}

	retry = 150;
	while (--retry && test_bit(FIMC_IS_GROUP_SHOT, &group->state)) {
		mgwarn(" thread stop waiting...", device, group);
		msleep(20);
	}

	if (!retry) {
		mgerr(" waiting(until thread stop) is fail", device, group);
		ret = -EINVAL;
	}

	fimc_is_gframe_flush(groupmgr, group);

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		mginfo(" sensor fcount: %d, fcount: %d\n", device, group,
			atomic_read(&group->sensor_fcount), group->fcount);

	clear_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(FIMC_IS_SUBDEV_START, &group->leader.state);
	clear_bit(FIMC_IS_GROUP_START, &group->state);

p_err:
	return ret;
}

int fimc_is_group_buffer_queue(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_queue *queue,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_resourcemgr *resourcemgr;
	struct fimc_is_device_ischain *device;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	u32 orientation;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!group->device);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);
	BUG_ON(group->id >= GROUP_ID_MAX);
	BUG_ON(!queue);
	BUG_ON(index >= FRAMEMGR_MAX_REQUEST);

	device = group->device;
	resourcemgr = device->resourcemgr;
	framemgr = &queue->framemgr;

	/* 1. check frame validation */
	frame = &framemgr->frame[index];
	if (!frame) {
		err("frame is null\n");
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!test_bit(FRAME_INI_MEM, &frame->memory))) {
		err("frame %d is NOT init", index);
		ret = EINVAL;
		goto p_err;
	}

	PROGRAM_COUNT(0);

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_22, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_FREE) {
		if (unlikely(frame->out_flag)) {
			mgwarn("output(0x%lX) is NOT completed", device, group, frame->out_flag);
			frame->out_flag = 0;
		}

		if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state) &&
			(framemgr->frame_req_cnt >= DIV_ROUND_UP(framemgr->frame_cnt, 2)))
				mgwarn(" request bufs : %d", device, group, framemgr->frame_req_cnt);

		/* orientation is set by user */
		orientation = frame->shot->uctl.scalerUd.orientation;
		memset(&frame->shot->uctl.scalerUd, 0, sizeof(struct camera2_scaler_uctl));
		frame->shot->uctl.scalerUd.orientation = orientation;

		frame->fcount = frame->shot->dm.request.frameCount;
		frame->rcount = frame->shot->ctl.request.frameCount;
		frame->work_data1 = groupmgr;
		frame->work_data2 = group;

#ifdef FIXED_FPS_DEBUG
		frame->shot->ctl.aa.aeTargetFpsRange[0] = FIXED_FPS_VALUE;
		frame->shot->ctl.aa.aeTargetFpsRange[1] = FIXED_FPS_VALUE;
		frame->shot->ctl.sensor.frameDuration = 1000000000/FIXED_FPS_VALUE;
#endif

		if (group->id == GROUP_ID_3AA0) {
			if (((vendor_aeflashMode == AA_FLASHMODE_OFF) && (frame->shot->ctl.aa.vendor_aeflashMode == AA_FLASHMODE_CANCEL)) ||
				((vendor_aeflashMode == AA_FLASHMODE_ON_ALWAYS) && (frame->shot->ctl.aa.vendor_aeflashMode == AA_FLASHMODE_CANCEL))) {
				frame->shot->ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
			}
			vendor_aeflashMode = frame->shot->ctl.aa.vendor_aeflashMode;
		}

		if (resourcemgr->limited_fps) {
			frame->shot->ctl.aa.aeTargetFpsRange[0] = resourcemgr->limited_fps;
			frame->shot->ctl.aa.aeTargetFpsRange[1] = resourcemgr->limited_fps;
			frame->shot->ctl.sensor.frameDuration = 1000000000/resourcemgr->limited_fps;
		}

#ifdef ENABLE_FAST_SHOT
		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
			memcpy(&group->fast_ctl.aa, &frame->shot->ctl.aa,
				sizeof(struct camera2_aa_ctl));
			memcpy(&group->fast_ctl.scaler, &frame->shot->ctl.scaler,
				sizeof(struct camera2_scaler_ctl));
		}
#endif

		fimc_is_frame_trans_fre_to_req(framemgr, frame);
	} else {
		err("frame(%d) is invalid state(%d)\n", index, frame->state);
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_22, flags);

	fimc_is_group_start_trigger(groupmgr, group, frame);

	PROGRAM_COUNT(1);

p_err:
	return ret;
}

int fimc_is_group_buffer_finish(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_device_ischain *device;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!group->device);
	BUG_ON(!group->leader.vctx);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);
	BUG_ON(group->id >= GROUP_ID_MAX);
	BUG_ON(index >= FRAMEMGR_MAX_REQUEST);

	device = group->device;
	framemgr = GET_SUBDEV_FRAMEMGR(&group->leader);

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_23, flags);

	fimc_is_frame_complete_head(framemgr, &frame);
	if (frame) {
		if (frame->index == index) {
			fimc_is_frame_trans_com_to_fre(framemgr, frame);

			frame->shot_ext->free_cnt = framemgr->frame_fre_cnt;
			frame->shot_ext->request_cnt = framemgr->frame_req_cnt;
			frame->shot_ext->process_cnt = framemgr->frame_pro_cnt;
			frame->shot_ext->complete_cnt = framemgr->frame_com_cnt;
		} else {
			mgerr("buffer index is NOT matched(%d != %d)", device, group,
				index, frame->index);
			fimc_is_frame_print_all(framemgr);
			ret = -EINVAL;
		}
	} else {
		mgerr("frame is empty from complete", device, group);
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_23, flags);

	PROGRAM_COUNT(15);

	return ret;
}

static int fimc_is_group_check_pre(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_device_ischain *device,
	struct fimc_is_group *gprev,
	struct fimc_is_group *group,
	struct fimc_is_group *gnext,
	struct fimc_is_frame *frame,
	struct fimc_is_group_frame **result)
{
	int ret = 0;
	struct fimc_is_group *group_leader;
	struct fimc_is_group_framemgr *gframemgr;
	struct fimc_is_group_frame *gframe;

	BUG_ON(!groupmgr);
	BUG_ON(!device);
	BUG_ON(!group);
	BUG_ON(!frame);
	BUG_ON(!frame->shot_ext);

	gframemgr = &groupmgr->gframemgr[device->instance];
	group_leader = groupmgr->leader[device->instance];

	/* invalid shot can be processed only on memory input */
	if (unlikely(!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state) &&
		frame->shot_ext->invalid)) {
		mgrerr("invalid shot", device, group, frame);
		ret = -EINVAL;
		goto p_err;
	}

	spin_lock_irq(&gframemgr->gframe_slock);

	if (gprev && !gnext) {
		/* tailer */
		fimc_is_gframe_group_head(group, &gframe);
		if (unlikely(!gframe)) {
			mgerr("gframe is NULL1", device, group);
			fimc_is_stream_status(groupmgr, group_leader);
			fimc_is_gframe_print_free(gframemgr);
			fimc_is_gframe_print_group(group_leader);
			spin_unlock_irq(&gframemgr->gframe_slock);
			ret = -EINVAL;
			goto p_err;
		}

		if (unlikely(frame->fcount != gframe->fcount)) {
			mgwarn("shot mismatch(%d != %d)", device, group,
				frame->fcount, gframe->fcount);
			gframe = fimc_is_gframe_rewind(groupmgr, group, frame->fcount);
			if (!gframe) {
				spin_unlock_irq(&gframemgr->gframe_slock);
				merr("rewinding is fail,can't recovery", group);
				goto p_err;
			}
		}

		fimc_is_gframe_s_info(gframe, group->slot, frame);
		fimc_is_gframe_check(gprev, group, gframe);
	} else if (!gprev && gnext) {
		/* leader */
		group->fcount++;

		fimc_is_gframe_free_head(gframemgr, &gframe);
		if (unlikely(!gframe)) {
			mgerr("gframe is NULL2", device, group);
			fimc_is_stream_status(groupmgr, group_leader);
			fimc_is_gframe_print_free(gframemgr);
			fimc_is_gframe_print_group(group_leader);
			spin_unlock_irq(&gframemgr->gframe_slock);
			group->fcount--;
			ret = -EINVAL;
			goto p_err;
		}

		if (unlikely(!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
			(frame->fcount != group->fcount))) {
			if (frame->fcount > group->fcount) {
				mgwarn("shot mismatch(%d, %d)", device, group,
					frame->fcount, group->fcount);
				group->fcount = frame->fcount;
			} else {
				spin_unlock_irq(&gframemgr->gframe_slock);
				mgerr("shot mismatch(%d, %d)", device, group,
					frame->fcount, group->fcount);
				group->fcount--;
				ret = -EINVAL;
				goto p_err;
			}
		}

		gframe->fcount = frame->fcount;
		fimc_is_gframe_s_info(gframe, group->slot, frame);
		fimc_is_gframe_check(gprev, group, gframe);
	} else if (gprev && gnext) {
		/* middler */
		fimc_is_gframe_group_head(group, &gframe);
		if (unlikely(!gframe)) {
			mgerr("gframe is NULL3", device, group);
			fimc_is_stream_status(groupmgr, group_leader);
			fimc_is_gframe_print_free(gframemgr);
			fimc_is_gframe_print_group(group_leader);
			spin_unlock_irq(&gframemgr->gframe_slock);
			ret = -EINVAL;
			goto p_err;
		}

		if (unlikely(frame->fcount != gframe->fcount)) {
			mgwarn("shot mismatch(%d != %d)", device, group,
				frame->fcount, gframe->fcount);
			gframe = fimc_is_gframe_rewind(groupmgr, group, frame->fcount);
			if (!gframe) {
				spin_unlock_irq(&gframemgr->gframe_slock);
				merr("rewinding is fail,can't recovery", group);
				goto p_err;
			}
		}

		fimc_is_gframe_s_info(gframe, group->slot, frame);
		fimc_is_gframe_check(gprev, group, gframe);
	} else {
		/* single */
		group->fcount++;

		fimc_is_gframe_free_head(gframemgr, &gframe);
		if (unlikely(!gframe)) {
			mgerr("gframe is NULL4", device, group);
			fimc_is_stream_status(groupmgr, group_leader);
			fimc_is_gframe_print_free(gframemgr);
			fimc_is_gframe_print_group(group_leader);
			spin_unlock_irq(&gframemgr->gframe_slock);
			ret = -EINVAL;
			goto p_err;
		}

		if (unlikely(!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
			(frame->fcount != group->fcount))) {
			if (frame->fcount > group->fcount) {
				mgwarn("shot mismatch(%d != %d)", device, group,
					frame->fcount, group->fcount);
				group->fcount = frame->fcount;
			} else {
				spin_unlock_irq(&gframemgr->gframe_slock);
				mgerr("shot mismatch(%d, %d)", device, group,
					frame->fcount, group->fcount);
				group->fcount--;
				ret = -EINVAL;
				goto p_err;
			}
		}

		fimc_is_gframe_s_info(gframe, group->slot, frame);
		fimc_is_gframe_check(gprev, group, gframe);
	}

	*result = gframe;

	spin_unlock_irq(&gframemgr->gframe_slock);

p_err:
	return ret;
}

static int fimc_is_group_check_post(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_device_ischain *device,
	struct fimc_is_group *gprev,
	struct fimc_is_group *group,
	struct fimc_is_group *gnext,
	struct fimc_is_frame *frame,
	struct fimc_is_group_frame *gframe)
{
	int ret = 0;
	struct fimc_is_group_framemgr *gframemgr;

	BUG_ON(!groupmgr);
	BUG_ON(!device);
	BUG_ON(!group);
	BUG_ON(!frame);
	BUG_ON(!gframe);

	gframemgr = &groupmgr->gframemgr[group->instance];

	spin_lock_irq(&gframemgr->gframe_slock);

	if (gprev && !gnext) {
		/* tailer */
		ret = fimc_is_gframe_trans_grp_to_fre(gframemgr, gframe, group);
		if (ret) {
			mgerr("fimc_is_gframe_trans_grp_to_fre is fail(%d)", device, group, ret);
			BUG();
		}
	} else if (!gprev && gnext) {
		/* leader */
		ret = fimc_is_gframe_trans_fre_to_grp(gframemgr, gframe, group, gnext);
		if (ret) {
			mgerr("fimc_is_gframe_trans_fre_to_grp is fail(%d)", device, group, ret);
			BUG();
		}
	} else if (gprev && gnext) {
		/* middler */
		ret = fimc_is_gframe_trans_grp_to_grp(gframemgr, gframe, group, gnext);
		if (ret) {
			mgerr("fimc_is_gframe_trans_grp_to_grp is fail(%d)", device, group, ret);
			BUG();
		}
	} else {
		/* single */
		gframe->fcount = frame->fcount;
	}

	spin_unlock_irq(&gframemgr->gframe_slock);

	return ret;
}

int fimc_is_group_shot(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame)
{
	int ret = 0;
	struct fimc_is_device_ischain *device;
	struct fimc_is_resourcemgr *resourcemgr;
	struct fimc_is_group *gprev, *gnext;
	struct fimc_is_group_frame *gframe;
	int async_step = 0;
	bool try_sdown = false;
	bool try_rdown = false;
	u32 slot;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!group->shot_callback);
	BUG_ON(!group->device);
	BUG_ON(!frame);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);
	BUG_ON(group->id >= GROUP_ID_MAX);

	set_bit(FIMC_IS_GROUP_SHOT, &group->state);
	atomic_dec(&group->rcount);
	slot = group->slot;

	if (unlikely(test_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state))) {
		mgwarn(" cancel by fstop1", group, group);
		ret = -EINVAL;
		goto p_err_cancel;
	}

	if (unlikely(test_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[slot]))) {
		mgerr(" cancel by gstop1", group, group);
		ret = -EINVAL;
		goto p_err_ignore;
	}

	PROGRAM_COUNT(2);
	ret = smp_shot_dec(group);
	if (ret) {
		mgerr(" down fail(%d) #1", group, group, ret);
		goto p_err_ignore;
	}
	try_sdown = true;

	PROGRAM_COUNT(3);
	ret = down_interruptible(&groupmgr->group_smp_res[slot]);
	if (ret) {
		mgerr(" down fail(%d) #2", group, group, ret);
		goto p_err_ignore;
	}
	try_rdown = true;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		if (smp_shot_get(group) < MIN_OF_SYNC_SHOTS) {
			PROGRAM_COUNT(4);
			ret = down_interruptible(&group->smp_trigger);
			if (ret) {
				mgerr(" down fail(%d) #3", group, group, ret);
				goto p_err_ignore;
			}
		} else {
			/*
			 * backup fcount can not be bigger than sensor fcount
			 * otherwise, duplicated shot can be generated.
			 * this is problem can be caused by hal qbuf timing
			 */
			if (atomic_read(&group->backup_fcount) >=
				atomic_read(&group->sensor_fcount)) {
				PROGRAM_COUNT(5);
				ret = down_interruptible(&group->smp_trigger);
				if (ret) {
					mgerr(" down fail(%d) #4", group, group, ret);
					goto p_err_ignore;
				}
			} else {
				/*
				 * this statement is execued only at initial.
				 * automatic increase the frame count of sensor
				 * for next shot without real frame start
				 */

				/* it's a async shot time */
				async_step = 1;
			}
		}

		if (unlikely(!test_bit(FRAME_INI_MEM, &frame->memory))) {
			mgerr(" frame memory is ALREADY released", group, group);
			ret = -EINVAL;
			goto p_err_ignore;
		}

		frame->fcount = atomic_read(&group->sensor_fcount);
		atomic_set(&group->backup_fcount, frame->fcount);
		frame->shot->dm.request.frameCount = frame->fcount;
		frame->shot->dm.sensor.timeStamp = fimc_is_get_timestamp();

		/* real automatic increase */
		if (async_step && (smp_shot_get(group) > MIN_OF_SYNC_SHOTS))
			atomic_inc(&group->sensor_fcount);
	}

	if (unlikely(test_bit(FIMC_IS_GROUP_FORCE_STOP, &group->state))) {
		mgwarn(" cancel by fstop2", group, group);
		ret = -EINVAL;
		goto p_err_cancel;
	}

	if (unlikely(test_bit(FIMC_IS_GGROUP_REQUEST_STOP, &groupmgr->group_state[slot]))) {
		mgerr(" cancel by gstop2", group, group);
		ret = -EINVAL;
		goto p_err_ignore;
	}

	PROGRAM_COUNT(6);
	device = group->device;
	gnext = group->gnext;
	gprev = group->gprev;
	resourcemgr = device->resourcemgr;
	gframe = NULL;

	ret = fimc_is_group_check_pre(groupmgr, device, gprev, group, gnext, frame, &gframe);
	if (unlikely(ret)) {
		merr(" fimc_is_group_check_pre is fail(%d)", device, ret);
		goto p_err_cancel;
	}

	if (unlikely(!gframe)) {
		merr(" gframe is NULL", device);
		goto p_err_cancel;
	}

#ifdef DEBUG_AA
	fimc_is_group_debug_aa_shot(group, frame);
#endif

#ifdef ENABLE_DVFS
	if ((!pm_qos_request_active(&device->user_qos)) && (sysfs_debug.en_dvfs)) {
		int scenario_id;

		mutex_lock(&resourcemgr->dvfs_ctrl.lock);

		/* try to find dynamic scenario to apply */
		scenario_id = fimc_is_dvfs_sel_dynamic(device);
		if (scenario_id > 0) {
			struct fimc_is_dvfs_scenario_ctrl *dynamic_ctrl = resourcemgr->dvfs_ctrl.dynamic_ctrl;
			mgrinfo("tbl[%d] dynamic scenario(%d)-[%s]\n", device, group, frame,
				resourcemgr->dvfs_ctrl.dvfs_table_idx,
				scenario_id,
				dynamic_ctrl->scenarios[dynamic_ctrl->cur_scenario_idx].scenario_nm);
			fimc_is_set_dvfs(device, scenario_id);
		}

		if ((scenario_id < 0) && (resourcemgr->dvfs_ctrl.dynamic_ctrl->cur_frame_tick == 0)) {
			struct fimc_is_dvfs_scenario_ctrl *static_ctrl = resourcemgr->dvfs_ctrl.static_ctrl;
			mgrinfo("tbl[%d] restore scenario(%d)-[%s]\n", device, group, frame,
				resourcemgr->dvfs_ctrl.dvfs_table_idx,
				static_ctrl->cur_scenario_id,
				static_ctrl->scenarios[static_ctrl->cur_scenario_idx].scenario_nm);
			fimc_is_set_dvfs(device, static_ctrl->cur_scenario_id);
		}

		mutex_unlock(&resourcemgr->dvfs_ctrl.lock);
	}
#endif

	PROGRAM_COUNT(7);

	ret = group->shot_callback(device, frame);
	if (unlikely(ret)) {
		mgerr(" shot_callback is fail(%d)", group, group, ret);
		goto p_err_cancel;
	}

	ret = fimc_is_group_check_post(groupmgr, device, gprev, group, gnext, frame, gframe);
	if (unlikely(ret)) {
		merr(" fimc_is_group_check_post is fail(%d)", device, ret);
		goto p_err_cancel;
	}

	fimc_is_itf_grp_shot(device, group, frame);
	atomic_inc(&group->scount);

	clear_bit(FIMC_IS_GROUP_SHOT, &group->state);
	PROGRAM_COUNT(12);

	return ret;

p_err_ignore:
	if (try_sdown)
		smp_shot_inc(group);

	if (try_rdown)
		up(&groupmgr->group_smp_res[slot]);

	clear_bit(FIMC_IS_GROUP_SHOT, &group->state);
	PROGRAM_COUNT(12);

	return ret;

p_err_cancel:
	fimc_is_group_cancel(group, frame);

	if (try_sdown)
		smp_shot_inc(group);

	if (try_rdown)
		up(&groupmgr->group_smp_res[slot]);

	clear_bit(FIMC_IS_GROUP_SHOT, &group->state);
	PROGRAM_COUNT(12);

	return ret;
}

int fimc_is_group_done(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame,
	u32 done_state)
{
	int ret = 0;
	struct fimc_is_device_ischain *device;
	struct fimc_is_group_framemgr *gframemgr;
	struct fimc_is_group_frame *gframe;
	struct fimc_is_group *gnext;

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!frame);
	BUG_ON(group->instance >= FIMC_IS_STREAM_COUNT);
	BUG_ON(group->id >= GROUP_ID_MAX);
	BUG_ON(!group->device);

	/* check shot & resource count validation */
	device = group->device;
	gnext = group->gnext;
	gframemgr = &groupmgr->gframemgr[group->instance];

	if (unlikely(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		(done_state != VB2_BUF_STATE_DONE))) {
		merr("G%d NOT DONE(reprocessing)\n", group, group->id);
		fimc_is_hw_logdump(device->interface);
	}

#ifdef DEBUG_AA
	fimc_is_group_debug_aa_done(group, frame);
#endif

	/* fd information backup */
	if (device->vra.leader == &group->leader)
		memcpy(&device->cur_peri_ctl.fdUd, &frame->shot->dm.stats,
			sizeof(struct camera2_fd_uctl));

	/* gframe should move to free list next group is existed and not done is oocured */
	if (unlikely((done_state != VB2_BUF_STATE_DONE) && gnext)) {
		spin_lock_irq(&gframemgr->gframe_slock);

		fimc_is_gframe_group_head(gnext, &gframe);
		if (gframe && (gframe->fcount == frame->fcount)) {
			ret = fimc_is_gframe_trans_grp_to_fre(gframemgr, gframe, gnext);
			if (ret) {
				mgerr("fimc_is_gframe_trans_grp_to_fre is fail(%d)", device, gnext, ret);
				BUG();
			}
		}

		spin_unlock_irq(&gframemgr->gframe_slock);
	}

	smp_shot_inc(group);
	up(&groupmgr->group_smp_res[group->slot]);

	return ret;
}
