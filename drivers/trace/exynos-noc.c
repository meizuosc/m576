/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * NOC BUS Debugging Driver for Samsung EXYNOS SoC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/exynos-noc.h>
#include <linux/pinctrl/pinctrl-samsung.h>
#include <plat/cpu.h>

#define NOC_LOGGING_REG_FAULTEN		(0x08)
#define NOC_LOGGING_REG_ERRVLD		(0x0C)
#define NOC_LOGGING_REG_ERRCLR		(0x10)
#define NOC_LOGGING_REG_ERRLOG0		(0x14)
#define NOC_LOGGING_REG_ERRLOG1		(0x18)
#define NOC_LOGGING_REG_ERRLOG2		(0x1C)
#define NOC_LOGGING_REG_ERRLOG3		(0x20)
#define NOC_LOGGING_REG_ERRLOG4		(0x24)
#define NOC_LOGGING_REG_ERRLOG5		(0x28)

#define START				(0)
#define END				(1)
#define ARRAY_BITS			(2)
#define ARRAY_SUBRANGE_MAX		(1024)

#define NEED_TO_CHECK			(0xCAFE)

struct noc_timeout {
	char *name;
	void __iomem *regs;
	u32 enabled;
	u32 enable_bit;
	u32 range_bits[ARRAY_BITS];
	struct list_head list;
};

struct noc_platdata {
	/* RouteID Information Bits */
	u32 init_bits[ARRAY_BITS];
	u32 target_bits[ARRAY_BITS];
	u32 sub_bits[ARRAY_BITS];
	u32 seq_bits[ARRAY_BITS];

	/* Registers Bits */
	u32 faulten_bits[ARRAY_BITS];
	u32 errvld_bits[ARRAY_BITS];
	u32 errclr_bits[ARRAY_BITS];
	u32 errlog0_lock_bits[ARRAY_BITS];
	u32 errlog0_opc_bits[ARRAY_BITS];
	u32 errlog0_errcode_bits[ARRAY_BITS];
	u32 errlog0_len1_bits[ARRAY_BITS];
	u32 errlog0_format_bits[ARRAY_BITS];
	u32 errlog1_bits[ARRAY_BITS];
	u32 errlog2_bits[ARRAY_BITS];
	u32 errlog3_bits[ARRAY_BITS];
	u32 errlog4_bits[ARRAY_BITS];
	u32 errlog5_bits[ARRAY_BITS];

	u32 init_num;
	u32 target_num;
	u32 sub_num;
	u32 sub_array;

	u32 init_flow;
	u32 target_flow;
	u32 subrange;
	u64 target_addr;

	u32 sub_index[ARRAY_SUBRANGE_MAX];
	u32 sub_addr[ARRAY_SUBRANGE_MAX];

	/* NOC-timeout block list */
	struct list_head timeout_list;
};

struct noc_dev {
	struct device			*dev;
	struct noc_platdata		*pdata;
	struct of_device_id		*match;
	int				irq;
	int				id;
	void __iomem			*regs;
	spinlock_t			ctrl_lock;
};

struct noc_panic_block {
	struct notifier_block nb_panic_block;
	struct noc_dev *pdev;
};

/* declare notifier_list */
static ATOMIC_NOTIFIER_HEAD(noc_notifier_list);

static const struct of_device_id noc_dt_match[] = {
	{ .compatible = "samsung,exynos-noc",
	  .data = NULL, },
	{},
};
MODULE_DEVICE_TABLE(of, noc_dt_match);

#define NOC_INIT_DESC_STRING		"init-desc"
#define NOC_TARGET_DESC_STRING		"target-desc"

static char* noc_get_desc(struct device_node *np,
				 const char* desc_str,
				 unsigned int desc_num)
{
	const char *desc_ret;

	of_property_read_string_index(np, desc_str, desc_num, (const char **)&desc_ret);

	return (char *)desc_ret;
}

