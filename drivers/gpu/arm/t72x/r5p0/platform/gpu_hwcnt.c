/* drivers/gpu/arm/.../platform/gpu_hwcnt.c
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
 * @file gpu_hwcnt.c
 * DVFS
 */
#ifdef SEC_HWCNT

#include <mali_kbase.h>
#include <mali_kbase_mem_linux.h>
#include "mali_kbase_platform.h"
#include "gpu_hwcnt.h"

mali_error exynos_gpu_hwcnt_update(struct kbase_device *kbdev)
{
	mali_error err = MALI_ERROR_FUNCTION_FAILED;

	KBASE_DEBUG_ASSERT(kbdev);

	if ((!kbdev->hwcnt.enable_for_utilization) ||(!kbdev->hwcnt.is_init)) {
		err = MALI_ERROR_NONE;
		goto out;
	}

	if (kbdev->js_data.runpool_irq.secure_mode == MALI_TRUE) {
		err = MALI_ERROR_NONE;
		goto out;
	}

#ifdef HWC_DUMP_DVFS_THREAD
	if (kbdev->hwcnt.is_powered && kbdev->hwcnt.kctx) {
		err = hwcnt_dump(kbdev->hwcnt.kctx);
		if (err != MALI_ERROR_NONE) {
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "hwcnt dump error in %s %d \n", __FUNCTION__, err);
			goto out;
		}
	}
#endif

	hwcnt_get_utilization_resource(kbdev);

	hwcnt_utilization_equation(kbdev);

out:
	return err;
}

void hwcnt_utilization_equation(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int total_util;
	unsigned int debug_util;

	KBASE_DEBUG_ASSERT(kbdev);

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%d, %d, %d\n",
			kbdev->hwcnt.resources.arith_words, kbdev->hwcnt.resources.ls_issues,
			kbdev->hwcnt.resources.tex_issues);

	total_util = kbdev->hwcnt.resources.arith_words * 25 +
		kbdev->hwcnt.resources.ls_issues * 40 + kbdev->hwcnt.resources.tex_issues * 35;

	debug_util = (kbdev->hwcnt.resources.arith_words << 24)
		| (kbdev->hwcnt.resources.ls_issues << 16) | (kbdev->hwcnt.resources.tex_issues << 8);

	if ((kbdev->hwcnt.resources.arith_words * 10 > kbdev->hwcnt.resources.ls_issues * 14) &&
			(kbdev->hwcnt.resources.ls_issues < 40) && (total_util > 1000) && (total_util < 4800)) {
		kbdev->hwcnt.cnt_for_bt_start++;
		kbdev->hwcnt.cnt_for_bt_stop = 0;
		if (kbdev->hwcnt.cnt_for_bt_start > 5) {
			platform->hwcnt_bt_clk = TRUE;
			kbdev->hwcnt.cnt_for_bt_start = 0;
		}
	} else {
		kbdev->hwcnt.cnt_for_bt_stop++;
		kbdev->hwcnt.cnt_for_bt_start = 0;
		if (kbdev->hwcnt.cnt_for_bt_stop > 5) {
			platform->hwcnt_bt_clk = FALSE;
			kbdev->hwcnt.cnt_for_bt_stop = 0;
		}
	}

	if (platform->hwcnt_bt_clk == TRUE)
		GPU_LOG(DVFS_INFO, LSI_HWCNT_BT_ON, 0u, debug_util, "hwcnt bt on\n");
	else
		GPU_LOG(DVFS_INFO, LSI_HWCNT_BT_OFF, 0u, debug_util, "hwcnt bt off\n");

	memset(kbdev->hwcnt.acc_buffer, 0, HWC_ACC_BUFFER_SIZE);
}

mali_error hwcnt_get_utilization_resource(struct kbase_device *kbdev)
{
	int num_cores;
	int mem_offset;
	unsigned int *acc_addr;
	u32 arith_words = 0, ls_issues = 0, tex_issues = 0, tripipe_active = 0, div_tripipe_active;
	mali_error err = MALI_ERROR_FUNCTION_FAILED;
	int i;

	if (kbdev->gpu_props.num_core_groups != 1) {
		/* num of core groups must be 1 on T76X */
		goto out;
	}

	acc_addr = (unsigned int*)kbdev->hwcnt.acc_buffer;

	num_cores = kbdev->gpu_props.num_cores;

	if (num_cores <= 4)
		mem_offset = MALI_SIZE_OF_HWCBLK * 3;
	else
		mem_offset = MALI_SIZE_OF_HWCBLK * 4;

	for (i=0; i < num_cores; i++)
	{
		if ( i == 3 && (num_cores == 5 || num_cores == 6) )
			mem_offset += MALI_SIZE_OF_HWCBLK;
		tripipe_active += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_TRIPIPE_ACTIVE);
		arith_words += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_ARITH_WORDS);
		ls_issues += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_LS_ISSUES);
		tex_issues += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_TEX_ISSUES);
	}

	div_tripipe_active = tripipe_active / 100;

	if (div_tripipe_active) {
		kbdev->hwcnt.resources.arith_words = arith_words / div_tripipe_active;
		kbdev->hwcnt.resources.ls_issues = ls_issues / div_tripipe_active;
		kbdev->hwcnt.resources.tex_issues = tex_issues / div_tripipe_active;
	}

	err = MALI_ERROR_NONE;

