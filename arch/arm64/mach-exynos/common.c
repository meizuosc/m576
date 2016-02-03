/*
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Common Codes for EXYNOS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/serial_core.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/export.h>
#include <linux/irqdomain.h>
#include <linux/of_address.h>
#include <linux/clocksource.h>
#include <linux/clk-provider.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/slab.h>
#include <linux/smc.h>

#include <asm/proc-fns.h>
#include <asm/exception.h>
#include <asm/mach/map.h>
#include <asm/cacheflush.h>

#include <mach/regs-pmu.h>
#include <mach/map.h>

#include <plat/cpu.h>

#include "common.h"

static const char name_exynos7420[] = "EXYNOS7420";
static const char name_exynos7580[] = "EXYNOS7580";
static const char name_exynos5433[] = "EXYNOS5433";

static void exynos7420_map_io(void);
static void exynos7580_map_io(void);
static void exynos5433_map_io(void);
static int exynos_init(void);

static struct cpu_table cpu_ids[] __initdata = {
	{
		.idcode		= EXYNOS7420_SOC_ID,
		.idmask		= EXYNOS7_SOC_MASK,
		.map_io		= exynos7420_map_io,
		.init		= exynos_init,
		.name		= name_exynos7420,
	}, {
		.idcode		= EXYNOS7580_SOC_ID,
		.idmask		= EXYNOS7_SOC_MASK,
		.map_io		= exynos7580_map_io,
		.init		= exynos_init,
		.name		= name_exynos7580,
	}, {
		.idcode		= EXYNOS5433_SOC_ID,
		.idmask		= EXYNOS5_SOC_MASK,
		.map_io		= exynos5433_map_io,
		.init		= exynos_init,
		.name		= name_exynos5433,
	},
};

/* Initial IO mappings */
static struct map_desc exynos_iodesc[] __initdata = {
	{
		.virtual	= (unsigned long)S5P_VA_CHIPID,
		.pfn		= __phys_to_pfn(EXYNOS_PA_CHIPID),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
};

static struct map_desc exynos7420_iodesc0[] __initdata = {
	{
		.virtual	= (unsigned long)S3C_VA_UART,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_UART),
		.length		= SZ_512K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_TOPC,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_TOPC),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_TOP0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_TOP0),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_TOP1,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_TOP1),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_ATLAS,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_ATLAS),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_APOLLO,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_APOLLO),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_G3D,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_G3D),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_MIF0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_MIF0),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_MIF1,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_MIF1),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_MIF2,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_MIF2),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_MIF3,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_MIF3),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_CCORE,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_CCORE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_IMEM,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_IMEM),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_CAM0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_CAM0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_CAM0_LOCAL,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_CAM0_LOCAL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_CAM1,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_CAM1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_CAM1_LOCAL,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_CAM1_LOCAL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_ISP0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_ISP0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_ISP0_LOCAL,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_ISP0_LOCAL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_ISP1,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_ISP1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_ISP1_LOCAL,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_ISP1_LOCAL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_VPP,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_VPP),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_PERIC0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_PERIC0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_PERIC1,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_PERIC1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_PERIS,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_PERIS),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_BUS0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_BUS0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_BUS1,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_BUS1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_DISP,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_DISP),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_AUD,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_AUD),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_FSYS0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_FSYS0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_FSYS1,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_FSYS1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_MSCL,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_MSCL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_G2D,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_G2D),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_MFC,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_MFC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_CMU_HEVC,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_CMU_HEVC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual        = (unsigned long)S5P_VA_PMU,
		.pfn            = __phys_to_pfn(EXYNOS7420_PA_PMU),
		.length         = SZ_64K,
		.type           = MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_PMU_LPI_CAM0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_PMU_LPI_CAM0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_PMU_LPI_CAM1,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_PMU_LPI_CAM1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_PMU_LPI_ISP0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_PMU_LPI_ISP0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},{
		.virtual	= (unsigned long)EXYNOS7420_VA_PMU_LPI_ATLAS,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_PMU_LPI_ATLAS),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual        = (unsigned long)S5P_VA_SYSRAM,
		.pfn            = __phys_to_pfn(EXYNOS7_PA_SYSRAM),
		.length         = SZ_4K,
		.type           = MT_DEVICE,
	}, {
		.virtual        = (unsigned long)S5P_VA_SYSRAM_NS,
		.pfn            = __phys_to_pfn(EXYNOS7420_PA_SYSRAM_NS),
		.length         = SZ_4K,
		.type           = MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7420_VA_SYSREG,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG),
		.length		= SZ_16K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_CHIPID6,
		.pfn		= __phys_to_pfn(EXYNOS_PA_CHIPID6),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_FIMCLITE0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_FIMC_BNSA),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_FIMCLITE1,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_FIMC_BNSB),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_FIMCLITE2,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_FIMC_BNSC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_FIMCLITE3,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_FIMC_BNSD),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_MIPICSI0,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_MIPI_CSIS0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_MIPICSI1,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_MIPI_CSIS1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_MIPICSI2,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_MIPI_CSIS2),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_MIPICSI3,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_MIPI_CSIS3),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_APM_SRAM,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_APM_SRAM),
		.length		= SZ_16K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_APM,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_APM),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_APM_NOTI,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_APM_NOTI),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_CAM0,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_RTC,
		.pfn		= __phys_to_pfn(EXYNOS7420_PA_RTC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}
};

