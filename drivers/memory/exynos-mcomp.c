/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/memblock.h>
#include "exynos-mcomp.h"

/* file rw */
#include <linux/file.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

#define NUM_DISK		4
#define CHK_SIZE		4096

u64 *test_swap_disk;
u64 *test_comp_buf;
u64 *test_comp_info;

unsigned long phy_addr;
dma_addr_t disk, comp, info;

static const char driver_name[] = "exynos-mcomp";

/* irq, SFR base, address information and tasklet */
struct memory_comp {
	unsigned int	irq;
	void __iomem	*base;
	u32 sswap_disk_addr[NUM_DISK];
	u32 comp_buf_addr[NUM_DISK];
	u32 comp_info_addr[NUM_DISK];
};

struct memory_comp mem_comp;
struct tasklet_struct tasklet;

int memory_decomp(unsigned char *decout_data, unsigned char *comp_data,
				unsigned int comp_len, unsigned char* Cout_data)
{
	register unsigned int Chdr_data;
	register unsigned int Dhdin;		// inner header of DAE
	unsigned char* start_comp_data;
	unsigned char* start_Cout_data;
	unsigned char* start_decout_data;

	start_comp_data = comp_data;
	start_Cout_data = Cout_data;
	start_decout_data = decout_data;

	do {
		Chdr_data = *comp_data;
		comp_data++;

		if(~Chdr_data & 0x01) {
			*((unsigned short*)Cout_data) = *((unsigned short*)comp_data);
			Cout_data += 2;
			comp_data += 2;
		} else {
			*((unsigned short*)Cout_data) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
			Cout_data+=2;

			if((*comp_data) >> 7) {
				*((unsigned short*)(Cout_data)) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
				Cout_data += 2;
			}
			comp_data++;
		}


		if(~Chdr_data & 0x02) {
			*((unsigned short*)Cout_data) = *((unsigned short*)comp_data);
			Cout_data+=2;
			comp_data+=2;
		} else {
			*((unsigned short*)Cout_data) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
			Cout_data += 2;

			if((*comp_data) >> 7) {
				*((unsigned short*)(Cout_data)) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
				Cout_data += 2;
			}
			comp_data++;
		}

		if(~Chdr_data & 0x04) {
			*((unsigned short*)Cout_data) = *((unsigned short*)comp_data);
			Cout_data += 2;
			comp_data += 2;
		} else {
			*((unsigned short*)Cout_data) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
			Cout_data += 2;
			if((*comp_data) >> 7) {
				*((unsigned short*)(Cout_data)) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
				Cout_data+=2;
			}
			comp_data++;
		}

		if(~Chdr_data & 0x08) {
			*((unsigned short*)Cout_data) = *((unsigned short*)comp_data);
			Cout_data += 2;
			comp_data += 2;
		} else {
			*((unsigned short*)Cout_data) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
			Cout_data += 2;

			if((*comp_data) >> 7) {
				*((unsigned short*)(Cout_data)) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
				Cout_data += 2;
			}
			comp_data++;
		}

		if(~Chdr_data & 0x10) {
			*((unsigned short*)Cout_data) = *((unsigned short*)comp_data);
			Cout_data += 2;
			comp_data += 2;
		} else {
			*((unsigned short*)Cout_data) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
			Cout_data += 2;

			if((*comp_data) >> 7) {
				*((unsigned short*)(Cout_data)) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
				Cout_data += 2;
			}
			comp_data++;
		}

		if(~Chdr_data & 0x20) {
			*((unsigned short*)Cout_data) = *((unsigned short*)comp_data);
			Cout_data += 2;
			comp_data += 2;
		} else {
			*((unsigned short*)Cout_data) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
			Cout_data += 2;

			if((*comp_data) >> 7) {
				*((unsigned short*)(Cout_data)) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
				Cout_data += 2;
			}
			comp_data++;
		}

		if(~Chdr_data & 0x40) {
			*((unsigned short*)Cout_data) = *((unsigned short*)comp_data);
			Cout_data += 2;
			comp_data += 2;
		} else {
			*((unsigned short*)Cout_data) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
			Cout_data += 2;

			if((*comp_data) >> 7) {
				*((unsigned short*)(Cout_data)) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
				Cout_data += 2;
			}
			comp_data++;
		}

		if(~Chdr_data & 0x80) {
			*((unsigned short*)Cout_data) = *((unsigned short*)comp_data);
			Cout_data += 2;
			comp_data += 2;
		} else {
			*((unsigned short*)Cout_data) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
			Cout_data += 2;

			if((*comp_data) >> 7) {
				*((unsigned short*)(Cout_data)) = *((unsigned short*)(Cout_data - ((*comp_data) & 0x7f)));
				Cout_data += 2;
			}
			comp_data++;
		}

	} while(comp_data - start_comp_data < comp_len);

	Cout_data = start_Cout_data;

	do {
		Dhdin = (*Cout_data);
		Cout_data += ((Dhdin & 0x01));
		*decout_data = ((Dhdin >> 0 & 0x01)) * (*Cout_data); 
		Cout_data += ((Dhdin >> 1 & 0x01));
		*(decout_data + 1) = ((Dhdin >> 1 & 0x01)) * (*Cout_data);
		Cout_data += ((Dhdin >> 2 & 0x01));
		*(decout_data + 2) = ((Dhdin >> 2 & 0x01)) * (*Cout_data); 
		Cout_data += ((Dhdin >> 3 & 0x01));
		*(decout_data + 3) = ((Dhdin >> 3 & 0x01)) * (*Cout_data); 
		Cout_data += ((Dhdin >> 4 & 0x01));
		*(decout_data + 4) = ((Dhdin >> 4 & 0x01)) * (*Cout_data); 
		Cout_data += ((Dhdin >> 5 & 0x01));
		*(decout_data + 5) = ((Dhdin >> 5 & 0x01)) * (*Cout_data); 
		Cout_data += ((Dhdin >> 6 & 0x01));
		*(decout_data + 6) = ((Dhdin >> 6 & 0x01)) * (*Cout_data); 
 		Cout_data += ((Dhdin >> 7 & 0x01));
		*(decout_data + 7) = ((Dhdin >> 7 & 0x01)) * (*Cout_data); 
		decout_data += 8;
		Cout_data += 1;
	} while(decout_data - start_decout_data < CHK_SIZE);

	Cout_data = start_Cout_data;
	decout_data = start_decout_data;

	return 1;
}
EXPORT_SYMBOL_GPL(memory_decomp);

