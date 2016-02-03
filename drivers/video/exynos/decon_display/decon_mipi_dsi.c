/* linux/drivers/video/decon_display/decon_mipi_dsi.c
 *
 * Samsung SoC MIPI-DSIM driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * Haowei Li <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/pm_runtime.h>
#include <linux/lcd.h>
#include <linux/gpio.h>

#include <video/mipi_display.h>

#include <plat/cpu.h>

#include <mach/map.h>
#include <mach/exynos5-mipiphy.h>

#include "decon_display_driver.h"
#include "decon_mipi_dsi.h"
#include "regs-dsim.h"
#include "decon_dt.h"
#include "decon_pm.h"
#include "dsim_reg.h"

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#include "decon_fb.h"
#else
#include <mach/regs-pmu.h>
#include "fimd_fb.h"
#endif

static DEFINE_MUTEX(dsim_rd_wr_mutex);
static DECLARE_COMPLETION(dsim_ph_wr_comp);
static DECLARE_COMPLETION(dsim_wr_comp);
static DECLARE_COMPLETION(dsim_rd_comp);

#define MIPI_WR_TIMEOUT msecs_to_jiffies(50)
#define MIPI_RD_TIMEOUT msecs_to_jiffies(50)

#ifdef CONFIG_OF
static const struct of_device_id exynos5_dsim[] = {
	{ .compatible = "samsung,exynos5-dsim" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos5_dsim);
#endif

struct mipi_dsim_device *dsim_for_decon;
EXPORT_SYMBOL(dsim_for_decon);


#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int s5p_mipi_dsi_hibernation_power_on(struct display_driver *dispdrv);
int s5p_mipi_dsi_hibernation_power_off(struct display_driver *dispdrv);
#endif


int s5p_dsim_init_d_phy(struct mipi_dsim_device *dsim, unsigned int enable)
{

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5_dism_phy_enable(0, enable);
#else
	unsigned int reg;

	reg = readl(S5P_MIPI_DPHY_CONTROL(1)) & ~(1 << 0);
	reg |= (enable << 0);
	writel(reg, S5P_MIPI_DPHY_CONTROL(1));

	reg = readl(S5P_MIPI_DPHY_CONTROL(1)) & ~(1 << 2);
	reg |= (enable << 2);
	writel(reg, S5P_MIPI_DPHY_CONTROL(1));
#endif
	return 0;
}

static void s5p_mipi_dsi_long_data_wr(struct mipi_dsim_device *dsim, unsigned long data0, unsigned int data1)
{
	unsigned int data_cnt = 0, payload = 0;

	/* in case that data count is more then 4 */
	for (data_cnt = 0; data_cnt < data1; data_cnt += 4) {
		/*
		 * after sending 4bytes per one time,
		 * send remainder data less then 4.
		 */
		if ((data1 - data_cnt) < 4) {
			if ((data1 - data_cnt) == 3) {
				payload = *(u8 *)(data0 + data_cnt) |
				    (*(u8 *)(data0 + (data_cnt + 1))) << 8 |
					(*(u8 *)(data0 + (data_cnt + 2))) << 16;
			dev_dbg(dsim->dev, "count = 3 payload = %x, %x %x %x\n",
				payload, *(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)),
				*(u8 *)(data0 + (data_cnt + 2)));
			} else if ((data1 - data_cnt) == 2) {
				payload = *(u8 *)(data0 + data_cnt) |
					(*(u8 *)(data0 + (data_cnt + 1))) << 8;
			dev_dbg(dsim->dev,
				"count = 2 payload = %x, %x %x\n", payload,
				*(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)));
			} else if ((data1 - data_cnt) == 1) {
				payload = *(u8 *)(data0 + data_cnt);
			}

			dsim_reg_wr_tx_payload(payload);
		/* send 4bytes per one time. */
		} else {
			payload = *(u8 *)(data0 + data_cnt) |
				(*(u8 *)(data0 + (data_cnt + 1))) << 8 |
				(*(u8 *)(data0 + (data_cnt + 2))) << 16 |
				(*(u8 *)(data0 + (data_cnt + 3))) << 24;

			dev_dbg(dsim->dev,
				"count = 4 payload = %x, %x %x %x %x\n",
				payload, *(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)),
				*(u8 *)(data0 + (data_cnt + 2)),
				*(u8 *)(data0 + (data_cnt + 3)));

			dsim_reg_wr_tx_payload(payload);
		}
	}
}

