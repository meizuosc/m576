/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/mutex.h>
#include <linux/mfd/samsung/core.h>

#include <mach/tmu.h>
#include "pm_domains-exynos5433.h"

static void exynos5_pd_enable_clk(struct exynos5430_pd_state *ptr, int nr_regs)
{
	for (; nr_regs > 0; nr_regs--, ptr++) {
		clk_prepare_enable(ptr->clock);
		DEBUG_PRINT_INFO("clock name : %s, usage_count : %d, SFR : 0x%x\n",
			ptr->clock->name, ptr->clock->enable_count, __raw_readl(ptr->reg));
	}
}

static void exynos5_pd_disable_clk(struct exynos5430_pd_state *ptr, int nr_regs)
{
	for (; nr_regs > 0; nr_regs--, ptr++) {
		if (ptr->clock->enable_count > 0) {
			clk_disable_unprepare(ptr->clock);
			DEBUG_PRINT_INFO("clock name : %s, usage_count : %d, SFR : 0x%x\n",
				ptr->clock->name, ptr->clock->enable_count, __raw_readl(ptr->reg));
		}
	}
}

static void exynos_pd_notify_power_state(struct exynos_pm_domain *pd, unsigned int turn_on)
{
#if defined(CONFIG_ARM_EXYNOS5433_BUS_DEVFREQ) && defined(CONFIG_PM_RUNTIME)
	exynos5_int_notify_power_status(pd->genpd.name, true);
	exynos5_isp_notify_power_status(pd->genpd.name, true);
	exynos5_disp_notify_power_status(pd->genpd.name, true);
#endif
}

/* exynos_pd_maudio_power_on_post - callback after power on.
 * @pd: power domain.
 */
static int exynos_pd_maudio_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_maudio_clk_save,
			ARRAY_SIZE(exynos_pd_maudio_clk_save));

	/* set noc-bus timeout */
	__raw_writel(1, S5P_VA_SYSREG_AUD + 0x700);

	return 0;
}

static int exynos_pd_maudio_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_save_sfr(exynos_pd_maudio_clk_save,
			ARRAY_SIZE(exynos_pd_maudio_clk_save));

	return 0;
}

static int exynos_pd_g3d_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

#if defined(CONFIG_REGULATOR_S2MPS13)
	s2m_set_g3d_pin(true);
#endif

	exynos5_pd_enable_clk(gateclks_g3d, ARRAY_SIZE(gateclks_g3d));

	exynos_tmu_core_control(false, EXYNOS_TMU_CORE_GPU);	/* disable G3D TMU core before G3D is turned off */

	return 0;
}

static int exynos_pd_g3d_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	exynos_tmu_core_control(true, EXYNOS_TMU_CORE_GPU);	/* enable G3D TMU core after G3D is turned on */

	/* set noc-bus timeout */
	__raw_writel(1, S5P_VA_SYSREG_G3D + 0x700);

	return 0;
}

static int exynos_pd_g3d_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	if (is_g3d_clk_en == false) {
		exynos5_pd_enable_clk(gateclks_g3d, ARRAY_SIZE(gateclks_g3d));
		is_g3d_clk_en = true;
	}

	exynos_save_sfr(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	return 0;
}

static int exynos_pd_g3d_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_disable_clk(gateclks_g3d, ARRAY_SIZE(gateclks_g3d));

#if defined(CONFIG_REGULATOR_S2MPS13)
	s2m_set_g3d_pin(false);
#endif

	return 0;
}

/* exynos_pd_mfc_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mfc_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_enable_clk(gateclks_mfc, ARRAY_SIZE(gateclks_mfc));

	return 0;
}

static int exynos_pd_mfc_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_mfc_clk_save,
			ARRAY_SIZE(exynos_pd_mfc_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* set noc-bus timeout */
	__raw_writel(1, S5P_VA_SYSREG_MFC0 + 0x700);

	/* dynamic clock gating enabled */
	__raw_writel(1, S5P_VA_SYSREG_MFC0 + 0x204);

	return 0;
}

static int exynos_pd_mfc_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	if (is_mfc_clk_en == false) {
		exynos5_pd_enable_clk(gateclks_mfc, ARRAY_SIZE(gateclks_mfc));
		is_mfc_clk_en = true;
	}

	exynos_save_sfr(exynos_pd_mfc_clk_save,
			ARRAY_SIZE(exynos_pd_mfc_clk_save));

	return 0;
}

