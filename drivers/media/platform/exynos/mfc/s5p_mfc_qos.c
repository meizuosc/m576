/*
 * linux/drivers/media/video/exynos/mfc/s5p_mfc_qos.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>

#include <plat/cpu.h>

#include "s5p_mfc_common.h"
#include "s5p_mfc_debug.h"
#include "s5p_mfc_pm.h"
#include "s5p_mfc_reg.h"

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
enum {
	MFC_QOS_ADD,
	MFC_QOS_UPDATE,
	MFC_QOS_REMOVE,
};

static void mfc_qos_operate(struct s5p_mfc_ctx *ctx, int opr_type, int idx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_platdata *pdata = dev->pdata;
	struct s5p_mfc_qos *qos_table = pdata->qos_table;

	if (ctx->buf_process_type & MFCBUFPROC_COPY) {
		if (pdata->qos_extra[idx].thrd_mb != MFC_QOS_FLAG_NODATA) {
			qos_table = pdata->qos_extra;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
			mfc_info_ctx("[Replace QOS] int: %d(%d), mif: %d, cpu: %d, kfc: %d\n",
					qos_table[idx].freq_int,
					qos_table[idx].freq_mfc,
					qos_table[idx].freq_mif,
					qos_table[idx].freq_cpu,
					qos_table[idx].freq_kfc);
#endif
		} else {
			mfc_info_ctx("[Replace QOS] idx(%d) has no table\n", idx + 1);
		}
	}

	switch (opr_type) {
	case MFC_QOS_ADD:
		MFC_TRACE_CTX("++ QOS add[%d] (int:%d, mfc:%d)\n",
			idx, qos_table[idx].freq_int, qos_table[idx].freq_mfc);

		mutex_lock(&dev->curr_rate_lock);
		pm_qos_add_request(&dev->qos_req_int,
				PM_QOS_DEVICE_THROUGHPUT,
				qos_table[idx].freq_int);
		dev->curr_rate = qos_table[idx].freq_mfc;
		mutex_unlock(&dev->curr_rate_lock);

		MFC_TRACE_CTX("-- QOS add[%d] (int:%d, mfc:%d)\n",
			idx, qos_table[idx].freq_int, qos_table[idx].freq_mfc);

		pm_qos_add_request(&dev->qos_req_mif,
				PM_QOS_BUS_THROUGHPUT,
				qos_table[idx].freq_mif);

#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		pm_qos_add_request(&dev->qos_req_cluster1,
				PM_QOS_CLUSTER1_FREQ_MIN,
				qos_table[idx].freq_cpu);
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_add_request(&dev->qos_req_cluster1,
				PM_QOS_CLUSTER1_FREQ_MIN,
				qos_table[idx].freq_cpu);
		pm_qos_add_request(&dev->qos_req_cluster0,
				PM_QOS_CLUSTER0_FREQ_MIN,
				qos_table[idx].freq_kfc);
#endif
		atomic_set(&dev->qos_req_cur, idx + 1);
		mfc_dbg_ctx("QoS request: %d\n", idx + 1);
		break;
	case MFC_QOS_UPDATE:
		if (dev->curr_rate < qos_table[idx].freq_mfc) {
			MFC_TRACE_CTX("++ QOS update[%d] (int:%d, mfc:%d->%d)\n",
				idx, qos_table[idx].freq_int, dev->curr_rate, qos_table[idx].freq_mfc);

			mutex_lock(&dev->curr_rate_lock);
			pm_qos_update_request(&dev->qos_req_int,
					qos_table[idx].freq_int);
			dev->curr_rate = qos_table[idx].freq_mfc;
			mutex_unlock(&dev->curr_rate_lock);

			MFC_TRACE_CTX("-- QOS update[%d] (int:%d, mfc:%d->%d)\n",
				idx, qos_table[idx].freq_int, dev->curr_rate, qos_table[idx].freq_mfc);

			pm_qos_update_request(&dev->qos_req_mif,
					qos_table[idx].freq_mif);
		} else {
			MFC_TRACE_CTX("++ QOS update[%d] (int:%d, mfc:%d->%d)\n",
				idx, qos_table[idx].freq_int, dev->curr_rate, qos_table[idx].freq_mfc);

			mutex_lock(&dev->curr_rate_lock);
			dev->curr_rate = qos_table[idx].freq_mfc;
			pm_qos_update_request(&dev->qos_req_int,
					qos_table[idx].freq_int);
			mutex_unlock(&dev->curr_rate_lock);

			MFC_TRACE_CTX("-- QOS update[%d] (int:%d, mfc:%d->%d)\n",
				idx, qos_table[idx].freq_int, dev->curr_rate, qos_table[idx].freq_mfc);

			pm_qos_update_request(&dev->qos_req_mif,
					qos_table[idx].freq_mif);
		}

#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		pm_qos_update_request(&dev->qos_req_cluster1,
				qos_table[idx].freq_cpu);
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_update_request(&dev->qos_req_cluster1,
				qos_table[idx].freq_cpu);
		pm_qos_update_request(&dev->qos_req_cluster0,
				qos_table[idx].freq_kfc);
#endif
		atomic_set(&dev->qos_req_cur, idx + 1);
		mfc_dbg_ctx("QoS update: %d\n", idx + 1);
		break;
	case MFC_QOS_REMOVE:
		MFC_TRACE_CTX("++ QOS remove(prev mfc:%d)\n",
							dev->curr_rate);

		mutex_lock(&dev->curr_rate_lock);
		dev->curr_rate = dev->min_rate;
		pm_qos_remove_request(&dev->qos_req_int);
		mutex_unlock(&dev->curr_rate_lock);

		MFC_TRACE_CTX("-- QOS remove(prev mfc:%d)\n",
							dev->curr_rate);
		pm_qos_remove_request(&dev->qos_req_mif);

#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		pm_qos_remove_request(&dev->qos_req_cluster1);
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_remove_request(&dev->qos_req_cluster1);
		pm_qos_remove_request(&dev->qos_req_cluster0);
#endif
		atomic_set(&dev->qos_req_cur, 0);
		mfc_dbg_ctx("QoS remove\n");
		break;
	default:
		mfc_err_ctx("Unknown request for opr [%d]\n", opr_type);
		break;
	}
}

static void mfc_qos_print(struct s5p_mfc_ctx *ctx,
		struct s5p_mfc_qos *qos_table, int index)
{
#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
	mfc_debug(2, "\tint: %d(%d), mif: %d, cpu: %d\n",
			qos_table[index].freq_int,
			qos_table[index].freq_mfc,
			qos_table[index].freq_mif,
			qos_table[index].freq_cpu);
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	mfc_dbg_ctx("\tint: %d(%d), mif: %d, cpu: %d, kfc: %d\n",
			qos_table[index].freq_int,
			qos_table[index].freq_mfc,
			qos_table[index].freq_mif,
			qos_table[index].freq_cpu,
			qos_table[index].freq_kfc);
#endif
}
static void mfc_qos_add_or_update(struct s5p_mfc_ctx *ctx, int total_mb)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_platdata *pdata = dev->pdata;
	struct s5p_mfc_qos *qos_table = pdata->qos_table;
	int i;

	for (i = (pdata->num_qos_steps - 1); i >= 0; i--) {
		mfc_debug(7, "QoS index: %d\n", i + 1);
		if (total_mb > qos_table[i].thrd_mb) {
#if defined(CONFIG_SOC_EXYNOS5430)
			/* Table is different between MAX dec and enc */
			if (i == (pdata->num_qos_steps - 1) &&
				ctx->type == MFCINST_ENCODER) {
				mfc_debug(2, "Change Table for encoder\n");
				i = i - 1;
			}
