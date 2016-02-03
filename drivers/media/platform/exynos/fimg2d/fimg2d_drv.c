/* linux/drivers/media/video/exynos/fimg2d/fimg2d_drv.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <asm/cacheflush.h>
#include <plat/cpu.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>
#include "fimg2d.h"
#include "fimg2d_clk.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

#define fimg2d_pm_qos_add(ctrl)				\
do { 							\
	fimg2d_pm_qos_add_cpu(ctrl);			\
	fimg2d_pm_qos_add_bus(ctrl);			\
} while (0)

#define fimg2d_pm_qos_remove(ctrl)			\
do {							\
	fimg2d_pm_qos_remove_cpu(ctrl);			\
	fimg2d_pm_qos_remove_bus(ctrl);			\
} while (0)

#define fimg2d_pm_qos_update(ctrl, status)		\
do {							\
	fimg2d_pm_qos_update_cpu(ctrl, status);		\
	fimg2d_pm_qos_update_bus(ctrl, status);		\
} while (0)

#define POLL_TIMEOUT	2	/* 2 msec */
#define POLL_RETRY	1000
#define CTX_TIMEOUT	msecs_to_jiffies(10000)	/* 10 sec */

#ifdef DEBUG
int g2d_debug = DBG_INFO;
module_param(g2d_debug, int, S_IRUGO | S_IWUSR);
#endif

static struct fimg2d_control *ctrl;
static struct fimg2d_qos g2d_qos_table[G2D_LV_END];

/* To prevent buffer release as memory compaction */
/* Lock */


void fimg2d_power_control(struct fimg2d_control *ctrl, enum fimg2d_pw_status status)
{
	if (status == FIMG2D_PW_ON) {
		if (ip_is_g2d_5h() || ip_is_g2d_5hp() || ip_is_g2d_7i()) {
			pm_runtime_get_sync(ctrl->dev);
			fimg2d_debug("Done pm_runtime_get_sync()\n");
		}
	} else if (status == FIMG2D_PW_OFF) {
		if (ip_is_g2d_5h() || ip_is_g2d_5hp())
			exynos5433_fimg2d_clk_set_osc(ctrl);
		if (ip_is_g2d_5h() || ip_is_g2d_5hp() || ip_is_g2d_7i()){
			pm_runtime_put_sync(ctrl->dev);
			fimg2d_debug("Done pm_runtime_put_sync()\n");
		}
	} else
		fimg2d_debug("status failed : %d\n", status);
}

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
void fimg2d_pm_qos_add_cpu(struct fimg2d_control *ctrl)
{
	pm_qos_add_request(&ctrl->exynos5_g2d_cluster1_qos, PM_QOS_CLUSTER1_FREQ_MIN, 0);
	pm_qos_add_request(&ctrl->exynos5_g2d_cluster0_qos, PM_QOS_CLUSTER0_FREQ_MIN, 0);
}

void fimg2d_pm_qos_remove_cpu(struct fimg2d_control *ctrl)
{
	pm_qos_remove_request(&ctrl->exynos5_g2d_cluster1_qos);
	pm_qos_remove_request(&ctrl->exynos5_g2d_cluster0_qos);
}

void fimg2d_pm_qos_update_cpu(struct fimg2d_control *ctrl,
				enum fimg2d_qos_status status)
{
	enum fimg2d_qos_level idx;
	unsigned long qflags;

	g2d_spin_lock(&ctrl->qoslock, qflags);
	if ((ctrl->qos_lv >= G2D_LV0) && (ctrl->qos_lv < G2D_LV_END))
		idx = ctrl->qos_lv;
	else
		goto err;
	g2d_spin_unlock(&ctrl->qoslock, qflags);

	if (status == FIMG2D_QOS_ON) {
		if (ctrl->pre_qos_lv != ctrl->qos_lv) {
#ifdef CONFIG_SCHED_HMP
			g2d_spin_lock(&ctrl->qoslock, qflags);
			if (idx == 0 && !ctrl->boost) {
				set_hmp_boost(true);
				ctrl->boost = true;
				fimg2d_debug("turn on hmp booster\n");
			}
			g2d_spin_unlock(&ctrl->qoslock, qflags);
#endif

			pm_qos_update_request(&ctrl->exynos5_g2d_cluster1_qos,
					g2d_qos_table[idx].freq_cpu);
			pm_qos_update_request(&ctrl->exynos5_g2d_cluster0_qos,
					g2d_qos_table[idx].freq_kfc);
			fimg2d_debug("idx:%d, freq_cpu:%d, freq_kfc:%d\n",
					idx, g2d_qos_table[idx].freq_cpu,
					g2d_qos_table[idx].freq_kfc);
		}
	} else if (status == FIMG2D_QOS_OFF) {
		pm_qos_update_request(&ctrl->exynos5_g2d_cluster1_qos, 0);
		pm_qos_update_request(&ctrl->exynos5_g2d_cluster0_qos, 0);

#ifdef CONFIG_SCHED_HMP
		g2d_spin_lock(&ctrl->qoslock, qflags);
		if (ctrl->boost) {
			set_hmp_boost(false);
			ctrl->boost = false;
			fimg2d_debug("turn off hmp booster\n");
		}
		g2d_spin_unlock(&ctrl->qoslock, qflags);
#endif
	}