int s5p_mipi_dsi_wr_data(struct mipi_dsim_device *dsim, unsigned int data_id,
	unsigned long data0, unsigned int data1)
{
	int ret = 0;
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	struct display_driver *dispdrv = get_display_driver();
	disp_pm_gate_lock(dispdrv, true);
	disp_pm_add_refcount(dispdrv);
#endif

	if (dsim->enabled == false || dsim->state != DSIM_STATE_HSCLKEN) {
		dev_info(dsim->dev, "MIPI DSIM is not ready. enabled %d state %d\n",
							dsim->enabled, dsim->state);
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
		disp_pm_gate_lock(dispdrv, false);
#endif
		return -EINVAL;
	}

	mutex_lock(&dsim_rd_wr_mutex);
	switch (data_id) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
		INIT_COMPLETION(dsim_ph_wr_comp);
		dsim_reg_clear_int(DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
		dsim_reg_wr_tx_header(data_id, data0, data1);
		if (!wait_for_completion_interruptible_timeout(&dsim_ph_wr_comp,
			MIPI_WR_TIMEOUT)) {
				dev_err(dsim->dev, "MIPI DSIM short packet write Timeout! 0x%lx\n", data0);
				ret = -ETIMEDOUT;
				goto exit;
		}
		break;

	/* general command */
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
		INIT_COMPLETION(dsim_ph_wr_comp);
		dsim_reg_clear_int(DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
		dsim_reg_wr_tx_header(data_id, data0, data1);
		if (!wait_for_completion_interruptible_timeout(&dsim_ph_wr_comp,
			MIPI_WR_TIMEOUT)) {
				dev_err(dsim->dev, "MIPI DSIM short packet write Timeout! 0x%lx\n", data0);
				ret = -ETIMEDOUT;
				goto exit;
		}
		break;

	/* packet types for video data */
	case MIPI_DSI_V_SYNC_START:
	case MIPI_DSI_V_SYNC_END:
	case MIPI_DSI_H_SYNC_START:
	case MIPI_DSI_H_SYNC_END:
	case MIPI_DSI_END_OF_TRANSMISSION:
		break;

	/* short and response packet types for command */
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		INIT_COMPLETION(dsim_ph_wr_comp);
		dsim_reg_clear_int_all();
		dsim_reg_wr_tx_header(data_id, data0, data1);
		if (!wait_for_completion_interruptible_timeout(&dsim_ph_wr_comp,
			MIPI_WR_TIMEOUT)) {
				dev_err(dsim->dev, "MIPI DSIM short packet write Timeout! 0x%lx\n", data0);
				ret = -ETIMEDOUT;
				goto exit;
		}
		break;

	/* long packet type and null packet */
	case MIPI_DSI_NULL_PACKET:
	case MIPI_DSI_BLANKING_PACKET:
		break;

	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	{
		unsigned int size;

		size = data1 * 4;
		INIT_COMPLETION(dsim_wr_comp);
		dsim_reg_clear_int(DSIM_INTSRC_SFR_PL_FIFO_EMPTY);
		/* if data count is less then 4, then send 3bytes data.  */
		if (data1 < 4) {
			unsigned int payload = 0;
			payload = *(u8 *)(data0) |
				*(u8 *)(data0 + 1) << 8 |
				*(u8 *)(data0 + 2) << 16;

			dsim_reg_wr_tx_payload(payload);

			dev_dbg(dsim->dev, "count = %d payload = %x,%x %x %x\n",
				data1, payload,
				*(u8 *)(data0),
				*(u8 *)(data0 + 1),
				*(u8 *)(data0 + 2));
		/* in case that data count is more then 4 */
		} else
			s5p_mipi_dsi_long_data_wr(dsim, data0, data1);

		/* put data into header fifo */
		dsim_reg_wr_tx_header(data_id, data1 & 0xff, (data1 & 0xff00) >> 8);
		if (!wait_for_completion_interruptible_timeout(&dsim_wr_comp,
			MIPI_WR_TIMEOUT)) {
				dev_err(dsim->dev, "MIPI DSIM write Timeout! %02X\n", *(u8 *)(data0));
				ret = -ETIMEDOUT;
				goto exit;
		}
		break;
	}

	/* packet typo for video data */
	case MIPI_DSI_PACKED_PIXEL_STREAM_16:
	case MIPI_DSI_PACKED_PIXEL_STREAM_18:
	case MIPI_DSI_PIXEL_STREAM_3BYTE_18:
	case MIPI_DSI_PACKED_PIXEL_STREAM_24:
		break;
	default:
		dev_warn(dsim->dev, "data id %x is not supported current DSI spec.\n", data_id);
		ret = -EINVAL;
		goto exit;
	}

exit:
	if (dsim->enabled && (ret == -ETIMEDOUT)) {
		dev_info(dsim->dev, "STATUS 0x%08X, INTSRC 0x%08X, INTMASK 0x%08X, \
				FIFOCTRL 0x%08X, MULTI_PKT 0x%08X\n",
				readl(dsim->reg_base + DSIM_STATUS),
				readl(dsim->reg_base + DSIM_INTSRC),
				readl(dsim->reg_base + DSIM_INTMSK),
				readl(dsim->reg_base + DSIM_FIFOCTRL),
				readl(dsim->reg_base + DSIM_MULTI_PKT));
		dsim_reg_set_fifo_ctrl(DSIM_FIFOCTRL_INIT_SFR);
	}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_gate_lock(dispdrv, false);
#endif
	mutex_unlock(&dsim_rd_wr_mutex);
	return ret;
}

static void s5p_mipi_dsi_rx_err_handler(struct mipi_dsim_device *dsim,
	u32 rx_fifo)
{
	/* Parse error report bit*/
	if (rx_fifo & (1 << 8))
		dev_err(dsim->dev, "SoT error!\n");
	if (rx_fifo & (1 << 9))
		dev_err(dsim->dev, "SoT sync error!\n");
	if (rx_fifo & (1 << 10))
		dev_err(dsim->dev, "EoT error!\n");
	if (rx_fifo & (1 << 11))
		dev_err(dsim->dev, "Escape mode entry command error!\n");
	if (rx_fifo & (1 << 12))
		dev_err(dsim->dev, "Low-power transmit sync error!\n");
	if (rx_fifo & (1 << 13))
		dev_err(dsim->dev, "HS receive timeout error!\n");
	if (rx_fifo & (1 << 14))
		dev_err(dsim->dev, "False control error!\n");
	/* Bit 15 is reserved*/
	if (rx_fifo & (1 << 16))
		dev_err(dsim->dev, "ECC error, single-bit(detected and corrected)!\n");
	if (rx_fifo & (1 << 17))
		dev_err(dsim->dev, "ECC error, multi-bit(detected, not corrected)!\n");
	if (rx_fifo & (1 << 18))
		dev_err(dsim->dev, "Checksum error(long packet only)!\n");
	if (rx_fifo & (1 << 19))
		dev_err(dsim->dev, "DSI data type not recognized!\n");
	if (rx_fifo & (1 << 20))
		dev_err(dsim->dev, "DSI VC ID invalid!\n");
	if (rx_fifo & (1 << 21))
		dev_err(dsim->dev, "Invalid transmission length!\n");
	/* Bit 22 is reserved */
	if (rx_fifo & (1 << 23))
		dev_err(dsim->dev, "DSI protocol violation!\n");
}

int s5p_mipi_dsi_rd_data(struct mipi_dsim_device *dsim, u32 data_id,
	 u32 addr, u32 count, u8 *buf, u8 rxfifo_done)
{
	u32 rx_fifo, rx_size = 0;
	int i, j, ret = 0;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	struct display_driver *dispdrv = get_display_driver();
#endif

	if (dsim->enabled == false || dsim->state != DSIM_STATE_HSCLKEN) {
		dev_dbg(dsim->dev, "MIPI DSIM is not ready.\n");
		return -EINVAL;
	}

	INIT_COMPLETION(dsim_rd_comp);

	/* Set the maximum packet size returned */
	s5p_mipi_dsi_wr_data(dsim,
		MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, count, 0);

	/* Read request */
	s5p_mipi_dsi_wr_data(dsim, data_id, addr, 0);
	if (!wait_for_completion_interruptible_timeout(&dsim_rd_comp,
		MIPI_RD_TIMEOUT)) {
		dev_err(dsim->dev, "MIPI DSIM read Timeout!\n");
		return -ETIMEDOUT;
	}

	mutex_lock(&dsim_rd_wr_mutex);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_gate_lock(dispdrv, true);
	disp_pm_add_refcount(dispdrv);
#endif
	rx_fifo = readl(dsim->reg_base + DSIM_RXFIFO);

	/* Parse the RX packet data types */
	switch (rx_fifo & 0xff) {
	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		s5p_mipi_dsi_rx_err_handler(dsim, rx_fifo);
		goto rx_error;
	case MIPI_DSI_RX_END_OF_TRANSMISSION:
		dev_dbg(dsim->dev, "EoTp was received from LCD module.\n");
		break;
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
		dev_dbg(dsim->dev, "Short Packet was received from LCD module.\n");
		for (i = 0; i <= count; i++)
			buf[i] = (rx_fifo >> (8 + i * 8)) & 0xff;
		break;
	case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
	case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
		dev_dbg(dsim->dev, "Long Packet was received from LCD module.\n");
		rx_size = (rx_fifo & 0x00ffff00) >> 8;
		dev_info(dsim->dev, "rx fifo : %8x, response : %x, rx_size : %d\n",
				rx_fifo, rx_fifo & 0xff, rx_size);
		/* Read data from RX packet payload */
		for (i = 0; i < rx_size >> 2; i++) {
			rx_fifo = readl(dsim->reg_base + DSIM_RXFIFO);
			for (j = 0; j < 4; j++)
				buf[(i*4)+j] = (u8)(rx_fifo >> (j * 8)) & 0xff;
		}
		if (rx_size % 4) {
			rx_fifo = readl(dsim->reg_base + DSIM_RXFIFO);
			for (j = 0; j < rx_size % 4; j++)
				buf[4 * i + j] =
					(u8)(rx_fifo >> (j * 8)) & 0xff;
		}
		break;
	default:
		dev_err(dsim->dev, "Packet format is invaild.\n");
		goto rx_error;
	}

	if (!rxfifo_done) {
		ret = rx_size;
		goto exit;
	}

	rx_fifo = readl(dsim->reg_base + DSIM_RXFIFO);
	if (rx_fifo != DSIM_RX_FIFO_READ_DONE) {
		dev_info(dsim->dev, "%s Can't find RX FIFO READ DONE FLAG : %x\n",
			__func__, rx_fifo);
		goto clear_rx_fifo;
	}
	ret = rx_size;
	goto exit;

clear_rx_fifo:
	i = 0;
	while (1) {
		rx_fifo = readl(dsim->reg_base + DSIM_RXFIFO);
		if ((rx_fifo == DSIM_RX_FIFO_READ_DONE) ||
				(i > DSIM_MAX_RX_FIFO))
			break;
		dev_info(dsim->dev, "%s clear rx fifo : %08x\n", __func__, rx_fifo);
		i++;
	}
	ret = 0;
	goto exit;

rx_error:
	dsim_reg_force_dphy_stop_state(1);
	usleep_range(3000, 4000);
	dsim_reg_force_dphy_stop_state(0);
	ret = -EPERM;
	goto exit;

exit:
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_gate_lock(dispdrv, false);
#endif
	mutex_unlock(&dsim_rd_wr_mutex);
	return ret;
}

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
void s5p_mipi_dsi_te_triggered(void)
{
	struct display_driver *dispdrv = get_display_driver();
	struct mipi_dsim_device *dsim = dispdrv->dsi_driver.dsim;

	if (!dsim->enabled)
		return;

	dsim_reg_set_pkt_go_ready();
}

void s5p_mipi_dsi_trigger_unmask(void)
{
	struct display_driver *dispdrv = get_display_driver();
	struct mipi_dsim_device *dsim = dispdrv->dsi_driver.dsim;

	if (likely(dsim->pktgo != DSIM_PKTGO_STANDBY))
		return;

	dsim_reg_set_pkt_go_cnt(0xff);
	dsim_reg_set_pkt_go_enable(true);
	dsim->pktgo = DSIM_PKTGO_ENABLED;

	dev_dbg(dsim->dev, "%s: DSIM_PKTGO_ENABLED", __func__);
}
#endif

#ifdef CONFIG_FB_WINDOW_UPDATE
int s5p_mipi_dsi_partial_area_command(struct mipi_dsim_device *dsim,
                                u32 x, u32 y, u32 w, u32 h)
{
	char data[5];
	int left = x;
	int top = y;
	int right = x + w - 1;
	int bottom = y + h - 1;
	int retry;

	if (x != dsim->lcd_win.x || w != dsim->lcd_win.w) {
		data[0] = MIPI_DCS_SET_COLUMN_ADDRESS;
		data[1] = (left >> 8) & 0xff;
		data[2] = left & 0xff;
		data[3] = (right >> 8) & 0xff;
		data[4] = right & 0xff;
		retry = 2;
		while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
					(unsigned int)data, ARRAY_SIZE(data)) != 0) {
			pr_info("%s:fail to write window update size a.\n", __func__);
			if (--retry <= 0) {
				pr_err("%s: size-a:failed: exceed retry count\n", __func__);
				return -1;
			}
		}
		dev_dbg(dsim->dev, "%s: x = %d,  w = %d\n", __func__, x, w);
	}

	if (y != dsim->lcd_win.y || h != dsim->lcd_win.h) {
		data[0] = MIPI_DCS_SET_PAGE_ADDRESS;
		data[1] = (top >> 8) & 0xff;
		data[2] = top & 0xff;
		data[3] = (bottom >> 8) & 0xff;
		data[4] = bottom & 0xff;
		retry = 2;
		while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
					(unsigned int)data, ARRAY_SIZE(data)) != 0) {
			pr_info("%s:fail to write window update size b.\n", __func__);
			if (--retry <= 0) {
				pr_err("%s: size-b:failed: exceed retry count\n", __func__);
				return -1;
			}
		}
		dev_dbg(dsim->dev, "%s: y = %d,  h = %d\n", __func__, y, h);
	}
	dsim->lcd_win.x = x;
	dsim->lcd_win.y = y;
	dsim->lcd_win.w = w;
	dsim->lcd_win.h = h;

	return 0;
}
#endif
void s5p_mipi_dsi_d_phy_onoff(struct mipi_dsim_device *dsim,
	unsigned int enable)
{
	s5p_dsim_init_d_phy(dsim, enable);
}