static struct map_desc exynos7580_iodesc[] __initdata = {
	{
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_TOP,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_TOP),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_MIF,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_MIF),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_APL,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_APL),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_CPU,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_CPU),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_IMEM,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_IMEM),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_AUD,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_AUD),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_G3D,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_G3D),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_BUS0,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_BUS0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_BUS1,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_BUS1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_MFCMSCL,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_MFCMSCL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_BUS2,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_BUS2),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_FSYS,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_FSYS),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_PERI,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_PERI),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_ISP_LOCAL,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_ISP_LOCAL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_ISP,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_ISP),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_CMU_DISP,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_CMU_DISP),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_PMU,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_PMU),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_SYSREG_DISP,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_SYSREG_DISP),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSRAM_NS,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_SYSRAM_NS),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_GPIO,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_GPIO_NFC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS7580_VA_SYSREG_MIF,
		.pfn		= __phys_to_pfn(EXYNOS7580_PA_SYSREG_MIF),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}
};

static struct map_desc exynos5433_iodesc0[] __initdata = {
	{
		.virtual	= (unsigned long)S3C_VA_UART,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_UART),
		.length		= SZ_256K,
		.type		= MT_DEVICE,
	}, {
		.virtual        = (unsigned long)S5P_VA_PMU,
		.pfn            = __phys_to_pfn(EXYNOS5433_PA_PMU),
		.length         = SZ_64K,
		.type           = MT_DEVICE,
	}, {
		.virtual        = (unsigned long)S5P_VA_SYSRAM,
		.pfn            = __phys_to_pfn(EXYNOS5_PA_SYSRAM),
		.length         = SZ_4K,
		.type           = MT_DEVICE,
	}, {
		.virtual        = (unsigned long)S5P_VA_SYSRAM_NS,
		.pfn            = __phys_to_pfn(EXYNOS5433_PA_SYSRAM_NS),
		.length         = SZ_4K,
		.type           = MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_TOP,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_TOP),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_EGL,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_EGL),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_KFC,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_KFC),
		.length		= SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_AUD,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_AUD),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_BUS1,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_BUS1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_BUS2,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_BUS2),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_CAM0,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_CAM0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_CAM0_LOCAL,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_CAM0_LOCAL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},{
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_CAM1,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_CAM1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_CAM1_LOCAL,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_CAM1_LOCAL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_CPIF,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_CPIF),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_DISP,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_DISP),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_FSYS,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_FSYS),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_G2D,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_G2D),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_G3D,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_G3D),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_GSCL,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_GSCL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_HEVC,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_HEVC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_IMEM,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_IMEM),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_ISP,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_ISP),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_ISP_LOCAL,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_ISP_LOCAL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_MFC0,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_MFC0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_MFC1,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_MFC1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_MIF,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_MIF),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_MSCL,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_MSCL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_PERIC,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_PERIC),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_CMU_PERIS,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_CMU_PERIS),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_DISP,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG_DISP),
		.length         = SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_CAM0,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG_CAM0),
		.length         = SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_CAM1,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG_CAM1),
		.length         = SZ_8K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_ISP,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG_ISP),
		.length         = SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_MFC0,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG_MFC0),
		.length         = SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_HEVC,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG_HEVC),
		.length         = SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_MSCL,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_MSCLSYS),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_GSCL,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG_GSCL),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_G3D,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG_G3D),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_G2D,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG_G2D),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_SYSREG_AUD,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_SYSREG_AUD),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_CHIPID2,
		.pfn		= __phys_to_pfn(EXYNOS_PA_CHIPID2),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_CHIPID3,
		.pfn		= __phys_to_pfn(EXYNOS_PA_CHIPID3),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_CHIPID4,
		.pfn		= __phys_to_pfn(EXYNOS_PA_CHIPID4),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_CHIPID5,
		.pfn		= __phys_to_pfn(EXYNOS_PA_CHIPID5),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_CHIPID6,
		.pfn		= __phys_to_pfn(EXYNOS_PA_CHIPID6),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},{
		.virtual	= (unsigned long)EXYNOS5433_VA_PMU_LPI_CAM0,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_PMU_LPI_CAM0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_PMU_LPI_CAM1,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_PMU_LPI_CAM1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)EXYNOS5433_VA_PMU_LPI_ISP,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_PMU_LPI_ISP),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},{
		.virtual	= (unsigned long)EXYNOS5433_VA_PMU_LPI_EAGLE,
		.pfn		= __phys_to_pfn(EXYNOS5433_PA_PMU_LPI_EAGLE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_FIMCLITE0,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_FIMC_LITE0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_FIMCLITE1,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_FIMC_LITE1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_FIMCLITE2,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_FIMC_LITE2),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_FIMCLITE3,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_FIMC_LITE3),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_MIPICSI0,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_MIPI_CSIS0),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_MIPICSI1,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_MIPI_CSIS1),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_MIPICSI2,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_MIPI_CSIS2),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_FIMC_FD,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_FIMC_FD),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)S5P_VA_LPASS,
		.pfn		= __phys_to_pfn(EXYNOS5_PA_LPASS),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	}
};