/* exynos_pd_mfc_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mfc_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_disable_clk(gateclks_mfc, ARRAY_SIZE(gateclks_mfc));

	return 0;
}

/* exynos_pd_hevc_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_hevc_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_enable_clk(gateclks_hevc, ARRAY_SIZE(gateclks_hevc));

	return 0;
}

static int exynos_pd_hevc_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_hevc_clk_save,
			ARRAY_SIZE(exynos_pd_hevc_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* set noc-bus timeout */
	__raw_writel(1, S5P_VA_SYSREG_HEVC + 0x700);

	/* dynamic clock gating enabled */
	__raw_writel(1, S5P_VA_SYSREG_HEVC + 0x204);

	return 0;
}

static int exynos_pd_hevc_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	if (is_hevc_clk_en == false) {
		exynos5_pd_enable_clk(gateclks_hevc, ARRAY_SIZE(gateclks_hevc));
		is_hevc_clk_en = true;
	}

	exynos_save_sfr(exynos_pd_hevc_clk_save,
			ARRAY_SIZE(exynos_pd_hevc_clk_save));

	return 0;
}

/* exynos_pd_hevc_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_hevc_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_disable_clk(gateclks_hevc, ARRAY_SIZE(gateclks_hevc));

	return 0;
}

/* exynos_pd_gscl_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_gscl_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_enable_clk(gateclks_gscl, ARRAY_SIZE(gateclks_gscl));

	return 0;
}

static int exynos_pd_gscl_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_gscl_clk_save,
			ARRAY_SIZE(exynos_pd_gscl_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* set noc-bus timeout */
	__raw_writel(1, S5P_VA_SYSREG_GSCL + 0x700);

	return 0;
}

static int exynos_pd_gscl_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	if (is_gscl_clk_en == false) {
		exynos5_pd_enable_clk(gateclks_gscl, ARRAY_SIZE(gateclks_gscl));
		is_gscl_clk_en = true;
	}

	exynos_save_sfr(exynos_pd_gscl_clk_save,
			ARRAY_SIZE(exynos_pd_gscl_clk_save));

	return 0;
}

/* exynos_pd_gscl_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_gscl_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_disable_clk(gateclks_gscl, ARRAY_SIZE(gateclks_gscl));

	return 0;
}

/* exynos_pd_disp_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable IP_MIF3 clk.
 */
static int exynos_pd_disp_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_enable_clk(gateclks_disp, ARRAY_SIZE(gateclks_disp));

	return 0;
}

/* exynos_pd_disp_power_on_post - setup after power on.
 * @pd: power domain.
 *
 * enable DISP dynamic clock gating
 */
static int exynos_pd_disp_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_disp_clk_save,
			ARRAY_SIZE(exynos_pd_disp_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* set noc-bus timeout */
	__raw_writel(1, sysreg_disp + 0x700);

	/* Enable DISP dynamic clock gating */
	writel(0xf, sysreg_disp + 0x204);
	writel(0x1f, sysreg_disp + 0x208);
	writel(0x0, sysreg_disp + 0x500);

	return 0;
}

/* exynos_pd_disp_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_MIF3 clk.
 * check Decon has been reset.
 */
static int exynos_pd_disp_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	if (is_disp_clk_en == false) {
		exynos5_pd_enable_clk(gateclks_disp, ARRAY_SIZE(gateclks_disp));
		is_disp_clk_en = true;
	}

	exynos_save_sfr(exynos_pd_disp_clk_save,
			ARRAY_SIZE(exynos_pd_disp_clk_save));

	return 0;
}

/* exynos_pd_disp_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable IP_MIF3 clk.
 * disable SRC_SEL_MIF4/5
 */