/*
 * mem_comp_irq - irq clear and scheduling the sswap thread
 */
static irqreturn_t mem_comp_irq_handler(int irq, void *dev_id)
{
	int done_st = 0;

	/* check the incorrect access */
	done_st = readl(mem_comp.base + ISR) & (0x1);

	if (!done_st) {
		pr_debug("%s: interrupt does not happen\n",
			__func__);
		return -EINVAL;
	}

	/* clear interrupt */
	writel(ISR_CLEAR, mem_comp.base + ISR);
	/* scheduling the sswap thread */
	tasklet_hi_schedule(&tasklet);
	return IRQ_HANDLED;
}

/*
 * memory_comp_start_compress - let HW IP for compressing start
 * @disk_num: disk number of sswap disk.
 * @nr_pages: the number of pages which request compressing
 *
 * Write the address of sswap disk, compbuf, compinfo into SFR.
 * and let HW IP start by writing CMD register, or return EBUSY
 * if HW IP is busy compressing another disk.
 */
int memory_comp_start_compress(u32 disk_num, u32 nr_pages)
{
	u32 temp_32;
	/* check for device whether or not HW IP is compressing */
	if (readl(mem_comp.base + CMD) & 0x1) {
		pr_debug("%s: compressor HW is busy!\n", __func__);
		return -EBUSY;
	}
	/* if 0, compress the maximum size, 8MB */
	if (!nr_pages)
		nr_pages = SZ_8M >> PAGE_SHIFT;

	/* input align addr */
	temp_32 = mem_comp.sswap_disk_addr[disk_num];
	temp_32 = temp_32 >> 12;
	__raw_writel(temp_32, mem_comp.base + DISK_ADDR);

	temp_32 = mem_comp.comp_buf_addr[disk_num];
	temp_32 = temp_32 >> 12;
	__raw_writel(temp_32, mem_comp.base + COMPBUF_ADDR);

	temp_32 = mem_comp.comp_info_addr[disk_num];
	temp_32 = temp_32 >> 12;
	__raw_writel(temp_32, mem_comp.base + COMPINFO_ADDR);

	/* set cmd of start */
	writel((nr_pages << CMD_PAGES) | CMD_START, mem_comp.base + CMD);

	return 1;
}
EXPORT_SYMBOL_GPL(memory_comp_start_compress);

static void mcomp_dotask(unsigned long data)
{

}

static int memory_compressor_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;
	int irq, i;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_debug("Fail to get map register\n");
		return -1;
	}
	mem_comp.base = devm_request_and_ioremap(dev, res);
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		pr_debug("Fail to get IRQ resource \n");
		return -1;
	}
	mem_comp.irq = irq;

	/* interrupt register */
	ret = request_irq(mem_comp.irq, mem_comp_irq_handler, IRQF_DISABLED,
			driver_name, NULL);

	/* mem_comp struct reset */
	for (i = 0; i < NUM_DISK; i++) {
		mem_comp.comp_info_addr[i] = 0;
		mem_comp.comp_buf_addr[i] = 0;
		mem_comp.sswap_disk_addr[i] = 0;
	}

	tasklet_init(&tasklet, mcomp_dotask, (unsigned long)&mem_comp);

	/* cci enable */
	__raw_writel(0x1 >> CONTROL_AWUSER, mem_comp.base + CONTROL);
	__raw_writel(0x1 >> CONTROL_ARUSER, mem_comp.base + CONTROL);

	dev_info(&pdev->dev, "Loaded driver for Mcomp\n");

	return ret;
}

static int memory_compressor_remove(struct platform_device *pdev)
{
	/* unmapping */
	iounmap(mem_comp.base);

	/* remove an interrupt handler */
	free_irq(mem_comp.irq, NULL);

	return 0;
}

static const struct of_device_id mcomp_dt_match[] = {
	{
		.compatible = "samsung,exynos-mcomp",
	},
	{},
};

static struct platform_driver mem_comp_driver = {
	.probe		= memory_compressor_probe,
	.remove		= memory_compressor_remove,
	.driver		= {
		.name	= "exynos-mcomp",
		.owner	= THIS_MODULE,
		.of_match_table = mcomp_dt_match,
	}
};

static int __init memory_compressor_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mem_comp_driver);
	if (!ret)
		pr_info("%s: init\n",
			mem_comp_driver.driver.name);

	return ret;
}

static void __exit memory_compressor_exit(void)
{
	platform_driver_unregister(&mem_comp_driver);
}
late_initcall(memory_compressor_init);
module_exit(memory_compressor_exit);

MODULE_DESCRIPTION("memory_compressor");
MODULE_AUTHOR("Samsung");
MODULE_LICENSE("GPL");
