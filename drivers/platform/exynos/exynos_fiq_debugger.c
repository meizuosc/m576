/*
 * Serial Debugger Interface for exynos
 *
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/uaccess.h>
#include <linux/of.h>

#include <../../../drivers/staging/android/fiq_debugger/fiq_debugger.h>

#include <linux/serial_s3c.h>

#include <mach/map.h>

struct exynos_fiq_debugger {
	struct fiq_debugger_pdata pdata;
	struct platform_device *pdev;
	void __iomem *debug_port_base;
	int irq;
	u32 baud;
	u32 frac_baud;
};

static inline struct exynos_fiq_debugger *get_dbg(struct platform_device *pdev)
{
	struct fiq_debugger_pdata *pdata = dev_get_platdata(&pdev->dev);
	return container_of(pdata, struct exynos_fiq_debugger, pdata);
}

static inline void exynos_write(struct exynos_fiq_debugger *dbg,
			       unsigned int val, unsigned int off)
{
	__raw_writel(val, dbg->debug_port_base + off);
}

static inline unsigned int exynos_read(struct exynos_fiq_debugger *dbg,
				      unsigned int off)
{
	return __raw_readl(dbg->debug_port_base + off);
}

static int debug_port_init(struct platform_device *pdev)
{
	struct exynos_fiq_debugger *dbg = get_dbg(pdev);
	unsigned long timeout;

	exynos_write(dbg, dbg->baud, S3C2410_UBRDIV);
	exynos_write(dbg, dbg->frac_baud, S3C2443_DIVSLOT);

	/* Mask and clear all interrupts */
	exynos_write(dbg, 0xF, S3C64XX_UINTM);
	exynos_write(dbg, 0xF, S3C64XX_UINTP);

	exynos_write(dbg, S3C2410_LCON_CS8, S3C2410_ULCON);
	exynos_write(dbg, S5PV210_UCON_DEFAULT, S3C2410_UCON);
	exynos_write(dbg, S5PV210_UFCON_DEFAULT, S3C2410_UFCON);
	exynos_write(dbg, 0, S3C2410_UMCON);

	/* Reset TX and RX fifos */
	exynos_write(dbg, S5PV210_UFCON_DEFAULT | S3C2410_UFCON_RESETBOTH,
		S3C2410_UFCON);

	timeout = jiffies + HZ;
	while (exynos_read(dbg, S3C2410_UFCON) & S3C2410_UFCON_RESETBOTH)
		if (time_after(jiffies, timeout))
			return -ETIMEDOUT;

	/* Enable all interrupts except TX */
	exynos_write(dbg, S3C64XX_UINTM_TXD_MSK, S3C64XX_UINTM);

	return 0;
}

static int debug_getc(struct platform_device *pdev)
{
	struct exynos_fiq_debugger *dbg = get_dbg(pdev);
	u32 stat;
	int ret = FIQ_DEBUGGER_NO_CHAR;

	/* Clear all pending interrupts */
	exynos_write(dbg, 0xF, S3C64XX_UINTP);

	stat = exynos_read(dbg, S3C2410_UERSTAT);
	if (stat & S3C2410_UERSTAT_BREAK)
		return FIQ_DEBUGGER_BREAK;

	stat = exynos_read(dbg, S3C2410_UTRSTAT);
	if (stat & S3C2410_UTRSTAT_RXDR)
		ret = exynos_read(dbg, S3C2410_URXH);

	return ret;
}

static void debug_putc(struct platform_device *pdev, unsigned int c)
{
	struct exynos_fiq_debugger *dbg = get_dbg(pdev);
	int count = loops_per_jiffy;

	if (exynos_read(dbg, S3C2410_ULCON) != S3C2410_LCON_CS8)
		debug_port_init(pdev);

	while (exynos_read(dbg, S3C2410_UFSTAT) & S5PV210_UFSTAT_TXFULL)
		if (--count == 0)
			return;

	exynos_write(dbg, c, S3C2410_UTXH);
}

