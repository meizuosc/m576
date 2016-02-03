/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef ___SAMSUNG_DECON_H__
#define ___SAMSUNG_DECON_H__

#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>
#include <media/exynos_mc.h>
#include <mach/devfreq.h>
#include <mach/bts.h>

#include "regs-decon.h"
#include "decon_common.h"
#include "./panels/decon_lcd.h"

extern struct ion_device *ion_exynos;
extern struct decon_device *decon_int_drvdata;
extern int decon_log_level;

/*
 * Lets Keep it same as 7420. This is the count for
 * maximum number of layers supported by this driver.
 * Real number of  HW layers (or Active layers) will be
 * provided by device tree.
 */
#define MAX_DECON_WIN		(7)

#define DRIVER_NAME		"decon"
#define MAX_NAME_SIZE		32

#define MAX_DECON_PADS		9
#define DECON_PAD_WB		6

#define MAX_BUF_PLANE_CNT	3
#define DECON_ENTER_LPD_CNT	3
#define MIN_BLK_MODE_WIDTH	144
#define MIN_BLK_MODE_HEIGHT	10

#define DECON_ENABLE		1
#define DECON_DISABLE		0
#define DECON_BACKGROUND	0
#define VSYNC_TIMEOUT_MSEC	50
#define MAX_BW_PER_WINDOW	(2560 * 1600 * 4 * 60)
#define LCD_DEFAULT_BPP		24

#define DECON_TZPC_OFFSET	3
#define DECON_ODMA_WB		8
#define MAX_DMA_TYPE		9

#ifdef CONFIG_DECON_PM_QOS_REQUESTS
#define MIF_DVFS_LVL_MIN	(111000)
#define MIF_DVFS_LVL_MAX	(825000)
#define INT_DVFS_LVL_MIN	(100000)
#define INT_DVFS_LVL_MAX	(400000)
#define INT_SCALING_FACTOR	(20)
#define MIF_SCALING_FACTOR	(20)
#endif

#define DECON_LOG_LEVEL_ERR		3
#define DECON_LOG_LEVEL_WARN		4
#define DECON_LOG_LEVEL_INFO		6
#define DECON_LOG_LEVEL_DBG		7

#ifdef CONFIG_FB_WINDOW_UPDATE
#define DECON_WIN_UPDATE_IDX	(7)
#define decon_win_update_dbg(fmt, ...)					\
	do {								\
		if (decon_log_level >= DECON_LOG_LEVEL_DBG)				\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)
#else
#define decon_win_update_dbg(fmt, ...) (while (0))
#endif

