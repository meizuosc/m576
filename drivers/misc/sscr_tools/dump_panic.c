#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/exynos_ion.h>
#include <linux/bootmem.h>
#include <linux/ioport.h>
#include <linux/slab.h>

#define PANIC_DEBUG
#ifdef PANIC_DEBUG
#define print_dbg(fmt, args...) printk("panic debug: " fmt, ##args)
#define print_err(fmt, args...) printk("panic err: " fmt, ##args)
#else
#define print_dbg(fmt, args...)
#define print_err(fmt, args...) printk("panic err: " fmt, ##args)
#endif

#define CURRUNT_LOG_MAGIC 0x474f4c43     /* "CLOG" */
#define LAST_LOG_MAGIC        0x474f4c4c     /* "LLOG" */
#define LOGBUF_FULL               0x01
#define LOGBUF_NOTFULL         0x00


struct sec_log_info{
	unsigned log_magic;
	unsigned log_ptr;
	unsigned for_alignread;   //in 64bit system the address read from io memory should aligned to 8
	char buff_full;
};

volatile unsigned int *sec_log_ptr = NULL;
volatile char *sec_log_buf = NULL;
static volatile unsigned cur_log;
static unsigned sec_log_size;
static phys_addr_t glb_log_base = 0;
static size_t glb_log_buf_size = 0;
static char *seclog_buffstatus_curr = NULL;
static char *seclog_buffstatus_last = NULL;
struct resource res;

extern void (*dump_to_memory)(char c);

static const struct of_device_id exynos5_sec_log_match[] = {
	{ .compatible = "samsung,sec_log_msg" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos5_sec_log_match);

static inline void emit_sec_log_char(char c)
{
	if(likely(sec_log_buf && sec_log_ptr)) {
		*(volatile char *)&(sec_log_buf[(*sec_log_ptr)++]) = c;
		if(unlikely((*sec_log_ptr) > sec_log_size -1)) {
			*sec_log_ptr = 0;
                    if(seclog_buffstatus_curr)
                        *seclog_buffstatus_curr = (char)LOGBUF_FULL;
             }
	}
}


#include <linux/memblock.h>
void *__iomem base_map;
extern phys_addr_t log_panic_base;

static void __iomem *seclog_vmap(phys_addr_t phys_addr, size_t size)
{
	int i;
	struct page **pages;
	unsigned int num_pages = (unsigned int)(size >> PAGE_SHIFT);
	void *pv;

	pages = kmalloc(num_pages * sizeof(*pages), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(phys_addr);
		phys_addr += PAGE_SIZE;
	}

	pv = vmap(pages, num_pages, VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	kfree(pages);

	return (void __iomem *)pv;
}

static int start_dump_message(phys_addr_t base, size_t size)
{
	int i;
	static int cur_log_init = 0;
	struct sec_log_info *p_sec_log = NULL;
#if 0
	// NOTE: If you have no-map attribute, then you do not need memblock_remove()
	//rm memory from useable memory region before ioremap
	int err = memblock_remove(base, (phys_addr_t)(size));
	if (err) {
		print_err("failed to remove CONFIG_DEFAULT_MEMORY_RESERVE_START_ADDR\n");
		return 1;
	}
#endif
#if 0
	//res = alloc_bootmem_low(sizeof(*res));
	res.name  = "sec_log";
	res.start = glb_log_base;
	res.end = glb_log_base + glb_log_buf_size - 1;
	res.flags = IORESOURCE_MEM;

	insert_resource(&iomem_resource, &res);
#endif
	base_map = seclog_vmap(base, size);
	if(NULL == base_map) {
		print_err("ioremap failed addr is:%llx, size is:%lx\n", (long long unsigned int)base, size);
		return 1;
	}
	for(i = 0; i < 2; i++) {
		//          p_sec_log = (struct sec_log_info *)(phys_to_virt(base) + i*(size/2));
		p_sec_log = (struct sec_log_info *)(base_map + i*(size/2));
		if( p_sec_log->log_magic == CURRUNT_LOG_MAGIC) {
			p_sec_log->log_magic = LAST_LOG_MAGIC;
			seclog_buffstatus_last = &p_sec_log->buff_full;
		} else {
			if(cur_log_init == 0)
			{
				p_sec_log->log_magic = CURRUNT_LOG_MAGIC;
				p_sec_log->log_ptr = 0;
				sec_log_ptr = &p_sec_log->log_ptr;
				// sec_log_buf = phys_to_virt(base) + i*(size/2) + sizeof(struct sec_log_info);
				sec_log_buf = base_map + i*(size/2) + sizeof(struct sec_log_info);
				sec_log_size = size/2 - sizeof(struct sec_log_info);
				cur_log_init = 1;
				cur_log = i;
				seclog_buffstatus_curr = &p_sec_log->buff_full;
				*seclog_buffstatus_curr = (char)LOGBUF_NOTFULL;
			}
			else
			{
				p_sec_log->log_magic  = 0;
			}
		}

	}

	printk("base_map:%p, log_buf:%p, struct size:%#lx, log_size:%#lx\n", base_map, sec_log_buf, sizeof(struct sec_log_info), (size_t)sec_log_size);

	memset((void*)sec_log_buf, 0, (size_t)(sec_log_size));

	print_dbg("%s cur_log %d, sec_log_buf: %p, sec_log_size: 0x%x\n", __func__, cur_log, sec_log_buf, sec_log_size);

	//change the emit function pointer, begin to dump
	dump_to_memory = emit_sec_log_char;

	return 0;
}

/*misc devices for read operation*/
int panic_msg_open(struct inode *inode,struct file *filp)
{
	return 0;
}

static unsigned long err_log_p = 0;

ssize_t panic_msg_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	struct sec_log_info *p_sec_log = NULL;
	int last_log_index;
	unsigned int content_size = 0;
	void* last_log_base_addr;
	char *content_buf = NULL;
	int err;

	last_log_index = cur_log == 1 ? 0 : 1;
	last_log_base_addr = base_map + last_log_index*(glb_log_buf_size/2);
	p_sec_log = (struct sec_log_info*)(last_log_base_addr);

	//	last_log_base_addr = glb_log_base + last_log_index*(glb_log_buf_size/2);
	//	p_sec_log = (struct sec_log_info*)(phys_to_virt(last_log_base_addr));

	if(p_sec_log->log_magic != LAST_LOG_MAGIC) {
		if (err_log_p!=(unsigned long)p_sec_log) {
			err_log_p = (unsigned long)p_sec_log;
			print_err("***************************************************************\n");
			print_err("error happened or it is the first time to booting, maigic is %x\n", p_sec_log->log_magic);
			print_err("***************************************************************\n");
		}
		return 0;
	}

	content_buf = (char *)(last_log_base_addr + sizeof(struct sec_log_info));
	//	content_buf = (char *)(phys_to_virt(last_log_base_addr + sizeof(struct sec_log_info)));
	content_size = glb_log_buf_size/2 - sizeof(struct sec_log_info);
	if(p_sec_log->log_ptr == 0) {
		err = copy_to_user(buff, content_buf, content_size);
		if(err) {
			print_err("copy to user failed\n");
		}
	} else {
		if((seclog_buffstatus_last) &&
				(*seclog_buffstatus_last == LOGBUF_NOTFULL)) {
			err = copy_to_user(buff, content_buf, p_sec_log->log_ptr);
			if(err) {
				print_err("copy to user failed\n");
			}
			content_size = (unsigned int)p_sec_log->log_ptr;
		}
		else {
                    int align_offset = 0;
                    int aligned_length = 0;
                    int align_size = sizeof(char *);
                    align_offset = (unsigned long)(&content_buf[p_sec_log->log_ptr]) % align_size;
                    if(!align_offset) {
        			err = copy_to_user(buff, &content_buf[p_sec_log->log_ptr], (content_size - p_sec_log->log_ptr));
        			if(err) {
        				print_err("copy to user failed\n");
        			}

        			err = copy_to_user(&buff[content_size - p_sec_log->log_ptr], content_buf, p_sec_log->log_ptr);
        			if(err) {
        				print_err("copy to user failed\n");
        			}
        		}
                    else {
                        char *buf = NULL;
                        aligned_length = align_size - align_offset;
                        buf = kmalloc(align_size, GFP_KERNEL);
                        if(NULL == buf) {
                            printk("kmalloc failed\n");
                            return -1;
                        }
                        else {
                                memcpy(buf, &content_buf[p_sec_log->log_ptr - align_offset], align_size);
                                err = copy_to_user(buff, buf + align_offset, aligned_length);
                                if(err) {
                                    print_err("copy to user failed\n");
                                }
                                kfree(buf);
                                err = copy_to_user(buff + aligned_length, &content_buf[p_sec_log->log_ptr + aligned_length], (content_size - p_sec_log->log_ptr - aligned_length));
                                if(err) {
                                    print_err("copy to user failed\n");
                                }
                                err = copy_to_user(&buff[content_size - p_sec_log->log_ptr], content_buf, p_sec_log->log_ptr);
                                if(err) {
                                    print_err("copy to user failed\n");
                                }
                        }
                    }
            }
        }
	return content_size;
}

int panic_msg_release(struct inode *inode,struct file *filp)
{
	return 0;
}

static struct file_operations panic_msg_ops = {
	.owner 	= THIS_MODULE,
	.open 	= panic_msg_open,
	.release= panic_msg_release,	
	.read	= panic_msg_read,
};

static struct miscdevice panic_msg_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.fops	= &panic_msg_ops,
	.name	= "panic_msg",
};