	return;
err:
	fimg2d_debug("invalid qos_lv:%d\n", ctrl->qos_lv);
}
#else
void fimg2d_pm_qos_add_cpu(struct fimg2d_control *ctrl) {}
void fimg2d_pm_qos_remove_cpu(struct fimg2d_control *ctrl) {}
void fimg2d_pm_qos_update_cpu(struct fimg2d_control *ctrl,
				enum fimg2d_qos_status status) {}
#endif

#ifdef CONFIG_PM_DEVFREQ
void fimg2d_pm_qos_add_bus(struct fimg2d_control *ctrl)
{
	pm_qos_add_request(&ctrl->exynos5_g2d_mif_qos,
				PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&ctrl->exynos5_g2d_int_qos,
				PM_QOS_DEVICE_THROUGHPUT, 0);
}

void fimg2d_pm_qos_remove_bus(struct fimg2d_control *ctrl)
{
	pm_qos_remove_request(&ctrl->exynos5_g2d_mif_qos);
	pm_qos_remove_request(&ctrl->exynos5_g2d_int_qos);
}

void fimg2d_pm_qos_update_bus(struct fimg2d_control *ctrl,
				enum fimg2d_qos_status status)
{
	enum fimg2d_qos_level idx;
	int ret = 0;
	unsigned long qflags;

	g2d_spin_lock(&ctrl->qoslock, qflags);
	if ((ctrl->qos_lv >= G2D_LV0) && (ctrl->qos_lv < G2D_LV_END))
		idx = ctrl->qos_lv;
	else
		goto err;
	g2d_spin_unlock(&ctrl->qoslock, qflags);

	if (status == FIMG2D_QOS_ON) {
		if (ctrl->pre_qos_lv != ctrl->qos_lv) {
			pm_qos_update_request(&ctrl->exynos5_g2d_mif_qos,
					g2d_qos_table[idx].freq_mif);
			pm_qos_update_request(&ctrl->exynos5_g2d_int_qos,
					g2d_qos_table[idx].freq_int);
			fimg2d_debug("idx:%d, freq_mif:%d, freq_int:%d, ret:%d\n",
					idx, g2d_qos_table[idx].freq_mif,
					g2d_qos_table[idx].freq_int, ret);
		}
	} else if (status == FIMG2D_QOS_OFF) {
		pm_qos_update_request(&ctrl->exynos5_g2d_mif_qos, 0);
		pm_qos_update_request(&ctrl->exynos5_g2d_int_qos, 0);
	}

	return;
err:
	fimg2d_debug("invalid qos_lv:%d\n", ctrl->qos_lv);
}
#else
void fimg2d_pm_qos_add_bus(struct fimg2d_control *ctrl) {}
void fimg2d_pm_qos_remove_bus(struct fimg2d_control *ctrl) {}
void fimg2d_pm_qos_update_bus(struct fimg2d_control *ctrl,
				enum fimg2d_qos_status status) {}
#endif

static int fimg2d_do_bitblt(struct fimg2d_control *ctrl)
{
	int ret;

	pm_runtime_get_sync(ctrl->dev);
	fimg2d_debug("Done pm_runtime_get_sync()\n");

	fimg2d_clk_on(ctrl);
	ret = ctrl->blit(ctrl);
	fimg2d_clk_off(ctrl);

	pm_runtime_put_sync(ctrl->dev);
	fimg2d_debug("Done pm_runtime_put_sync()\n");

	return ret;
}

#ifdef BLIT_WORKQUE
static void fimg2d_worker(struct work_struct *work)
{
	fimg2d_debug("start kernel thread\n");
	fimg2d_do_bitblt(ctrl);
}
static DECLARE_WORK(fimg2d_work, fimg2d_worker);

static int fimg2d_context_wait(struct fimg2d_context *ctx)
{
	int ret;

	ret = wait_event_timeout(ctx->wait_q, !atomic_read(&ctx->ncmd),
			CTX_TIMEOUT);
	if (!ret) {
		fimg2d_err("ctx %p wait timeout\n", ctx);
		return -ETIME;
	}

	if (ctx->state == CTX_ERROR) {
		ctx->state = CTX_READY;
		fimg2d_err("ctx %p error before blit\n", ctx);
		return -EINVAL;
	}

	return 0;
}
#endif

static irqreturn_t fimg2d_irq(int irq, void *dev_id)
{
	fimg2d_debug("irq\n");
	spin_lock(&ctrl->bltlock);
	ctrl->stop(ctrl);
	spin_unlock(&ctrl->bltlock);

	return IRQ_HANDLED;
}

static int fimg2d_request_bitblt(struct fimg2d_control *ctrl,
		struct fimg2d_context *ctx)
{
#ifdef BLIT_WORKQUE
	unsigned long flags;