static irqreturn_t s5p_mipi_dsi_interrupt_handler(int irq, void *dev_id)
{
	unsigned int int_src = 0;
	struct mipi_dsim_device *dsim = dev_id;
	int framedone = 0;
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();
#endif

	spin_lock(&dsim->slock);
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	if (dispdrv->platform_status > DISP_STATUS_PM0 &&
			!dispdrv->pm_status.clock_enabled) {
		dev_err(dsim->dev, "IRQ occured during clock-gating!\n");
		spin_unlock(&dsim->slock);
		return IRQ_HANDLED;
	}
#endif
	int_src = readl(dsim->reg_base + DSIM_INTSRC);

	/* Test bit */
	if (int_src & SFR_PL_FIFO_EMPTY)
		complete(&dsim_wr_comp);
	if (int_src & SFR_PH_FIFO_EMPTY)
		complete(&dsim_ph_wr_comp);
	if (int_src & RX_DAT_DONE)
		complete(&dsim_rd_comp);
	if (int_src & MIPI_FRAME_DONE)
		framedone = 1;
	if (int_src & ERR_RX_ECC)
		dev_err(dsim->dev, "RX ECC Multibit error was detected!\n");
	dsim_reg_clear_int(int_src);

	spin_unlock(&dsim->slock);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	/* tiggering power event for PM */
	if (framedone)
		disp_pm_dec_refcount(dispdrv);
#endif

	return IRQ_HANDLED;
}