#elif defined(CONFIG_SOC_EXYNOS7420)
			/* Table is different between decoder and encoder */
			if (dev->has_enc_ctx && qos_table[i].has_enc_table) {
				mfc_debug(2, "Table changes %d -> %d\n", i, i - 1);
				i = i - 1;
			}
#endif
			if (atomic_read(&dev->qos_req_cur) == 0) {
				mfc_qos_print(ctx, qos_table, i);
				mfc_qos_operate(ctx, MFC_QOS_ADD, i);
			} else if (atomic_read(&dev->qos_req_cur) != (i + 1)) {
				mfc_qos_print(ctx, qos_table, i);
				mfc_qos_operate(ctx, MFC_QOS_UPDATE, i);
			}
			break;
		}
	}
}

static inline int get_ctx_mb(struct s5p_mfc_ctx *ctx)
{
	int mb_width, mb_height, fps;

	mb_width = (ctx->img_width + 15) / 16;
	mb_height = (ctx->img_height + 15) / 16;
	fps = ctx->framerate / 1000;

	mfc_debug(2, "ctx[%d:%s], %d x %d @ %d fps\n", ctx->num,
			(ctx->type == MFCINST_ENCODER ? "ENC" : "DEC"),
			ctx->img_width, ctx->img_height, fps);

	return mb_width * mb_height * fps;
}

void s5p_mfc_qos_on(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_ctx *qos_ctx;
	int found = 0, total_mb = 0;

	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		total_mb += get_ctx_mb(qos_ctx);
		if (qos_ctx == ctx)
			found = 1;
	}

	if (!found) {
		list_add_tail(&ctx->qos_list, &dev->qos_queue);
		total_mb += get_ctx_mb(ctx);
	}

	dev->has_enc_ctx = 0;
	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		if (qos_ctx->type == MFCINST_ENCODER)
			dev->has_enc_ctx = 1;
	}

	mfc_qos_add_or_update(ctx, total_mb);
}

void s5p_mfc_qos_off(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_ctx *qos_ctx;
	int found = 0, total_mb = 0;

	if (list_empty(&dev->qos_queue)) {
		if (atomic_read(&dev->qos_req_cur) != 0) {
			mfc_err_ctx("MFC request count is wrong!\n");
			mfc_qos_operate(ctx, MFC_QOS_REMOVE, 0);
		}

		return;
	}

	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		total_mb += get_ctx_mb(qos_ctx);
		if (qos_ctx == ctx)
			found = 1;
	}

	if (found) {
		list_del(&ctx->qos_list);
		total_mb -= get_ctx_mb(ctx);
	}

	dev->has_enc_ctx = 0;
	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		if (qos_ctx->type == MFCINST_ENCODER)
			dev->has_enc_ctx = 1;
	}

	if (list_empty(&dev->qos_queue))
		mfc_qos_operate(ctx, MFC_QOS_REMOVE, 0);
	else
		mfc_qos_add_or_update(ctx, total_mb);
}
#endif
