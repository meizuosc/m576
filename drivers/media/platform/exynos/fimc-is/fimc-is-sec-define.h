#ifndef FIMC_IS_SEC_DEFINE_H
#define FIMC_IS_SEC_DEFINE_H

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"

#include "fimc-is-device-sensor.h"
#include "fimc-is-device-ischain.h"
#include "crc32.h"
#include "fimc-is-companion.h"

#define FW_CORE_VER		0
#define FW_PIXEL_SIZE		1
#define FW_ISP_COMPANY		3
#define FW_SENSOR_MAKER		4
#define FW_PUB_YEAR		5
#define FW_PUB_MON		6
#define FW_PUB_NUM		7
#define FW_MODULE_COMPANY	9
#define FW_VERSION_INFO		10

#define FW_ISP_COMPANY_BROADCOMM	'B'
#define FW_ISP_COMPANY_TN		'C'
#define FW_ISP_COMPANY_FUJITSU		'F'
#define FW_ISP_COMPANY_INTEL		'I'
#define FW_ISP_COMPANY_LSI		'L'
#define FW_ISP_COMPANY_MARVELL		'M'
#define FW_ISP_COMPANY_QUALCOMM		'Q'
#define FW_ISP_COMPANY_RENESAS		'R'
#define FW_ISP_COMPANY_STE		'S'
#define FW_ISP_COMPANY_TI		'T'
#define FW_ISP_COMPANY_DMC		'D'

#define FW_SENSOR_MAKER_SF		'F'
#define FW_SENSOR_MAKER_SLSI		'L'
#define FW_SENSOR_MAKER_SONY		'S'

#define FW_MODULE_COMPANY_SEMCO		'S'
#define FW_MODULE_COMPANY_GUMI		'O'
#define FW_MODULE_COMPANY_CAMSYS	'C'
#define FW_MODULE_COMPANY_PATRON	'P'
#define FW_MODULE_COMPANY_MCNEX		'M'
#define FW_MODULE_COMPANY_LITEON	'L'
#define FW_MODULE_COMPANY_VIETNAM	'V'
#define FW_MODULE_COMPANY_SHARP		'J'
#define FW_MODULE_COMPANY_NAMUGA	'N'
#define FW_MODULE_COMPANY_POWER_LOGIX	'A'
#define FW_MODULE_COMPANY_DI		'D'

#define FW_2P2_F		"F16LL"
#define FW_2P2_I		"I16LL"
#define FW_3L2		"C13LL"
#define FW_IMX135	"C13LS"
#define FW_IMX134	"D08LS"
#define FW_IMX240	"H16LS"
#define FW_IMX240_Q	"H16US"
#define FW_IMX240_Q_C1	"H16UL"
#define FW_2P2_12M	"G16LL"
#define FW_4H5		"F08LL"

#define SDCARD_FW
#define FIMC_IS_SETFILE_SDCARD_PATH		"/data/media/0/"
#define FIMC_IS_FW				"fimc_is_fw2.bin"
#define FIMC_IS_FW_2P2				"fimc_is_fw2_2p2.bin"
#define FIMC_IS_FW_2P2_12M				"fimc_is_fw2_2p2_12m.bin"
#define FIMC_IS_FW_3L2				"fimc_is_fw2_3l2.bin"
#define FIMC_IS_FW_4H5				"fimc_is_fw2_4h5.bin"
#define FIMC_IS_FW_IMX134			"fimc_is_fw2_imx134.bin"
#define FIMC_IS_FW_IMX240		"fimc_is_fw2_imx240.bin"
#define FIMC_IS_FW_COMPANION_EVT0				"companion_fw_evt0.bin"
#define FIMC_IS_FW_COMPANION_EVT1				"companion_fw_evt1.bin"
#define FIMC_IS_FW_COMPANION_2P2_EVT1				"companion_fw_2p2_evt1.bin"
#define FIMC_IS_FW_COMPANION_2P2_12M_EVT1				"companion_fw_2p2_12m_evt1.bin"
#define FIMC_IS_FW_COMPANION_IMX240_EVT1				"companion_fw_imx240_evt1.bin"
#define FIMC_IS_FW_SDCARD			"/data/media/0/fimc_is_fw2.bin"
#define FIMC_IS_IMX240_SETF			"setfile_imx240.bin"
#define FIMC_IS_IMX135_SETF			"setfile_imx135.bin"
#define FIMC_IS_IMX134_SETF			"setfile_imx134.bin"
#define FIMC_IS_4H5_SETF			"setfile_4h5.bin"
#define FIMC_IS_3L2_SETF			"setfile_3l2.bin"
#define FIMC_IS_6B2_SETF			"setfile_6b2.bin"
#define FIMC_IS_8B1_SETF			"setfile_8b1.bin"
#define FIMC_IS_6D1_SETF			"setfile_6d1.bin"
#define FIMC_IS_2P2_SETF			"setfile_2p2.bin"
#define FIMC_IS_2P2_12M_SETF			"setfile_2p2_12m.bin"
#define FIMC_IS_COMPANION_MASTER_SETF			"companion_master_setfile.bin"
#define FIMC_IS_COMPANION_MODE_SETF			"companion_mode_setfile.bin"
#define FIMC_IS_COMPANION_2P2_MASTER_SETF			"companion_2p2_master_setfile.bin"
#define FIMC_IS_COMPANION_2P2_MODE_SETF			"companion_2p2_mode_setfile.bin"
#define FIMC_IS_COMPANION_IMX240_MASTER_SETF			"companion_imx240_master_setfile.bin"
#define FIMC_IS_COMPANION_IMX240_MODE_SETF			"companion_imx240_mode_setfile.bin"
#define FIMC_IS_COMPANION_2P2_12M_MASTER_SETF			"companion_2p2_12m_master_setfile.bin"
#define FIMC_IS_COMPANION_2P2_12M_MODE_SETF			"companion_2p2_12m_mode_setfile.bin"
#define FIMC_IS_FW_PATH				"/system/vendor/firmware/"
#define FIMC_IS_FW_DUMP_PATH		"/data/"

