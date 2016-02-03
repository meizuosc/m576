/*
 * Platform Dependent file for Samsung Exynos
 *
 * Copyright (C) 1999-2014, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_custom_exynos.c 500926 2014-09-05 14:59:02Z $
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/if.h>
#include <plat/gpio-cfg.h>
#include <linux/skbuff.h>
#include <plat/devs.h>
#include <linux/random.h>
#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/wlan_plat.h>
#include <linux/regulator/machine.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <mach/regs-pmu.h>
#include <linux/interrupt.h>

#ifdef CONFIG_DHD_USE_STATIC_BUF
#define CONFIG_BROADCOM_WIFI_RESERVED_MEM
#endif

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define WLAN_STATIC_DHD_INFO_BUF	7
#define WLAN_STATIC_DHD_WLFC_BUF        8
#define WLAN_STATIC_DHD_IF_FLOW_LKUP    9
#define WLAN_STATIC_DHD_FLOWRING	10
#define WLAN_STATIC_DHD_MEMDUMP_BUF	11
#define WLAN_STATIC_DHD_MEMDUMP_RAM	12

#define WLAN_SCAN_BUF_SIZE		(64 * 1024)
#define WLAN_DHD_INFO_BUF_SIZE		(24 * 1024)//(16 * 1024)
#define WLAN_DHD_WLFC_BUF_SIZE      (32 * 1024)//(16 * 1024)
#define WLAN_DHD_IF_FLOW_LKUP_SIZE      (36 * 1024)//(20 * 1024)
#define WLAN_DHD_MEMDUMP_SIZE		(800 * 1024)

#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#ifdef CONFIG_BCMDHD_PCIE
#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	0
#define WLAN_SECTION_SIZE_2	0
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_1PAGE_RESERVED_BUF_NUM	4
#define DHD_SKB_1PAGE_BUF_NUM	((32) + (DHD_SKB_1PAGE_RESERVED_BUF_NUM))
#define DHD_SKB_2PAGE_BUF_NUM	0
#define DHD_SKB_4PAGE_BUF_NUM	0

#else

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_1PAGE_BUF_NUM	8
#define DHD_SKB_2PAGE_BUF_NUM	8
#define DHD_SKB_4PAGE_BUF_NUM	1
#endif /* CONFIG_BCMDHD_PCIE */

#define WLAN_SKB_1_2PAGE_BUF_NUM	((DHD_SKB_1PAGE_BUF_NUM) + \
	(DHD_SKB_2PAGE_BUF_NUM))
#define WLAN_SKB_BUF_NUM	((WLAN_SKB_1_2PAGE_BUF_NUM) + \
	(DHD_SKB_4PAGE_BUF_NUM))

#define PREALLOC_TX_FLOWS		40
#define PREALLOC_COMMON_MSGRINGS	2
#define WLAN_FLOWRING_NUM \
	((PREALLOC_TX_FLOWS) + (PREALLOC_COMMON_MSGRINGS))
#define WLAN_DHD_FLOWRING_SIZE	((PAGE_SIZE) * (7))
#ifdef CONFIG_BCMDHD_PCIE
static void *wlan_static_flowring[WLAN_FLOWRING_NUM];
#else
void *wlan_static_flowring = NULL;
#endif /* CONFIG_BCMDHD_PCIE */


static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

void *wlan_static_scan_buf0 = NULL;
void *wlan_static_scan_buf1 = NULL;
void *wlan_static_dhd_info_buf = NULL;
void *wlan_static_dhd_wlfc_buf = NULL;
void *wlan_static_if_flow_lkup = NULL;
void *wlan_static_dhd_memdump_buf = NULL;
void *wlan_static_dhd_memdump_ram = NULL;