	g2d_spin_lock(&ctrl->bltlock, flags);
	fimg2d_debug("dispatch ctx %p to kernel thread\n", ctx);
	queue_work(ctrl->work_q, &fimg2d_work);
	g2d_spin_unlock(&ctrl->bltlock, flags);
	return fimg2d_context_wait(ctx);
#else
	return fimg2d_do_bitblt(ctrl);
#endif
}

static int fimg2d_open(struct inode *inode, struct file *file)
{
	struct fimg2d_context *ctx;
	unsigned long flags, qflags, count;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		fimg2d_err("not enough memory for ctx\n");
		return -ENOMEM;
	}
	file->private_data = (void *)ctx;

	g2d_spin_lock(&ctrl->bltlock, flags);
	fimg2d_add_context(ctrl, ctx);
	count = atomic_read(&ctrl->nctx);
	g2d_spin_unlock(&ctrl->bltlock, flags);

	if (count == 1) {
		g2d_spin_lock(&ctrl->qoslock, qflags);
		ctrl->pre_qos_lv = G2D_LV3;
		ctrl->qos_lv = G2D_LV2;
		g2d_spin_unlock(&ctrl->qoslock, qflags);
		fimg2d_pm_qos_update(ctrl, FIMG2D_QOS_ON);
	} else {
		fimg2d_debug("count:%ld, fimg2d_pm_pos_update is "
						"already called\n", count);
	}
	return 0;
}

static int fimg2d_release(struct inode *inode, struct file *file)
{
	struct fimg2d_context *ctx = file->private_data;
	int retry = POLL_RETRY;
	unsigned long flags, count;

	fimg2d_debug("ctx %p\n", ctx);
	while (retry--) {
		if (!atomic_read(&ctx->ncmd))
			break;
		mdelay(POLL_TIMEOUT);
	}

	g2d_spin_lock(&ctrl->bltlock, flags);
	fimg2d_del_context(ctrl, ctx);
	count = atomic_read(&ctrl->nctx);
	g2d_spin_unlock(&ctrl->bltlock, flags);

	if (!count)
		fimg2d_pm_qos_update(ctrl, FIMG2D_QOS_OFF);
	else {
		fimg2d_debug("count:%ld, fimg2d_pm_pos_update is "
						"not called yet\n", count);
	}
	kfree(ctx);
	return 0;
}

static int fimg2d_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
}

static unsigned int fimg2d_poll(struct file *file, struct poll_table_struct *wait)
{
	return 0;
}

static int store_user_dst(struct fimg2d_blit __user *buf,
		struct fimg2d_dma *dst_buf)
{
	struct fimg2d_blit blt;
	struct fimg2d_clip *clp;
	struct fimg2d_image dst_img;
	int clp_h, bpp, stride;

	int len = sizeof(struct fimg2d_image);

	memset(&dst_img, 0, len);

	if (copy_from_user(&blt, buf, sizeof(blt)))
		return -EFAULT;

	if (blt.dst)
		if (copy_from_user(&dst_img, blt.dst, len))
			return -EFAULT;

	clp = &blt.param.clipping;
	clp_h = clp->y2 - clp->y1;

	bpp = bit_per_pixel(&dst_img, 0);
	stride = width2bytes(dst_img.width, bpp);

	dst_buf->addr = dst_img.addr.start + (stride * clp->y1);
	dst_buf->size = stride * clp_h;

	return 0;
}

