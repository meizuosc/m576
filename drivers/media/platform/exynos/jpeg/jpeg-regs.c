/* linux/drivers/media/platform/exynos/jpeg4/jpeg_regs.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Register interface file for jpeg v4.x driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>

#include "jpeg.h"
#include "jpeg_regs.h"

/*
 * Quantization tables provided by IOC/IEC 10918-1 K.1 and K.2
 * for YUV420 and YUV422
 */
static const unsigned char default_luma_qtable[64] __aligned(64) = {
	16, 11, 10, 16, 24,  40,  51,  61,
	12, 12, 14, 19, 26,  58,  60,  55,
	14, 13, 16, 24, 40,  57,  69,  56,
	14, 17, 22, 29, 51,  87,  80,  62,
	18, 22, 37, 56, 68,  109, 103, 77,
	24, 35, 55, 64, 81,  104, 113, 92,
	49, 64, 78, 87, 103, 121, 120, 101,
	72, 92, 95, 98, 112, 100, 103, 99
};

static const unsigned char default_chroma_qtable[64] __aligned(64) = {
	17, 18, 24, 47, 99, 99, 99, 99,
	18, 21, 26, 66, 99, 99, 99, 99,
	24, 26, 56, 99, 99, 99, 99, 99,
	47, 66, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99
};

const unsigned char qtbl_order[64] __aligned(64) = {
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

/* ITU Luminace Huffman Table */
static unsigned int ITU_H_TBL_LEN_DC_LUMINANCE[4] = {
	0x01050100, 0x01010101, 0x00000001, 0x00000000
};
static unsigned int ITU_H_TBL_VAL_DC_LUMINANCE[3] = {
	0x03020100, 0x07060504, 0x0b0a0908
};

/* ITU Chrominace Huffman Table */
static unsigned int ITU_H_TBL_LEN_DC_CHROMINANCE[4] = {
	0x01010300, 0x01010101, 0x00010101, 0x00000000
};
static unsigned int ITU_H_TBL_VAL_DC_CHROMINANCE[3] = {
	0x03020100, 0x07060504, 0x0b0a0908
};

/* ITU Luminace Huffman Table */
static unsigned int ITU_H_TBL_LEN_AC_LUMINANCE[4] = {
	0x03010200, 0x03040203, 0x04040505, 0x7d010000
};

static unsigned int ITU_H_TBL_VAL_AC_LUMINANCE[41] = {
	0x00030201, 0x12051104, 0x06413121, 0x07615113,
	0x32147122, 0x08a19181, 0xc1b14223, 0xf0d15215,
	0x72623324, 0x160a0982, 0x1a191817, 0x28272625,
	0x35342a29, 0x39383736, 0x4544433a, 0x49484746,
	0x5554534a, 0x59585756, 0x6564635a, 0x69686766,
	0x7574736a, 0x79787776, 0x8584837a, 0x89888786,
	0x9493928a, 0x98979695, 0xa3a29a99, 0xa7a6a5a4,
	0xb2aaa9a8, 0xb6b5b4b3, 0xbab9b8b7, 0xc5c4c3c2,
	0xc9c8c7c6, 0xd4d3d2ca, 0xd8d7d6d5, 0xe2e1dad9,
	0xe6e5e4e3, 0xeae9e8e7, 0xf4f3f2f1, 0xf8f7f6f5,
	0x0000faf9
};

/* ITU Chrominace Huffman Table */
static u32 ITU_H_TBL_LEN_AC_CHROMINANCE[4] = {
	0x02010200, 0x04030404, 0x04040507, 0x77020100
};
static u32 ITU_H_TBL_VAL_AC_CHROMINANCE[41] = {
	0x03020100, 0x21050411, 0x41120631, 0x71610751,
	0x81322213, 0x91421408, 0x09c1b1a1, 0xf0523323,
	0xd1726215, 0x3424160a, 0x17f125e1, 0x261a1918,
	0x2a292827, 0x38373635, 0x44433a39, 0x48474645,
	0x54534a49, 0x58575655, 0x64635a59, 0x68676665,
	0x74736a69, 0x78777675, 0x83827a79, 0x87868584,
	0x928a8988, 0x96959493, 0x9a999897, 0xa5a4a3a2,
	0xa9a8a7a6, 0xb4b3b2aa, 0xb8b7b6b5, 0xc3c2bab9,
	0xc7c6c5c4, 0xd2cac9c8, 0xd6d5d4d3, 0xdad9d8d7,
	0xe5e4e3e2, 0xe9e8e7e6, 0xf4f3f2ea, 0xf8f7f6f5,
	0x0000faf9
};

void jpeg_sw_reset(void __iomem *base)
{
	unsigned int reg;
	int i = 100000; /* JPEG Polling Count */

	reg = readl(base + S5P_JPEG_CNTL_REG);
	writel((reg & S5P_JPEG_ENC_DEC_MODE_MASK),
			base + S5P_JPEG_CNTL_REG);

	reg = readl(base + S5P_JPEG_CNTL_REG);
	writel(reg & ~S5P_JPEG_SOFT_RESET_HI,
			base + S5P_JPEG_CNTL_REG);

#ifdef JPEG_64BIT_COMPILE_ERROR
	ndelay(100000);
#else
	while (i > 0)
		--i;
#endif

	writel(reg | S5P_JPEG_SOFT_RESET_HI,
			base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_enc_dec_mode(void __iomem *base, enum jpeg_mode mode)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_CNTL_REG);
	/* set jpeg mod register */
	if (mode == DECODING) {
		writel((reg & S5P_JPEG_ENC_DEC_MODE_MASK) | S5P_JPEG_DEC_MODE,
			base + S5P_JPEG_CNTL_REG);
	} else {/* encode */
		writel((reg & S5P_JPEG_ENC_DEC_MODE_MASK) | S5P_JPEG_ENC_MODE,
			base + S5P_JPEG_CNTL_REG);
	}
}

void jpeg_set_image_fmt(void __iomem *base, u32 cfg)
{
	writel(cfg, base + S5P_JPEG_IMG_FMT_REG);
}

#define QTBL_VAL_TO_SFR(tbl, idx) (((tbl)[qtbl_order[(idx) * 4 + 0]] << 0) \
				| ((tbl)[qtbl_order[(idx) * 4 + 1]] << 8) \
				| ((tbl)[qtbl_order[(idx) * 4 + 2]] << 16) \
				| ((tbl)[qtbl_order[(idx) * 4 + 3]] << 24))