static void dsim_device_to_clks(struct mipi_dsim_device *dsim,
		struct dsim_clks *clks)
{
	clks->hs_clk = dsim->lcd_info->hs_clk;
	clks->esc_clk = dsim->lcd_info->esc_clk;
}

static void clks_to_dsim_device(struct dsim_clks *clks,
		struct mipi_dsim_device *dsim)
{
	dsim->hs_clk = clks->hs_clk;
	dsim->escape_clk = clks->esc_clk;
	dsim->byte_clk = clks->byte_clk;
}

int s5p_mipi_dsi_enable_for_tui(struct mipi_dsim_device *dsim)
{
	struct display_driver *dispdrv;
	struct dsim_clks clks;

	mutex_lock(&dsim->lock);

	/* get a reference of the display driver */
	dispdrv = get_display_driver();

	if (dsim->enabled == true) {
		mutex_unlock(&dsim->lock);
		return 0;
	}

	GET_DISPDRV_OPS(dispdrv).enable_display_driver_power(dsim->dev);

	if (dsim->dsim_lcd_drv->resume)
		dsim->dsim_lcd_drv->resume(dsim);

	/* DPHY power on */
	s5p_mipi_dsi_d_phy_onoff(dsim, 1);

	dsim->state = DSIM_STATE_INIT;

	dsim_reg_init(dsim->lcd_info, dsim->dsim_config->e_no_data_lane + 1);

	dsim_device_to_clks(dsim, &clks);
	dsim_reg_set_clocks(&clks, DSIM_LANE_CLOCK | dsim->data_lane, 1);
	clks_to_dsim_device(&clks, dsim);

	dsim_reg_set_lanes(DSIM_LANE_CLOCK | dsim->data_lane, 1);

	dsim->state = DSIM_STATE_STOP;

	GET_DISPDRV_OPS(dispdrv).reset_display_driver_panel(dsim->dev);

	dsim_reg_set_hs_clock(1);

	dsim->state = DSIM_STATE_HSCLKEN;
	/* enable interrupts */
	dsim_reg_set_int(1);

	dsim->enabled = true;

	dsim->dsim_lcd_drv->displayon(dsim);

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	dsim->pktgo = DSIM_PKTGO_STANDBY;
#endif

#ifdef CONFIG_FB_WINDOW_UPDATE
	dsim->lcd_win.x = 0;
	dsim->lcd_win.y = 0;
	dsim->lcd_win.w = dsim->lcd_info->xres;
	dsim->lcd_win.h = dsim->lcd_info->yres;
#endif

	mutex_unlock(&dsim->lock);

	return 0;
}

