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
#include <mach/memory_compressor.h>
#include <linux/delay.h>

/* file rw */
#include <linux/file.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

#define WITH_PLAT		0
#define NUM_INTERRUPT		289 
#define SFR_BASE_ADDR		0x11170000
#define NUM_DISK		4

#define CHK_SIZE		4096
#define PATTERN_SRC		0x80
#define PATTERN_COUNT_MASK	0x2f

//test 
#define test_size		SZ_2M 

u32 **test_swap_disk;
u32 **test_comp_buf;
u32 **test_comp_info;
dma_addr_t* bus_addr;
u8 *fw_buf;
bool go;

#if 1
struct timespec before_ts;
struct timespec after_ts;
unsigned long timeval;

static long update_timeval(struct timespec lhs, struct timespec rhs)
{
	long val;
	struct timespec ts;

	ts = timespec_sub(rhs, lhs);
	val = timespec_to_ns(&ts);

	return val;
}
#endif

static const char driver_name[] = "mem_comp";

/* irq, SFR base, address information and tasklet */
struct memory_comp {
	unsigned int	irq;
	void __iomem	*base;
	u32 *sswap_disk_addr[NUM_DISK];
	u32 *comp_buf_addr[NUM_DISK];
	u32 *comp_info_addr[NUM_DISK];

	struct tasklet_struct *tasklet;
};

struct memory_comp mem_comp;