/*
 * Calculate quantizer values according to the formula
 * suggestedby IETF RFC2435.
 */
static inline u32 jpeg_find_qtbl_for_hw(unsigned int idx, unsigned int factor,
					const unsigned char tbl[])
{
	u32 val = 0;
	u32 ent;
	unsigned int i = idx + 4;

	while (i-- > idx) {
		val <<= 8;
		ent = (tbl[qtbl_order[i]] * factor + 50) / 100;
		val |= max(min(ent, 255U), 1U);
	}

	return val;
}

void jpeg_set_enc_tbl(void __iomem *base, unsigned int quality_factor)
{
	unsigned int i;

	quality_factor = min(quality_factor, 100U);
	quality_factor = max(quality_factor, 1U);
	quality_factor = (quality_factor < 50) ?
			5000 / quality_factor : 200 - (quality_factor * 2);

	for (i = 0; i < (NUM_QTABLE_VALUES / 4); i++)
		__raw_writel(jpeg_find_qtbl_for_hw(i * 4, quality_factor,
							default_luma_qtable),
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG + i * 4);
	for (i = 0; i < (NUM_QTABLE_VALUES / 4); i++)
		__raw_writel(jpeg_find_qtbl_for_hw(i * 4, quality_factor,
							default_chroma_qtable),
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
					+ NUM_QTABLE_VALUES + (i * 4));
}

void jpeg_set_enc_custom_tbl(void __iomem *base, const unsigned char qtbl[])
{
	unsigned int i;

	for (i = 0; i < (NUM_QTABLE_VALUES / 4); i++)
		__raw_writel(QTBL_VAL_TO_SFR(qtbl, i),
			base + S5P_JPEG_QUAN_TBL_ENTRY_REG + (i * 4));
	qtbl = qtbl + NUM_QTABLE_VALUES;
	for (i = 0; i < (NUM_QTABLE_VALUES / 4); i++)
		__raw_writel(QTBL_VAL_TO_SFR(qtbl, i),
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
					+ NUM_QTABLE_VALUES + (i * 4));
}