static long fimg2d_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct fimg2d_context *ctx;
	struct mm_struct *mm;
	struct fimg2d_dma *usr_dst;

	ctx = file->private_data;

	switch (cmd) {
	case FIMG2D_BITBLT_BLIT:

		mm = get_task_mm(current);
		if (!mm) {
			fimg2d_err("no mm for ctx\n");
			return -ENXIO;
		}

		mutex_lock(&ctrl->drvlock);
		ctx->mm = mm;

		if (atomic_read(&ctrl->drvact) ||
				atomic_read(&ctrl->suspended)) {
			fimg2d_err("driver is unavailable, do sw fallback\n");
			mutex_unlock(&ctrl->drvlock);
			mmput(mm);
			return -EPERM;
		}

		ret = fimg2d_add_command(ctrl, ctx, (struct fimg2d_blit __user *)arg);
		if (ret) {
			fimg2d_err("add command not allowed.\n");
			mutex_unlock(&ctrl->drvlock);
			mmput(mm);
			return ret;
		}

		fimg2d_pm_qos_update(ctrl, FIMG2D_QOS_ON);

		usr_dst = kzalloc(sizeof(struct fimg2d_dma), GFP_KERNEL);
		if (!usr_dst) {
			fimg2d_err("failed to allocate memory for fimg2d_dma\n");
			mutex_unlock(&ctrl->drvlock);
			mmput(mm);
			return -ENOMEM;
		}

		ret = store_user_dst((struct fimg2d_blit __user *)arg, usr_dst);
		if (ret) {
			fimg2d_err("store_user_dst() not allowed.\n");
			mutex_unlock(&ctrl->drvlock);
			kfree(usr_dst);
			mmput(mm);
			return ret;
		}

		ret = fimg2d_request_bitblt(ctrl, ctx);
		if (ret) {
			fimg2d_info("request bitblit not allowed, "
					"so passing to s/w fallback.\n");
			mutex_unlock(&ctrl->drvlock);
			kfree(usr_dst);
			mmput(mm);
			return -EBUSY;
		}

		mutex_unlock(&ctrl->drvlock);

		fimg2d_debug("addr : %p, size : %zd\n",
				(void *)usr_dst->addr, usr_dst->size);
#ifndef CCI_SNOOP
		fimg2d_dma_unsync_inner(usr_dst->addr,
				usr_dst->size, DMA_FROM_DEVICE);
#endif
		kfree(usr_dst);
		mmput(mm);
		break;

	case FIMG2D_BITBLT_VERSION:
	{
		struct fimg2d_version ver;
		struct fimg2d_platdata *pdata;

#ifdef CONFIG_OF
		pdata = ctrl->pdata;
#else
		pdata = to_fimg2d_plat(ctrl->dev);

#endif
		ver.hw = pdata->hw_ver;
		ver.sw = 0;
		fimg2d_info("version info. hw(0x%x), sw(0x%x)\n",
				ver.hw, ver.sw);
		if (copy_to_user((void *)arg, &ver, sizeof(ver)))
			return -EFAULT;
		break;
	}
	case FIMG2D_BITBLT_ACTIVATE:
	{
		enum driver_act act;

		if (copy_from_user(&act, (void *)arg, sizeof(act)))
			return -EFAULT;

		mutex_lock(&ctrl->drvlock);
		atomic_set(&ctrl->drvact, act);
		if (act == DRV_ACT) {
			fimg2d_power_control(ctrl, FIMG2D_PW_OFF);
			fimg2d_info("fimg2d driver is activated\n");
		} else {
			fimg2d_power_control(ctrl, FIMG2D_PW_ON);
			fimg2d_info("fimg2d driver is deactivated\n");
		}
		mutex_unlock(&ctrl->drvlock);
		break;
	}
	default:
		fimg2d_err("unknown ioctl\n");
		ret = -EFAULT;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static int compat_get_fimg2d_param(struct fimg2d_param __user *data,
				struct compat_fimg2d_param __user *data32)
{
	int err;
	compat_ulong_t ul;
	compat_int_t i;
	enum rotation rotate_mode;
	enum premultiplied premult;
	enum scaling scaling_mode;
	enum repeat repeat_mode;
	enum bluescreen bluscr_mode;
	unsigned char g_alpha;
	bool b;

	err = get_user(ul, &data32->solid_color);
	err |= put_user(ul, &data->solid_color);
	err |= get_user(g_alpha, &data32->g_alpha);
	err |= put_user(g_alpha, &data->g_alpha);
	err |= get_user(b, &data32->dither);
	err |= put_user(b, &data->dither);
	err |= get_user(rotate_mode, &data32->rotate);
	err |= put_user(rotate_mode, &data->rotate);
	err |= get_user(premult, &data32->premult);
	err |= put_user(premult, &data->premult);
	err |= get_user(scaling_mode, &data32->scaling.mode);
	err |= put_user(scaling_mode, &data->scaling.mode);
	err |= get_user(i, &data32->scaling.src_w);
	err |= put_user(i, &data->scaling.src_w);
	err |= get_user(i, &data32->scaling.src_h);
	err |= put_user(i, &data->scaling.src_h);
	err |= get_user(i, &data32->scaling.dst_w);
	err |= put_user(i, &data->scaling.dst_w);
	err |= get_user(i, &data32->scaling.dst_h);
	err |= put_user(i, &data->scaling.dst_h);
	err |= get_user(repeat_mode, &data32->repeat.mode);
	err |= put_user(repeat_mode, &data->repeat.mode);
	err |= get_user(ul, &data32->repeat.pad_color);
	err |= put_user(ul, &data->repeat.pad_color);
	err |= get_user(bluscr_mode, &data32->bluscr.mode);
	err |= put_user(bluscr_mode, &data->bluscr.mode);
	err |= get_user(ul, &data32->bluscr.bs_color);
	err |= put_user(ul, &data->bluscr.bg_color);
	err |= get_user(b, &data32->clipping.enable);
	err |= put_user(b, &data->clipping.enable);
	err |= get_user(i, &data32->clipping.x1);
	err |= put_user(i, &data->clipping.x1);
	err |= get_user(i, &data32->clipping.y1);
	err |= put_user(i, &data->clipping.y1);
	err |= get_user(i, &data32->clipping.x2);
	err |= put_user(i, &data->clipping.x2);
	err |= get_user(i, &data32->clipping.y2);
	err |= put_user(i, &data->clipping.y2);

	return err;
}

static int compat_get_fimg2d_image(struct fimg2d_image __user *data,
						compat_uptr_t uaddr)
{
	struct compat_fimg2d_image __user *data32 = compat_ptr(uaddr);
	compat_int_t i;
	compat_ulong_t ul;
	enum pixel_order order;
	enum color_format fmt;
	enum addr_space addr_type;
	bool need_cacheopr;
	int err;

	err = get_user(i, &data32->width);
	err |= put_user(i, &data->width);
	err |= get_user(i, &data32->height);
	err |= put_user(i, &data->height);
	err |= get_user(i, &data32->stride);
	err |= put_user(i, &data->stride);
	err |= get_user(order, &data32->order);
	err |= put_user(order, &data->order);
	err |= get_user(fmt, &data32->fmt);
	err |= put_user(fmt, &data->fmt);
	err |= get_user(addr_type, &data32->addr.type);
	err |= put_user(addr_type, &data->addr.type);
	err |= get_user(ul, &data32->addr.start);
	err |= put_user(ul, &data->addr.start);
	err |= get_user(addr_type, &data32->plane2.type);
	err |= put_user(addr_type, &data->plane2.type);
	err |= get_user(ul, &data32->plane2.start);
	err |= put_user(ul, &data->plane2.start);
	err |= get_user(i, &data32->rect.x1);
	err |= put_user(i, &data->rect.x1);
	err |= get_user(i, &data32->rect.y1);
	err |= put_user(i, &data->rect.y1);
	err |= get_user(i, &data32->rect.x2);
	err |= put_user(i, &data->rect.x2);
	err |= get_user(i, &data32->rect.y2);
	err |= put_user(i, &data->rect.y2);
	err |= get_user(need_cacheopr, &data32->need_cacheopr);
	err |= put_user(need_cacheopr, &data->need_cacheopr);

	return err;
}

static long compat_fimg2d_ioctl32(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	switch (cmd) {
	case COMPAT_FIMG2D_BITBLT_BLIT:
	{
		struct compat_fimg2d_blit __user *data32;
		struct fimg2d_blit __user *data;
		struct mm_struct *mm;
		enum blit_op op;
		enum blit_sync sync;
		enum fimg2d_qos_level qos_lv;
		compat_uint_t seq_no;
		unsigned long stack_cursor = 0;
		int err;

		mm = get_task_mm(current);
		if (!mm) {
			fimg2d_err("no mm for ctx\n");
			return -ENXIO;
		}

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data) {
			fimg2d_err("failed to allocate user compat space\n");
			mmput(mm);
			return -ENOMEM;
		}

		stack_cursor += sizeof(*data);
		if (clear_user(data, sizeof(*data))) {
				 fimg2d_err("failed to access to userspace\n");
				 mmput(mm);
				 return -EPERM;
		 }

		err = get_user(op, &data32->op);
		err |= put_user(op, &data->op);
		if (err) {
			fimg2d_err("failed to get compat data\n");
			mmput(mm);
			return err;
		}

		err = compat_get_fimg2d_param(&data->param, &data32->param);
		if (err) {
			fimg2d_err("failed to get compat data\n");
			mmput(mm);
			return err;
		}

		if (data32->src) {
			data->src = compat_alloc_user_space(sizeof(*data->src) +
								stack_cursor);
			if (!data->src) {
				fimg2d_err("failed to allocate user compat space\n");
				mmput(mm);
				return -ENOMEM;
			}

			stack_cursor += sizeof(*data->src);
			err = compat_get_fimg2d_image(data->src, data32->src);
			if (err) {
				fimg2d_err("failed to get compat data\n");
				mmput(mm);
				return err;
			}
		}

		if (data32->msk) {
			data->msk = compat_alloc_user_space(sizeof(*data->msk) +
								stack_cursor);
			if (!data->msk) {
				fimg2d_err("failed to allocate user compat space\n");
				mmput(mm);
				return -ENOMEM;
			}

			stack_cursor += sizeof(*data->msk);
			err = compat_get_fimg2d_image(data->msk, data32->msk);
			if (err) {
				fimg2d_err("failed to get compat data\n");
				mmput(mm);
				return err;
			}
		}

		if (data32->tmp) {
			data->tmp = compat_alloc_user_space(sizeof(*data->tmp) +
								stack_cursor);
			if (!data->tmp) {
				fimg2d_err("failed to allocate user compat space\n");
				mmput(mm);
				return -ENOMEM;
			}

			stack_cursor += sizeof(*data->tmp);
			err = compat_get_fimg2d_image(data->tmp, data32->tmp);
			if (err) {
				fimg2d_err("failed to get compat data\n");
				mmput(mm);
				return err;
			}
		}

		if (data32->dst) {
			data->dst = compat_alloc_user_space(sizeof(*data->dst) +
								stack_cursor);
			if (!data->dst) {
				fimg2d_err("failed to allocate user compat space\n");
				mmput(mm);
				return -ENOMEM;
			}

			stack_cursor += sizeof(*data->dst);
			err = compat_get_fimg2d_image(data->dst, data32->dst);
			if (err) {
				fimg2d_err("failed to get compat data\n");
				mmput(mm);
				return err;
			}
		}

		err = get_user(sync, &data32->sync);
		err |= put_user(sync, &data->sync);
		if (err) {
			fimg2d_err("failed to get compat data\n");
			mmput(mm);
			return err;
		}

		err = get_user(seq_no, &data32->seq_no);
		err |= put_user(seq_no, &data->seq_no);
		if (err) {
			fimg2d_err("failed to get compat data\n");
			mmput(mm);
			return err;
		}

		err = get_user(qos_lv, &data32->qos_lv);
		err |= put_user(qos_lv, &data->qos_lv);
		if (err) {
			fimg2d_err("failed to get compat data\n");
			mmput(mm);
			return err;
		}

		err = file->f_op->unlocked_ioctl(file,
				FIMG2D_BITBLT_BLIT, (unsigned long)data);
		mmput(mm);
		return err;
	}
	case COMPAT_FIMG2D_BITBLT_VERSION:
	{
		struct compat_fimg2d_version __user *data32;
		struct fimg2d_version __user *data;
		compat_uint_t i;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data) {
			fimg2d_err("failed to allocate user compat space\n");
			return -ENOMEM;
		}

		err = get_user(i, &data32->hw);
		err |= put_user(i, &data->hw);
		err |= get_user(i, &data32->sw);
		err |= put_user(i, &data->sw);

		if (err)
			return err;

		return file->f_op->unlocked_ioctl(file,
				FIMG2D_BITBLT_VERSION, (unsigned long)data);
	}
	case FIMG2D_BITBLT_ACTIVATE:
	{
		return file->f_op->unlocked_ioctl(file,
				FIMG2D_BITBLT_ACTIVATE, arg);
	}
	default:
		fimg2d_err("unknown ioctl\n");
		return -EINVAL;
	}
}
#endif