static void debug_flush(struct platform_device *pdev)
{
	struct exynos_fiq_debugger *dbg = get_dbg(pdev);
	int count = loops_per_jiffy * HZ;

	while (!(exynos_read(dbg, S3C2410_UTRSTAT) & S3C2410_UTRSTAT_TXE))
		if (--count == 0)
			return;
}

static int debug_suspend(struct platform_device *pdev)
{
	struct exynos_fiq_debugger *dbg = get_dbg(pdev);

	exynos_write(dbg, 0xF, S3C64XX_UINTM);

	return 0;
}

static int debug_resume(struct platform_device *pdev)
{
	debug_port_init(pdev);

	return 0;
}

static int  __exynos_fiq_debug_init(struct platform_device *pdev, int id, int irq)
{
	int ret = -ENOMEM;
	struct exynos_fiq_debugger *dbg = NULL;
	struct resource *res;
	int res_count = 0;

	if (id >= CONFIG_SERIAL_SAMSUNG_UARTS)
		return -EINVAL;

	dbg = kzalloc(sizeof(struct exynos_fiq_debugger), GFP_KERNEL);
	if (!dbg) {
		pr_err("exynos_fiq_debugger: failed to allocate fiq debugger\n");
		goto err_free;
	}

	res = kzalloc(sizeof(struct resource) * 1, GFP_KERNEL);
	if (!res) {
		pr_err("Failed to alloc fiq debugger resources\n");
		goto err_res;
	}

	pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if(!pdev) {
		pr_err("Failed to alloc fiq debugger platform device\n");
		goto err_pdev;
	}

	dbg->pdata.uart_init = debug_port_init;
	dbg->pdata.uart_getc = debug_getc;
	dbg->pdata.uart_putc = debug_putc;
	dbg->pdata.uart_flush = debug_flush;
	dbg->pdata.uart_dev_suspend = debug_suspend;
	dbg->pdata.uart_dev_resume = debug_resume;

	dbg->pdev = pdev;
	dbg->debug_port_base = S3C_VA_UARTx(id);
	dbg->baud = exynos_read(dbg, S3C2410_UBRDIV);
	dbg->frac_baud = exynos_read(dbg, S3C2443_DIVSLOT);

	res[0].flags = IORESOURCE_IRQ;
	res[0].start = irq;
	res[0].end = irq;
	res[0].name = "uart_irq";
	res_count++;

	pdev->name = "fiq_debugger";
	pdev->id = id;
	pdev->dev.platform_data = &dbg->pdata;
	pdev->resource = res;
	pdev->num_resources = res_count;

	if (platform_device_register(pdev)) {
		pr_err("Failed to register fiq debugger\n");
		goto err_free;
	}

	return 0;

err_pdev:
	kfree(pdev);
err_res:
	kfree(res);
err_free:
	kfree(dbg);
	return ret;
}

static int exynos_fiq_debug_probe(struct platform_device *pdev)
{
	struct resource *res;
	int uart_irq;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No mem resource\n");
		return -EINVAL;
	}

	uart_irq = platform_get_irq_byname(pdev, "uart_irq");
	if (uart_irq < 0) {
		dev_err(&pdev->dev, "No IRQ for uart_irq, error=%d\n", uart_irq);
		return uart_irq;
	}

	__exynos_fiq_debug_init(pdev, CONFIG_S3C_LOWLEVEL_UART_PORT, uart_irq);

	return 0;
}

static const struct of_device_id fiq_debugger_match[] = {
	{.compatible = "samsung,exynos_fiq_debugger",},
	{},
};

static struct platform_driver exynos_fiq_debug_driver = {
	.probe      = exynos_fiq_debug_probe,
	.driver     = {
		.name   = "exynos_fiq_debug",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(fiq_debugger_match),
	},
};

static int __init exynos_fiq_debugger_init(void)
{
	return platform_driver_register(&exynos_fiq_debug_driver);
}

postcore_initcall_sync(exynos_fiq_debugger_init);
