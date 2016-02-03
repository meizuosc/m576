/*
 * Ramoops Interface for exynos
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/pstore.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/compiler.h>
#include <linux/pstore_ram.h>
#include <linux/of.h>

#include <linux/exynos_ion.h>

#ifdef CONFIG_OF
static int ramoops_dt_platform_data(struct device *dev, struct ramoops_platform_data *pdata)
{
	int ret = -ENOMEM;
	struct device_node *np = dev->of_node;

	if (!of_device_is_compatible(np, "samsung,exynos_ramoops")) {
		ret = -ENODEV;
		goto err_out;
	}

	//of_property_read_u32(np, "ramoops,mem_address", (int*)&pdata->mem_address);
	//of_property_read_u32(np, "ramoops,mem_size", (int*)&pdata->mem_size);
	of_property_read_u32(np, "ramoops,record_size", (int*)&pdata->record_size);
	of_property_read_u32(np, "ramoops,console_size", (int*)&pdata->console_size);
	of_property_read_u32(np, "ramoops,ftrace_size", (int*)&pdata->ftrace_size);
	of_property_read_u32(np, "ramoops,dump_oops", &pdata->dump_oops);
	of_property_read_u32(np, "ramoops,ecc_info_block_size", &pdata->ecc_info.block_size);
	of_property_read_u32(np, "ramoops,ecc_info_ecc_size", &pdata->ecc_info.ecc_size);
	of_property_read_u32(np, "ramoops,ecc_info_symsize", &pdata->ecc_info.symsize);
	of_property_read_u32(np, "ramoops,ecc_info_poly", &pdata->ecc_info.poly);

	pr_debug("%s: mem_address 0x%08lx,mem_size 0x%08lx, record_size 0x%08lx, \n"
			"console_size 0x%08lx, ftrace_size 0x%08lx, dump_oops %d, \n"
			"ecc_info.block_size %d, ecc_info.ecc_size %d, \n "
			"ecc_info.symsize %d, ecc_info.poly %d\n", __func__ ,
			pdata->mem_address, pdata->mem_size, pdata->record_size,
			pdata->console_size, pdata->ftrace_size, pdata->dump_oops,
			pdata->ecc_info.block_size, pdata->ecc_info.ecc_size,
			pdata->ecc_info.symsize, pdata->ecc_info.poly);

	ret = 0;

err_out:
	return ret;
}
#else
static int ramoops_dt_platform_data(struct device *dev, struct ramoops_platform_data *pdata)
{
	return 0;
}
#endif

static int exynos_ramoops_probe(struct platform_device *pdev)
{
	int ret = -ENOMEM;
	struct ramoops_platform_data *ramoops_data = NULL;
	struct platform_device *ramoops_dev = NULL;
	phys_addr_t base = 0;
	size_t size = 0;

	ramoops_data = kzalloc(sizeof(struct ramoops_platform_data), GFP_KERNEL);
	if (!ramoops_data) {
		pr_err("exynos_ramoops: failed to allocate ramoops_data\n");
		goto err;
	}

	ramoops_dev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if(!ramoops_dev) {
		pr_err("Failed to alloc ramoops platform device\n");
		goto err;
	}

	if (ion_exynos_contig_heap_info(ION_EXYNOS_ID_RAMOOPS, &base, &size)) {
		pr_err("Failed to get contig head info\n");
		goto err;
	}
	ramoops_data->mem_address = base;
	ramoops_data->mem_size = size;

	ret = ramoops_dt_platform_data(&pdev->dev, ramoops_data);
	if (ret) {
		pr_err("Failed to parse ramoops dt platform data\n");
		goto err;
	}

	ramoops_dev->name = "ramoops";
	ramoops_dev->dev.platform_data = ramoops_data;

	if (platform_device_register(ramoops_dev)) {
		pr_err("Failed to register ramoops\n");
		goto err;
	}

	return 0;

err:
	if (ramoops_dev) kfree(ramoops_dev);
	if (ramoops_data) kfree(ramoops_data);
	return ret;
}

static const struct of_device_id ramoops_match[] = {
	{.compatible = "samsung,exynos_ramoops",},
	{},
};

static struct platform_driver exynos_ramoops_driver = {
	.probe      = exynos_ramoops_probe,
	.driver     = {
		.name   = "exynos_ramoops",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(ramoops_match),
	},
};

static int __init exynos_ramoops_init(void)
{
	return platform_driver_register(&exynos_ramoops_driver);
}

late_initcall(exynos_ramoops_init);
