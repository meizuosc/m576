/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __EXYNOS_IPC_H__
#define __EXYNOS_IPC_H__

#include <linux/types.h>
#include "modem_v1.h"

#define EXYNOS_SINGLE_MASK			(0b11000000)
#define EXYNOS_MULTI_START_MASK	(0b10000000)
#define EXYNOS_MULTI_LAST_MASK		(0b01000000)

#define EXYNOS_START_MASK			0xABCD
#define EXYNOS_START_OFFSET		0
#define EXYNOS_START_SIZE			2

#define EXYNOS_FRAME_SEQ_OFFSET	2
#define EXYNOS_FRAME_SIZE			2

#define EXYNOS_FRAG_CONFIG_OFFSET	4
#define EXYNOS_FRAG_CONFIG_SIZE	2

#define EXYNOS_LEN_OFFSET			6
#define EXYNOS_LEN_SIZE			2

#define EXYNOS_CH_ID_OFFSET		8
#define EXYNOS_CH_SIZE				1

#define EXYNOS_CH_SEQ_OFFSET		9
#define EXYNOS_CH_SEQ_SIZE			1

#define EXYNOS_HEADER_SIZE		12

#define EXYNOS_DATA_LOOPBACK_CHANNEL	82

#define EXYNOS_FMT_NUM		1
#define EXYNOS_RFS_NUM		10

enum exynos_ch_id {
	EXYNOS_CH_ID_MULTIPDP = 0,

	EXYNOS_CH_ID_PDP_0 = 1,    /*rmnet0*/
	EXYNOS_CH_ID_PDP_1,
	EXYNOS_CH_ID_PDP_2,
	EXYNOS_CH_ID_PDP_3,
	EXYNOS_CH_ID_PDP_4,
	EXYNOS_CH_ID_PDP_5,
	EXYNOS_CH_ID_PDP_6,
	EXYNOS_CH_ID_PDP_7,
	EXYNOS_CH_ID_PDP_8,
	EXYNOS_CH_ID_PDP_9,

	EXYNOS_CH_ID_BT_DUN = 21,  /*umts_router*/

	EXYNOS_CH_ID_RFS_0 = 41,   /*umts_rfs*/
	EXYNOS_CH_ID_RFS_1,
	EXYNOS_CH_ID_RFS_2,
	EXYNOS_CH_ID_RFS_3,
	EXYNOS_CH_ID_RFS_4,
	EXYNOS_CH_ID_RFS_5,
	EXYNOS_CH_ID_RFS_6,
	EXYNOS_CH_ID_RFS_7,
	EXYNOS_CH_ID_RFS_8,
	EXYNOS_CH_ID_RFS_9,

	EXYNOS_CH_ID_CPLOG = 81,   /*umts_dm0*/
	EXYNOS_CH_ID_LOOPBACK,     /*umts_loopback*/

	EXYNOS_CH_ID_BOOT = 241,
	EXYNOS_CH_ID_DUMP = 242,

	EXYNOS_CH_ID_FMT_0 = 245,   /*umts_ipc0*/
	EXYNOS_CH_ID_FMT_1,
	EXYNOS_CH_ID_FMT_2,
	EXYNOS_CH_ID_FMT_3,
	EXYNOS_CH_ID_FMT_4,

	EXYNOS_CH_ID_MAX = 255,
	EXYNOS_CH_ID_FLOW_CTRL = 255
};

struct __packed frag_config {
	u8 frame_first:1,
	frame_last:1,
	packet_index:6;
	u8 frame_index;
};

/* EXYNOS link-layer header */
struct __packed exynos_link_header {
	u16 seq;
	struct frag_config cfg;
	u16 len;
	u16 reserved_1;
	u8 ch_id;
	u8 ch_seq;
	u16 reserved_2;
};

struct __packed exynos_seq_num {
	u16 frame_cnt;
	u8 ch_cnt[255];
};

struct exynos_frame_data {
	/* Frame length calculated from the length fields */
	unsigned int len;

	/* The length of link layer header */
	unsigned int hdr_len;

	/* The length of received header */
	unsigned int hdr_rcvd;

	/* The length of link layer payload */
	unsigned int pay_len;

	/* The length of received data */
	unsigned int pay_rcvd;

	/* The length of link layer padding */
	unsigned int pad_len;