/* fops */
static const struct file_operations fimg2d_fops = {
	.owner          = THIS_MODULE,
	.open           = fimg2d_open,
	.release        = fimg2d_release,
	.mmap           = fimg2d_mmap,
	.poll           = fimg2d_poll,
	.unlocked_ioctl = fimg2d_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl 	= compat_fimg2d_ioctl32,
#endif
};

/* miscdev */
static struct miscdevice fimg2d_dev = {
	.minor		= FIMG2D_MINOR,
	.name		= "fimg2d",
	.fops		= &fimg2d_fops,
};

static int fimg2d_setup_controller(struct fimg2d_control *ctrl)
{
	atomic_set(&ctrl->drvact, DRV_ACT);
	atomic_set(&ctrl->suspended, 0);
	atomic_set(&ctrl->clkon, 0);
	atomic_set(&ctrl->busy, 0);
	atomic_set(&ctrl->nctx, 0);

	spin_lock_init(&ctrl->bltlock);
	mutex_init(&ctrl->drvlock);
	ctrl->boost = false;

	INIT_LIST_HEAD(&ctrl->cmd_q);
	init_waitqueue_head(&ctrl->wait_q);
	fimg2d_register_ops(ctrl);

#ifdef BLIT_WORKQUE
	ctrl->work_q = create_singlethread_workqueue("kfimg2dd");
	if (!ctrl->work_q)
		return -ENOMEM;
#endif

	return 0;
}

