#ifndef FIMC_IS_TYPE_H
#define FIMC_IS_TYPE_H

#include <linux/v4l2-mediabus.h>

#include <media/v4l2-device.h>

enum fimc_is_device_type {
	FIMC_IS_DEVICE_SENSOR,
	FIMC_IS_DEVICE_ISCHAIN
};

struct fimc_is_window {
	u32 o_width;
	u32 o_height;
	u32 width;
	u32 height;
	u32 offs_h;
	u32 offs_v;
	u32 otf_width;
	u32 otf_height;
};

struct fimc_is_fmt {
	char				*name;
	enum v4l2_mbus_pixelcode	mbus_code;
	u32				pixelformat;
	u32				field;
	u32				num_planes;
};

struct fimc_is_image {
	u32			framerate;
	struct fimc_is_window	window;
	struct fimc_is_fmt	format;
};

struct fimc_is_crop {
	u32			x;
	u32			y;
	u32			w;
	u32			h;
};

#define INIT_CROP(c) ((c)->x = 0, (c)->y = 0, (c)->w = 0, (c)->h = 0)
#define IS_NULL_CROP(c) !((c)->x + (c)->y + (c)->w + (c)->h)
#define COMPARE_CROP(c1, c2) (((c1)->x == (c2)->x) && ((c1)->y == (c2)->y) && ((c1->w) == (c2)->w) && ((c1)->h == (c2)->h))
#define CORRECT_NEGATIVE_CROP(c1) \
	do { \
		if (unlikely((((signed int)(c1)->x) < 0 ) || (((signed int)(c1)->y) < 0 ))) { \
			pr_err("%s(), err value!x:%d, y:%d, will change minus value to 0\n", \
				__func__, c1->x, c1->y); \
		} \
		((c1)->x = (((signed int)(c1)->x) < 0 ) ? 0 : (c1)->x, \
			(c1)->y = (((signed int)(c1)->y) < 0 ) ? 0 : (c1)->y); \
	} while (0)

#define TO_WORD_OFFSET(byte_offset) ((byte_offset) >> 2)

#endif