	/* The length of received padding */
	unsigned int pad_rcvd;

	/* Header buffer */
	u8 hdr[EXYNOS_HEADER_SIZE];
};

static inline bool exynos_start_valid(u8 *frm)
{
	u16 cfg = *(u16 *)(frm + EXYNOS_START_OFFSET);
	return cfg == EXYNOS_START_MASK ? true : false;
}

static inline bool exynos_multi_start_valid(u8 *frm)
{
	u16 cfg = *(u16 *)(frm + EXYNOS_FRAG_CONFIG_OFFSET);
	return ((cfg >> 8) & EXYNOS_MULTI_START_MASK) == EXYNOS_MULTI_START_MASK;
}

static inline bool exynos_multi_last_valid(u8 *frm)
{
	u16 cfg = *(u16 *)(frm + EXYNOS_FRAG_CONFIG_OFFSET);
	return ((cfg >> 8) & EXYNOS_MULTI_LAST_MASK) == EXYNOS_MULTI_LAST_MASK;
}

static inline bool exynos_single_frame(u8 *frm)
{
	u16 cfg = *(u16 *)(frm + EXYNOS_FRAG_CONFIG_OFFSET);
	return ((cfg >> 8) & EXYNOS_SINGLE_MASK) == EXYNOS_SINGLE_MASK;
}

static inline u8 exynos_get_ch(u8 *frm)
{
	return frm[EXYNOS_CH_ID_OFFSET];
}

static inline unsigned int exynos_get_frame_seq(u8 *frm)
{
	u16 cfg = *(u16 *)(frm + EXYNOS_FRAME_SEQ_OFFSET);
	return cfg;
}

static inline unsigned int exynos_get_ch_seq(u8 *frm)
{
	return frm[EXYNOS_CH_SEQ_OFFSET];
}

static inline unsigned int exynos_calc_padding_size(unsigned int len)
{
	unsigned int residue = len & 0x7;
	return residue ? (8 - residue) : 0;
}

static inline unsigned int exynos_get_frame_len(u8 *frm)
{
	return (unsigned int)*(u16 *)(frm + EXYNOS_LEN_OFFSET);
}

static inline bool exynos_fmt_ch(u8 ch)
{
	return (ch >= EXYNOS_CH_ID_FMT_0 && ch <= EXYNOS_CH_ID_FMT_4) ? true : false;
}

static inline bool exynos_rfs_ch(u8 ch)
{
	return (ch >= EXYNOS_CH_ID_RFS_0 && ch <= EXYNOS_CH_ID_RFS_9) ?
		true : false;
}

static inline bool exynos_boot_ch(u8 ch)
{
	return (ch == EXYNOS_CH_ID_BOOT) ? true : false;
}

static inline bool exynos_dump_ch(u8 ch)
{
	return (ch == EXYNOS_CH_ID_LOOPBACK) ? true: false;
}

static inline bool exynos_udl_ch(u8 ch)
{
	return (ch == EXYNOS_CH_ID_BOOT || ch == EXYNOS_CH_ID_DUMP) ?
		true : false;
}

static inline bool exynos_ipc_ch(u8 ch)
{
	return (ch > 0 && (ch != EXYNOS_CH_ID_BOOT && ch != EXYNOS_CH_ID_DUMP)) ?
		true : false;
}

static inline bool exynos_ps_ch(u8 ch)
{
	return (ch >= EXYNOS_CH_ID_PDP_0 && ch <= EXYNOS_CH_ID_PDP_9) ?
		true : false;
}

static inline bool exynos_log_ch(u8 ch)
{
	return (ch == EXYNOS_CH_ID_CPLOG) ? true : false;
}

static inline bool exynos_router_ch(u8 ch)
{
	return (ch == EXYNOS_CH_ID_BT_DUN) ? true : false;
}

static inline unsigned int exynos_get_total_len(u8 *frm)
{
	unsigned int len;
	unsigned int pad;

	len = exynos_get_frame_len(frm);
	pad = exynos_calc_padding_size(len) ? exynos_calc_padding_size(len) : 0;
	return len + pad;
}

static inline bool exynos_padding_exist(u8 *frm)
{
	return exynos_calc_padding_size(exynos_get_frame_len(frm)) ? true : false;
}
#endif