#ifdef CONFIG_OF
static int parse_g2d_qos_platdata(struct device_node *np, char *node_name,
         struct fimg2d_qos *pdata)
{
	int ret = 0;
	struct device_node *np_qos;

	np_qos = of_find_node_by_name(np, node_name);
	if (!np_qos) {
		pr_err("%s: could not find fimg2d qos platdata node\n",
				node_name);
		return -EINVAL;
	}

	of_property_read_u32(np_qos, "freq_int", &pdata->freq_int);
	of_property_read_u32(np_qos, "freq_mif", &pdata->freq_mif);
	of_property_read_u32(np_qos, "freq_cpu", &pdata->freq_cpu);
	of_property_read_u32(np_qos, "freq_kfc", &pdata->freq_kfc);

	fimg2d_info("cpu_min:%d, kfc_min:%d, mif_min:%d, int_min:%d\n"
			, pdata->freq_cpu, pdata->freq_kfc
			, pdata->freq_mif, pdata->freq_int);

	return ret;
}

static void g2d_parse_dt(struct device_node *np, struct fimg2d_platdata *pdata)
{
	struct device_node *np_qos;

	if (!np)
		return;

	of_property_read_u32(np, "ip_ver", &pdata->ip_ver);

	np_qos = of_get_child_by_name(np, "g2d_qos_table");
	if (!np_qos) {
		struct device_node *np_pdata =
				of_find_node_by_name(NULL, "fimg2d_pdata");
		if (!np_pdata)
			BUG();

		np_qos = of_get_child_by_name(np_pdata, "g2d_qos_table");
		if (!np_qos)
			BUG();
	}

	parse_g2d_qos_platdata(np_qos, "g2d_qos_variant_0", &g2d_qos_table[0]);
	parse_g2d_qos_platdata(np_qos, "g2d_qos_variant_1", &g2d_qos_table[1]);
	parse_g2d_qos_platdata(np_qos, "g2d_qos_variant_2", &g2d_qos_table[2]);
	parse_g2d_qos_platdata(np_qos, "g2d_qos_variant_3", &g2d_qos_table[3]);
	parse_g2d_qos_platdata(np_qos, "g2d_qos_variant_4", &g2d_qos_table[4]);

}
#else
static void g2d_parse_dt(struct device_node *np, struct g2d_dev *gsc)
{
	return;
}
#endif