static int exynos_pd_disp_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_disable_clk(gateclks_disp, ARRAY_SIZE(gateclks_disp));

	reg = __raw_readl(EXYNOS5430_SRC_SEL_MIF4);
	reg &= ~((1 << 24) | (1 << 20) | (1 << 16) | (1 << 8) | (1 << 4) | (1 << 0));
	__raw_writel(reg, EXYNOS5430_SRC_SEL_MIF4);

	reg = __raw_readl(EXYNOS5430_SRC_SEL_MIF5);
	reg &= ~((1 << 24) | (1 << 20) | (1 << 16) | (1 << 8) | (1 << 4) | (1 << 0));
	__raw_writel(reg, EXYNOS5430_SRC_SEL_MIF5);

	reg = __raw_readl(EXYNOS5430_SRC_SEL_MIF6);
	reg &= ~((1 << 8) | (1 << 4) | (1 << 0));
	__raw_writel(reg, EXYNOS5430_SRC_SEL_MIF6);

	reg = __raw_readl(EXYNOS5430_SRC_SEL_MIF7);
	reg &= ~((1 << 24) | (1 << 20) | (1 << 16) | (1 << 8) | (1 << 4) | (1 << 0));
	__raw_writel(reg, EXYNOS5430_SRC_SEL_MIF7);

	return 0;
}

struct exynos5433_mscl_clk {
	struct clk *fin_pll;
	struct clk *mout_aclk_mscl_400_user;
};

static int exynos_pd_mscl_init(struct exynos_pm_domain *pd)
{
	struct exynos5433_mscl_clk *msclclk;

	msclclk = kmalloc(sizeof(*msclclk), GFP_KERNEL);
	if (!msclclk) {
		pr_err("%s: failed to allocate private data\n", __func__);
		return -ENOMEM;
	}

	msclclk->mout_aclk_mscl_400_user =
		clk_get(NULL, "mux_aclk_mscl_400_user");
	if (IS_ERR(msclclk->mout_aclk_mscl_400_user)) {
		pr_err("%s: failed to get mout_aclk_mscl_400_user\n", __func__);
		goto err_mout;
	}

	msclclk->fin_pll = clk_get(NULL, "fin_pll");
	if (IS_ERR(msclclk->fin_pll)) {
		pr_err("%s: failed to get fin_pll\n", __func__);
		goto err_fin;
	}

	pd->priv = msclclk;

	return 0;
err_fin:
	clk_put(msclclk->mout_aclk_mscl_400_user);
err_mout:
	kfree(msclclk);
	return -ENOENT;
}

/* exynos_pd_mscl_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mscl_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_enable_clk(gateclks_mscl, ARRAY_SIZE(gateclks_mscl));

	return 0;
}

static int exynos_pd_mscl_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_mscl_clk_save,
			ARRAY_SIZE(exynos_pd_mscl_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* set noc-bus timeout */
	__raw_writel(1, S5P_VA_SYSREG_MSCL + 0x700);

	/* dynamic clock gating enabled */
	__raw_writel(1, S5P_VA_SYSREG_MSCL + 0x204);
	__raw_writel(0, S5P_VA_SYSREG_MSCL + 0x500);

	return 0;
}

static int exynos_pd_mscl_power_off_pre(struct exynos_pm_domain *pd)
{
	struct exynos5433_mscl_clk *msclclk = pd->priv;

	DEBUG_PRINT_INFO("%s\n", __func__);

	if (is_mscl_clk_en == false) {
		exynos5_pd_enable_clk(gateclks_mscl, ARRAY_SIZE(gateclks_mscl));
		is_mscl_clk_en = true;
	}

	exynos_save_sfr(exynos_pd_mscl_clk_save,
			ARRAY_SIZE(exynos_pd_mscl_clk_save));

	return clk_set_parent(msclclk->mout_aclk_mscl_400_user,
				msclclk->fin_pll);
}

/* exynos_pd_mscl_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mscl_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_disable_clk(gateclks_mscl, ARRAY_SIZE(gateclks_mscl));

	reg = __raw_readl(EXYNOS5430_SRC_SEL_TOP_MSCL);
	reg &= ~((1 << 8 ) | (1 << 4) | (1 << 0));
	__raw_writel(reg, EXYNOS5430_SRC_SEL_TOP_MSCL);

	return 0;
}

/* exynos_pd_g2d_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_g2d_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_enable_clk(gateclks_g2d, ARRAY_SIZE(gateclks_g2d));

	return 0;
}

static int exynos_pd_g2d_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_g2d_clk_save,
			ARRAY_SIZE(exynos_pd_g2d_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* set noc-bus timeout */
	__raw_writel(1, S5P_VA_SYSREG_G2D + 0x700);

	return 0;
}