static int panic_msg_probe(struct platform_device *pdev)
{
	//	struct resource *res;
	phys_addr_t base = 0;
	size_t size = 0;
	int ret;

	if (ion_exynos_contig_heap_info(ION_EXYNOS_ID_SECLOG,&base, &size))
		return -EINVAL;
	glb_log_base = base;
	glb_log_buf_size = size;

	print_dbg("glb_log_base %#llx, size %#lx\n", (long long unsigned int)base, size);

	ret = misc_register(&panic_msg_dev);
	if(ret<0)
	{
		print_err("register misc device failed!\n");
		goto exit;
	}
	ret = start_dump_message(glb_log_base, glb_log_buf_size);
	if(ret)
	{
                print_err("register misc device failed!!\n");
                goto exit;
        }
	return 0;

exit:
	misc_deregister(&panic_msg_dev);
	return ret;	
}

static int panic_msg_remove (struct platform_device *pdev)
{
	misc_deregister(&panic_msg_dev);	
	return 0;
}

static struct platform_driver panic_msg_driver = {
	.probe = panic_msg_probe,
	.remove = panic_msg_remove,
	.driver = {
		.name = "sec_log_msg",
		.owner = THIS_MODULE,
		.of_match_table = exynos5_sec_log_match,
	},
};

static int __init panic_msg_init(void)
{
	return platform_driver_register(&panic_msg_driver);
}
static void __exit panic_msg_exit(void)
{
	platform_driver_unregister(&panic_msg_driver);
}


module_init(panic_msg_init);
module_exit(panic_msg_exit);

MODULE_LICENSE("Dual BSD/GPL");