static void file_read(void)
{
	unsigned int ret = 0, i;
//	unsigned int file_index =0, file_count = 0;
	long fsize, nread;
	struct file *fp;
	mm_segment_t old_fs;
	char *fname = kzalloc(sizeof(char) * 25, GFP_KERNEL);

	pr_info("%s > \n", __func__);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

#if 1 
	for(i = 0; i < 512; i++) {
		sprintf(fname, "/data/dump/%d", i);
		fp = filp_open(fname, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			printk("open error\n");
			fp = NULL;
			return;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;

		fw_buf = vmalloc(fsize);

		if (!fw_buf) {
			pr_err("failed to allocate memory\n");
			ret = -ENOMEM;
		}

		nread = vfs_read(fp, fw_buf, fsize, &fp->f_pos);
		memcpy(mem_comp.sswap_disk_addr[0] + (SZ_4K * i), fw_buf, fsize);

		if (nread != fsize) {
			pr_err("failed to read firmware file, %ld Bytes\n", nread);
			ret = -EIO;
		}
		filp_close(fp, current->files);
		fp = NULL;
		vfree(fw_buf);

	}
#endif
#if 0
	start_fw_buf = t_src;

	do {
		pr_info(" %x", *t_src);
		t_src++;
	} while(t_src - start_fw_buf < nread - 1);

	if (fw_buf) {
		vfree(fw_buf);
		fw_buf = NULL;
	}

	if (fp) {
		filp_close(fp, current->files);
		fp = NULL;
	}

#endif
	set_fs(old_fs);

	return; 
}

int memory_decomp(unsigned char *decout_data, unsigned char *comp_data,
				unsigned int comp_len, unsigned char* Cout_data)
{
	register unsigned int Chdr_data;
	register unsigned int Dhdin;		// inner header of DAE
	unsigned char* start_comp_data; 
	unsigned char* start_Cout_data; 
	unsigned char* start_decout_data; 

	pr_info(" %s > \n", __func__);
	start_comp_data = comp_data;
	pr_info("comp_data %p 1> \n", start_comp_data);
	start_Cout_data = Cout_data; 
	pr_info("ccout %p 2> \n", start_Cout_data);
	start_decout_data = decout_data;
	pr_info("decout %p 3> \n", start_decout_data);

	pr_info("comp_data %x \n", (unsigned int)*comp_data);

	do {
		Chdr_data = *comp_data;
		comp_data++;
	pr_info("comp_data %p \n", comp_data);
	pr_info("comp_data %x \n", *comp_data);

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

	pr_info("decout result \n");
	do {
		pr_info("%x", *decout_data);
		decout_data += 1;
	} while(decout_data - start_decout_data < CHK_SIZE);
	return 1; 
}
EXPORT_SYMBOL_GPL(memory_decomp);

/*
 * mem_comp_irq - irq clear and scheduling the sswap thread
 */
static irqreturn_t mem_comp_irq_handler(int irq, void *dev_id)
{
	int done_st = 0;
	unsigned long time = 0;
	unsigned int test_s = test_size;

//	pr_info("%s >\n", __func__);

	/* check the incorrect access */
	done_st = readl(mem_comp.base + ISR) & (0x1);

	if (!done_st) {
		pr_debug("%s: interrupt does not happen\n",
			__func__);
		return -EINVAL;
	}
	getnstimeofday(&after_ts);

	time = update_timeval(before_ts, after_ts);

	pr_info("%10u byte \t time: %10lu \t MB/s: %10lu\n",
				test_s, time , (test_s * 954) / time);
	
	/* clear interrupt */
	writel(ISR_CLEAR, mem_comp.base + ISR);
	/* scheduling the sswap thread */
//	tasklet_hi_schedule(mem_comp.tasklet);
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
	u32 temp;
	pr_info("%s > \n", __func__);
	/* check for device whether or not HW IP is compressing */
	if (readl(mem_comp.base + CMD) & 0x1) {
		pr_debug("%s: compressor HW is busy!\n", __func__);
		return -EBUSY;
	}
	/* if 0, compress the maximum size, 8MB */
	if (!nr_pages)
		nr_pages = SZ_8M >> PAGE_SHIFT;

	/* input align addr */
	temp = *mem_comp.sswap_disk_addr[disk_num];
	temp = temp >> 12;
	pr_info(" %x \n", temp);
	__raw_writel(temp, mem_comp.base + DISK_ADDR);

	temp = *mem_comp.comp_buf_addr[disk_num];
	temp = temp >> 12;
	pr_info(" %x \n", temp);
	__raw_writel(temp, mem_comp.base + COMPBUF_ADDR);

	temp = *mem_comp.comp_info_addr[disk_num];
	temp = temp >> 12;
	pr_info(" %x \n", temp);
	__raw_writel(temp, mem_comp.base + COMPINFO_ADDR);

	/* set cmd of start */
	getnstimeofday(&before_ts);
	writel((nr_pages << CMD_PAGES) | CMD_START, mem_comp.base + CMD);
	return 1;
}
EXPORT_SYMBOL_GPL(memory_comp_start_compress);


/*
 * memory_comp_init - initialize the memory_comp struct
 * @nr_disk: the number of the sswap disks
 * @sswap_disk: Pointer array of sswap disk address
 * @comp_info: Pointer array comp info address
 * @comp_buf: Pointer array of comp buffer address
 * @tasklet: Pointer of sswap thread's tasklet
 */
void memory_comp_init(u32 nr_disk, u32 **sswap_disk, u32 **comp_buf,
				u32 **comp_info)//, struct tasklet_struct *tasklet)
{
	int i = 0;

	/* TODO - Need to Check: clk, power up */

	/* setting of sswap addr, comp_buf addr, comp_info_addr */
	for (i = 0; i < nr_disk; i++) {
		mem_comp.sswap_disk_addr[i] = sswap_disk[i];
		mem_comp.comp_buf_addr[i] = comp_buf[i];
		mem_comp.comp_info_addr[i] = comp_info[i];
	}

	/* interrupt enable */
	writel(IER_ENABLE, mem_comp.base + IER);

	/* let hardware enable */
	writel((0xff << CONTROL_THRESHOLD) | (1 << CONTROL_DRCG_DISABLE), mem_comp.base + CONTROL);
	/* end */

	/* setting of tasklet */
//	mem_comp.tasklet = tasklet;

}
EXPORT_SYMBOL_GPL(memory_comp_init);

/* unit test function */
static void mcomp_test_src_init(void)
{

}
#if 0
static void mcomp_dotask(unsigned long data)
{
	
//	struct memory_comp *mem_comp = (struct memory_comp *)data; 
	pr_info("%s] \n", __func__);
//	pr_info("%d \n", mem_comp.comp_buf_addr[0]);
}
#endif
static int unitest_init(void)
{
	u32 nr_disk = 1;
	u32 disk_num = 0;
	dma_addr_t *disk, *comp, *info;
	u32 test_s = test_size;

	pr_info("%s > \n", __func__);

	if(test_size > SZ_4M)
		test_s = SZ_4M;

	test_swap_disk = kzalloc(sizeof(u32 *) * 1, GFP_KERNEL);
	test_comp_buf = kzalloc(sizeof(u32 *) * 1, GFP_KERNEL);
	test_comp_info = kzalloc(sizeof(u32 *) * 1, GFP_KERNEL);

	disk = kzalloc(sizeof(dma_addr_t), GFP_KERNEL);
	comp = kzalloc(sizeof(dma_addr_t), GFP_KERNEL);
	info = kzalloc(sizeof(dma_addr_t), GFP_KERNEL);


	test_swap_disk[disk_num] = dma_alloc_coherent(NULL, test_size, disk, GFP_KERNEL);
	pr_info(" %p\n", test_swap_disk[disk_num]);
//	test_swap_disk[disk_num] = (u32 *)0xf9d00000;//	dma_alloc_coherent(NULL, test_size, disk, GFP_KERNEL);
//	test_swap_disk[disk_num] = dma_alloc_coherent(NULL, test_s, disk, GFP_KERNEL);
//	test_swap_disk[disk_num] = dma_alloc_writecombine(NULL, test_s, disk, GFP_KERNEL);
	test_comp_buf[disk_num] = dma_alloc_coherent(NULL, test_s, comp, GFP_KERNEL);
	test_comp_info[disk_num] = dma_alloc_coherent(NULL, test_s, info, GFP_KERNEL);
	

	memory_comp_init(nr_disk, test_swap_disk, test_comp_buf,
					test_comp_info);//, mem_comp.tasklet);

#if 1 
	for(disk_num = 0; disk_num < nr_disk; disk_num++) {
		memset(mem_comp.sswap_disk_addr[disk_num], 0x80, test_size);
		pr_info("disk_num: %d addr: %lx --- value: %x \n", disk_num,
					(unsigned long)mem_comp.sswap_disk_addr[disk_num],
							*mem_comp.sswap_disk_addr[disk_num]);
		pr_info("alloc_addr comp_buf: %p \t comp_info: %p \n",
					mem_comp.comp_buf_addr[disk_num],
						mem_comp.comp_info_addr[disk_num]);

	}
#endif
	memory_comp_start_compress(disk_num, test_size / SZ_4K);
	pr_info("%lu\n", (unsigned long)mem_comp.sswap_disk_addr[disk_num]);
#if 0
	/* src print */
	buff = mem_comp.sswap_disk_addr[0];
	do {
		pr_info("addr: %x - value: %x\n",(unsigned int)buff, (unsigned int)*buff);
		buff++;
	} while ((buff - mem_comp.sswap_disk_addr[0]) < SZ_32K);
#endif	
	mcomp_test_src_init();
	return 0;
}

static int debug_mcomp_init_get(void *data, u64 *val)
{
	pr_info("%s] \n", __func__);
	unitest_init(); 
	
	*val = 1;
	return 0;
}

static int debug_mcomp_unitest_get(void *data, u64 *val)
{
	*val = go;

	return 0;
}


static int debug_mcomp_unitest_set(void *data, u64 val)
{
	u32 disk_num = 0;

	pr_info("%s > \n", __func__);
	memset(mem_comp.sswap_disk_addr[disk_num], 0x0, test_size);
	file_read();

	if (val) {
		/* comp_buffer, info allocation */
		pr_info("test_size %d\n", test_size);
		memory_comp_start_compress(disk_num, test_size / SZ_4K);

		pr_info("%lu\n", (unsigned long)mem_comp.sswap_disk_addr[disk_num]);

		pr_info("compressed done\n");

		/* decompress data */
#if 0 
		memory_decomp(decout_data, 
			(unsigned char *)mem_comp.comp_buf_addr[0], 1024, decomp_data);
#endif
	}

	test_swap_disk[disk_num] = (u32 *)0xf9d00000;
	return 0;
}

static int debug_mcomp_performance_get(void *data, u64 *val)
{
	pr_info("%s] \n", __func__);
	*val = go;

	return 0;
}


static int debug_mcomp_performance_set(void *data, u64 val)
{
	return 0;
}


DEFINE_SIMPLE_ATTRIBUTE(debug_mcomp_unitest_fops, debug_mcomp_unitest_get, debug_mcomp_unitest_set, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(debug_mcomp_init_fops, debug_mcomp_init_get, NULL, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(debug_mcomp_performance_fops, debug_mcomp_performance_get, debug_mcomp_performance_set, "%llx\n");

static int memory_compressor_probe(struct platform_device *pdev)
{
	int i = 0;
	int ret = 0;
	struct dentry *d;

	mem_comp.irq = NUM_INTERRUPT + 32;
	mem_comp.base = ioremap_nocache(SFR_BASE_ADDR, 256);
	pr_info(" %s > base addr: %lx\n", __func__, (unsigned long)mem_comp.base);
	/* interrupt register */
	ret = request_irq(mem_comp.irq, mem_comp_irq_handler, IRQF_DISABLED,
			driver_name, NULL);
	if (ret < 0)
		pr_debug("cannot claim IRQ\n");
	else
		pr_debug("%s: ok..\n", __func__);

	/* mem_comp struct reset */
	for (i = 0; i < NUM_DISK; i++) {
		mem_comp.comp_info_addr[i] = NULL;
		mem_comp.comp_buf_addr[i] = NULL;
		mem_comp.sswap_disk_addr[i] = NULL;
	}

//	tasklet_init(mem_comp.tasklet, mcomp_dotask), (unsigned int) mem_comp); 
	/* debugfs init */
	d = debugfs_create_dir("exynos_mcomp", NULL);

	debugfs_create_file("init", 0644, d, NULL, &debug_mcomp_init_fops);
	debugfs_create_file("start", 0644, d, NULL, &debug_mcomp_unitest_fops);
	debugfs_create_file("performance", 0644, d, NULL, &debug_mcomp_performance_fops);

	dev_info(&pdev->dev, "Loaded driver for Mcomp\n");
	unitest_init();

	return ret;
}

static int memory_compressor_remove(struct platform_device *pdev)
{
	/* unmapping */
	iounmap(mem_comp.base);
	/* remove an interrupt handler*/
	free_irq(mem_comp.irq, NULL);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mcomp_dt_match[] = {
	{ .compatible = "samsung,exynos-mcomp",},
};
MODULE_DEVICE_TABLE(of, exynos_cpufreq_match);
#endif

static struct platform_driver mem_comp_driver = {
	.probe		= memory_compressor_probe,
	.remove		= memory_compressor_remove,
	.driver		= {
		.name	= driver_name,
		.owner	= THIS_MODULE,
		.of_match_table = mcomp_dt_match,
	},
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