int s5p_mipi_dsi_disable_for_tui(struct mipi_dsim_device *dsim)
{
	struct display_driver *dispdrv;

	mutex_lock(&dsim->lock);

	/* get a reference of the display driver */
	dispdrv = get_display_driver();

	if (dsim->enabled == false) {
		mutex_unlock(&dsim->lock);
		return 0;
	}

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	dsim_reg_set_pkt_go_cnt(0x0);
	dsim_reg_set_pkt_go_enable(false);
	dsim->pktgo = DSIM_PKTGO_DISABLED;
#endif
	dsim->dsim_lcd_drv->suspend(dsim);
	dsim->enabled = false;

	/* disable interrupts */
	dsim_reg_set_int(0);

	/* disable HS clock */
	dsim_reg_set_hs_clock(0);

	/* make CLK/DATA Lane as LP00 */
	dsim_reg_set_lanes(DSIM_LANE_CLOCK | dsim->data_lane, 0);

	dsim_reg_set_clocks(NULL, DSIM_LANE_CLOCK | dsim->data_lane, 0);

	dsim_reg_sw_reset();

	dsim->state = DSIM_STATE_SUSPEND;

	s5p_mipi_dsi_d_phy_onoff(dsim, 0);

	GET_DISPDRV_OPS(dispdrv).disable_display_driver_power(dsim->dev);

	mutex_unlock(&dsim->lock);

	return 0;
}

