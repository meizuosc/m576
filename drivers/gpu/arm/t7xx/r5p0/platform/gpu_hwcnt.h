/* drivers/gpu/arm/.../platform/gpu_hwcnt.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_hwcnt.h
 * DVFS
 */
#ifdef MALI_SEC_HWCNT

#ifndef __GPU_HWCNT_H
#define __GPU_HWCNT_H __FILE__

#include <platform/mali_kbase_platform.h>

#define MALI_SEC_HWCNT_DUMP_DVFS_THREAD

#define MALI_SIZE_OF_HWCBLK 64

enum HWCNT_OFFSET {
	OFFSET_SHADER_20 = 20,
	OFFSET_SHADER_21 = 21,
	OFFSET_TRIPIPE_ACTIVE = 26,
	OFFSET_ARITH_WORDS = 27,
	OFFSET_LS_ISSUES = 32,
	OFFSET_TEX_ISSUES = 42,
};

#define LV1_SHIFT       20
#define LV2_BASE_MASK       0x3ff
#define LV2_PT_MASK     0xff000
#define LV2_SHIFT       12
#define LV1_DESC_MASK       0x3
#define LV2_DESC_MASK       0x2

#define HWC_MODE_UTILIZATION 0x80510010
#define HWC_MODE_GPR_EN 0x80510001
#define HWC_MODE_GPR_DIS 0x80510002

#define HWC_ACC_BUFFER_SIZE     4096		// bytes

static inline unsigned long kbase_virt_to_phys(struct mm_struct *mm, unsigned long vaddr)
{
	unsigned long *pgd;
	unsigned long *lv1d, *lv2d;

	pgd = (unsigned long *)mm->pgd;

	lv1d = pgd + (vaddr >> LV1_SHIFT);

	if ((*lv1d & LV1_DESC_MASK) != 0x1) {
		printk("invalid LV1 descriptor, "
				"pgd %p lv1d 0x%lx vaddr 0x%lx\n",
				pgd, *lv1d, vaddr);
		return 0;
	}

	lv2d = (unsigned long *)phys_to_virt(*lv1d & ~LV2_BASE_MASK) +
		((vaddr & LV2_PT_MASK) >> LV2_SHIFT);

	if ((*lv2d & LV2_DESC_MASK) != 0x2) {
		printk("invalid LV2 descriptor, "
				"pgd %p lv2d 0x%lx vaddr 0x%lx\n",
				pgd, *lv2d, vaddr);
		return 0;
	}

	return (*lv2d & PAGE_MASK) | (vaddr & (PAGE_SIZE-1));
}

static inline void* kbase_kmap_from_physical_address(struct kbase_device *kbdev)
{
	return kmap(pfn_to_page(PFN_DOWN(kbdev->hwcnt.phy_addr)));
}

static inline void kbase_kunmap_from_physical_address(struct kbase_device *kbdev)
{
	return kunmap(pfn_to_page(PFN_DOWN(kbdev->hwcnt.phy_addr)));
}

extern mali_error kbase_instr_hwcnt_util_dump(struct kbase_device *kbdev);

mali_error exynos_gpu_hwcnt_update(struct kbase_device *kbdev);

bool hwcnt_check_conditions(struct kbase_device *kbdev);
void hwcnt_value_clear(struct kbase_device *kbdev);
void hwcnt_utilization_equation(struct kbase_device *kbdev);
mali_error hwcnt_get_utilization_resource(struct kbase_device *kbdev);
mali_error hwcnt_get_gpr_resource(struct kbase_device *kbdev, struct kbase_uk_hwcnt_gpr_dump *dump);
extern void hwcnt_accumulate_resource(struct kbase_device *kbdev);
mali_error hwcnt_dump(struct kbase_context *kctx);
void hwcnt_start(struct kbase_device *kbdev);
void hwcnt_stop(struct kbase_device *kbdev);
mali_error hwcnt_setup(struct kbase_context *kctx, struct kbase_uk_hwcnt_setup *setup);
void exynos_hwcnt_init(struct kbase_device *kbdev);
void exynos_hwcnt_remove(struct kbase_device *kbdev);

#endif /* __GPU_HWCNT_H */

#endif