void jpeg_set_encode_huffman_table(void __iomem *base)
{
	unsigned int i;

	for (i = 0; i < 4; i++) {
		__raw_writel((unsigned int)ITU_H_TBL_LEN_DC_LUMINANCE[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + (i*0x04));
	}

	for (i = 0; i < 3; i++) {
		__raw_writel((unsigned int)ITU_H_TBL_VAL_DC_LUMINANCE[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x10 + (i*0x04));
	}

	for (i = 0; i < 4; i++) {
		__raw_writel((unsigned int)ITU_H_TBL_LEN_DC_CHROMINANCE[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x20 + (i*0x04));
	}

	for (i = 0; i < 3; i++) {
		__raw_writel((unsigned int)ITU_H_TBL_VAL_DC_CHROMINANCE[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x30 + (i*0x04));
	}

	for (i = 0; i < 4; i++) {
		__raw_writel((unsigned int)ITU_H_TBL_LEN_AC_LUMINANCE[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x40 + (i*0x04));
	}

	for (i = 0; i < 41; i++) {
		__raw_writel((unsigned int)ITU_H_TBL_VAL_AC_LUMINANCE[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x50 + (i*0x04));
	}

	for (i = 0; i < 4; i++) {
		__raw_writel((unsigned int)ITU_H_TBL_LEN_AC_CHROMINANCE[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x100 + (i*0x04));
	}

	for (i = 0; i < 41; i++) {
		__raw_writel((unsigned int)ITU_H_TBL_VAL_AC_CHROMINANCE[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x110 + (i*0x04));
	}
}

void jpeg_set_interrupt(void __iomem *base)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_INT_EN_REG) & ~S5P_JPEG_INT_EN_MASK;
	writel(S5P_JPEG_INT_EN_ALL, base + S5P_JPEG_INT_EN_REG);
}

void jpeg_clean_interrupt(void __iomem *base)
{
	writel(0, base + S5P_JPEG_INT_EN_REG);
}

unsigned int jpeg_get_int_status(void __iomem *base)
{
	unsigned int	int_status;

	int_status = readl(base + S5P_JPEG_INT_STATUS_REG);

	return int_status;
}

void jpeg_set_huf_table_enable(void __iomem *base, int value)
{
	unsigned int	reg;

	reg = readl(base + S5P_JPEG_CNTL_REG) & ~S5P_JPEG_HUF_TBL_EN;

	if (value == 1)
		writel(reg | S5P_JPEG_HUF_TBL_EN, base + S5P_JPEG_CNTL_REG);
	else
		writel(reg | ~S5P_JPEG_HUF_TBL_EN, base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_dec_scaling(void __iomem *base, unsigned int factor)
{
	unsigned int	reg;

	BUG_ON(factor > 3);

	reg = readl(base + S5P_JPEG_CNTL_REG) &
				~S5P_JPEG_DEC_IMG_RESLN_TYPE_MASK;

	writel(reg | S5P_JPEG_DEC_IMG_RESLN_TYPE(factor),
			base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_stream_size(void __iomem *base,
		unsigned int x_value, unsigned int y_value)
{
	writel(0x0, base + S5P_JPEG_IMG_SIZE_REG); /* clear */
	writel(S5P_JPEG_X_SIZE(x_value) | S5P_JPEG_Y_SIZE(y_value),
			base + S5P_JPEG_IMG_SIZE_REG);
}

void jpeg_set_stream_addr(void __iomem *base, unsigned int address)
{
	writel(address, base + S5P_JPEG_OUT_MEM_BASE_REG);
}

void jpeg_set_image_addr(void __iomem *base, struct m2m1shot_buffer_dma *buf,
		struct jpeg_fmt *fmt, unsigned int width, unsigned int height)
{
	unsigned int pixsize;
	dma_addr_t addr = buf->plane[0].dma_addr;

	pixsize = width * height;

	switch (fmt->color_planes) {
	case 1:
		writel(addr, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
		writel(0, base + S5P_JPEG_IMG_BA_PLANE_2_REG);
		writel(0, base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		break;
	case 2:
		if (fmt->mem_planes == 1) {
			writel(addr, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
			writel(addr + pixsize,
					base + S5P_JPEG_IMG_BA_PLANE_2_REG);
			writel(0, base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		} else if (fmt->mem_planes == 2) {
			writel(addr, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
			writel(buf->plane[1].dma_addr,
					base + S5P_JPEG_IMG_BA_PLANE_2_REG);
			writel(0, base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		}
		break;
	case 3:
		if (fmt->mem_planes == 1) {
			writel(addr, base + S5P_JPEG_IMG_BA_PLANE_1_REG);

			if (fmt->fourcc != V4L2_PIX_FMT_YVU420)
				writel(addr + pixsize,
					base + S5P_JPEG_IMG_BA_PLANE_2_REG);

			if (fmt->fourcc == V4L2_PIX_FMT_YUV444_3P) {
				writel(addr + (pixsize * 2),
					base + S5P_JPEG_IMG_BA_PLANE_3_REG);
			} else if (fmt->fourcc == V4L2_PIX_FMT_YUV422V_3P) {
				writel(addr + (pixsize + (pixsize / 2)),
					base + S5P_JPEG_IMG_BA_PLANE_3_REG);
			} else {
				if (fmt->fourcc == V4L2_PIX_FMT_YVU420) {
					writel(addr + (pixsize + (pixsize / 4)),
						base + S5P_JPEG_IMG_BA_PLANE_2_REG);
					writel(addr + pixsize,
						base + S5P_JPEG_IMG_BA_PLANE_3_REG);
				} else {
					writel(addr + (pixsize + (pixsize / 4)),
						base + S5P_JPEG_IMG_BA_PLANE_3_REG);
				}
			}
		} else if (fmt->mem_planes == 3) {
			if (fmt->fourcc == V4L2_PIX_FMT_YVU420M)
				swap(buf->plane[1].dma_addr, buf->plane[2].dma_addr);

			writel(addr, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
			writel(buf->plane[1].dma_addr,
					base + S5P_JPEG_IMG_BA_PLANE_2_REG);
			writel(buf->plane[2].dma_addr,
					base + S5P_JPEG_IMG_BA_PLANE_3_REG);

		}
		break;
	default:
		break;
	}
}

void jpeg_set_decode_tbl_select(void __iomem *base,
		struct jpeg_tables_info *tinfo)
{
	unsigned int	reg = 0;
	unsigned int	i;

	for (i = 0; i < MAX_COMPS; i++) {
		reg |= tinfo->quant_tbl_no[i] << (i * 2) |
			(((tinfo->ac_tbl_no[i] * 2) +
			  tinfo->dc_tbl_no[i])) << ((i * 2) + 6);
	}
	writel(reg, base + S5P_JPEG_TBL_SEL_REG);
}


void jpeg_set_encode_hoff_cnt(void __iomem *base, unsigned int fourcc)
{
	if (fourcc == V4L2_PIX_FMT_JPEG_GRAY)
		writel(0xd2, base + S5P_JPEG_HUFF_CNT_REG);
	else
		writel(0x1a2, base + S5P_JPEG_HUFF_CNT_REG);
}

unsigned int jpeg_get_stream_size(void __iomem *base)
{
	unsigned int size;

	size = readl(base + S5P_JPEG_BITSTREAM_SIZE_REG);
	return size;
}

void jpeg_set_dec_bitstream_size(void __iomem *base, unsigned int size, unsigned int burstoffset)
{
	size = ALIGN(size + burstoffset, 32) / 32;
	writel(size, base + S5P_JPEG_BITSTREAM_SIZE_REG);
}

void jpeg_set_timer_count(void __iomem *base, unsigned int size)
{
	writel(size, base + S5P_JPEG_INT_TIMER_COUNT_REG);
}

int jpeg_set_number_of_component(void __iomem *base, unsigned int num_component)
{
	unsigned int reg = 0;

	if (num_component < 1 || num_component > 3)
		return -EINVAL;

	reg = readl(base + S5P_JPEG_TBL_SEL_REG);

	writel(reg | S5P_JPEG_NUMBER_OF_COMPONENTS(num_component),
		base + S5P_JPEG_TBL_SEL_REG);
	return 0;
}

void jpeg_alpha_value_set(void __iomem *base, unsigned int alpha)
{
	writel(S5P_JPEG_ARGB32(alpha), base + S5P_JPEG_PADDING_REG);
}

void jpeg_dec_window_ctrl(void __iomem *base, unsigned int is_start)
{
	writel(is_start & 1, base + S5P_JPEG_DEC_WINDOW_CNTL);
}

void jpeg_set_window_margin(void __iomem *base, unsigned int top,
		unsigned int bottom, unsigned int left, unsigned int right)
{
	jpeg_dec_window_ctrl(base, true);
	writel(S5P_JPEG_IMG_TOP_MARGIN(top), base + S5P_JPEG_DEC_WINDOW_MARN_1);
	writel(S5P_JPEG_IMG_BOTTOM_MARGIN(bottom),
			base + S5P_JPEG_DEC_WINDOW_MARN_1);

	writel(S5P_JPEG_IMG_LEFT_MARGIN(left),
			base + S5P_JPEG_DEC_WINDOW_MARN_2);
	writel(S5P_JPEG_IMG_RIGHT_MARGIN(right),
			base + S5P_JPEG_DEC_WINDOW_MARN_2);
	jpeg_dec_window_ctrl(base, false);
}

void jpeg_get_window_margin(void __iomem *base, unsigned int *top,
		unsigned int *bottom, unsigned int *left, unsigned int *right)
{
	*top = readl(base + S5P_JPEG_DEC_WINDOW_MARN_1) &
		S5P_JPEG_IMG_TOP_MARGIN_MASK;
	*bottom = readl(base + S5P_JPEG_DEC_WINDOW_MARN_1) &
		S5P_JPEG_IMG_BOTTOM_MARGIN_MASK;

	*left = readl(base + S5P_JPEG_DEC_WINDOW_MARN_2) &
		S5P_JPEG_IMG_LEFT_MARGIN_MASK;
	*right = readl(base + S5P_JPEG_DEC_WINDOW_MARN_2) &
		S5P_JPEG_IMG_RIGHT_MARGIN_MASK;
}

static void configure_four_quantizers(void __iomem *base,
				const unsigned char qtbl[], int idx, int tq)
{
	u32 val = qtbl[idx] | (qtbl[idx + 1] << 8) |
			(qtbl[idx + 2] << 16) | (qtbl[idx + 3] << 24);
	unsigned int offset =  S5P_JPEG_QUAN_TBL_ENTRY_REG +
				tq * JPEG_MCU_SIZE + idx;
	__raw_writel(val, base + offset);
}

void jpeg_set_decode_qtables(void __iomem *base, const unsigned char qtbls[])
{
	int i;
	for (i = 0; i < 4; i++) {
		/*
		 * zero quantizer should never happen. Thus it is reserved
		 * to indicate that the quantizer table is not valid
		 */
		if (qtbls[i * JPEG_MCU_SIZE]) {
			int j;
			for (j = 0; j < JPEG_MCU_SIZE; j += 4)
				configure_four_quantizers(base,
					&qtbls[i * JPEG_MCU_SIZE], j, i);
		}
	}
}

void jpeg_set_decode_huffman_tables(void __iomem *base,
				    const unsigned char htbls[])
{
	unsigned int i;
	for (i = 0; i < JPEG_HUFFMAN_TABLE_SIZE; i += 4) {
		u32 val;
		val = htbls[i] | (htbls[i + 1] << 8) |
			(htbls[i + 2] << 16) | (htbls[i + 3] << 24);
		if (val)
			__raw_writel(val,
				base + S5P_JPEG_HUFF_TBL_ENTRY_REG + i);
	}
}

void jpeg_set_decode_table_selection(void __iomem *base, unsigned int table_sel)
{
	u32 val = __raw_readl(base + S5P_JPEG_TBL_SEL_REG);
	/* we always regards JPEG has 3 components */
	__raw_writel((val & 0xFFF) | table_sel | (3 << 16),
		base + S5P_JPEG_TBL_SEL_REG);
}

void jpeg_show_sfr_status(void __iomem *base)
{
	u32 cfg;
	int cnt = 100000;

	cfg = readl(base + S5P_JPEG_TBL_SEL_REG);
	writel(cfg | S5P_JPEG_QUAN_TABLE_READ_START,
			base + S5P_JPEG_TBL_SEL_REG);

	while (!(readl(base + S5P_JPEG_TBL_SEL_REG) &
			S5P_JPEG_HUFF_QUAN_TABLE_DATA_VALID) && cnt > 0) {
		cnt--;
	}

	pr_info("JPEG dumping registers\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			base, 0x68, false);
	if (!cnt) {
		pr_err("Failed to read JPEG Huff & Quant Table\n");
		return;
	}

	pr_info("JPEG Quantization Table\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			base + 0x100, 0x100, false);
	pr_info("JPEG Huffman Table\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			base + 0x200, 0x1C0, false);
	pr_info("End of JPEG dumping registers\n");
	writel(cfg, base + S5P_JPEG_TBL_SEL_REG);
}
