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

#ifndef FIMC_IS_DEBUG_H
#define FIMC_IS_DEBUG_H

#ifdef SUPPORTED_A5_MEMORY_SIZE_UP
#define FIMC_IS_DEBUG_REGION_ADDR	(0x01F40000)
#else 
#define FIMC_IS_DEBUG_REGION_ADDR	(0x01940000)
#endif
#define DEBUG_CNT			(0x0007D000) /* 500KB */
#define DEBUG_OFFSET			FIMC_IS_DEBUG_REGION_ADDR
#define DEBUGCTL_OFFSET			(DEBUG_OFFSET + DEBUG_CNT)

#define DEBUG_SENTENCE_MAX		300

enum fimc_is_debug_state {
	FIMC_IS_DEBUG_OPEN
};

struct fimc_is_debug {
	struct dentry		*root;
	struct dentry		*logfile;
	struct dentry		*imgfile;

	/* log dump */
	size_t			debug_cnt;
	struct fimc_is_minfo	*minfo;

	/* image dump */
	u32			dump_count;
	ulong			img_cookie;
	ulong			img_kvaddr;
	size_t			size;

	/* debug message */
	size_t			dsentence_pos;
	char			dsentence[DEBUG_SENTENCE_MAX];

	unsigned long		state;
};

extern struct fimc_is_debug fimc_is_debug;

int fimc_is_debug_probe(void);
int fimc_is_debug_open(struct fimc_is_minfo *minfo);
int fimc_is_debug_close(void);

void fimc_is_dmsg_init(void);
void fimc_is_dmsg_concate(const char *fmt, ...);
char *fimc_is_dmsg_print(void);

int imgdump_request(ulong cookie, ulong kvaddr, size_t size);

#endif