out:
	return err;
}

mali_error hwcnt_get_gpr_resource(struct kbase_device *kbdev, struct kbase_uk_hwcnt_gpr_dump *dump)
{
	int num_cores;
	int mem_offset;
	unsigned int* acc_addr;
	u32 shader_20 = 0, shader_21 = 0;
	mali_error err = MALI_ERROR_FUNCTION_FAILED;
	int i;

	KBASE_DEBUG_ASSERT(kbdev);

	if (kbdev->gpu_props.num_core_groups != 1) {
		/* num of core groups must be 1 on T76X */
		goto out;
	}

	num_cores = kbdev->gpu_props.num_cores;
	acc_addr = (unsigned int*)kbdev->hwcnt.acc_buffer;

	if (num_cores <= 4)
		mem_offset = MALI_SIZE_OF_HWCBLK * 3;
	else
		mem_offset = MALI_SIZE_OF_HWCBLK * 4;

	for (i=0; i < num_cores; i++)
	{
		if ( i == 3 && (num_cores == 5 || num_cores == 6) )
			mem_offset += MALI_SIZE_OF_HWCBLK;
		shader_20 += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_SHADER_20);
		shader_21 += *(acc_addr + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_SHADER_21);
	}

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "[%d] [%d]\n", shader_20, shader_21);

	dump->shader_20 = shader_20;
	dump->shader_21 = shader_21;

	memset(kbdev->hwcnt.acc_buffer, 0, HWC_ACC_BUFFER_SIZE);
	err = MALI_ERROR_NONE;

out:
	return err;
}

void hwcnt_accumulate_resource(struct kbase_device *kbdev)
{
	unsigned int *addr, *acc_addr;
	int i;

	KBASE_DEBUG_ASSERT(kbdev);

	if ((!kbdev->hwcnt.kctx) || (!kbdev->hwcnt.acc_buffer))
		return;

	addr = (unsigned int *)kbdev->hwcnt.kspace_addr;
	acc_addr = (unsigned int*)kbdev->hwcnt.acc_buffer;

	/* following copy code will be optimized soon */
	for (i=0; i < HWC_ACC_BUFFER_SIZE / 4; i++)
	{
		*(acc_addr + i) += *(addr + i);
	}

	/* wondering why following buffer clear code is required */
	memset(kbdev->hwcnt.kspace_addr, 0, HWC_ACC_BUFFER_SIZE);
}

mali_error hwcnt_dump(struct kbase_context *kctx)
{
	mali_error err = MALI_ERROR_FUNCTION_FAILED;
	struct kbase_device *kbdev;

	if (kctx == NULL) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "kctx is NULL error in %s %d \n", __FUNCTION__, err);
		goto out;
	}

	kbdev = kctx->kbdev;

	if ((!kbdev->hwcnt.kctx) || (!kbdev->hwcnt.is_init) || (!kbdev->hwcnt.is_powered )) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "hwcnt condition error in %s %d \n", __FUNCTION__, err);
		err = MALI_ERROR_NONE;
		goto out;
	}

	err = kbase_instr_hwcnt_dump(kbdev->hwcnt.kctx);

	if (err != MALI_ERROR_NONE) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "kbase_instr_hwcnt_dump error in %s %d \n", __FUNCTION__, err);
		goto out;
	}

	err = kbase_instr_hwcnt_clear(kbdev->hwcnt.kctx);

	if (err != MALI_ERROR_NONE) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "kbase_instr_hwcnt_clear error in %s %d \n", __FUNCTION__, err);
		goto out;
	}

	hwcnt_accumulate_resource(kbdev);

	err = MALI_ERROR_NONE;

 out:
	return err;
}

void hwcnt_start(struct kbase_device *kbdev)
{
	struct kbase_context *kctx;
	KBASE_DEBUG_ASSERT(kbdev);

	if (kbdev->hwcnt.suspended_kctx) {
		kctx = kbdev->hwcnt.suspended_kctx;
		kbdev->hwcnt.suspended_kctx = NULL;
	} else {
		kctx = kbdev->hwcnt.kctx;
		if (kctx == NULL)
		{
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "hwcnt exception!!!! Both suspended_ktctx and kctx are NULL\n");
			BUG();
		}
	}

	if (kctx) {
		mali_error err;
		err = kbase_instr_hwcnt_enable_internal_sec(kbdev, kctx, &kbdev->hwcnt.suspended_state, false);

		if (err != MALI_ERROR_NONE)
		{
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "Failed to restore instrumented hardware counters on resume\n");
			kbdev->hwcnt.kctx = kctx;
		}
	}
}

