/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_FRAME_MGR_H
#define FIMC_IS_FRAME_MGR_H

#include <linux/kthread.h>
#include "fimc-is-time.h"
#include "fimc-is-config.h"

#define FIMC_IS_MAX_BUFS	VIDEO_MAX_FRAME
#define FIMC_IS_MAX_PLANES	VIDEO_MAX_PLANES

#define FRAMEMGR_ID_INVALID	0x000000
#define FRAMEMGR_ID_SSX 	0x000100
#define FRAMEMGR_ID_3XS 	0x000200
#define FRAMEMGR_ID_3XC 	0x000400
#define FRAMEMGR_ID_3XP 	0x000800
#define FRAMEMGR_ID_IXS 	0x001000
#define FRAMEMGR_ID_IXC 	0x002000
#define FRAMEMGR_ID_IXP 	0x004000
#define FRAMEMGR_ID_DIS 	0x008000
#define FRAMEMGR_ID_SCC 	0x010000
#define FRAMEMGR_ID_SCP 	0x020000
#define FRAMEMGR_ID_SHOT	(FRAMEMGR_ID_SSX | FRAMEMGR_ID_3XS | \
				 FRAMEMGR_ID_IXS | FRAMEMGR_ID_DIS)
#define FRAMEMGR_ID_STREAM	(FRAMEMGR_ID_3XC | FRAMEMGR_ID_3XP | \
				 FRAMEMGR_ID_SCC | FRAMEMGR_ID_DIS | \
				 FRAMEMGR_ID_SCP)
/* #define TRACE_FRAME */
#define TRACE_ID		(FRAMEMGR_ID_SHOT | FRAMEMGR_ID_STREAM)

#define FRAMEMGR_MAX_REQUEST	VIDEO_MAX_FRAME

#define FMGR_IDX_0		(1 << 0 )
#define FMGR_IDX_1		(1 << 1 )
#define FMGR_IDX_2		(1 << 2 )
#define FMGR_IDX_3		(1 << 3 )
#define FMGR_IDX_4		(1 << 4 )
#define FMGR_IDX_5		(1 << 5 )
#define FMGR_IDX_6		(1 << 6 )
#define FMGR_IDX_7		(1 << 7 )
#define FMGR_IDX_8		(1 << 8 )
#define FMGR_IDX_9		(1 << 9 )
#define FMGR_IDX_10		(1 << 10)
#define FMGR_IDX_11		(1 << 11)
#define FMGR_IDX_12		(1 << 12)
#define FMGR_IDX_13		(1 << 13)
#define FMGR_IDX_14		(1 << 14)
#define FMGR_IDX_15		(1 << 15)
#define FMGR_IDX_16		(1 << 16)
#define FMGR_IDX_17		(1 << 17)
#define FMGR_IDX_18		(1 << 18)
#define FMGR_IDX_19		(1 << 19)
#define FMGR_IDX_20		(1 << 20)
#define FMGR_IDX_21		(1 << 21)
#define FMGR_IDX_22		(1 << 22)
#define FMGR_IDX_23		(1 << 23)
#define FMGR_IDX_24		(1 << 24)
#define FMGR_IDX_25		(1 << 25)
#define FMGR_IDX_26		(1 << 26)
#define FMGR_IDX_27		(1 << 27)
#define FMGR_IDX_28		(1 << 28)
#define FMGR_IDX_29		(1 << 29)
#define FMGR_IDX_30		(1 << 30)
#define FMGR_IDX_31		(1 << 31)

#define framemgr_e_barrier_irqs(this, index, flag) \
	this->sindex |= index; spin_lock_irqsave(&this->slock, flag)
#define framemgr_x_barrier_irqr(this, index, flag) \
	spin_unlock_irqrestore(&this->slock, flag); this->sindex &= ~index
#define framemgr_e_barrier_irq(this, index) \
	this->sindex |= index; spin_lock_irq(&this->slock)
#define framemgr_x_barrier_irq(this, index) \
	spin_unlock_irq(&this->slock); this->sindex &= ~index
#define framemgr_e_barrier(this, index) \
	this->sindex |= index; spin_lock(&this->slock)
#define framemgr_x_barrier(this, index) \
	spin_unlock(&this->slock); this->sindex &= ~index

enum fimc_is_frame_shot_state {
	FIMC_IS_FRAME_STATE_FREE,
	FIMC_IS_FRAME_STATE_REQUEST,
	FIMC_IS_FRAME_STATE_PROCESS,
	FIMC_IS_FRAME_STATE_COMPLETE,
	FIMC_IS_FRAME_STATE_INVALID
};