int s5p_mipi_dsi_enable(struct mipi_dsim_device *dsim)
{
	struct display_driver *dispdrv;
	struct dsim_clks clks;

	dev_info(dsim->dev, "+%s\n", __func__);

	mutex_lock(&dsim->lock);

	/* get a reference of the display driver */
	dispdrv = get_display_driver();

	if (dsim->enabled == true) {
		mutex_unlock(&dsim->lock);
		return 0;
	}

	GET_DISPDRV_OPS(dispdrv).init_display_dsi_clocks(dsim->dev);
	GET_DISPDRV_OPS(dispdrv).enable_display_driver_power(dsim->dev);

	if (dsim->dsim_lcd_drv->resume)
		dsim->dsim_lcd_drv->resume(dsim);

	/* DPHY power on */
	s5p_mipi_dsi_d_phy_onoff(dsim, 1);

	dsim->state = DSIM_STATE_INIT;

	dsim_reg_init(dsim->lcd_info, dsim->dsim_config->e_no_data_lane + 1);

	dsim_device_to_clks(dsim, &clks);
	dsim_reg_set_clocks(&clks, DSIM_LANE_CLOCK | dsim->data_lane, 1);
	clks_to_dsim_device(&clks, dsim);

	dsim_reg_set_lanes(DSIM_LANE_CLOCK | dsim->data_lane, 1);

	dsim->state = DSIM_STATE_STOP;

	GET_DISPDRV_OPS(dispdrv).reset_display_driver_panel(dsim->dev);

	dsim_reg_set_hs_clock(1);

	dsim->state = DSIM_STATE_HSCLKEN;
	/* enable interrupts */
	dsim_reg_set_int(1);

	/* usleep_range(1000, 1500); */

	dsim->enabled = true;

	dsim->dsim_lcd_drv->init(dsim);
	dsim->dsim_lcd_drv->displayon(dsim);

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	dsim->pktgo = DSIM_PKTGO_STANDBY;
#endif

#ifdef CONFIG_FB_WINDOW_UPDATE
	dsim->lcd_win.x = 0;
	dsim->lcd_win.y = 0;
	dsim->lcd_win.w = dsim->lcd_info->xres;
	dsim->lcd_win.h = dsim->lcd_info->yres;
#endif

	mutex_unlock(&dsim->lock);

	dev_info(dsim->dev, "-%s\n", __func__);

	return 0;
}