static unsigned int noc_get_bits(u32 *bits, unsigned int val)
{
	unsigned int ret = 0, i;

	/* Make masking value by checking from start-bit to end-bit */
	for (i = bits[START]; i <= bits[END]; i++)
		ret = (ret | (1 << i));

	return ret & val;
}

static void noc_logging_dump_raw(struct noc_dev *noc)
{
	struct noc_platdata *pdata = noc->pdata;
	unsigned int errlog0, errlog1, errlog2, errlog3, errlog4, errlog5;
	char *init_desc, *target_desc;

	errlog0 = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRLOG0);
	errlog1 = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRLOG1);
	errlog2 = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRLOG2);
	errlog3 = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRLOG3);
	errlog4 = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRLOG4);
	errlog5 = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRLOG5);

	init_desc = noc_get_desc(noc->dev->of_node, NOC_INIT_DESC_STRING, pdata->init_flow);
	target_desc = noc_get_desc(noc->dev->of_node, NOC_TARGET_DESC_STRING, pdata->target_flow);

	dev_info(noc->dev,"%s ----> %s, "
			  "initflow:%x, targetflow:%x, subrange:%x\n",
			  init_desc, target_desc,
			  pdata->init_flow, pdata->target_flow, pdata->subrange);

	dev_err(noc->dev, "\n=============================================\n"
			"Errlog Raw Registers\n"
			"\tErrLog0  : 0x%x\n"
			"\tErrLog1  : 0x%x\n"
			"\tErrLog2  : 0x%x\n"
			"\tErrLog3  : 0x%x\n"
			"\tErrLog4  : 0x%x\n"
			"\tErrLog5  : 0x%x\n"
			"=============================================\n",
		errlog0, errlog1, errlog2, errlog3, errlog4, errlog5);

	dev_err(noc->dev, "\n=============================================\n"
			"Errlog0 Parsing Registers\n"
			"\topc      : 0x%x\n"
			"\tErrorCode: 0x%x\n"
			"\tLen1     : 0x%x\n"
			"\tFormat   : 0x%x\n"
			"Errlog(1-5) Parsing Registers\n"
			"\tErrLog1  : 0x%x\n"
			"\tErrLog2  : 0x%x\n"
			"\tErrLog3  : 0x%x\n"
			"\tErrLog4  : 0x%x\n"
			"\tErrLog5  : 0x%x\n"
			"\tTarget Address : 0x%llx\n"
			"=============================================\n",
		noc_get_bits(pdata->errlog0_opc_bits, errlog0) >> pdata->errlog0_opc_bits[START],
		noc_get_bits(pdata->errlog0_errcode_bits, errlog0) >> pdata->errlog0_errcode_bits[START],
		noc_get_bits(pdata->errlog0_len1_bits, errlog0) >> pdata->errlog0_len1_bits[START],
		noc_get_bits(pdata->errlog0_format_bits, errlog0) >> pdata->errlog0_format_bits[START],
		noc_get_bits(pdata->errlog1_bits, errlog1) >> pdata->errlog1_bits[START],
		noc_get_bits(pdata->errlog2_bits, errlog2) >> pdata->errlog2_bits[START],
		noc_get_bits(pdata->errlog3_bits, errlog3) >> pdata->errlog3_bits[START],
		noc_get_bits(pdata->errlog4_bits, errlog4) >> pdata->errlog4_bits[START],
		noc_get_bits(pdata->errlog5_bits, errlog5) >> pdata->errlog5_bits[START],
		pdata->target_addr);

	if (!pdata->target_addr)
		dev_err(noc->dev, "Target Address is not valid, Needs to check\n");
}