static void *brcm_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;

	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;

	if (section == WLAN_STATIC_DHD_INFO_BUF) {
		if (size > WLAN_DHD_INFO_BUF_SIZE) {
			pr_err("request DHD_INFO size(%lu) is bigger than"
				" static size(%d).\n", size,
				WLAN_DHD_INFO_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}

	if (section == WLAN_STATIC_DHD_WLFC_BUF)  {
		if (size > WLAN_DHD_WLFC_BUF_SIZE) {
			pr_err("request DHD_WLFC size(%lu) is bigger than"
				" static size(%d).\n",
				size, WLAN_DHD_WLFC_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_wlfc_buf;
	}

	if (section == WLAN_STATIC_DHD_IF_FLOW_LKUP)  {
		if (size > WLAN_DHD_IF_FLOW_LKUP_SIZE) {
			pr_err("request DHD_WLFC size(%lu) is bigger than"
				" static size(%d).\n",
				size, WLAN_DHD_WLFC_BUF_SIZE);
			return NULL;
		}
		return wlan_static_if_flow_lkup;
	}

	if (section == WLAN_STATIC_DHD_FLOWRING)
		return wlan_static_flowring;

	if (section == WLAN_STATIC_DHD_MEMDUMP_BUF) {
		if (size > WLAN_DHD_MEMDUMP_SIZE) {
			pr_err("request DHD_MEMDUMP_BUF size(%lu) is bigger"
				" than static size(%d).\n",
				size, WLAN_DHD_MEMDUMP_SIZE);
			return NULL;
		}
		return wlan_static_dhd_memdump_buf;
	}

	if (section == WLAN_STATIC_DHD_MEMDUMP_RAM) {
		if (size > WLAN_DHD_MEMDUMP_SIZE) {
			pr_err("request DHD_MEMDUMP_RAM size(%lu) is bigger"
				" than static size(%d).\n",
				size, WLAN_DHD_MEMDUMP_SIZE);
			return NULL;
		}
		return wlan_static_dhd_memdump_ram;
	}

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int brcm_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < DHD_SKB_1PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

#if !defined(CONFIG_BCMDHD_PCIE)
	for (i = DHD_SKB_1PAGE_BUF_NUM; i < WLAN_SKB_1_2PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;
#endif /* !CONFIG_BCMDHD_PCIE */

	for (i = 0; i < PREALLOC_WLAN_SEC_NUM; i++) {
		if (wlan_mem_array[i].size > 0) {
			wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

			if (!wlan_mem_array[i].mem_ptr)
				goto err_mem_alloc;
		}
	}

	wlan_static_scan_buf0 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf0) {
		pr_err("Failed to alloc wlan_static_scan_buf0\n");
		goto err_mem_alloc;
	}

	wlan_static_scan_buf1 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf1) {
		pr_err("Failed to alloc wlan_static_scan_buf1\n");
		goto err_mem_alloc;
	}

	wlan_static_dhd_info_buf = kmalloc(WLAN_DHD_INFO_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_info_buf) {
		pr_err("Failed to alloc wlan_static_dhd_info_buf\n");
		goto err_mem_alloc;
	}

#ifdef CONFIG_BCMDHD_PCIE
	wlan_static_if_flow_lkup = kmalloc(WLAN_DHD_IF_FLOW_LKUP_SIZE,
		GFP_KERNEL);
	if (!wlan_static_if_flow_lkup) {
		pr_err("Failed to alloc wlan_static_if_flow_lkup\n");
		goto err_mem_alloc;
	}

	memset(wlan_static_flowring, 0, sizeof(wlan_static_flowring));
	for (j = 0; j < WLAN_FLOWRING_NUM; j++) {
		wlan_static_flowring[j] =
			kmalloc(WLAN_DHD_FLOWRING_SIZE, GFP_KERNEL | __GFP_ZERO);

		if (!wlan_static_flowring[j])
			goto err_mem_alloc;
	}
#else
	wlan_static_dhd_wlfc_buf = kmalloc(WLAN_DHD_WLFC_BUF_SIZE,
		GFP_KERNEL);
	if (!wlan_static_dhd_wlfc_buf) {
		pr_err("Failed to alloc wlan_static_dhd_wlfc_buf\n");
		goto err_mem_alloc;
	}
#endif /* CONFIG_BCMDHD_PCIE */

#ifdef CONFIG_BCMDHD_DEBUG_PAGEALLOC
	wlan_static_dhd_memdump_buf = kmalloc(WLAN_DHD_MEMDUMP_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_memdump_buf) {
		pr_err("Failed to alloc wlan_static_dhd_memdump_buf\n");
		goto err_mem_alloc;
	}

	wlan_static_dhd_memdump_ram = kmalloc(WLAN_DHD_MEMDUMP_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_memdump_ram) {
		pr_err("Failed to alloc wlan_static_dhd_memdump_ram\n");
		goto err_mem_alloc;
	}
#endif /* CONFIG_BCMDHD_DEBUG_PAGEALLOC */

	pr_err("%s: WIFI MEM Allocated\n", __FUNCTION__);
	return 0;

err_mem_alloc:
#ifdef CONFIG_BCMDHD_DEBUG_PAGEALLOC
	if (wlan_static_dhd_memdump_ram)
		kfree(wlan_static_dhd_memdump_ram);

	if (wlan_static_dhd_memdump_buf)
		kfree(wlan_static_dhd_memdump_buf);
#endif /* CONFIG_BCMDHD_DEBUG_PAGEALLOC */

#ifdef CONFIG_BCMDHD_PCIE
	for (j = 0; j < WLAN_FLOWRING_NUM; j++) {
		if (wlan_static_flowring[j])
			kfree(wlan_static_flowring[j]);
	}

	if (wlan_static_if_flow_lkup)
		kfree(wlan_static_if_flow_lkup);
#else
	if (wlan_static_dhd_wlfc_buf)
		kfree(wlan_static_dhd_wlfc_buf);
#endif /* CONFIG_BCMDHD_PCIE */
	if (wlan_static_dhd_info_buf)
		kfree(wlan_static_dhd_info_buf);

	if (wlan_static_scan_buf1)
		kfree(wlan_static_scan_buf1);

	if (wlan_static_scan_buf0)
		kfree(wlan_static_scan_buf0);

	pr_err("Failed to mem_alloc for WLAN\n");

	for (j = 0; j < i; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0; j < i; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */




static unsigned int power_on_gpio;  // GPG0(4)
//static unsigned int wifi_gpio;
static unsigned int irq_gpio; // GPA2(3)

static struct resource espresso_wifi_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE |
#ifdef CONFIG_BCMDHD_PCIE
				IORESOURCE_IRQ_HIGHEDGE,
#else
				IORESOURCE_IRQ_HIGHLEVEL,
#endif /* CONFIG_BCMDHD_PCIE */
	},
};

#ifdef CONFIG_BCMDHD_PCIE

#define WIFI_PCIE_CH_NUM 1
extern int exynos_pcie_poweron(int ch_num);
extern void exynos_pcie_poweroff(int ch_num);

static int espresso_wifi_power(int on)
{
	printk("[WLAN] Wifi Power %s.\n", on ? "On":"Off");

	if (!on) {
		gpio_set_value(power_on_gpio, on);
		exynos_pcie_poweroff(WIFI_PCIE_CH_NUM);
		msleep(100);
	}

	if(on) {
		gpio_set_value(power_on_gpio, on);
		exynos_pcie_poweron(WIFI_PCIE_CH_NUM);
		msleep(200);
	}

	return 0;
}

static int espresso_wifi_reset(int on)
{
	return 0;
}

#else  /* CONFIG_BCMDHD_SDIO */
//for sleeping issue bug fixing, Murphy 20150202
static int detect_flag = 0;

void set_detect_flag(void)
{
 	detect_flag = 1;
	pr_info("shaohua enter %s, set detect flag = %d \n", __func__, detect_flag);
}

void clear_detect_flag(void)
{
 	detect_flag = 0;
	pr_info("shaohua enter %s, clear detect flag = %d \n", __func__, detect_flag);
}
EXPORT_SYMBOL(clear_detect_flag);

int check_detect_flag(void)
{
	pr_info("shaohua enter %s, check detect flag = %d \n", __func__, detect_flag);
	return detect_flag;
}
EXPORT_SYMBOL(check_detect_flag);
//for sleeping issue bug fixing...
extern void (*wifi_status_cb)(struct platform_device *, int state);
extern struct platform_device *wifi_mmc_dev;

static int espresso_wifi_set_carddetect(int val)
{
	pr_info("%s: %d\n", __func__, val);

	if (wifi_status_cb){
		set_detect_flag(); // add flag to confirm the mmc rescan is called by  bcm wifi driver, Murphy 20150313
		wifi_status_cb(wifi_mmc_dev, val);
		}	
	else
		pr_warning("%s: Nobody to notify\n", __func__);

	return 0;
}

static int espresso_wifi_power(int on)
{

	printk("[WLAN] Wifi Power %s.\n", on ? "On":"Off");

	msleep(500);

	gpio_set_value(power_on_gpio, 0);
	if(on) {
		gpio_set_value(power_on_gpio, on);
		msleep(500);
	}

	return 0;
}

static int espresso_wifi_reset(int on)
{
	printk("%s: reset\n", __func__);
        gpio_set_value(power_on_gpio, 0);
        msleep(10);
        if(on) {
                gpio_set_value(power_on_gpio, on);
                msleep(200);
        }

	return 0;
}
#endif

#define WIFI_FACTORY_MAC_FILE "/amt/WIFI_nvram.txt"
#define WIFI_RANDOM_MAC_FILE "/data/wifimac"

static int espresso_wifi_get_mac_addr(unsigned char *buf)
{
	struct file *fp      = NULL;
	char macbuffer[18]   = {0};
	unsigned int fs_buffer[6]  = {0};
	int i = 0;
	mm_segment_t oldfs    = {0};
	char *mac_file       = "/data/calibration/mac_addr";
	int ret = 0;
	int no_mac = 0;

	pr_info("%s(), no_mac is %d\n", __func__, no_mac);
	fp = filp_open(mac_file, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_info("%s(), open %s err: %ld\n", __func__, mac_file, PTR_ERR(fp));
		if(no_mac) {
			get_random_bytes(buf, 6);
			buf[0] = 0x38;
			buf[1] = 0xBC;
			buf[2] = 0x1A;
		}

		pr_info("%s: write file %s\n", __func__, mac_file);

		snprintf(macbuffer, sizeof(macbuffer),"%02X:%02X:%02X:%02X:%02X:%02X\n",
				buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

		fp = filp_open(mac_file, O_RDWR | O_CREAT, 0644);
		if (IS_ERR(fp)) {
			pr_err("%s:create file %s error(%ld)\n", __func__, mac_file, PTR_ERR(fp));
		} else {
			oldfs = get_fs();
			set_fs(get_ds());

			if (fp->f_mode & FMODE_WRITE) {
				ret = fp->f_op->write(fp, (const char *)macbuffer,
						sizeof(macbuffer), &fp->f_pos);
				if (ret < 0)
					pr_err("%s:write file %s error(%d)\n", __func__, mac_file, ret);
			}
			set_fs(oldfs);
			filp_close(fp, NULL);
		}

	} else {
		pr_debug("%s(), open %s success\n",
			__func__, mac_file);
		if(no_mac) {
			ret = kernel_read(fp, 0, macbuffer, 18);
			if(ret <= 17) {
				pr_info("%s: read mac_info error, get random mac address\n", __func__);
				get_random_bytes(buf, 6);
			} else {
				macbuffer[17] = '\0';
				pr_debug("%s: read mac_info from file ok\n", __func__);
				sscanf(macbuffer, "%02X:%02X:%02X:%02X:%02X:%02X",
						(unsigned int *)&(fs_buffer[0]), (unsigned int *)&(fs_buffer[1]),
						(unsigned int *)&(fs_buffer[2]), (unsigned int *)&(fs_buffer[3]),
						(unsigned int *)&(fs_buffer[4]), (unsigned int *)&(fs_buffer[5]));
				for (i = 3; i < 6; i ++)
					buf[i] = (unsigned char)fs_buffer[i];
			}
			if (fp)
				filp_close(fp, NULL);

			buf[0] = 0x38;
			buf[1] = 0xBC;
			buf[2] = 0x1A;
		}
	}

	pr_info("mac address mac=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

	return 0;

}

/* Customized Locale table : OPTIONAL feature */
#define WLC_CNTRY_BUF_SZ	4
struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];
	char custom_locale[WLC_CNTRY_BUF_SZ];
	int  custom_locale_rev;
};
static struct cntry_locales_custom espresso_wifi_translate_custom_table[] = {
	/* Table should be filled out based on custom platform regulatory requirement */
	{"",	 "XY", 0}, /* Universal if Country code is unknown or empty */
	{"IR", "XY", 0}, /* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XY", 0}, /* Universal if Country code is SUDAN */
	{"SY", "XY", 0}, /* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"GL", "XY", 0}, /* Universal if Country code is GREENLAND */
	{"PS", "XY", 0}, /* Universal if Country code is PALESTINE */
	{"TL", "XY", 0}, /* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XY", 0}, /* Universal if Country code is MARSHALL ISLANDS */
	{"PK", "XY", 0}, /* Universal if Country code is PAKISTAN */
	{"CK", "XY", 0}, /* Universal if Country code is Cook Island (13.4.27)*/
	{"CU", "XY", 0}, /* Universal if Country code is Cuba (13.4.27)*/
	{"FK", "XY", 0}, /* Universal if Country code is Falkland Island (13.4.27)*/
	{"FO", "XY", 0}, /* Universal if Country code is Faroe Island (13.4.27)*/
	{"GI", "XY", 0}, /* Universal if Country code is Gibraltar (13.4.27)*/
	{"IM", "XY", 0}, /* Universal if Country code is Isle of Man (13.4.27)*/
	{"CI", "XY", 0}, /* Universal if Country code is Ivory Coast (13.4.27)*/
	{"JE", "XY", 0}, /* Universal if Country code is Jersey (13.4.27)*/
	{"KP", "XY", 0}, /* Universal if Country code is North Korea (13.4.27)*/
	{"FM", "XY", 0}, /* Universal if Country code is Micronesia (13.4.27)*/
	{"MM", "XY", 0}, /* Universal if Country code is Myanmar (13.4.27)*/
	{"NU", "XY", 0}, /* Universal if Country code is Niue (13.4.27)*/
	{"NF", "XY", 0}, /* Universal if Country code is Norfolk Island (13.4.27)*/
	{"PN", "XY", 0}, /* Universal if Country code is Pitcairn Islands (13.4.27)*/
	{"PM", "XY", 0}, /* Universal if Country code is Saint Pierre and Miquelon (13.4.27)*/
	{"SS", "XY", 0}, /* Universal if Country code is South_Sudan (13.4.27)*/
	{"AL", "AL", 2},
	{"DZ", "DZ", 1},
	{"AS", "AS", 12}, /* changed 2 -> 12*/
	{"AI", "AI", 1},
	{"AG", "AG", 2},
	{"AR", "AR", 21},
	{"AW", "AW", 2},
	{"AU", "AU", 6},
	{"AT", "AT", 4},
	{"AZ", "AZ", 2},
	{"BS", "BS", 2},
	{"BH", "BH", 4},	/* changed 24 -> 4*/
	{"BD", "BD", 2},
	{"BY", "BY", 3},
	{"BE", "BE", 4},
	{"BM", "BM", 12},
	{"BA", "BA", 2},
	{"BR", "BR", 4},
	{"VG", "VG", 2},
	{"BN", "BN", 4},
	{"BG", "BG", 4},
	{"KH", "KH", 2},
	{"CA", "CA", 31},
	{"KY", "KY", 3},
	{"CN", "CN", 38},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"HR", "HR", 4},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DK", "DK", 4},
	{"EE", "EE", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FR", "FR", 5},
	{"GF", "GF", 2},
	{"DE", "DE", 7},
	{"GR", "GR", 4},
	{"GD", "GD", 2},
	{"GP", "GP", 2},
	{"GU", "GU", 12},
	{"HK", "HK", 2},
	{"HU", "HU", 4},
	{"IS", "IS", 4},
	{"IN", "IN", 3},
	{"ID", "KR", 25}, /* ID/1 -> KR/24 */
	{"IE", "IE", 5},
	{"IL", "BO", 0},	/* IL/7 -> BO/0 */
	{"IT", "IT", 4},
	{"JP", "JP", 58},
	{"JO", "JO", 3},
	{"KW", "KW", 5},
	{"LA", "LA", 2},
	{"LV", "LV", 4},
	{"LB", "LB", 5},
	{"LS", "LS", 2},
	{"LI", "LI", 4},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"MO", "MO", 2},
	{"MK", "MK", 2},
	{"MW", "MW", 1},
	{"MY", "MY", 3},
	{"MV", "MV", 3},
	{"MT", "MT", 4},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MU", "MU", 2},
	{"YT", "YT", 2},
	{"MX", "MX", 20},
	{"MD", "MD", 2},
	{"MC", "MC", 1},
	{"ME", "ME", 2},
	{"MA", "MA", 2},
	{"NP", "NP", 3},
	{"NL", "NL", 4},
	{"AN", "AN", 2},
	{"NZ", "NZ", 4},
	{"NO", "NO", 4},
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PG", "PG", 2},
	{"PY", "PY", 2},
	{"PE", "PE", 20},
	{"PH", "PH", 5},
	{"PL", "PL", 4},
	{"PT", "PT", 4},
	{"PR", "PR", 20},
	{"RE", "RE", 2},
	{"RO", "RO", 4},
	{"SN", "SN", 2},
	{"RS", "RS", 2},
	{"SG", "SG", 4},
	{"SK", "SK", 4},
	{"SI", "SI", 4},
	{"ES", "ES", 4},
	{"LK", "LK", 1},
	{"SE", "SE", 4},
	{"CH", "CH", 4},
	{"TW", "TW", 1},
	{"TH", "TH", 5},
	{"TT", "TT", 3},
	{"TR", "TR", 7},
	{"AE", "AE", 6},
	{"UG", "UG", 2},
	{"GB", "GB", 6},
	{"UY", "UY", 1},
	{"VI", "VI", 13},
	{"VA", "VA", 12}, /* changed 2 -> 12 */
	{"VE", "VE", 3},
	{"VN", "VN", 4},
	{"MA", "MA", 1},
	{"ZM", "ZM", 2},
	{"EC", "EC", 21},
	{"SV", "SV", 19},
	{"KR", "KR", 57},
	{"RU", "RU", 13},
	{"UA", "UA", 8},
	{"GT", "GT", 1},
	{"MN", "MN", 1},
	{"NI", "NI", 2},
	{"US", "Q2", 57},
};

static void *espresso_wifi_get_country_code(char *ccode)
{
	int size = ARRAY_SIZE(espresso_wifi_translate_custom_table);
	int i;

	if (!ccode)
		return NULL;

	for (i = 0; i < size; i++)
		if (strcmp(ccode,
			espresso_wifi_translate_custom_table[i].iso_abbrev) == 0)
			return (void *)&espresso_wifi_translate_custom_table[i];
	return (void *)&espresso_wifi_translate_custom_table[0];
}
static struct wifi_platform_data espresso_wifi_control = {
	.set_power		= espresso_wifi_power,
	.set_reset		= espresso_wifi_reset,
#ifdef CONFIG_BCMDHD_SDIO
	.set_carddetect		= espresso_wifi_set_carddetect,
#endif
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc		= brcm_wlan_mem_prealloc,
#else
	.mem_prealloc		= NULL,
#endif
	.get_mac_addr		= espresso_wifi_get_mac_addr,
	.get_country_code	= espresso_wifi_get_country_code,
};
static struct platform_device espresso_wifi_device = {
	.name		= "bcmdhd_wlan",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(espresso_wifi_resources),
	.resource	= espresso_wifi_resources,
	.dev		= {
		.platform_data = &espresso_wifi_control,
	},
};
static struct platform_device *espresso_wlan_devices[] = {
	&espresso_wifi_device,
};

/****************************************************/
/******** driver espresso_wlan is to get gpio from dts ********/
/****************************************************/
static const struct of_device_id espresso_wlan_match[] = {
	{ .compatible = "broadcom,bcm43455_wifi" },
	{},
};
MODULE_DEVICE_TABLE(of, espresso_wlan_match);

static  void espresso_wlan_parse_dt(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;

	power_on_gpio = of_get_gpio(np, 1);
	irq_gpio = of_get_gpio(np, 0);

	
	ret = gpio_request(power_on_gpio, "wlan_power_on");
	if (ret < 0)
		dev_err(&pdev->dev, "%s: wlan_power_on gpio request failed", __func__);



	ret = gpio_direction_output(power_on_gpio, 0);

	if (ret < 0)
		dev_err(&pdev->dev, "%s: wlan_power_on gpio direction failed", __func__);


	/* Setup wlan IRQ */
	 ret = gpio_request(irq_gpio, "oob_irq");
	if (ret < 0)
		dev_err(&pdev->dev, "%s: oob_irq request failed", __func__);

	//s3c_gpio_setpull(irq_gpio, S3C_GPIO_PULL_NONE);
	//s3c_gpio_cfgpin(irq_gpio, 0xf);

	espresso_wifi_resources[0].start = gpio_to_irq(irq_gpio);
	espresso_wifi_resources[0].end = gpio_to_irq(irq_gpio);

}

static int espresso_wlan_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s\n", __func__);
	gpio_free(power_on_gpio);
	gpio_free(irq_gpio);

	return 0;
}

