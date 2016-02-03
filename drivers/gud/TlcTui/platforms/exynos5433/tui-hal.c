/*
 * Copyright (c) 2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <decon_fb.h>
#include <t-base-tui.h>
#include "dciTui.h"
#include "tlcTui.h"
#include "tui-hal.h"

#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
/* I2C register for reset */
#define HSI2C5_PA_BASE_ADDRESS	0x14ED0000
#define HSI2C_CTL		0x00
#define HSI2C_TRAILIG_CTL	0x08
#define HSI2C_CONF		0x40
#define HSI2C_SW_RST		(1u << 31)
#define HSI2C_FUNC_MODE_I2C	(1u << 0)
#define HSI2C_MASTER		(1u << 3)
#define HSI2C_TRAILING_COUNT	(0xf)
#define HSI2C_AUTO_MODE		(1u << 31)
#endif

#define TUI_MEMPOOL_SIZE	0
extern uint32_t hal_tui_video_space_alloc(void);

struct tui_mempool {
	void * va;
	unsigned long pa;
	size_t size;
};

struct tui_mempool g_tuiMemPool;

static bool allocateTuiMemoryPool(struct tui_mempool *pool, size_t size)
{
	bool ret = false;
	void * tuiMemPool = NULL;

	pr_info("%s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	if (!size) {
		pr_debug("TUI frame buffer: nothing to allocate.");
		return true;
	}

	tuiMemPool = kmalloc(size, GFP_KERNEL);
	if (!tuiMemPool) {
		pr_debug("ERROR Could not allocate TUI memory pool");
	}
	else if (ksize(tuiMemPool) < size) {
		pr_debug("ERROR TUI memory pool allocated size is too small. required=%d allocated=%d", size, ksize(tuiMemPool));
		kfree(tuiMemPool);
	}
	else {
		pool->va = tuiMemPool;
		pool->pa = virt_to_phys(tuiMemPool);
		pool->size = ksize(tuiMemPool);
		ret = true;
	}
	return ret;
}

static void freeTuiMemoryPool(struct tui_mempool *pool)
{
	if(pool->va) {
		kfree(pool->va);
		memset(pool, 0, sizeof(*pool));
	}
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,9,0)
static int is_device_ok(struct device *fbdev, void *p)
#else
static int is_device_ok(struct device *fbdev, const void *p)
#endif
{
	return 1;
}

static struct device *get_fb_dev(void)
{
	struct device *fbdev = NULL;

	/* get the first framebuffer device */
	/* [TODO] Handle properly when there are more than one framebuffer */
	fbdev = class_find_device(fb_class, NULL, NULL, is_device_ok);
	if (NULL == fbdev) {
		pr_debug("ERROR cannot get framebuffer device\n");
		return NULL;
	}
	return fbdev;
}

static struct fb_info *get_fb_info(struct device *fbdev)
{
	struct fb_info *fb_info;

	if (!fbdev->p) {
		pr_debug("ERROR framebuffer device has no private data\n");
		return NULL;
	}

	fb_info = (struct fb_info *) dev_get_drvdata(fbdev);
	if (!fb_info) {
		pr_debug("ERROR framebuffer device has no fb_info\n");
		return NULL;
	}

	return fb_info;
}

static void blank_framebuffer(int getref)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct s3c_fb_win *win;
	struct s3c_fb *sfb;
	struct s3c_fb_platdata *pd;

	fbdev = get_fb_dev();
	if (!fbdev) {
		return;
	}

	fb_info = get_fb_info(fbdev);
	if (!fb_info) {
		return;
	}

	/*
	 * hold a reference to the dsim device, to prevent it from going into
	 * power management during tui session
	 */
	win = fb_info->par;
	sfb = win->parent;
	pd = sfb->pdata;

	if (getref) {
		pm_runtime_get_sync(sfb->dev);
	}

	printk(KERN_ERR "[%s] blank the framebuffer \n", __func__);
	/* blank the framebuffer */
	lock_fb_info(fb_info);
	console_lock();
	fb_info->flags |= FBINFO_MISC_USEREVENT;
	printk("%s call fb_blank\n", __func__);
	fb_blank(fb_info, FB_BLANK_POWERDOWN);
	fb_info->flags &= ~FBINFO_MISC_USEREVENT;
	console_unlock();
	unlock_fb_info(fb_info);
	printk("%s call s3c_fb_deactivate_vsync\n", __func__);
//	s3c_fb_deactivate_vsync(sfb);
}

static void unblank_framebuffer(int releaseref)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct s3c_fb_win *win;
	struct s3c_fb *sfb;
	struct s3c_fb_platdata *pd;

	fbdev = get_fb_dev();
	if (!fbdev)
		return;

	fb_info = get_fb_info(fbdev);
	if (!fb_info)
		return;


	/*
	 * Release the reference we took at the beginning of the TUI session
	 */
	win = fb_info->par;
	sfb = win->parent;
	pd = sfb->pdata;

	printk("%s call s3c_fb_activate_vsync\n", __func__);
	s3c_fb_activate_vsync(sfb);

	/*
	 * Unblank the framebuffer
	 */
	console_lock();
	fb_info->flags |= FBINFO_MISC_USEREVENT;
	fb_blank(fb_info, FB_BLANK_UNBLANK);
	fb_info->flags &= ~FBINFO_MISC_USEREVENT;
	console_unlock();

	if (releaseref)
		pm_runtime_put_sync(sfb->dev);

}

