/* linux/drivers/media/platform/exynos/jpeg/jpeg-controls.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Core file for Samsung H/W Jpeg driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "jpeg.h"

int exynos_jpeg_read_dqt(struct jpeg_ctx *ctx, unsigned char __user *dqt)
{
	unsigned int next_source = 0;
	unsigned char pq_tq;
	unsigned int length;
	unsigned char twobyte[2];

	/* only two q-tables are allowed */
	if (!ctx->custom_qtbl)
		ctx->custom_qtbl = kmalloc(JPEG_MCU_SIZE * 4, GFP_KERNEL);
	if (!ctx->custom_qtbl) {
		dev_err(ctx->jpeg_dev->dev,
			"%s: Failed to allocate custom q-table\n", __func__);
		return -ENOMEM;
	}

	if (get_user(twobyte[0], dqt + next_source++) ||
		get_user(twobyte[1], dqt + next_source++)) {
		dev_err(ctx->jpeg_dev->dev,
			"%s: Failed to read DQT marker\n", __func__);
		return -EFAULT;
	}

	if ((twobyte[0] != 0xFF) || (twobyte[1] != 0xDB)) {
		dev_err(ctx->jpeg_dev->dev,
			"%s: Invalid DQT marker 0x%02x%02x\n",
			__func__, twobyte[0], twobyte[1]);
		return -EINVAL;
	}

	if (get_user(twobyte[0], dqt + next_source++) ||
		get_user(twobyte[1], dqt + next_source++)) {
		dev_err(ctx->jpeg_dev->dev,
			"%s: Failed to read DQT length\n", __func__);
		return -EFAULT;
	}

	length = (twobyte[0] << (sizeof(twobyte[0]) * 8)) + twobyte[1];

	while (length > (JPEG_MCU_SIZE + 1)) { /* MCU size + table selector */
		if (get_user(pq_tq, dqt + next_source++)) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: Failed to read Q-Table selector\n",
				__func__);
			return -EFAULT;
		}
		length--;

		if ((pq_tq >> 4) != 0) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: non-zero Pq in DQT is not allowed\n",
				__func__);
			return -EINVAL;
		}

		if ((pq_tq & 0xF) > 4) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: Invalid Q-Table ID %d\n",
				__func__, pq_tq & 0xF);
			return -EINVAL;
		}

		if (copy_from_user(
			ctx->custom_qtbl + (pq_tq & 0xF) * JPEG_MCU_SIZE,
					dqt + next_source, JPEG_MCU_SIZE)) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: Failed to read %dth Q-Table\n",
				__func__, pq_tq & 0xF);
			return -EFAULT;
		}

		length -= JPEG_MCU_SIZE;
		next_source += JPEG_MCU_SIZE;
	}

	return 0;
}

/*
 * Read user-provided huffman table for decompression
 *
 * The user-provided huffman tables should consist of 4 huffman tables as
 * specified in DHT. The beginning address of DHT should be given.
 * The user-provided huffman table should be in the following form:
 * - The following sequence should be repeated 4 times.
 *   - Tc (higher 4-bits) and Th (lower 4-bits)
 *   - 16 length fields
 *   - value fields defined by length fields
 * Either of Th and Tc should be 0 or 1.
 */
static unsigned short jpeg_hufftable_offsets[2][2] = { /* [Tc][Th] */
	{0, JPEG_HUFFMAN_DCVAL_BYTES + JPEG_HUFFMAN_LENGTH_BYTES},
	{JPEG_HUFFMAN_AC_OFFSET,
		JPEG_HUFFMAN_AC_OFFSET +
			JPEG_HUFFMAN_LENGTH_BYTES + JPEG_HUFFMAN_ACVAL_BYTES},
};

static unsigned int jpeg_count_huffman_values(unsigned char *htbl)
{
	unsigned int i, count = 0;
	for (i = 0; i < JPEG_HUFFMAN_LENGTH_BYTES; i++)
		count += htbl[i];
	return count;
}