void hwcnt_stop(struct kbase_device *kbdev)
{
	struct kbase_context *kctx;
	KBASE_DEBUG_ASSERT(kbdev);

	kctx = kbdev->hwcnt.kctx;

	if (kctx == NULL)
	{
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "hwcnt exception!!!! kctx is NULL\n");
		return;
	}

	kbdev->hwcnt.suspended_kctx = kctx;

	/* Relevant state was saved into hwcnt.suspended_state when enabling the
	 * counters */

	if (kctx)
	{
		KBASE_DEBUG_ASSERT(kctx->jctx.sched_info.ctx.flags & KBASE_CTX_FLAG_PRIVILEGED);
		kbase_instr_hwcnt_disable_sec(kctx);
	}
}

/**
 * @brief Configure HW counters collection
 */
mali_error hwcnt_setup(struct kbase_context *kctx, struct kbase_uk_hwcnt_setup *setup)
{
	mali_error err = MALI_ERROR_FUNCTION_FAILED;
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	KBASE_DEBUG_ASSERT(NULL != kctx);

	kbdev = kctx->kbdev;
	KBASE_DEBUG_ASSERT(NULL != kbdev);

	if (NULL == setup) {
		/* Bad parameter - abort */
		goto out;
	}

	platform = (struct exynos_context *) kbdev->platform_context;

	if ((!platform->hwcnt_gathering_status) && (!platform->hwcnt_gpr_status)) {
		err = MALI_ERROR_NONE;
		goto out;
	}

	if ((platform->hwcnt_gpr_status) && (setup->padding == HWC_MODE_GPR_EN)) {

		if (!kbdev->hwcnt.is_init) {
			err = MALI_ERROR_NONE;
			goto out;
		}

		mutex_lock(&kbdev->hwcnt.mlock);

		kbdev->hwcnt.s_enable_for_utilization = kbdev->hwcnt.enable_for_utilization;
		kbdev->hwcnt.enable_for_utilization = FALSE;
		kbdev->hwcnt.enable_for_gpr = TRUE;
		kbdev->hwcnt.kctx_gpr = kctx;
		platform->hwcnt_bt_clk = 0;

		KBASE_TRACE_ADD_EXYNOS(kbdev, LSI_HWCNT_ON_GPR, NULL, NULL, 0u, (long unsigned int)kbdev->hwcnt.kspace_addr);

		err = MALI_ERROR_NONE;

		mutex_unlock(&kbdev->hwcnt.mlock);
		goto out;
	} else if ((platform->hwcnt_gpr_status) && (setup->padding == HWC_MODE_GPR_DIS)) {
		if (!kbdev->hwcnt.is_init) {
			err = MALI_ERROR_NONE;
			goto out;
		}

		mutex_lock(&kbdev->hwcnt.mlock);

		KBASE_TRACE_ADD_EXYNOS(kbdev, LSI_HWCNT_OFF_GPR, NULL, NULL, 0u, (long unsigned int)kbdev->hwcnt.kspace_addr);

		kbdev->hwcnt.enable_for_gpr = FALSE;
		kbdev->hwcnt.enable_for_utilization = kbdev->hwcnt.s_enable_for_utilization;
		kbdev->hwcnt.kctx_gpr = NULL;
		platform->hwcnt_bt_clk = 0;
		err = MALI_ERROR_NONE;

		mutex_unlock(&kbdev->hwcnt.mlock);
		goto out;
	}

	if ((setup->dump_buffer != 0ULL) && (setup->padding == HWC_MODE_UTILIZATION)) {
		err = kbase_instr_hwcnt_enable_internal_sec(kbdev, kctx, setup, true);
		mutex_lock(&kbdev->hwcnt.mlock);

		if (kbdev->hwcnt.kctx)
			hwcnt_stop(kbdev);

		kbdev->hwcnt.enable_for_utilization = TRUE;
		platform->hwcnt_bt_clk = 0;
		kbdev->hwcnt.cnt_for_stop = 0;
		kbdev->hwcnt.cnt_for_bt_stop = 0;
		mutex_unlock(&kbdev->hwcnt.mlock);
	}

out:
	return err;
}