static int exynos_pd_g2d_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	if (is_g2d_clk_en == false) {
		exynos5_pd_enable_clk(gateclks_g2d, ARRAY_SIZE(gateclks_g2d));
		is_g2d_clk_en = true;
	}

	exynos_save_sfr(exynos_pd_g2d_clk_save,
			ARRAY_SIZE(exynos_pd_g2d_clk_save));

	return 0;
}

/* exynos_pd_g2d_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_g2d_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_disable_clk(gateclks_g2d, ARRAY_SIZE(gateclks_g2d));

	return 0;
}

/* exynos_pd_isp_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_isp_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_enable_clk(gateclks_isp, ARRAY_SIZE(gateclks_isp));

	exynos_tmu_core_control(false, EXYNOS_TMU_CORE_ISP);	/* disable ISP TMU core before ISP is turned off */

	return 0;
}

static int exynos_pd_isp_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(0x3, S5P_VA_SYSREG_ISP + 0x204);
	__raw_writel(0x7, S5P_VA_SYSREG_ISP + 0x208);
	__raw_writel(0x7F, S5P_VA_SYSREG_ISP + 0x20C);
	__raw_writel(0xC, S5P_VA_SYSREG_ISP + 0x500);

	exynos_tmu_core_control(true, EXYNOS_TMU_CORE_ISP);	/* enable ISP TMU core after ISP is turned on */

	return 0;
}

/* exynos_pd_isp_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_ISP1 clk.
 */
static int exynos_pd_isp_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s\n", __func__);

	if (is_isp_clk_en == false) {
		exynos5_pd_enable_clk(gateclks_isp, ARRAY_SIZE(gateclks_isp));
		is_isp_clk_en = true;
	}

	/* For prevent FW clock gating */
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_ISP_LOCAL);
	__raw_writel(0x0000003F, EXYNOS5430_ENABLE_PCLK_ISP_LOCAL);
	__raw_writel(0x0000003F, EXYNOS5430_ENABLE_SCLK_ISP_LOCAL);
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_IP_ISP_LOCAL0);
	__raw_writel(0x0000000F, EXYNOS5430_ENABLE_IP_ISP_LOCAL1);

	/* For prevent HOST clock gating */
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_ISP0);
	__raw_writel(0x0003FFFF, EXYNOS5430_ENABLE_ACLK_ISP1);
	__raw_writel(0x00003FFF, EXYNOS5430_ENABLE_ACLK_ISP2);
	__raw_writel(0x03FFFFFF, EXYNOS5430_ENABLE_PCLK_ISP);
	__raw_writel(0x0000003F, EXYNOS5430_ENABLE_SCLK_ISP);
	__raw_writel(0x000003FF, EXYNOS5430_ENABLE_IP_ISP0);
	__raw_writel(0x0000FFFF, EXYNOS5430_ENABLE_IP_ISP1);
	__raw_writel(0x00003FFF, EXYNOS5430_ENABLE_IP_ISP2);
	__raw_writel(0x0000000F, EXYNOS5430_ENABLE_IP_ISP3);

	__raw_writel(0x0000003F, EXYNOS_PMU_LPI_MASK_ISP_BUSMASTER);

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_ISP1);
	reg |= (1 << 12 | 1 << 11 | 1 << 8 | 1 << 7 | 1 << 2);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_ISP1);

	exynos_save_sfr(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	return 0;
}

/* exynos_pd_isp_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_isp_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_disable_clk(gateclks_isp, ARRAY_SIZE(gateclks_isp));

	return 0;
}

/* exynos_pd_cam0_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_cam0_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_enable_clk(gateclks_cam0, ARRAY_SIZE(gateclks_cam0));

	return 0;
}

static int exynos_pd_cam0_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_cam0_clk_save,
			ARRAY_SIZE(exynos_pd_cam0_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* set noc-bus timeout */
	__raw_writel(1, S5P_VA_SYSREG_CAM0 + 0x700);

	/* dynamic clock gating enabled */
	__raw_writel(0x3, S5P_VA_SYSREG_CAM0 + 0x204);
	__raw_writel(0x7, S5P_VA_SYSREG_CAM0 + 0x208);
	__raw_writel(0x7FF, S5P_VA_SYSREG_CAM0 + 0x20C);
	__raw_writel(0x1F, S5P_VA_SYSREG_CAM0 + 0x500);

	return 0;
}

/* exynos_pd_cam0_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_CAM01 clk.
 */
