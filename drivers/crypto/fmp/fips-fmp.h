/*
 * Exynos FMP test driver for FIPS
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _FIPS_FMP_H_
#define _FIPS_FMP_H_

#include "fips-fmp-info.h"

struct fips_fmp_ops {
	int (*init)(struct platform_device *pdev, uint32_t mode);
	int (*set_key)(struct platform_device *pdev, uint32_t mode, uint8_t *key, uint32_t key_len);
	int (*set_iv)(struct platform_device *pdev, uint32_t mode, uint8_t *iv, uint32_t iv_len);
	int (*run)(struct platform_device *pdev, uint32_t mode, uint8_t *data, uint32_t len);
	int (*exit)(void);
};

struct fmps {
	struct fips_fmp_ops *ops;
};

#define MAX_TAP		8
#define XBUFSIZE	8

struct cipher_testvec {
	char *key;
	char *iv;
	char *input;
	char *result;
	unsigned short tap[MAX_TAP];
	int np;
	unsigned char also_non_np;
	unsigned char klen;
	unsigned short ilen;
	unsigned short rlen;
};

struct cipher_test_suite {
	struct {
		struct cipher_testvec *vecs;
		unsigned int count;
	} enc;
};

#endif