enum fimc_is_frame_reqeust {
	/* SCC, SCP frame done,
	   ISP meta done */
	REQ_FRAME,
	/* 3AA shot done */
	REQ_3AA_SHOT,
	/* ISP shot done */
	REQ_ISP_SHOT,
	/* DIS shot done */
	REQ_DIS_SHOT
};

enum fimc_is_frame_mem {
	/* initialized memory */
	FRAME_INI_MEM,
	/* mapped memory */
	FRAME_MAP_MEM
};

struct fimc_is_frame {
	struct list_head	list;
	struct kthread_work	work;
	void			*work_data1;
	void			*work_data2;

	/* group leader use */
	struct camera2_shot	*shot;
	struct camera2_shot_ext	*shot_ext;
	ulong			kvaddr_shot;
	u32			dvaddr_shot;
	ulong			cookie_shot;
	size_t			shot_size;

	/* stream use */
	struct camera2_stream	*stream;
	u32			stream_size;

	/* common use */
	u32			planes;
	u32			kvaddr_buffer[FIMC_IS_MAX_PLANES];
	u32			dvaddr_buffer[FIMC_IS_MAX_PLANES];

	/* internal use */
	unsigned long		memory;
	u32			state;
	u32			fcount;
	u32			rcount;
	u32			index;
	u32			result;
	unsigned long		req_flag;
	unsigned long		out_flag;
	struct vb2_buffer	*vb;

	/* for overwriting framecount check */
	bool			has_fcount;

	/* time measure externally */
	struct timeval		*tzone;
	/* time measure internally */
	struct fimc_is_monitor	mpoint[TMM_END];
	#ifdef PRINT_BUFADDR
	u32	sizes[FIMC_IS_MAX_PLANES];
	#endif
};

struct fimc_is_framemgr {
	u32			id;
	struct fimc_is_frame	frame[FRAMEMGR_MAX_REQUEST];

	struct list_head	frame_free_head;
	struct list_head	frame_request_head;
	struct list_head	frame_process_head;
	struct list_head	frame_complete_head;

	u32			frame_cnt;
	u32			frame_fre_cnt;
	u32			frame_req_cnt;
	u32			frame_pro_cnt;
	u32			frame_com_cnt;

	spinlock_t		slock;
	ulong			sindex;
};

int fimc_is_frame_probe(struct fimc_is_framemgr *this, u32 id);
int fimc_is_frame_open(struct fimc_is_framemgr *this, u32 buffers);
int fimc_is_frame_close(struct fimc_is_framemgr *this);
int fimc_is_frame_flush(struct fimc_is_framemgr *this);
void fimc_is_frame_print_all(struct fimc_is_framemgr *this);

int fimc_is_frame_s_free_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);
int fimc_is_frame_g_free_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame **frame);
void fimc_is_frame_free_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame **frame);
void fimc_is_frame_print_free_list(struct fimc_is_framemgr *this);

int fimc_is_frame_s_request_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);
int fimc_is_frame_g_request_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame **frame);
void fimc_is_frame_request_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame **frame);
void fimc_is_frame_print_request_list(struct fimc_is_framemgr *this);

int fimc_is_frame_s_process_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);
int fimc_is_frame_g_process_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame **frame);
void fimc_is_frame_process_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame **frame);
void fimc_is_frame_print_process_list(struct fimc_is_framemgr *this);

int fimc_is_frame_s_complete_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);
int fimc_is_frame_g_complete_shot(struct fimc_is_framemgr *this,
	struct fimc_is_frame **frame);
void fimc_is_frame_complete_head(struct fimc_is_framemgr *this,
	struct fimc_is_frame **frame);
void fimc_is_frame_print_complete_list(struct fimc_is_framemgr *this);

int fimc_is_frame_trans_fre_to_req(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);
int fimc_is_frame_trans_fre_to_com(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);
int fimc_is_frame_trans_req_to_pro(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);
int fimc_is_frame_trans_req_to_com(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);
int fimc_is_frame_trans_req_to_fre(struct fimc_is_framemgr *this,
	struct fimc_is_frame *item);
int fimc_is_frame_trans_pro_to_com(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);
int fimc_is_frame_trans_pro_to_fre(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);
int fimc_is_frame_trans_com_to_fre(struct fimc_is_framemgr *this,
	struct fimc_is_frame *frame);

int fimc_is_frame_swap_process_head(struct fimc_is_framemgr *this);

#endif