static int fimg2d_sysmmu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long iova, int flags, void *token)
{
	struct fimg2d_bltcmd *cmd;

	cmd = fimg2d_get_command(ctrl);
	if (!cmd) {
		fimg2d_err("no available command\n");
		goto done;
	}

	fimg2d_debug_command(cmd);

	if (atomic_read(&ctrl->busy)) {
		fimg2d_err("dumping g2d registers..\n");
		ctrl->dump(ctrl);
	}
done:
	return 0;
}

static int fimg2d_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct fimg2d_platdata *pdata;
#ifdef CONFIG_OF
	struct device *dev = &pdev->dev;
	int id = 0;
#else
	pdata = to_fimg2d_plat(&pdev->dev);
#endif

	dev_info(&pdev->dev, "++%s\n", __func__);

#ifdef CONFIG_OF
	if (dev->of_node) {
		id = of_alias_get_id(pdev->dev.of_node, "fimg2d");
	} else {
		id = pdev->id;
		pdata = dev->platform_data;
		if (!pdata) {
			dev_err(&pdev->dev, "no platform data\n");
			return -EINVAL;
		}
	}
#else
	if (!to_fimg2d_plat(&pdev->dev)) {
		fimg2d_err("failed to get platform data\n");
		return -ENOMEM;
	}
#endif
	/* global structure */
	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl) {
		fimg2d_err("failed to allocate memory for controller\n");
		return -ENOMEM;
	}

#ifdef CONFIG_OF
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		fimg2d_err("failed to allocate memory for controller\n");
		kfree(ctrl);
		return -ENOMEM;
	}
	ctrl->pdata = pdata;
	g2d_parse_dt(dev->of_node, ctrl->pdata);
#endif

	/* setup global ctrl */
	ret = fimg2d_setup_controller(ctrl);
	if (ret) {
		fimg2d_err("failed to setup controller\n");
		goto drv_free;
	}
	ctrl->dev = &pdev->dev;

	/* memory region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		fimg2d_err("failed to get resource\n");
		ret = -ENOENT;
		goto drv_free;
	}

	ctrl->mem = request_mem_region(res->start, resource_size(res),
					pdev->name);
	if (!ctrl->mem) {
		fimg2d_err("failed to request memory region\n");
		ret = -ENOMEM;
		goto drv_free;
	}

	/* ioremap */
	ctrl->regs = ioremap(res->start, resource_size(res));
	if (!ctrl->regs) {
		fimg2d_err("failed to ioremap for SFR\n");
		ret = -ENOENT;
		goto mem_free;
	}
	fimg2d_debug("base address: 0x%lx\n", (unsigned long)res->start);

	/* irq */
	ctrl->irq = platform_get_irq(pdev, 0);
	if (!ctrl->irq) {
		fimg2d_err("failed to get irq resource\n");
		ret = -ENOENT;
		goto reg_unmap;
	}
	fimg2d_debug("irq: %d\n", ctrl->irq);

	ret = request_irq(ctrl->irq, fimg2d_irq, IRQF_DISABLED,
			pdev->name, ctrl);
	if (ret) {
		fimg2d_err("failed to request irq\n");
		ret = -ENOENT;
		goto reg_unmap;
	}

	ret = fimg2d_clk_setup(ctrl);
	if (ret) {
		fimg2d_err("failed to setup clk\n");
		ret = -ENOENT;
		goto irq_free;
	}

	spin_lock_init(&ctrl->qoslock);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(ctrl->dev);
	fimg2d_info("enable runtime pm\n");
#else
	fimg2d_clk_on(ctrl);
#endif

#ifdef FIMG2D_IOVMM_PAGETABLE
	exynos_create_iovmm(dev, 3, 3);
#endif
	iovmm_set_fault_handler(dev, fimg2d_sysmmu_fault_handler, ctrl);

	fimg2d_debug("register sysmmu page fault handler\n");

	/* misc register */
	ret = misc_register(&fimg2d_dev);
	if (ret) {
		fimg2d_err("failed to register misc driver\n");
		goto clk_release;
	}

	fimg2d_pm_qos_add(ctrl);

	dev_info(&pdev->dev, "fimg2d registered successfully\n");

	return 0;

clk_release:
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(ctrl->dev);
#else
	fimg2d_clk_off(ctrl);
#endif
	fimg2d_clk_release(ctrl);

irq_free:
	free_irq(ctrl->irq, NULL);
reg_unmap:
	iounmap(ctrl->regs);
mem_free:
	release_mem_region(res->start, resource_size(res));
drv_free:
#ifdef BLIT_WORKQUE
	if (ctrl->work_q)
		destroy_workqueue(ctrl->work_q);
#endif
	mutex_destroy(&ctrl->drvlock);
#ifdef CONFIG_OF
	kfree(pdata);
#endif
	kfree(ctrl);

	return ret;
}