static void noc_logging_parse_route(struct noc_dev *noc)
{
	struct noc_platdata *pdata = noc->pdata;

	unsigned int init_id, target_id, sub_id, val, bits;
	unsigned int errlog3 = 0, errlog4 = 0, i;

	val = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRLOG1);
	bits = noc_get_bits(pdata->errlog1_bits, val);

	init_id = noc_get_bits(pdata->init_bits, bits) >> pdata->init_bits[START];
	target_id = noc_get_bits(pdata->target_bits, bits) >> pdata->target_bits[START];
	sub_id = noc_get_bits(pdata->sub_bits, bits) >> pdata->sub_bits[START];

	pdata->init_flow = init_id;
	pdata->target_flow = target_id;
	pdata->subrange = sub_id;

	/* Calculate target address */
	errlog3 = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRLOG3);
	errlog4 = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRLOG4);

	errlog3 = noc_get_bits(pdata->errlog3_bits, errlog3) >> pdata->errlog3_bits[START];
	errlog4 = noc_get_bits(pdata->errlog4_bits, errlog4) >> pdata->errlog4_bits[START];
	val = (init_id * (pdata->target_num * pdata->sub_num)) +
	       (target_id * pdata->target_num) + sub_id;

	for (i = 0; i < pdata->sub_array; i++) {
		if (pdata->sub_index[i] == val) {
			if (pdata->sub_addr[i] == NEED_TO_CHECK) {
				pdata->target_addr = 0;
			} else {
				pdata->target_addr = ((u64)errlog4 << 32);
				pdata->target_addr |= (errlog3 + pdata->sub_addr[i]);
			}
			break;
		}
	}
}

static void noc_logging_dump(struct noc_dev *noc)
{
	noc_logging_parse_route(noc);
	noc_logging_dump_raw(noc);
}

static irqreturn_t noc_logging_irq(int irq, void *data)
{
	struct noc_dev *noc = (struct noc_dev *)data;
	struct noc_platdata *pdata = noc->pdata;
	unsigned int bits;
	unsigned int val;

	/* Check error has been logged */
	val = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRVLD);
	bits = noc_get_bits(pdata->errvld_bits, val);

	if (bits) {
		char *init_desc;

		dev_info(noc->dev, "BUS monitor information: %d interrupt occurs.\n", (irq - 32));
		noc_logging_dump(noc);

		/* error clear */
		bits = noc_get_bits(pdata->errclr_bits, 1);
		__raw_writel(bits, noc->regs + NOC_LOGGING_REG_ERRCLR);

		/* This code is for finding out the source */
		init_desc = noc_get_desc(noc->dev->of_node, NOC_INIT_DESC_STRING,
						pdata->init_flow);

		/* call notifier_call_chain of noc */
		atomic_notifier_call_chain(&noc_notifier_list, 0, init_desc);

		if (init_desc && strncmp(init_desc, "P_CCI", strlen("P_CCI"))) {
			if (!strncmp(init_desc, "G3D0", strlen("G3D0"))
				|| !strncmp(init_desc, "G3D1", strlen("G3D1"))) {
				if (pdata->target_addr)
					panic("Error detected by BUS monitor.");
			} else {
				panic("Error detected by BUS monitor.");
			}
		}
	}

	return IRQ_HANDLED;
}

void noc_notifier_chain_register(struct notifier_block *block)
{
	atomic_notifier_chain_register(&noc_notifier_list, block);
}

static int noc_logging_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	struct noc_panic_block *noc_panic = (struct noc_panic_block *)nb;
	struct noc_dev *noc = noc_panic->pdev;
	struct noc_platdata *pdata = noc->pdata;
	unsigned int bits;
	unsigned int val;

	if (!IS_ERR_OR_NULL(noc)) {
		/* Check error has been logged */
		val = __raw_readl(noc->regs + NOC_LOGGING_REG_ERRVLD);
		bits = noc_get_bits(pdata->errvld_bits, val);

		if (bits)
			noc_logging_dump(noc);
		else
			dev_info(noc->dev, "BUS monitor did not detect any error.\n");
	}
	return 0;
}

static void noc_timeout_init(struct noc_dev *noc)
{
	struct noc_timeout *timeout;
	struct list_head *entry;
	u32 val;

	if (list_empty(&noc->pdata->timeout_list))
		return;

	list_for_each(entry, &noc->pdata->timeout_list) {
		timeout = list_entry(entry, struct noc_timeout, list);
		if (timeout && timeout->enabled) {
			/* NOC-timeout is not supported under 1.2 rev of exynos7420 */
			if (!soc_is_exynos7420() ||
				samsung_rev() >= EXYNOS7420_REV_1_2) {
				val = __raw_readl(timeout->regs);
				val |= (0x1) << timeout->enable_bit;
				__raw_writel(val, timeout->regs);

				dev_info(noc->dev, "%s: noc-timeout enabled(bit:%d)\n",
					timeout->name, timeout->enable_bit);
			}
		}
	}
}