//   cat /sys/devices/wifi.43/wifi_test
int test_flag = 0;
static ssize_t wifi_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	test_flag = 1;
	return sprintf(buf, "%d\n", test_flag);
}

static DEVICE_ATTR(wifi_test, S_IRUGO,wifi_test_show, NULL);

static int espresso_wlan_probe(struct platform_device *pdev)
{
	int ret =0;

	dev_info(&pdev->dev, "%s: name %s\n", __func__, pdev->name);
	
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	brcm_init_wlan_mem();
#endif
	espresso_wlan_parse_dt(pdev);
	ret = platform_add_devices(espresso_wlan_devices,
			ARRAY_SIZE(espresso_wlan_devices));
	if (device_create_file(&pdev->dev, &dev_attr_wifi_test))
		pr_info("[WIFI] bcm43455 wif_test sys file create failed\n");
	return ret;
}

static struct platform_driver espresso_wlan_driver = {
	.probe		= espresso_wlan_probe,
	.remove		= espresso_wlan_remove,
	.driver		= {
		.name		= "espresso_wlan",
		.owner = THIS_MODULE,
		.of_match_table	= espresso_wlan_match,
	},
};

module_platform_driver(espresso_wlan_driver);

MODULE_DESCRIPTION("Espresso wlan driver");
MODULE_LICENSE("GPL");