static int fimg2d_remove(struct platform_device *pdev)
{
	struct fimg2d_platdata *pdata;
#ifdef CONFIG_OF
	pdata = ctrl->pdata;
#else
	pdata = to_fimg2d_plat(ctrl->dev);
#endif
	fimg2d_pm_qos_remove(ctrl);

	misc_deregister(&fimg2d_dev);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&pdev->dev);
#else
	fimg2d_clk_off(ctrl);
#endif

	fimg2d_clk_release(ctrl);
	free_irq(ctrl->irq, NULL);

	if (ctrl->mem) {
		iounmap(ctrl->regs);
		release_resource(ctrl->mem);
		kfree(ctrl->mem);
	}

#ifdef BLIT_WORKQUE
	destroy_workqueue(ctrl->work_q);
#endif
	mutex_destroy(&ctrl->drvlock);
	kfree(ctrl);
	kfree(pdata);
	return 0;
}

static int fimg2d_suspend(struct device *dev)
{
	unsigned long flags;
	int retry = POLL_RETRY;

	g2d_spin_lock(&ctrl->bltlock, flags);
	atomic_set(&ctrl->suspended, 1);
	g2d_spin_unlock(&ctrl->bltlock, flags);
	while (retry--) {
		if (fimg2d_queue_is_empty(&ctrl->cmd_q))
			break;
		mdelay(POLL_TIMEOUT);
	}
	if (ip_is_g2d_5h() || ip_is_g2d_5hp())
		exynos5433_fimg2d_clk_set_osc(ctrl);
	fimg2d_info("suspend... done\n");
	return 0;
}

static int fimg2d_resume(struct device *dev)
{
	unsigned long flags;
	int ret = 0;

	g2d_spin_lock(&ctrl->bltlock, flags);
	atomic_set(&ctrl->suspended, 0);
	g2d_spin_unlock(&ctrl->bltlock, flags);
	/* G2D clk gating mask */
	if (ip_is_g2d_5ar2()) {
		fimg2d_clk_on(ctrl);
		fimg2d_clk_off(ctrl);
	} else if (ip_is_g2d_5hp()) {
		ret = exynos5430_fimg2d_clk_set(ctrl);
		if (ret) {
			fimg2d_err("failed to exynos5430_fimg2d_clk_set()\n");
			return -ENOENT;
		}
	}
	fimg2d_info("resume... done\n");
	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int fimg2d_runtime_suspend(struct device *dev)
{
	fimg2d_debug("runtime suspend... done\n");
	if (ip_is_g2d_5h() || ip_is_g2d_5hp())
		exynos5433_fimg2d_clk_set_osc(ctrl);
	return 0;
}

static int fimg2d_runtime_resume(struct device *dev)
{
	int ret = 0;

	if (ip_is_g2d_5r()) {
		ret = fimg2d_clk_set_gate(ctrl);
		if (ret) {
			fimg2d_err("failed to fimg2d_clk_set_gate()\n");
			ret = -ENOENT;
		}
	} else if (ip_is_g2d_5h() || ip_is_g2d_5hp()) {
		ret = exynos5430_fimg2d_clk_set(ctrl);
		if (ret) {
			fimg2d_err("failed to exynos5430_fimg2d_clk_set()\n");
			ret = -ENOENT;
		}
	}

	fimg2d_debug("runtime resume... done\n");
	return ret;
}
#endif

static const struct dev_pm_ops fimg2d_pm_ops = {
	.suspend		= fimg2d_suspend,
	.resume			= fimg2d_resume,
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend	= fimg2d_runtime_suspend,
	.runtime_resume		= fimg2d_runtime_resume,
#endif
};

static const struct of_device_id exynos_fimg2d_match[] = {
	{
		.compatible = "samsung,s5p-fimg2d",
	},
	{},
};

MODULE_DEVICE_TABLE(of, exynos_fimg2d_match);

static struct platform_driver fimg2d_driver = {
	.probe		= fimg2d_probe,
	.remove		= fimg2d_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s5p-fimg2d",
		.pm     = &fimg2d_pm_ops,
		.of_match_table = exynos_fimg2d_match,
	},
};

static char banner[] __initdata =
	"Exynos Graphics 2D driver, (c) 2011 Samsung Electronics\n";

static int __init fimg2d_register(void)
{
	pr_info("%s", banner);
	return platform_driver_register(&fimg2d_driver);
}

static void __exit fimg2d_unregister(void)
{
	platform_driver_unregister(&fimg2d_driver);
}

int fimg2d_ip_version_is(void)
{
	struct fimg2d_platdata *pdata;

#ifdef CONFIG_OF
	pdata = ctrl->pdata;
#else
	pdata = to_fimg2d_plat(ctrl->dev);
#endif

	return pdata->ip_ver;
}

module_init(fimg2d_register);
module_exit(fimg2d_unregister);

MODULE_AUTHOR("Eunseok Choi <es10.choi@samsung.com>");
MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_DESCRIPTION("Samsung Graphics 2D driver");
MODULE_LICENSE("GPL");