int exynos_jpeg_read_dht(struct jpeg_ctx *ctx, unsigned char __user *dht)
{
	unsigned int i, next_source = 0;
	unsigned char tc_th;
	unsigned int length;
	unsigned char twobyte[2];
	/*
	 * The layout of ctx->user_htables:
	 * [0x000...0x00F]: DC Table 0(Luma) lengths
	 * [0x010...0x01F]: DC Table 0(Luma) values
	 * [0x020...0x02F]: DC Table 1(Chroma) lengths
	 * [0x030...0x03F]: DC Table 1(Chroma) values
	 * [0x040...0x04F]: AC Table 0(Luma) lengths
	 * [0x050...0x0FF]: AC Table 0(Luma) values
	 * [0x100...0x10F]: AC Table 1(Chroma) lengths
	 * [0x110...0x1BF]: AC Table 1(Chroma) values
	 */
	if (!ctx->user_htables)
		ctx->user_htables =
			kzalloc(JPEG_HUFFMAN_TABLE_SIZE, GFP_KERNEL);
	if (!ctx->user_htables) {
		dev_err(ctx->jpeg_dev->dev,
			"%s: Failed to allocate huffman tables\n", __func__);
		return -ENOMEM;
	}

	if (get_user(twobyte[0], dht + next_source++) ||
		get_user(twobyte[1], dht + next_source++)) {
		dev_err(ctx->jpeg_dev->dev,
			"%s: Failed to read DHT marker\n", __func__);
		return -EFAULT;
	}

	if ((twobyte[0] != 0xFF) || (twobyte[1] != 0xC4)) {
		dev_err(ctx->jpeg_dev->dev,
			"%s: Invalid DHT marker 0x%02x%02x\n",
			__func__, twobyte[0], twobyte[1]);
		return -EINVAL;
	}

	if (get_user(twobyte[0], dht + next_source++) ||
		get_user(twobyte[1], dht + next_source++)) {
		dev_err(ctx->jpeg_dev->dev,
			"%s: Failed to read DHT length\n", __func__);
		return -EFAULT;
	}

	length = (twobyte[0] << (sizeof(twobyte[0]) * 8)) + twobyte[1];

	for (i = 0; i < 4; i++) {
		unsigned int offset;
		unsigned int num_values;
		/* Huffman Table identifier check */
		if (length < (JPEG_HUFFMAN_LENGTH_BYTES + 1)) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: too small length for DHT\n", __func__);
			return -EINVAL;
		}
		length -= JPEG_HUFFMAN_LENGTH_BYTES + 1;

		if (get_user(tc_th, dht + next_source++)) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: Failed to read %dth Huffmantable IDs\n",
				__func__, i);
			return -EFAULT;
		}

		if (((tc_th & 0xF) > 1) || ((tc_th >> 4) > 1)) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: Unsupport tc_th(%x) for %d th Hufftable\n",
				__func__, tc_th, i);
			return -EINVAL;
		}

		offset = jpeg_hufftable_offsets[tc_th >> 4][tc_th & 0xF];

		if (copy_from_user(ctx->user_htables + offset,
				dht + next_source, JPEG_HUFFMAN_LENGTH_BYTES)) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: Failed to read %dth huffmantable length\n",
				__func__, i);
			return -EFAULT;
		}

		num_values = jpeg_count_huffman_values(
						ctx->user_htables + offset);
		if (((offset < JPEG_HUFFMAN_AC_OFFSET) &&
				(num_values > JPEG_HUFFMAN_DCVAL_BYTES)) ||
			((offset >= JPEG_HUFFMAN_AC_OFFSET) &&
				(num_values > JPEG_HUFFMAN_ACVAL_BYTES))) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: Too many values for %dth huffmantable\n",
				__func__, i);
			return -EINVAL;
		}

		if (length < num_values) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: too small length for DHT\n", __func__);
			return -EINVAL;
		}
		length -= num_values;

		offset += JPEG_HUFFMAN_LENGTH_BYTES;
		next_source += JPEG_HUFFMAN_LENGTH_BYTES;

		if (copy_from_user(ctx->user_htables + offset,
					dht + next_source, num_values)) {
			dev_err(ctx->jpeg_dev->dev,
				"%s: Failed to read %dth huffmantable values\n",
				__func__, i);
			return -EFAULT;
		}

		next_source += num_values;
	}

	return 0;
}