static int exynos_pd_cam0_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s\n", __func__);

	if (is_cam0_clk_en == false) {
		exynos5_pd_enable_clk(gateclks_cam0, ARRAY_SIZE(gateclks_cam0));
		is_cam0_clk_en = true;
	}

	/* For prevent FW clock gating */
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_CAM0_LOCAL);
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_PCLK_CAM0_LOCAL);
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_SCLK_CAM0_LOCAL);
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_IP_CAM0_LOCAL0);
	__raw_writel(0x0000001F, EXYNOS5430_ENABLE_IP_CAM0_LOCAL1);

	/* For prevent HOST clock gating */
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_CAM00);
	__raw_writel(0xFFFFFFFF, EXYNOS5430_ENABLE_ACLK_CAM01);
	__raw_writel(0x000003FF, EXYNOS5430_ENABLE_ACLK_CAM02);
	__raw_writel(0x03FFFFFF, EXYNOS5430_ENABLE_PCLK_CAM0);
	__raw_writel(0x000001FF, EXYNOS5430_ENABLE_SCLK_CAM0);
	__raw_writel(0x000003FF, EXYNOS5430_ENABLE_IP_CAM00);
	__raw_writel(0x007FFFFF, EXYNOS5430_ENABLE_IP_CAM01);
	__raw_writel(0x000003FF, EXYNOS5430_ENABLE_IP_CAM02);
	__raw_writel(0x0000001F, EXYNOS5430_ENABLE_IP_CAM03);

	/* LPI disable */
	__raw_writel(0x0000001F, EXYNOS_PMU_LPI_MASK_CAM0_BUSMASTER);

	/* Related async-bridge clock on */
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_CAM01);
	reg |= (1 << 12);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_CAM01);

	exynos_save_sfr(exynos_pd_cam0_clk_save,
			ARRAY_SIZE(exynos_pd_cam0_clk_save));

	return 0;
}

/* exynos_pd_cam0_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_cam0_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_disable_clk(gateclks_cam0, ARRAY_SIZE(gateclks_cam0));

	return 0;
}

/* exynos_pd_cam1_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_cam1_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_enable_clk(gateclks_cam1, ARRAY_SIZE(gateclks_cam1));

	return 0;
}

static int exynos_pd_cam1_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos_restore_sfr(exynos_pd_cam1_clk_save,
			ARRAY_SIZE(exynos_pd_cam1_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* set noc-bus timeout */
	__raw_writel(1, S5P_VA_SYSREG_CAM1 + 0x700);

	/* dynamic clock gating enabled */
	__raw_writel(0x3, S5P_VA_SYSREG_CAM1 + 0x204);
	__raw_writel(0x7, S5P_VA_SYSREG_CAM1 + 0x208);
	__raw_writel(0xFFF, S5P_VA_SYSREG_CAM1 + 0x20C);
	__raw_writel(0x3, S5P_VA_SYSREG_CAM1 + 0x500);

	return 0;
}

/* exynos_pd_cam1_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_CAM11 clk.
 */
static int exynos_pd_cam1_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s\n", __func__);

	if (is_cam1_clk_en ==false) {
		exynos5_pd_enable_clk(gateclks_cam1, ARRAY_SIZE(gateclks_cam1));
		is_cam1_clk_en = true;
	}

	/* For prevent FW clock gating */
	__raw_writel(0x0000001F, EXYNOS5430_ENABLE_ACLK_CAM1_LOCAL);
	__raw_writel(0x00003FFF, EXYNOS5430_ENABLE_PCLK_CAM1_LOCAL);
	__raw_writel(0x00000007, EXYNOS5430_ENABLE_SCLK_CAM1_LOCAL);
	__raw_writel(0x00007FFF, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0);
	__raw_writel(0x00000007, EXYNOS5430_ENABLE_IP_CAM1_LOCAL1);

	/* For prevent HOST clock gating */
	__raw_writel(0x0000001F, EXYNOS5430_ENABLE_ACLK_CAM10);
	__raw_writel(0x3FFFFFFF, EXYNOS5430_ENABLE_ACLK_CAM11);
	__raw_writel(0x000007FF, EXYNOS5430_ENABLE_ACLK_CAM12);
	__raw_writel(0x0FFFFFFF, EXYNOS5430_ENABLE_PCLK_CAM1);
	__raw_writel(0x0000FFFF, EXYNOS5430_ENABLE_SCLK_CAM1);
	__raw_writel(0x00FFFFFF, EXYNOS5430_ENABLE_IP_CAM10);
	__raw_writel(0x003FFFFF, EXYNOS5430_ENABLE_IP_CAM11);
	__raw_writel(0x000007FF, EXYNOS5430_ENABLE_IP_CAM12);

	/* LPI disable */
	__raw_writel(0x00000003, EXYNOS_PMU_LPI_MASK_CAM1_BUSMASTER);

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_CAM11);
	reg |= (1 << 19 | 1 << 18 | 1 << 16 | 1 << 15 | 1 << 14 | 1 << 13);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_CAM11);

	exynos_save_sfr(exynos_pd_cam1_clk_save,
			ARRAY_SIZE(exynos_pd_cam1_clk_save));

	return 0;
}