uint32_t hal_tui_init(void)
{
	/* Allocate memory pool for the framebuffer
	 */
	if (!allocateTuiMemoryPool(&g_tuiMemPool, TUI_MEMPOOL_SIZE)) {
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	return TUI_DCI_OK;
}

void hal_tui_exit(void)
{
	/* delete memory pool if any */
	if (g_tuiMemPool.va) {
		freeTuiMemoryPool(&g_tuiMemPool);
	}
}

uint32_t hal_tui_alloc(tuiAllocBuffer_t *allocbuffer, size_t allocsize, uint32_t number)
{
	uint32_t ret = TUI_DCI_ERR_INTERNAL_ERROR;

	g_tuiMemPool.pa = hal_tui_video_space_alloc();
	g_tuiMemPool.size = 0x2000000;

	if (!allocbuffer) {
		pr_debug("%s(%d): allocbuffer is null\n", __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	pr_debug("%s(%d): Requested size=0x%x x %u chunks\n", __func__, __LINE__, allocsize, number);

	if ((size_t)allocsize == 0) {
		pr_debug("%s(%d): Nothing to allocate\n", __func__, __LINE__);
		return TUI_DCI_OK;
	}

	if (number != 2) {
		pr_debug("%s(%d): Unexpected number of buffers requested\n", __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	if ((size_t)(allocsize*number) <= g_tuiMemPool.size) {
		/* requested buffer fits in the memory pool */
		allocbuffer[0].pa = (uint64_t) g_tuiMemPool.pa;
		allocbuffer[1].pa = (uint64_t) (g_tuiMemPool.pa + g_tuiMemPool.size/2);
//		pr_debug("%s(%d): allocated at %16x\n", __func__, __LINE__, allocbuffer[0].pa);
//		pr_debug("%s(%d): allocated at %16x\n", __func__, __LINE__, allocbuffer[1].pa);
		ret = TUI_DCI_OK;
	} else {
		/* requested buffer is bigger than the memory pool, return an
		   error */
		pr_debug("%s(%d): %s\n", __func__, __LINE__, "Memory pool too small");
		ret = TUI_DCI_ERR_INTERNAL_ERROR;
	}

	return ret;
}

#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
void tui_i2c_reset(void)
{
	void __iomem *i2c_reg;
	u32 tui_i2c;
	u32 i2c_conf;

	i2c_reg = ioremap(HSI2C5_PA_BASE_ADDRESS, SZ_4K);
	tui_i2c = readl(i2c_reg + HSI2C_CTL);
	tui_i2c |= HSI2C_SW_RST;
	writel(tui_i2c, i2c_reg + HSI2C_CTL);

	tui_i2c = readl(i2c_reg + HSI2C_CTL);
	tui_i2c &= ~HSI2C_SW_RST;
	writel(tui_i2c, i2c_reg + HSI2C_CTL);

	writel(0x4c4c4c00, i2c_reg + 0x0060);
	writel(0x26004c4c, i2c_reg + 0x0064);
	writel(0x99, i2c_reg + 0x0068);

	i2c_conf = readl(i2c_reg + HSI2C_CONF);
	writel((HSI2C_FUNC_MODE_I2C | HSI2C_MASTER), i2c_reg + HSI2C_CTL);

	writel(HSI2C_TRAILING_COUNT, i2c_reg + HSI2C_TRAILIG_CTL);
	writel(i2c_conf | HSI2C_AUTO_MODE, i2c_reg + HSI2C_CONF);

	iounmap(i2c_reg);
}
#endif

void hal_tui_free(void)
{
}

uint32_t hal_tui_deactivate(void)
{
	/* Set linux TUI flag */
	trustedui_set_mask(TRUSTEDUI_MODE_TUI_SESSION);
	trustedui_blank_set_counter(0);
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	printk(KERN_ERR "blanking!\n");

	blank_framebuffer(1);
	/* TODO-[2014-03-19]-julare01: disabled for Arndale board but this should
	 * be re enabled and put into a HAL */
//		disable_irq(gpio_to_irq(190));
#endif
	trustedui_set_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);

	return TUI_DCI_OK;
}

uint32_t hal_tui_activate(void)
{
	// Protect NWd
	trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("Unblanking\n");
	/* TODO-[2014-03-19]-julare01: disabled for Arndale board but this should
	 * be re enabled and put into a HAL */
//		enable_irq(gpio_to_irq(190));
	unblank_framebuffer(1);
#endif

	/* Clear linux TUI flag */
	trustedui_set_mode(TRUSTEDUI_MODE_OFF);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("Unsetting TUI flag (blank counter=%d)", trustedui_blank_get_counter());
	if (0 < trustedui_blank_get_counter()) {
		blank_framebuffer(0);
	}
#endif

	return TUI_DCI_OK;
}
