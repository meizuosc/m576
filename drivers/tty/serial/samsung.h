/*
 * Driver for Samsung SoC onboard UARTs.
 *
 * Ben Dooks, Copyright (c) 2003-2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/pm_qos.h>

#define S3C24XX_UART_PORT_RESUME		0x0
#define S3C24XX_UART_PORT_SUSPEND		0x3
#define S3C24XX_UART_PORT_LPM			0x5

#define S3C24XX_SERIAL_CTRL_NUM			0x4
#define S3C24XX_SERIAL_BUAD_NUM			0x2

struct s3c24xx_uart_info {
	char			*name;
	unsigned int		type;
	unsigned int		fifosize;
	unsigned long		rx_fifomask;
	unsigned long		rx_fifoshift;
	unsigned long		rx_fifofull;
	unsigned long		tx_fifomask;
	unsigned long		tx_fifoshift;
	unsigned long		tx_fifofull;
	unsigned int		def_clk_sel;
	unsigned long		num_clks;
	unsigned long		clksel_mask;
	unsigned long		clksel_shift;

	/* uart port features */

	unsigned int		has_divslot:1;

	/* uart controls */
	int (*reset_port)(struct uart_port *, struct s3c2410_uartcfg *);
};

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
struct uart_dma_data {
	unsigned ch;
	unsigned int busy;
	unsigned int req_size;
	unsigned long fifo_base;
	enum dma_ch req_ch;
	enum dma_transfer_direction direction;
};

struct exynos_uart_dma {
	unsigned int use_dma;

	dma_addr_t tx_src_addr;
	dma_addr_t rx_dst_addr;

	struct uart_dma_data tx;
	struct uart_dma_data rx;

	struct samsung_dma_ops *ops;
	struct platform_device *pdev;

	char *rx_buff;
};
#endif

struct s3c24xx_serial_drv_data {
	struct s3c24xx_uart_info	*info;
	struct s3c2410_uartcfg		*def_cfg;
	unsigned int			fifosize[CONFIG_SERIAL_SAMSUNG_UARTS];
};

struct s3c24xx_uart_port {
	struct list_head		node;
	unsigned char			rx_claimed;
	unsigned char			tx_claimed;
	unsigned long			baudclk_rate;

	unsigned int			rx_irq;
	unsigned int			tx_irq;

#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	unsigned int                    err_irq;
	unsigned int                    err_occurred;
#endif
	int				check_separated_clk;
	struct s3c24xx_uart_info	*info;
	struct clk			*clk;
	struct clk			*separated_clk;
	struct clk			*baudclk;
	struct uart_port		port;
#ifdef CONFIG_SERIAL_SAMSUNG_DMA
	struct exynos_uart_dma          uart_dma;
#endif
	struct s3c24xx_serial_drv_data	*drv_data;

	u32				uart_irq_affinity;
	s32				mif_qos_val;
	s32				cpu_qos_val;
	u32				use_default_irq;
	unsigned long			qos_timeout;

#define DOMAIN_TOP	0
#define DOMAIN_AUD	1
	u32				domain;
	atomic_t			serial_suspend;
	unsigned int			uart_lpm_wake;
	struct work_struct		uart_port_lpm_work;

	/* uart port sfr save/restore */
	unsigned int			serial_ctrl[S3C24XX_SERIAL_CTRL_NUM];
	unsigned int			serial_baud[S3C24XX_SERIAL_BUAD_NUM];

	/* reference to platform data */
	struct s3c2410_uartcfg		*cfg;

	struct notifier_block		aud_uart_notifier;
	unsigned int			aud_uart_notifier_suspend;
	struct platform_device		*pdev;

	struct pm_qos_request		s3c24xx_uart_mif_qos;
	struct pm_qos_request		s3c24xx_uart_cpu_qos;
	struct delayed_work		qos_work;
};

/* conversion functions */

#define s3c24xx_dev_to_port(__dev) (struct uart_port *)dev_get_drvdata(__dev)

/* register access controls */

#define portaddr(port, reg) ((port)->membase + (reg))
#define portaddrl(port, reg) ((unsigned long *)((port)->membase + (reg)))

#define rd_regb(port, reg) (__raw_readb(portaddr(port, reg)))
#define rd_regl(port, reg) (__raw_readl(portaddr(port, reg)))

#define wr_regb(port, reg, val) __raw_writeb(val, portaddr(port, reg))
#define wr_regl(port, reg, val) __raw_writel(val, portaddr(port, reg))

#if defined(CONFIG_SERIAL_SAMSUNG_DEBUG) && \
    defined(CONFIG_DEBUG_LL) && \
    !defined(MODULE)

extern void printascii(const char *);

static void dbg(const char *fmt, ...)
{
	va_list va;
	char buff[256];

	va_start(va, fmt);
	vsprintf(buff, fmt, va);
	va_end(va);

	printascii(buff);
}

#else
#define dbg(x...) do { } while (0)
#endif