/* exynos_pd_cam1_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_cam1_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s\n", __func__);

	exynos5_pd_disable_clk(gateclks_cam1, ARRAY_SIZE(gateclks_cam1));

	reg = __raw_readl(EXYNOS5430_SRC_SEL_TOP_CAM1);
	reg &= ~((1 << 24) | (1 << 20) | (1 << 16) | (1 << 8) | (1 << 4) | (1 << 0));
	__raw_writel(reg, EXYNOS5430_SRC_SEL_TOP_CAM1);

	return 0;
}

static unsigned int check_power_status(struct exynos_pm_domain *pd, int power_flags,
					unsigned int timeout)
{
	/* check STATUS register */
	while ((__raw_readl(pd->base+0x4) & LOCAL_PWR_CFG) != power_flags) {
		if (timeout == 0) {
			pr_err("%s@%p: %08x, %08x, %08x\n",
					pd->genpd.name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			return 0;
		}
		--timeout;
		cpu_relax();
		usleep_range(8, 10);
	}

	return timeout;
}

#define TIMEOUT_COUNT	5000 /* about 50ms, based on 1ms */
static int exynos_pd_power_off_custom(struct exynos_pm_domain *pd, int power_flags)
{
	unsigned long timeout;

	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);
	if (likely(pd->base)) {
		/* sc_feedback to OPTION register */
		__raw_writel(0x0102, pd->base+0x8);

		/* on/off value to CONFIGURATION register */
		__raw_writel(power_flags, pd->base);

		timeout = check_power_status(pd, power_flags, TIMEOUT_COUNT);

		if (unlikely(!timeout)) {
			pr_err(PM_DOMAIN_PREFIX "%s can't control power, timeout\n",
					pd->name);
			mutex_unlock(&pd->access_lock);
			return -ETIMEDOUT;
		} else {
			pr_info(PM_DOMAIN_PREFIX "%s power down success\n", pd->name);
		}

		if (unlikely(timeout < (TIMEOUT_COUNT >> 1))) {
			pr_warn("%s@%p: %08x, %08x, %08x\n",
					pd->name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			pr_warn(PM_DOMAIN_PREFIX "long delay found during %s is %s\n",
					pd->name, power_flags ? "on":"off");
		}
	}
	pd->status = power_flags;
	mutex_unlock(&pd->access_lock);

	DEBUG_PRINT_INFO("%s@%p: %08x, %08x, %08x\n",
				pd->genpd.name, pd->base,
				__raw_readl(pd->base),
				__raw_readl(pd->base+4),
				__raw_readl(pd->base+8));

	return 0;
}

