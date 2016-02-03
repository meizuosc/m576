/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
//#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/regs-pmu.h>
#include <sound/soc.h>

#include "board-meizu-audio.h"

static inline void meizu_enable_32khz(bool on)
{
#define EXYNOS_RTCCON_REG (S5P_VA_RTC + 0x0040)
#define EXYNOS_RTCCON_CLKOUTEN (1 << 9)

	unsigned long reg;
	/* enable codec_32khz: PMIC_32KHz->rtc->xrtcclko */
	writel(0x0, EXYNOS_PMUREG(0x001C)); // RTC_CLKO_SEL: 0x105c_001c
	reg = __raw_readl(EXYNOS_RTCCON_REG);
	if (on)
		reg |= EXYNOS_RTCCON_CLKOUTEN;
	else
		reg &= ~EXYNOS_RTCCON_CLKOUTEN;
	__raw_writel(reg, EXYNOS_RTCCON_REG);
}

void meizu_enable_mclk(bool on)
{
	/* enable mclk: 24MHz->xxti->xclkout */
	writel(on ? 0x1000 : 0x1001, EXYNOS_PMUREG(0x0A00)); // PMU_DEBUG: 0x105c_0a00
}

void meizu_audio_clock_init(void)
{
	meizu_enable_mclk(true);
	meizu_enable_32khz(true);
}


int meizu_audio_regulator_init(struct device *dev)
{
	struct regulator *vddcore_nr;
	int ret = 0;

	/* Voice processor ES705 startup sequencing:
	 *   VDD_IO should be brought up before VDD_CORE,
	 *   RESET_N pin should not be de-asserted without CLK_IN present.
	 * Note that VDD_IO & VDD_CORE are present at system startup,
	 * so we disable VDD_CORE then re-enable here to ensure power sequence.
	 */

	vddcore_nr = devm_regulator_get(dev, "vdd11_nr");
	if (IS_ERR(vddcore_nr)) {
		ret = PTR_ERR(vddcore_nr);
		dev_err(dev, "Failed to get LDO(vdd11_nr)\n");
		return ret;
	}

	ret = regulator_force_disable(vddcore_nr);
	if (ret) {
		dev_err(dev, "Failed to disable LDO(vdd11_nr): %d\n", ret);
		return ret;
	}

	usleep_range(10000, 10000); // delay 10ms

	ret = regulator_enable(vddcore_nr);
	if (ret) {
		dev_err(dev, "Failed to enable LDO(vdd11_nr): %d\n", ret);
		return ret;
	}

	return ret;
}


int set_aud_pll_rate(struct device *dev, unsigned long rate)
{
	struct clk *fout_aud_pll;

	fout_aud_pll = clk_get(dev, "fout_aud_pll");
	if (IS_ERR(fout_aud_pll)) {
		dev_err(dev, "Failed to get fout_aud_pll\n");
		return PTR_ERR(fout_aud_pll);
	}

	if (rate == clk_get_rate(fout_aud_pll))
		goto out;

	clk_set_rate(fout_aud_pll, rate);
	pr_debug("%s: aud_pll rate = %ld\n", __func__, clk_get_rate(fout_aud_pll));
out:
	clk_put(fout_aud_pll);

	return 0;
}