int s5p_mipi_dsi_disable(struct mipi_dsim_device *dsim)
{
	struct display_driver *dispdrv;

	dev_info(dsim->dev, "+%s\n", __func__);

	mutex_lock(&dsim->lock);

	/* get a reference of the display driver */
	dispdrv = get_display_driver();

	if (dsim->enabled == false) {
		mutex_unlock(&dsim->lock);
		return 0;
	}

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	dsim_reg_set_pkt_go_cnt(0x0);
	dsim_reg_set_pkt_go_enable(false);
	dsim->pktgo = DSIM_PKTGO_DISABLED;
#endif
	dsim->dsim_lcd_drv->suspend(dsim);
	dsim->enabled = false;

	/* disable interrupts */
	dsim_reg_set_int(0);

	/* disable HS clock */
	dsim_reg_set_hs_clock(0);

	/* make CLK/DATA Lane as LP00 */
	dsim_reg_set_lanes(DSIM_LANE_CLOCK | dsim->data_lane, 0);

	dsim_reg_set_clocks(NULL, DSIM_LANE_CLOCK | dsim->data_lane, 0);

	dsim_reg_sw_reset();

	dsim->state = DSIM_STATE_SUSPEND;

	s5p_mipi_dsi_d_phy_onoff(dsim, 0);

	GET_DISPDRV_OPS(dispdrv).disable_display_driver_power(dsim->dev);

	mutex_unlock(&dsim->lock);

	dev_info(dsim->dev, "-%s\n", __func__);

	return 0;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int s5p_mipi_dsi_lcd_off(struct mipi_dsim_device *dsim)
{
	struct display_driver *dispdrv;
	/* get a reference of the display driver */
	dispdrv = get_display_driver();

	/* disable interrupts */
	dsim_reg_set_int(0);

	dsim->enabled = false;
	dsim->dsim_lcd_drv->suspend(dsim);
	dsim->state = DSIM_STATE_SUSPEND;

	GET_DISPDRV_OPS(dispdrv).disable_display_driver_power(dsim->dev);

	return 0;
}
#endif

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int s5p_mipi_dsi_ulps_handling(u32 en, u32 lanes)
{
	struct decon_lcd *lcd_info = decon_get_lcd_info();
	int ret = 0;

	/* Enter ULPS mode clk & data */
	switch (lcd_info->ddi_type) {
	case TYPE_OF_SM_DDI:
		ret = dsim_reg_set_smddi_ulps(en, lanes);
		break;
	case TYPE_OF_MAGNA_DDI:
		/* Does not support it, nothing to do */
                pr_debug("%s:Does not support ULPS\n", __func__);
		ret = -EBUSY;
		break;
	case TYPE_OF_NORMAL_DDI:
	default:
		ret = dsim_reg_set_ulps(en, lanes);
		break;
	}
	return ret;
}

int s5p_mipi_dsi_hibernation_power_on(struct display_driver *dispdrv)
{
	struct mipi_dsim_device *dsim = dispdrv->dsi_driver.dsim;
	struct dsim_clks clks;

	mutex_lock(&dsim->lock);

	if (dsim->enabled == true) {
		mutex_unlock(&dsim->lock);
		return 0;
	}

	GET_DISPDRV_OPS(dispdrv).init_display_dsi_clocks(dsim->dev);

	/* PPI signal disable + D-PHY reset */
	s5p_mipi_dsi_d_phy_onoff(dsim, 1);

	dsim->state = DSIM_STATE_INIT;

	/* DSIM init */
	dsim_reg_init(dsim->lcd_info, dsim->dsim_config->e_no_data_lane + 1);

	dsim_device_to_clks(dsim, &clks);
	dsim_reg_set_clocks(&clks, DSIM_LANE_CLOCK | dsim->data_lane, 1);
	clks_to_dsim_device(&clks, dsim);

	dsim_reg_set_lanes(DSIM_LANE_CLOCK | dsim->data_lane, 1);

	/* Exit ULPS mode clk & data */
	s5p_mipi_dsi_ulps_handling(0, dsim->data_lane);

	dsim->state = DSIM_STATE_STOP;

	dsim_reg_set_hs_clock(1);

	dsim->state = DSIM_STATE_HSCLKEN;
	dsim_reg_set_int(1);

	dsim->enabled = true;

	mutex_unlock(&dsim->lock);

	return 0;
}

int s5p_mipi_dsi_hibernation_power_off(struct display_driver *dispdrv)
{
	struct mipi_dsim_device *dsim = dispdrv->dsi_driver.dsim;

	mutex_lock(&dsim->lock);

	if (dsim->enabled == false) {
		mutex_unlock(&dsim->lock);
		return 0;
	}

	dsim_reg_set_int(0);

	dsim_reg_set_hs_clock(0);

	/* Enter ULPS mode clk & data */
	if (!s5p_mipi_dsi_ulps_handling(1, dsim->data_lane))
		dsim->state = DSIM_STATE_ULPS;

	/* DSIM STOP SEQUENCE */
	/* CLK and LANE disable */
	dsim_reg_set_lanes(DSIM_LANE_CLOCK | dsim->data_lane, 0);

	dsim_reg_set_clocks(NULL, DSIM_LANE_CLOCK | dsim->data_lane, 0);

	/* S/W reset */
	dsim_reg_sw_reset();

	/* PPI signal disable + D-PHY reset */
	s5p_mipi_dsi_d_phy_onoff(dsim, 0);

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	dsim->pktgo = DSIM_PKTGO_STANDBY;
#endif

	dsim->enabled = false;

	mutex_unlock(&dsim->lock);
	return 0;
}
#endif

int create_mipi_dsi_controller(struct platform_device *pdev)
{
	struct mipi_dsim_device *dsim = NULL;
	struct display_driver *dispdrv;
#if defined(CONFIG_S5P_LCD_INIT)
	struct dsim_clks clks;
#endif
	int ret = -1;

	/* get a reference of the display driver */
	dispdrv = get_display_driver();

	if (!dsim)
		dsim = kzalloc(sizeof(struct mipi_dsim_device),
			GFP_KERNEL);
	if (!dsim) {
		dev_err(&pdev->dev, "failed to allocate dsim object.\n");
		return -EFAULT;
	}

	dispdrv->dsi_driver.dsim = dsim;

	dsim->dev = &pdev->dev;
	dsim->id = pdev->id;

	spin_lock_init(&dsim->slock);

	dsim->dsim_config = dispdrv->dt_ops.get_display_dsi_drvdata();

	dsim->lcd_info = decon_get_lcd_info();

	dsim->reg_base = devm_request_and_ioremap(&pdev->dev, dispdrv->dsi_driver.regs);
	if (!dsim->reg_base) {
		dev_err(&pdev->dev, "mipi-dsi: failed to remap io region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	/*
	 * it uses frame done interrupt handler
	 * only in case of MIPI Video mode.
	 */
	dsim->irq = dispdrv->dsi_driver.dsi_irq_no;
	if (request_irq(dsim->irq, s5p_mipi_dsi_interrupt_handler,
			IRQF_DISABLED, "mipi-dsi", dsim)) {
		dev_err(&pdev->dev, "request_irq failed.\n");
		goto err_irq;
	}

	dsim->dsim_lcd_drv = dsim->dsim_config->dsim_ddi_pd;

	dsim->timing.bps = 0;

	mutex_init(&dsim_rd_wr_mutex);
	mutex_init(&dsim->lock);

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	dsim->pktgo = DSIM_PKTGO_STANDBY;
#endif

	/* set using lanes */
	switch (dsim->dsim_config->e_no_data_lane) {
	case DSIM_DATA_LANE_1:
		dsim->data_lane = DSIM_LANE_DATA0;
		break;
	case DSIM_DATA_LANE_2:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1;
		break;
	case DSIM_DATA_LANE_3:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2;
		break;
	case DSIM_DATA_LANE_4:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2 | DSIM_LANE_DATA3;
		break;
	default:
		dev_info(dsim->dev, "data lane is invalid.\n");
		return -EINVAL;
	};

#ifdef CONFIG_S5P_LCD_INIT
	/* DPHY power on */
	s5p_mipi_dsi_d_phy_onoff(dsim, 1);

	dsim->state = DSIM_STATE_INIT;

	dsim_reg_init(dsim->lcd_info, dsim->dsim_config->e_no_data_lane + 1);

	dsim_device_to_clks(dsim, &clks);
	dsim_reg_set_clocks(&clks, DSIM_LANE_CLOCK | dsim->data_lane, 1);
	clks_to_dsim_device(&clks, dsim);

	dsim_reg_set_lanes(DSIM_LANE_CLOCK | dsim->data_lane, 1);

	dsim->state = DSIM_STATE_STOP;

	GET_DISPDRV_OPS(dispdrv).enable_display_driver_power(&pdev->dev);
	GET_DISPDRV_OPS(dispdrv).reset_display_driver_panel(dsim->dev);

	dsim_reg_set_hs_clock(1);

	dsim->state = DSIM_STATE_HSCLKEN;

	/* enable interrupts */
	dsim_reg_set_int(1);

	dsim->enabled = true;

	dsim->dsim_lcd_drv->probe(dsim);
	dsim->dsim_lcd_drv->init(dsim);
#else
	dsim_reg_set_clear_rx_fifo();

	dsim_reg_init_probe(dsim->lcd_info, dsim->dsim_config->e_no_data_lane + 1);
	dsim_reg_set_lanes(DSIM_LANE_CLOCK | dsim->data_lane, 1);

	GET_DISPDRV_OPS(dispdrv).enable_display_driver_power(&pdev->dev);

	dsim_reg_set_hs_clock(1);

	dsim->state = DSIM_STATE_HSCLKEN;

	/* enable interrupts */
	dsim_reg_set_int(1);

	dsim->enabled = true;

	dsim->dsim_lcd_drv->probe(dsim);
#endif
	dsim_for_decon = dsim;
	dev_info(&pdev->dev, "mipi-dsi driver(%s mode) has been probed.\n",
		(dsim->dsim_config->e_interface == DSIM_COMMAND) ?
			"CPU" : "RGB");

	dispdrv->dsi_driver.dsim = dsim;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	if (dispdrv->dsi_driver.ops) {
		dispdrv->dsi_driver.ops->pwr_on = s5p_mipi_dsi_hibernation_power_on;
		dispdrv->dsi_driver.ops->pwr_off = s5p_mipi_dsi_hibernation_power_off;
	}
#endif

#ifdef CONFIG_FB_WINDOW_UPDATE
	dsim->lcd_win.x = 0;
	dsim->lcd_win.y = 0;
	dsim->lcd_win.w = dsim->lcd_info->xres;
	dsim->lcd_win.h = dsim->lcd_info->yres;
#endif

	return 0;

err_irq:
	release_resource(dispdrv->dsi_driver.regs);
	kfree(dispdrv->dsi_driver.regs);

	iounmap((void __iomem *) dsim->reg_base);

err_mem_region:
	clk_disable(dsim->clock);
	clk_put(dsim->clock);

	kfree(dsim);
	return ret;

}

MODULE_AUTHOR("Haowei li <haowei.li@samsung.com>");
MODULE_DESCRIPTION("Samusung MIPI-DSI driver");
MODULE_LICENSE("GPL");