static void noc_logging_init(struct noc_dev *noc)
{
	struct noc_platdata *pdata = noc->pdata;
	unsigned int bits;

	/* first of all, error clear at occurs previous */
	bits = noc_get_bits(pdata->errclr_bits, 1);
	__raw_writel(bits, noc->regs + NOC_LOGGING_REG_ERRCLR);

	/* enable logging init */
	bits = noc_get_bits(pdata->faulten_bits, 1);
	__raw_writel(bits, noc->regs + NOC_LOGGING_REG_FAULTEN);
}

static int noc_dt_parse(struct device_node *np,
				struct noc_dev *noc)
{
	struct noc_platdata *pdata = noc->pdata;
	struct device_node *time_np, *time_child_np = NULL;
	struct noc_timeout *timeout;
	u32 regs[2];

	if (!np || !pdata)
		return -EINVAL;

	/* Read NOC-BUS Logging setting */
	of_property_read_u32_array(np, "seq-bits", pdata->seq_bits, 2);
	of_property_read_u32_array(np, "sub-bits", pdata->sub_bits, 2);
	of_property_read_u32_array(np, "target-bits", pdata->target_bits, 2);
	of_property_read_u32_array(np, "init-bits", pdata->init_bits, 2);

	of_property_read_u32_array(np, "faulten-bits", pdata->faulten_bits, 2);
	of_property_read_u32_array(np, "errvld-bits", pdata->errvld_bits, 2);
	of_property_read_u32_array(np, "errclr-bits", pdata->errclr_bits, 2);

	of_property_read_u32_array(np, "errlog0-lock-bits", pdata->errlog0_lock_bits, 2);
	of_property_read_u32_array(np, "errlog0-opc-bits", pdata->errlog0_opc_bits, 2);
	of_property_read_u32_array(np, "errlog0-errcode-bits", pdata->errlog0_errcode_bits, 2);
	of_property_read_u32_array(np, "errlog0-len1-bits", pdata->errlog0_len1_bits, 2);
	of_property_read_u32_array(np, "errlog0-format-bits", pdata->errlog0_format_bits, 2);
	of_property_read_u32_array(np, "errlog1-bits", pdata->errlog1_bits, 2);
	of_property_read_u32_array(np, "errlog2-bits", pdata->errlog2_bits, 2);
	of_property_read_u32_array(np, "errlog3-bits", pdata->errlog3_bits, 2);
	of_property_read_u32_array(np, "errlog4-bits", pdata->errlog4_bits, 2);
	of_property_read_u32_array(np, "errlog5-bits", pdata->errlog5_bits, 2);

	of_property_read_u32(np, "init-num", &pdata->init_num);
	of_property_read_u32(np, "target-num", &pdata->target_num);
	of_property_read_u32(np, "sub-num", &pdata->sub_num);
	of_property_read_u32(np, "sub-array", &pdata->sub_array);

	of_property_read_u32_array(np, "sub-index", pdata->sub_index, pdata->sub_array);
	of_property_read_u32_array(np, "sub-addr", pdata->sub_addr, pdata->sub_array);

	INIT_LIST_HEAD(&pdata->timeout_list);

	/* Check NOC-BUS Timeout setting(Option) */
	time_np = of_get_child_by_name(np, "timeout");
	if (!time_np)
		return 0;