#define decon_err(fmt, ...)							\
	do {									\
		if (decon_log_level >= DECON_LOG_LEVEL_ERR)					\
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define decon_warn(fmt, ...)							\
	do {									\
		if (decon_log_level >= DECON_LOG_LEVEL_WARN)					\
			pr_warn(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define decon_info(fmt, ...)							\
	do {									\
		if (decon_log_level >= DECON_LOG_LEVEL_INFO)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define decon_dbg(fmt, ...)							\
	do {									\
		if (decon_log_level >= DECON_LOG_LEVEL_DBG)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

/*
 * DECON_STATE_ON : disp power on, decon/dsim clock on & lcd on
 * DECON_STATE_LPD_ENT_REQ : disp power on, decon/dsim clock on, lcd on & request for LPD
 * DECON_STATE_LPD_EXIT_REQ : disp power off, decon/dsim clock off, lcd on & request for LPD exit.
 * DECON_STATE_LPD : disp power off, decon/dsim clock off & lcd on
 * DECON_STATE_OFF : disp power off, decon/dsim clock off & lcd off
 */
enum decon_state {
	DECON_STATE_INIT = 0,
	DECON_STATE_ON,
	DECON_STATE_LPD_ENT_REQ,
	DECON_STATE_LPD_EXIT_REQ,
	DECON_STATE_LPD,
	DECON_STATE_OFF
};

enum decon_ip_version {
	IP_VER_DECON_7I = BIT(0),
};

struct exynos_decon_platdata {
	enum decon_ip_version	ip_ver;
	enum decon_psr_mode	psr_mode;
	enum decon_trig_mode	trig_mode;
	enum decon_dsi_mode	dsi_mode;
	int	max_win;
	int	default_win;
	u32	disp_pll_clk;
};

struct decon_vsync {
	wait_queue_head_t	wait;
	ktime_t			timestamp;
	bool			active;
	int			irq_refcount;
	struct mutex		irq_lock;
	struct task_struct	*thread;
};

/*
 * @width: The width of display in mm
 * @height: The height of display in mm
 */
struct decon_fb_videomode {
	struct fb_videomode videomode;
	unsigned short width;
	unsigned short height;

	u8 cs_setup_time;
	u8 wr_setup_time;
	u8 wr_act_time;
	u8 wr_hold_time;
	u8 auto_cmd_rate;
	u8 frame_skip:2;
	u8 rs_pol:1;
};

struct decon_fb_pd_win {
	struct decon_fb_videomode win_mode;

	unsigned short		default_bpp;
	unsigned short		max_bpp;
	unsigned short		virtual_x;
	unsigned short		virtual_y;
	unsigned short		width;
	unsigned short		height;
};

struct decon_dma_buf_data {
	struct ion_handle		*ion_handle;
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	struct sg_table			*sg_table;
	dma_addr_t			dma_addr;
	struct sync_fence		*fence;
};

struct decon_win_rect {
	int x;
	int y;
	u32 w;
	u32 h;
};

struct decon_resources {
	struct clk *pclk;
	struct clk *aclk;
	struct clk *eclk;
	struct clk *vclk;
	struct clk *dsd;
	struct clk *lh_disp0;
	struct clk *lh_disp1;
	struct clk *aclk_disp;
	struct clk *pclk_disp;
	struct clk *mif_pll;
	struct clk *aclk_disp_200;
};

struct decon_rect {
	int	left;
	int	top;
	int	right;
	int	bottom;
};

struct decon_win {
	struct decon_fb_pd_win		windata;
	struct decon_device		*decon;
	struct fb_info			*fbinfo;
	struct media_pad		pad;

	struct decon_fb_videomode	win_mode;
	struct decon_dma_buf_data	dma_buf_data[MAX_BUF_PLANE_CNT];
	struct fb_var_screeninfo	prev_var;
	struct fb_fix_screeninfo	prev_fix;

	int	fps;
	int	index;
	int	use;
	int	local;
	unsigned long	state;
	u32	pseudo_palette[16];
};

struct decon_user_window {
	int x;
	int y;
};

struct s3c_fb_user_plane_alpha {
	int		channel;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};

struct s3c_fb_user_chroma {
	int		enabled;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};

struct s3c_fb_user_ion_client {
	int	fd[MAX_BUF_PLANE_CNT];
	int	offset;
};

enum decon_pixel_format {
	/* RGB 32bit */
	DECON_PIXEL_FORMAT_ARGB_8888 = 0,
	DECON_PIXEL_FORMAT_ABGR_8888,
	DECON_PIXEL_FORMAT_RGBA_8888,
	DECON_PIXEL_FORMAT_BGRA_8888,
	DECON_PIXEL_FORMAT_XRGB_8888,
	DECON_PIXEL_FORMAT_XBGR_8888,
	DECON_PIXEL_FORMAT_RGBX_8888,
	DECON_PIXEL_FORMAT_BGRX_8888,
	/* RGB 16 bit */
	DECON_PIXEL_FORMAT_RGBA_5551,
	DECON_PIXEL_FORMAT_RGB_565,
	/* YUV422 2P */
	DECON_PIXEL_FORMAT_NV16,
	DECON_PIXEL_FORMAT_NV61,
	/* YUV422 3P */
	DECON_PIXEL_FORMAT_YVU422_3P,
	/* YUV420 2P */
	DECON_PIXEL_FORMAT_NV12,
	DECON_PIXEL_FORMAT_NV21,
	DECON_PIXEL_FORMAT_NV12M,
	DECON_PIXEL_FORMAT_NV21M,
	/* YUV420 3P */
	DECON_PIXEL_FORMAT_YUV420,
	DECON_PIXEL_FORMAT_YVU420,
	DECON_PIXEL_FORMAT_YUV420M,
	DECON_PIXEL_FORMAT_YVU420M,

	DECON_PIXEL_FORMAT_MAX,
};

enum decon_blending {
	DECON_BLENDING_NONE = 0,
	DECON_BLENDING_PREMULT = 1,
	DECON_BLENDING_COVERAGE = 2,
	DECON_BLENDING_MAX = 3,
};

struct exynos_hdmi_data {
	enum {
		EXYNOS_HDMI_STATE_PRESET = 0,
		EXYNOS_HDMI_STATE_ENUM_PRESET,
		EXYNOS_HDMI_STATE_CEC_ADDR,
		EXYNOS_HDMI_STATE_HDCP,
		EXYNOS_HDMI_STATE_AUDIO,
	} state;
	struct	v4l2_dv_timings timings;
	struct	v4l2_enum_dv_timings etimings;
	__u32	cec_addr;
	__u32	audio_info;
	int	hdcp;
};

enum vpp_rotate {
	VPP_ROT_NORMAL = 0x0,
	VPP_ROT_XFLIP,
	VPP_ROT_YFLIP,
	VPP_ROT_180,
	VPP_ROT_90,
	VPP_ROT_90_XFLIP,
	VPP_ROT_90_YFLIP,
	VPP_ROT_270,
};

enum vpp_csc_eq {
	BT_601_NARROW = 0x0,
	BT_601_WIDE,
	BT_709_NARROW,
	BT_709_WIDE,
};


struct vpp_params {
	dma_addr_t addr[MAX_BUF_PLANE_CNT];
	enum vpp_rotate rot;
	enum vpp_csc_eq eq_mode;
};

struct decon_frame {
	int x;
	int y;
	u32 w;
	u32 h;
	u32 f_w;
	u32 f_h;
};

struct decon_win_config {
	enum {
		DECON_WIN_STATE_DISABLED = 0,
		DECON_WIN_STATE_COLOR,
		DECON_WIN_STATE_BUFFER,
		DECON_WIN_STATE_UPDATE,
	} state;

	union {
		__u32 color;
		struct {
			int				fd_idma[3];
			int				fence_fd;
			int				plane_alpha;
			enum decon_blending		blending;
			enum decon_idma_type		idma_type;
			enum decon_pixel_format		format;
			struct vpp_params		vpp_parm;
			/* no read area of IDMA */
			struct decon_win_rect		block_area;
			/* source framebuffer coordinates */
			struct decon_frame		src;
		};
	};

	/* destination OSD coordinates */
	struct decon_frame dst;
	bool protection;
};

struct decon_reg_data {
	struct list_head		list;
	u32				shadowcon;
	u32				wincon[MAX_DECON_WIN];
	u32				win_rgborder[MAX_DECON_WIN];
	u32				winmap[MAX_DECON_WIN];
	u32				vidosd_a[MAX_DECON_WIN];
	u32				vidosd_b[MAX_DECON_WIN];
	u32				vidosd_c[MAX_DECON_WIN];
	u32				vidosd_d[MAX_DECON_WIN];
	u32				vidw_alpha0[MAX_DECON_WIN];
	u32				vidw_alpha1[MAX_DECON_WIN];
	u32				blendeq[MAX_DECON_WIN - 1];
	u32				buf_start[MAX_DECON_WIN];
	struct decon_dma_buf_data	dma_buf_data[MAX_DECON_WIN][MAX_BUF_PLANE_CNT];
	unsigned int			bandwidth;
	u32				win_overlap_cnt;
	u32                             offset_x[MAX_DECON_WIN];
	u32                             offset_y[MAX_DECON_WIN];
	u32                             whole_w[MAX_DECON_WIN];
	u32                             whole_h[MAX_DECON_WIN];
	struct decon_win_config		win_config[MAX_DECON_WIN];
	struct decon_win_rect		block_rect[MAX_DECON_WIN];
#ifdef CONFIG_FB_WINDOW_UPDATE
	struct decon_win_rect		update_win;
	bool            		need_update;
#endif
	bool				protection[MAX_DECON_WIN];
};

struct decon_win_config_data {
	int	fence;
	int	fd_odma;
	struct decon_win_config config[MAX_DECON_WIN + 1];
};

union decon_ioctl_data {
	struct decon_user_window user_window;
	struct s3c_fb_user_plane_alpha user_alpha;
	struct s3c_fb_user_chroma user_chroma;
	struct exynos_hdmi_data hdmi_data;
	struct decon_win_config_data win_data;
	u32 vsync;
};

struct decon_underrun_stat {
	int	prev_overlap_cnt;
	int	cur_overlap_cnt;
	int	chmap;
	int	fifo_level;
	unsigned long aclk;
	unsigned long lh_disp0;
	unsigned long mif_pll;
	unsigned long used_windows;
};

struct decon_device {
	void __iomem			*regs;
	struct device			*dev;
	struct exynos_decon_platdata	*pdata;
	struct media_pad		pads[MAX_DECON_PADS];
	struct v4l2_subdev		sd;
	struct decon_win		*windows[MAX_DECON_WIN];
	struct decon_resources		res;
	struct v4l2_subdev		*output_sd;
	struct exynos_md		*mdev;

	struct mutex			update_regs_list_lock;
	struct list_head		update_regs_list;
	struct task_struct		*update_regs_thread;
	struct kthread_worker		update_regs_worker;
	struct kthread_work		update_regs_work;
	struct mutex			lpd_lock;
	struct work_struct		lpd_work;
	struct workqueue_struct 	*lpd_wq;
	atomic_t			lpd_trig_cnt;
	atomic_t			lpd_block_cnt;

	struct ion_client		*ion_client;
	struct sw_sync_timeline 	*timeline;
	int				timeline_max;
	struct sw_sync_timeline 	*wb_timeline;
	int				wb_timeline_max;

	struct mutex			output_lock;
	struct mutex			mutex;
	spinlock_t			slock;
	struct decon_vsync		vsync_info;
	enum decon_state        	state;
	enum decon_output_type		out_type;
	int				mic_enabled;
	int				prev_overlap_cnt;
	int				cur_overlap_cnt;
	int				id;
	int				n_sink_pad;
	int				n_src_pad;
	union decon_ioctl_data 		ioctl_data;
	struct decon_lcd 		*lcd_info;
#ifdef CONFIG_FB_WINDOW_UPDATE
	struct decon_win_rect  		update_win;
	bool    			need_update;
#endif
	struct decon_underrun_stat	underrun_stat;
	void __iomem			*cam_status[2];
	u32				prev_protection_bitmask;
	u32				cur_protection_bitmask;
	struct decon_dma_buf_data	wb_dma_buf_data;

	unsigned int			irq;
#ifdef CONFIG_DECON_PM_QOS_REQUESTS
	struct pm_qos_request	mif_qos;
	struct pm_qos_request	int_qos;
#endif
};

static inline struct decon_device *get_decon_drvdata(u32 id)
{
	return decon_int_drvdata;
}

/* register access subroutines */
static inline u32 decon_read(u32 id, u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(id);
	return readl(decon->regs + reg_id);
}

static inline u32 decon_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = decon_read(id, reg_id);
	val &= (~mask);
	return val;
}

static inline void decon_write(u32 id, u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(id);
	writel(val, decon->regs + reg_id);
}

static inline void decon_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct decon_device *decon = get_decon_drvdata(id);
	u32 old = decon_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, decon->regs + reg_id);
}

/* common function API */
bool decon_validate_x_alignment(struct decon_device *decon, int x, u32 w,
		u32 bits_per_pixel);
int find_subdev_mipi(struct decon_device *decon);
int find_subdev_hdmi(struct decon_device *decon);
int create_link_mipi(struct decon_device *decon);
int create_link_hdmi(struct decon_device *decon);
int decon_int_register_irq(struct platform_device *pdev, struct decon_device *decon);
irqreturn_t decon_int_irq_handler(int irq, void *dev_data);
int decon_int_get_clocks(struct decon_device *decon);
void decon_int_set_clocks(struct decon_device *decon);
int decon_int_register_lpd_work(struct decon_device *decon);
int decon_exit_lpd(struct decon_device *decon);
int decon_lpd_block_exit(struct decon_device *decon);
int decon_lcd_off(struct decon_device *decon);
int decon_enable(struct decon_device *decon);
int decon_disable(struct decon_device *decon);
void decon_lpd_enable(void);

/* internal only function API */
int decon_fb_config_eint_for_te(struct platform_device *pdev, struct decon_device *decon);
int decon_int_create_vsync_thread(struct decon_device *decon);
int decon_int_create_psr_thread(struct decon_device *decon);
void decon_int_destroy_vsync_thread(struct decon_device *decon);
void decon_int_destroy_psr_thread(struct decon_device *decon);
int decon_int_set_lcd_config(struct decon_device *decon);
int decon_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
int decon_pan_display(struct fb_var_screeninfo *var,
			      struct fb_info *info);

/* external only function API */
struct decon_lcd *find_porch(struct v4l2_mbus_framefmt mbus_fmt);
int decon_get_hdmi_config(struct decon_device *decon,
		struct exynos_hdmi_data *hdmi_data);
int decon_set_hdmi_config(struct decon_device *decon,
		struct exynos_hdmi_data *hdmi_data);

/* POWER and ClOCK API */
int init_display_decon_clocks(struct device *dev);
int disable_display_decon_clocks(struct device *dev);
void decon_clock_on(struct decon_device *decon);
void decon_clock_off(struct decon_device *decon);
u32 decon_reg_get_cam_status(void __iomem *);
void decon_reg_set_block_mode(u32 id, u32 win_idx, u32 x, u32 y, u32 h, u32 w, u32 enable);
void decon_reg_set_tui_va(u32 id, u32 va);
void decon_update_qos_requests(struct decon_device *decon,
		struct decon_reg_data *regs, bool default_qos);

/* LPD related */
static inline void decon_lpd_block(struct decon_device *decon)
{
	if (!decon->id)
		atomic_inc(&decon->lpd_block_cnt);
}

static inline bool decon_is_lpd_blocked(struct decon_device *decon)
{
	return (atomic_read(&decon->lpd_block_cnt) > 0);
}

static inline int decon_get_lpd_block_cnt(struct decon_device *decon)
{
	return atomic_read(&decon->lpd_block_cnt);
}

static inline void decon_lpd_unblock(struct decon_device *decon)
{
	if (!decon->id) {
		if (decon_is_lpd_blocked(decon))
			atomic_dec(&decon->lpd_block_cnt);
	}
}

static inline void decon_lpd_block_reset(struct decon_device *decon)
{
	if (!decon->id)
		atomic_set(&decon->lpd_block_cnt, 0);
}

static inline void decon_lpd_trig_reset(struct decon_device *decon)
{
	if (!decon->id)
		atomic_set(&decon->lpd_trig_cnt, 0);
}

#ifdef CONFIG_DECON_LPD_DISPLAY_WITH_CAMERA
static inline bool is_cam_not_running(struct decon_device *decon)
{
	return !((decon_reg_get_cam_status(decon->cam_status[0]) & 0xF) ||
		(decon_reg_get_cam_status(decon->cam_status[1]) & 0xF));
}
#else
static inline bool is_cam_not_running(struct decon_device *decon)
{
	return true;
}
#endif

static inline bool decon_lpd_enter_cond(struct decon_device *decon)
{
	return ((atomic_inc_return(&decon->lpd_trig_cnt) > DECON_ENTER_LPD_CNT) &&
		(atomic_read(&decon->lpd_block_cnt) <= 0) && is_cam_not_running(decon));
}

/* IOCTL commands */
#define S3CFB_WIN_POSITION		_IOW('F', 203, \
						struct decon_user_window)
#define S3CFB_WIN_SET_PLANE_ALPHA	_IOW('F', 204, \
						struct s3c_fb_user_plane_alpha)
#define S3CFB_WIN_SET_CHROMA		_IOW('F', 205, \
						struct s3c_fb_user_chroma)
#define S3CFB_SET_VSYNC_INT		_IOW('F', 206, __u32)

#define S3CFB_GET_ION_USER_HANDLE	_IOWR('F', 208, \
						struct s3c_fb_user_ion_client)
#define S3CFB_WIN_CONFIG		_IOW('F', 209, \
						struct decon_win_config_data)
#define S3CFB_WIN_PSR_EXIT		_IOW('F', 210, int)

#define EXYNOS_GET_HDMI_CONFIG		_IOW('F', 220, \
						struct exynos_hdmi_data)
#define EXYNOS_SET_HDMI_CONFIG		_IOW('F', 221, \
						struct exynos_hdmi_data)

#define DECON_IOC_LPD_EXIT_LOCK		_IOW('L', 0, u32)
#define DECON_IOC_LPD_UNLOCK		_IOW('L', 1, u32)

#endif /* ___SAMSUNG_DECON_H__ */