static struct exynos_pd_callback pd_callback_list[] = {
	{
		.name = "pd-maudio",
		.on_post = exynos_pd_maudio_power_on_post,
		.off_pre = exynos_pd_maudio_power_off_pre,
	}, {
		.name = "pd-mfc",
		.on_pre = exynos_pd_mfc_power_on_pre,
		.on_post = exynos_pd_mfc_power_on_post,
		.off_pre = exynos_pd_mfc_power_off_pre,
		.off_post = exynos_pd_mfc_power_off_post,
	}, {
		.name = "pd-hevc",
		.on_pre = exynos_pd_hevc_power_on_pre,
		.on_post = exynos_pd_hevc_power_on_post,
		.off_pre = exynos_pd_hevc_power_off_pre,
		.off_post = exynos_pd_hevc_power_off_post,
	}, {
		.name = "pd-gscl",
		.on_pre = exynos_pd_gscl_power_on_pre,
		.on_post = exynos_pd_gscl_power_on_post,
		.off_pre = exynos_pd_gscl_power_off_pre,
		.off_post = exynos_pd_gscl_power_off_post,
	}, {
		.name = "pd-g3d",
		.on_pre = exynos_pd_g3d_power_on_pre,
		.on_post = exynos_pd_g3d_power_on_post,
		.off_pre = exynos_pd_g3d_power_off_pre,
		.off_post = exynos_pd_g3d_power_off_post,
	}, {
		.name = "pd-disp",
		.on_pre = exynos_pd_disp_power_on_pre,
		.on_post = exynos_pd_disp_power_on_post,
		.off_pre = exynos_pd_disp_power_off_pre,
		.off_post = exynos_pd_disp_power_off_post,
	}, {
		.name = "pd-mscl",
		.init = exynos_pd_mscl_init,
		.on_pre = exynos_pd_mscl_power_on_pre,
		.on_post = exynos_pd_mscl_power_on_post,
		.off_pre = exynos_pd_mscl_power_off_pre,
		.off_post = exynos_pd_mscl_power_off_post,
	}, {
		.name = "pd-g2d",
		.on_pre = exynos_pd_g2d_power_on_pre,
		.on_post = exynos_pd_g2d_power_on_post,
		.off_pre = exynos_pd_g2d_power_off_pre,
		.off_post = exynos_pd_g2d_power_off_post,
	}, {
		.name = "pd-isp",
		.on_pre = exynos_pd_isp_power_on_pre,
		.on_post = exynos_pd_isp_power_on_post,
		.off_pre = exynos_pd_isp_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_isp_power_off_post,
	}, {
		.name = "pd-cam0",
		.on_pre = exynos_pd_cam0_power_on_pre,
		.on_post = exynos_pd_cam0_power_on_post,
		.off_pre = exynos_pd_cam0_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_cam0_power_off_post,
	}, {
		.name = "pd-cam1",
		.on_pre = exynos_pd_cam1_power_on_pre,
		.on_post = exynos_pd_cam1_power_on_post,
		.off_pre = exynos_pd_cam1_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_cam1_power_off_post,
	},
};

struct exynos_pd_callback * exynos_pd_find_callback(struct exynos_pm_domain *pd)
{
	struct exynos_pd_callback *cb = NULL;
	int i;

	sysreg_disp = ioremap(EXYNOS5433_PA_SYSREG_DISP, SZ_4K);
	if (IS_ERR_OR_NULL(sysreg_disp))
		pr_err("PM DOMAIN : ioremap of SYSREG_DISP fail\n");

	/* find callback function for power domain */
	for (i=0, cb = &pd_callback_list[0]; i<ARRAY_SIZE(pd_callback_list); i++, cb++) {
		if (strcmp(cb->name, pd->name))
			continue;

		DEBUG_PRINT_INFO("%s: found callback function\n", pd->name);
		break;
	}

	pd->cb = cb;
	return cb;
}

int exynos_pd_clk_get(struct exynos_pm_domain *pd)
{
	int i = 0, match = 0;
	struct exynos5430_pd_state *gateclks = NULL;

	for (i = 0; i < ARRAY_SIZE(pd_clk_list); i++) {
		if (strcmp(pd_clk_list[i].name, pd->name))
			continue;
		DEBUG_PRINT_INFO("%s: found clk name list\n", pd->name);
		match = i;
		gateclks = pd_clk_list[i].gateclks;
		break;
	}

	if (i == ARRAY_SIZE(pd_clk_list)) {
		pr_err(PM_DOMAIN_PREFIX "there is no pd name  %s in clklist\n", pd->name);
		return -ENODEV;
	} else if (!gateclks) {
		DEBUG_PRINT_INFO("%s: no clk name list\n", pd->name);
		return 0;
	}

	for (i = 0; i < pd_clk_list[match].num_gateclks; i++) {
		gateclks[i].clock = samsung_clk_get_by_reg((unsigned long)gateclks[i].reg, gateclks[i].bit_offset);
		if (IS_ERR(gateclks[i].clock)) {
			pr_err(PM_DOMAIN_PREFIX "clk_get of %s's gate clock has been failed\n", pd_clk_list[match].name);
			return -ENODEV;
		}
	}

	return 0;
}