#define FIMC_IS_FW_BASE_MASK			((1 << 26) - 1)
#define FIMC_IS_VERSION_SIZE			42
#define FIMC_IS_SETFILE_VER_OFFSET		0x40
#define FIMC_IS_SETFILE_VER_SIZE		52

#define FIMC_IS_CAL_SDCARD			"/data/cal_data.bin"

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT)
#define FIMC_IS_MAX_FW_SIZE			(8 * 1024)
#define HEADER_CRC32_LEN			(80 / 2)
#define OEM_CRC32_LEN				(64 / 2)
#define AWB_CRC32_LEN				(32 / 2)
#define SHADING_CRC32_LEN			(6623 / 2)
#else
/*#define FIMC_IS_MAX_CAL_SIZE			(20 * 1024)*/
#define FIMC_IS_MAX_FW_SIZE			(2048 * 1024)
#define HEADER_CRC32_LEN (224 / 2)
#define OEM_CRC32_LEN (192 / 2)
#define AWB_CRC32_LEN (32 / 2)
#define SHADING_CRC32_LEN (2336 / 2)
#endif

#define FIMC_IS_MAX_COMPANION_FW_SIZE			(120 * 1024)
#define FIMC_IS_CAL_START_ADDR			(0x013D0000)
#define FIMC_IS_CAL_RETRY_CNT			(2)
#define FIMC_IS_FW_RETRY_CNT			(2)
#define FROM_VERSION_V004 '4'
#define FROM_VERSION_V005 '5'

enum {
        CC_BIN1 = 0,
        CC_BIN2,
        CC_BIN3,
        CC_BIN4,
        CC_BIN5,
        CC_BIN6,
        CC_BIN_MAX,
};

int fimc_is_sec_set_force_caldata_dump(bool fcd);

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos);
ssize_t read_data_from_file(char *name, char *buf, size_t count, loff_t *pos);

int fimc_is_sec_get_sysfs_finfo(struct fimc_is_from_info **finfo);
int fimc_is_sec_get_sysfs_pinfo(struct fimc_is_from_info **pinfo);
int fimc_is_sec_get_cal_buf(char **buf);
int fimc_is_sec_get_loaded_fw(char **buf);
int fimc_is_sec_get_loaded_c1_fw(char **buf);

int fimc_is_sec_get_camid_from_hal(char *fw_name, char *setf_name);
int fimc_is_sec_get_camid(void);
int fimc_is_sec_set_camid(int id);
int fimc_is_sec_get_pixel_size(char *header_ver);

int fimc_is_sec_readfw(struct fimc_is_core *core);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT)
int fimc_is_sec_readcal_eeprom(struct device *dev, int isSysfsRead);
int fimc_is_sec_fw_sel_eeprom(struct device *dev, char *fw_name, char *setf_name, int isSysfsRead);
#else
int fimc_is_sec_readcal(struct fimc_is_core *core, int isSysfsRead);
int fimc_is_sec_fw_sel(struct fimc_is_core *core, struct device *dev, char *fw_name, char *setf_name, int isSysfsRead);
#endif
#ifdef CONFIG_COMPANION_USE
int fimc_is_sec_concord_fw_sel(struct fimc_is_core *core, struct device *dev,
	char *fw_name, char *master_setf_name, char *mode_setf_name, int isSysfsRead);
#endif
int fimc_is_sec_fw_revision(char *fw_ver);
int fimc_is_sec_fw_revision(char *fw_ver);
bool fimc_is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2);

bool fimc_is_sec_check_fw_crc32(char *buf);
bool fimc_is_sec_check_cal_crc32(char *buf);
void fimc_is_sec_make_crc32_table(u32 *table, u32 id);

int fimc_is_sec_gpio_enable(struct exynos_platform_fimc_is *pdata, char *name, bool on);
int fimc_is_sec_core_voltage_select(struct device *dev, char *header_ver);
int fimc_is_sec_ldo_enable(struct device *dev, char *name, bool on);

int fimc_is_spi_reset_by_core(struct spi_device *spi, void *buf, u32 rx_addr, size_t size);
int fimc_is_spi_read_by_core(struct spi_device *spi, void *buf, u32 rx_addr, size_t size);
#ifdef CONFIG_COMPANION_USE
void fimc_is_set_spi_config(struct fimc_is_spi_gpio *spi_gpio, int func, bool ssn);
#endif
#endif /* FIMC_IS_SEC_DEFINE_H */