#define INFORM_NONE		0x0
#define INFORM_RAMDUMP		0xd
#define INFORM_RECOVERY		0xf

void exynos_restart(char mode, const char *cmd)
{
	u32 val, restart_inform;
	void __iomem *addr;

	val = 0x1;
	addr = EXYNOS_PMU_SWRESET;

	restart_inform = INFORM_NONE;

	if (cmd) {
		if (!strcmp((char *)cmd, "recovery"))
			restart_inform = INFORM_RECOVERY;
		else if(!strcmp((char *)cmd, "ramdump"))
			restart_inform = INFORM_RAMDUMP;
	}
	/*Write inform register to make system into recovery mode after restart ping.gao@samsung.com*/
	__raw_writel(restart_inform, EXYNOS_PMU_SYSIP_DAT0);

	__raw_writel(val, addr);
}

/*
 * exynos_map_io
 *
 * register the standard cpu IO areas
 */

void __init exynos_init_io(struct map_desc *mach_desc, int size)
{
	iotable_init(exynos_iodesc, ARRAY_SIZE(exynos_iodesc));

	if (mach_desc)
		iotable_init(mach_desc, size);

	/* detect cpu id and rev. */
	s5p_init_cpu(S5P_VA_CHIPID);

	s3c_init_cpu(samsung_cpu_id, cpu_ids, ARRAY_SIZE(cpu_ids));
}

static void __init exynos7420_map_io(void)
{
	iotable_init(exynos7420_iodesc0, ARRAY_SIZE(exynos7420_iodesc0));
}

static void __init exynos7580_map_io(void)
{
	iotable_init(exynos7580_iodesc, ARRAY_SIZE(exynos7580_iodesc));
}

static void __init exynos5433_map_io(void)
{
	iotable_init(exynos5433_iodesc0, ARRAY_SIZE(exynos5433_iodesc0));
}

struct bus_type exynos_subsys = {
	.name		= "exynos-core",
	.dev_name	= "exynos-core",
};

static struct device exynos4_dev = {
	.bus	= &exynos_subsys,
};

static int __init exynos_core_init(void)
{
	return subsys_system_register(&exynos_subsys, NULL);
}
core_initcall(exynos_core_init);

static int __init exynos_init(void)
{
	printk(KERN_INFO "EXYNOS: Initializing architecture\n");

	return device_register(&exynos4_dev);
}

static int  __init exynos_set_debug_mem(void)
{
	int ret;
	static char *smc_debug_mem;
	char *phys;

	smc_debug_mem = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if (!smc_debug_mem) {
		pr_err("%s: kmalloc for smc_debug failed.\n", __func__);
		return 0;
	}

	/* to map & flush memory */
	memset(smc_debug_mem, 0x00, PAGE_SIZE);
	__dma_flush_range(smc_debug_mem, smc_debug_mem+PAGE_SIZE);

	phys = (char *)virt_to_phys(smc_debug_mem);
	pr_err("%s: alloc kmem for smc_dbg virt: 0x%p phys: 0x%p size: %ld.\n",
			__func__, smc_debug_mem, phys, PAGE_SIZE);
	ret = exynos_smc(SMC_CMD_SET_DEBUG_MEM, (u64)phys, (u64)PAGE_SIZE, 0);

	/* correct return value is input size */
	if (ret != PAGE_SIZE) {
		pr_err("%s: Can not set the address to el3 monitor. "
				"ret = 0x%x. free the kmem\n", __func__, ret);
		kfree(smc_debug_mem);
	}

	return 0;
}
arch_initcall(exynos_set_debug_mem);