void exynos_hwcnt_init(struct kbase_device *kbdev)
{
	struct kbase_uk_hwcnt_setup setup_arg;
	struct kbase_context *kctx;
	struct kbase_uk_mem_alloc mem;
	struct kbase_va_region *reg;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (platform->hwcnt_gathering_status == false)
		goto out;

	kctx = kbase_create_context(kbdev, false);

	if (kctx) {
		kbdev->hwcnt.kctx = kctx;
	} else {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "hwcnt error!, hwcnt_init is failed\n");
		goto out;
	}

	mem.va_pages = mem.commit_pages = mem.extent = 1;
	mem.flags = BASE_MEM_PROT_GPU_WR | BASE_MEM_PROT_CPU_RD | BASE_MEM_HINT_CPU_RD;

	reg = kbase_mem_alloc(kctx, mem.va_pages, mem.commit_pages, mem.extent, &mem.flags, &mem.gpu_va, &mem.va_alignment);

#if defined(CONFIG_64BIT)
	kbase_gpu_vm_lock(kctx);
	if (MALI_ERROR_NONE != kbase_gpu_mmap(kctx, reg, 0, 1, 1)) {
		kbase_gpu_vm_unlock(kctx);
		platform->hwcnt_gathering_status = false;
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "exynos_hwcnt_init error!mmap fail\n");
		kbase_mem_free(kbdev->hwcnt.kctx, kbdev->hwcnt.suspended_state.dump_buffer);
		goto out;
	}
	kbase_gpu_vm_unlock(kctx);
#endif

	kctx->kbdev->hwcnt.phy_addr = reg->alloc->pages[0];
	kctx->kbdev->hwcnt.enable_for_utilization = FALSE;
	kctx->kbdev->hwcnt.enable_for_gpr = FALSE;
	kctx->kbdev->hwcnt.suspended_kctx = NULL;
	kctx->kbdev->hwcnt.timeout = msecs_to_jiffies(100);
	kctx->kbdev->hwcnt.is_powered = FALSE;
	mutex_init(&kbdev->hwcnt.mlock);

#if defined(CONFIG_64BIT)
	setup_arg.dump_buffer = reg->start_pfn << PAGE_SHIFT;
#else
	setup_arg.dump_buffer = mem.gpu_va;
#endif
	setup_arg.jm_bm =  platform->hwcnt_choose_jm;
	setup_arg.shader_bm = platform->hwcnt_choose_shader;
	setup_arg.tiler_bm =  platform->hwcnt_choose_tiler;
	setup_arg.l3_cache_bm =  platform->hwcnt_choose_l3_cache;
	setup_arg.mmu_l2_bm =  platform->hwcnt_choose_mmu_l2;
	setup_arg.padding = HWC_MODE_UTILIZATION;

	kctx->kbdev->hwcnt.kspace_addr = kbase_kmap_from_physical_address(kbdev);

	if (MALI_ERROR_NONE != hwcnt_setup(kctx, &setup_arg)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "hwcnt_setup is failed\n");
		goto out;
	}

	kctx->kbdev->hwcnt.acc_buffer = kmalloc(HWC_ACC_BUFFER_SIZE, GFP_KERNEL);

	if (kctx->kbdev->hwcnt.acc_buffer)
		memset(kctx->kbdev->hwcnt.acc_buffer, 0, HWC_ACC_BUFFER_SIZE);
	else
		goto out;

	kbdev->hwcnt.is_init = TRUE;
	if(kbdev->pm.pm_current_policy->id == KBASE_PM_POLICY_ID_ALWAYS_ON) {
		mutex_lock(&kbdev->hwcnt.mlock);
		if (!kbdev->hwcnt.kctx)
			hwcnt_start(kbdev);
		mutex_unlock(&kbdev->hwcnt.mlock);
	}
	return;
out:
	kbdev->hwcnt.is_init = FALSE;
	return;
}

void exynos_hwcnt_remove(struct kbase_device *kbdev)
{
	struct exynos_context *platform;

	if (!kbdev->hwcnt.is_init)
		return;

	if (kbdev->hwcnt.kctx && kbdev->hwcnt.suspended_state.dump_buffer)
		kbase_mem_free(kbdev->hwcnt.kctx, kbdev->hwcnt.suspended_state.dump_buffer);

	if (kbdev->hwcnt.acc_buffer)
		kfree(kbdev->hwcnt.acc_buffer);

	platform = (struct exynos_context *) kbdev->platform_context;

	kbdev->hwcnt.enable_for_gpr = FALSE;
	kbdev->hwcnt.enable_for_utilization = FALSE;
	kbdev->hwcnt.kctx_gpr = NULL;
	kbdev->hwcnt.kctx = NULL;
	kbdev->hwcnt.is_init = FALSE;
	platform->hwcnt_bt_clk = 0;

	if (kbdev->hwcnt.kspace_addr) {
		kbase_kunmap_from_physical_address(kbdev);
		kbdev->hwcnt.kspace_addr = 0;
	}

	mutex_destroy(&kbdev->hwcnt.mlock);
}
#endif