	/* NOC-BUS timeout setting */
	while ((time_child_np = of_get_next_child(time_np, time_child_np)) != NULL) {
		timeout = devm_kzalloc(noc->dev, sizeof(struct noc_timeout), GFP_KERNEL);
		if (!timeout) {
			dev_err(noc->dev, "failed to allocate memory for noc-timeout\n");
			continue;
		}
		of_property_read_string(time_child_np, "nickname", (const char **)&timeout->name);
		of_property_read_u32_array(time_child_np, "reg", regs, 2);
		timeout->regs = ioremap(regs[0], regs[1]);
		if (!timeout->regs) {
			dev_err(noc->dev, "failed to ioremap for noc-timeout: %s\n", timeout->name);
			devm_kfree(noc->dev, timeout);
			continue;
		}
		of_property_read_u32(time_child_np, "enabled", &timeout->enabled);
		of_property_read_u32(time_child_np, "enable-bit", &timeout->enable_bit);
		of_property_read_u32_array(time_child_np, "range-bits", timeout->range_bits, 2);
		list_add(&timeout->list, &pdata->timeout_list);

		dev_info(noc->dev, "%s: 0x%x is registered successfully for noc-timeout\n",
						timeout->name, regs[0]);
	}
	of_node_put(time_np);
	return 0;
}

static int noc_probe(struct platform_device *pdev)
{
	struct noc_dev *noc;
	struct noc_platdata *pdata = NULL;
	struct noc_panic_block *noc_panic = NULL;
	const struct of_device_id *match;
	struct resource *res;
	int ret;

	noc = devm_kzalloc(&pdev->dev, sizeof(struct noc_dev), GFP_KERNEL);
	if (!noc) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"private data\n");
		return -ENOMEM;
	}
	noc->dev = &pdev->dev;
	match = of_match_node(noc_dt_match, pdev->dev.of_node);
	noc->match = (struct of_device_id *)match;

	spin_lock_init(&noc->ctrl_lock);

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct noc_platdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"platform data\n");
		return -ENOMEM;
	}
	noc->pdata = pdata;

	ret = noc_dt_parse(pdev->dev.of_node, noc);
	if (ret) {
		dev_err(&pdev->dev, "failed to assign device tree parsing\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENXIO;

	noc->regs = devm_request_and_ioremap(&pdev->dev, res);
	if (noc->regs == NULL) {
		dev_err(&pdev->dev, "failed to claim register region\n");
		return -ENOENT;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		return -ENXIO;

	noc->irq = res->start;
	ret = devm_request_irq(&pdev->dev, noc->irq, noc_logging_irq,
					0, dev_name(&pdev->dev), noc);
	if (ret) {
		dev_err(&pdev->dev, "irq request failed\n");
		return -ENXIO;
	}

	noc_panic = devm_kzalloc(&pdev->dev, sizeof(struct noc_panic_block), GFP_KERNEL);
	if (!noc_panic) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"panic handler data\n");
	} else {
		noc_panic->nb_panic_block.notifier_call = noc_logging_panic_handler;
		noc_panic->pdev = noc;
		atomic_notifier_chain_register(&panic_notifier_list, &noc_panic->nb_panic_block);
	}

	platform_set_drvdata(pdev, noc);
	dev_info(&pdev->dev, "Exynos NOC Debugging Driver registered successfully\n");

	noc_timeout_init(noc);
	noc_logging_init(noc);

	return 0;
}

static int noc_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int noc_suspend(struct device *dev)
{
	return 0;
}

static int noc_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct noc_dev *noc = platform_get_drvdata(pdev);

	noc_timeout_init(noc);
	noc_logging_init(noc);

	return 0;
}

static SIMPLE_DEV_PM_OPS(noc_pm_ops,
			 noc_suspend,
			 noc_resume);
#define NOC_PM	(noc_pm_ops)
#else
#define NOC_PM	NULL
#endif

static struct platform_driver exynos_noc_driver = {
	.probe		= noc_probe,
	.remove		= noc_remove,
	.driver		= {
		.name		= "exynos-noc",
		.of_match_table	= noc_dt_match,
		.pm		= &noc_pm_ops,
	},
};

module_platform_driver(exynos_noc_driver);

MODULE_DESCRIPTION("Samsung Exynos NOC-BUS DEBUGGING DRIVER");
MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-noc");
