/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */





/**
 * @file mali_kbase_context.c
 * Base kernel context APIs
 */

#include <mali_kbase.h>
#include <mali_midg_regmap.h>

#if SLSI_INTEGRATION
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <mach/cpufreq.h>
#include <platform/mali_kbase_platform.h>
#if defined(SET_MINLOCK)
#include "platform/mali_kbase_platform.h"
#include "platform/gpu_dvfs_handler.h"
extern struct pm_qos_request exynos5_g3d_cpu_egl_min_qos;
#endif
#endif

#define MEMPOOL_PAGES 16384

/**
 * @brief Create a kernel base context.
 *
 * Allocate and init a kernel base context. Calls
 * kbase_create_os_context() to setup OS specific structures.
 */
kbase_context *kbase_create_context(kbase_device *kbdev)
{
	kbase_context *kctx;
	mali_error mali_err;
	char current_name[sizeof(current->comm)];

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	/* zero-inited as lot of code assume it's zero'ed out on create */
	kctx = vzalloc(sizeof(*kctx));

	if (!kctx)
		goto out;

	kctx->kbdev = kbdev;
	kctx->as_nr = KBASEP_AS_NR_INVALID;
#ifdef SLSI_INTEGRATION
	kctx->ctx_status = CTX_UNINITIALIZED;
#endif
#ifdef CONFIG_MALI_TRACE_TIMELINE
	kctx->timeline.owner_tgid = task_tgid_nr(current);
#endif
	atomic_set(&kctx->setup_complete, 0);
	atomic_set(&kctx->setup_in_progress, 0);
	kctx->keep_gpu_powered = MALI_FALSE;
	spin_lock_init(&kctx->mm_update_lock);
	kctx->process_mm = NULL;
	atomic_set(&kctx->nonmapped_pages, 0);
	get_task_comm(current_name, current);
	strncpy((char *)(&kctx->name), current_name, 32);

	if (MALI_ERROR_NONE != kbase_mem_allocator_init(&kctx->osalloc, MEMPOOL_PAGES))
		goto free_kctx;

	kctx->pgd_allocator = &kctx->osalloc;
	atomic_set(&kctx->used_pages, 0);
	atomic_set(&kctx->used_pmem_pages, 0);
	atomic_set(&kctx->used_tmem_pages, 0);

	if (kbase_jd_init(kctx))
		goto free_allocator;

	mali_err = kbasep_js_kctx_init(kctx);
	if (MALI_ERROR_NONE != mali_err)
		goto free_jd;	/* safe to call kbasep_js_kctx_term  in this case */

	mali_err = kbase_event_init(kctx);
	if (MALI_ERROR_NONE != mali_err)
		goto free_jd;

	mutex_init(&kctx->reg_lock);

	INIT_LIST_HEAD(&kctx->waiting_soft_jobs);
#ifdef CONFIG_KDS
	INIT_LIST_HEAD(&kctx->waiting_kds_resource);
#endif

	mali_err = kbase_mmu_init(kctx);
	if (MALI_ERROR_NONE != mali_err)
		goto free_event;

	kctx->pgd = kbase_mmu_alloc_pgd(kctx);
	if (!kctx->pgd)
		goto free_mmu;

	if (MALI_ERROR_NONE != kbase_mem_allocator_alloc(&kctx->osalloc, 1, &kctx->aliasing_sink_page))
		goto no_sink_page;
	if (kbase_create_os_context(&kctx->osctx))
		goto no_os_context;

	kctx->cookies = KBASE_COOKIE_MASK;

	/* Make sure page 0 is not used... */
	if (kbase_region_tracker_init(kctx))
		goto no_region_tracker;
#ifdef CONFIG_GPU_TRACEPOINTS
	atomic_set(&kctx->jctx.work_id, 0);
#endif
#ifdef CONFIG_MALI_TRACE_TIMELINE
	atomic_set(&kctx->timeline.jd_atoms_in_flight, 0);
#endif

#ifdef SLSI_INTEGRATION
	kctx->ctx_status = CTX_INITIALIZED;
	kctx->ctx_need_qos = false;
#endif

	/* default non-legacy */
	kctx->legacy_app = 0;

	return kctx;

no_region_tracker:
	kbase_destroy_os_context(&kctx->osctx);
no_sink_page:
	kbase_mem_allocator_free(&kctx->osalloc, 1, &kctx->aliasing_sink_page, 0);
no_os_context:
	kbase_mmu_free_pgd(kctx);
free_mmu:
	kbase_mmu_term(kctx);
free_event:
	kbase_event_cleanup(kctx);
free_jd:
	/* Safe to call this one even when didn't initialize (assuming kctx was sufficiently zeroed) */
	kbasep_js_kctx_term(kctx);
	kbase_jd_exit(kctx);
free_allocator:
	kbase_mem_allocator_term(&kctx->osalloc);
free_kctx:
    vfree(kctx);
out:
	return NULL;

}
KBASE_EXPORT_SYMBOL(kbase_create_context)

static void kbase_reg_pending_dtor(struct kbase_va_region *reg)
{
	pr_info("Freeing pending unmapped region\n");
	kbase_mem_phy_alloc_put(reg->alloc);
	kfree(reg);
}

/**
 * @brief Destroy a kernel base context.
 *
 * Destroy a kernel base context. Calls kbase_destroy_os_context() to
 * free OS specific structures. Will release all outstanding regions.
 */
void kbase_destroy_context(kbase_context *kctx)
{
	kbase_device *kbdev;
	int pages;
	unsigned long pending_regions_to_clean;
#ifdef SLSI_INTEGRATION
	struct exynos_context *platform;
	if (!kctx) {
		printk("An uninitialized or destroyed context is tried to be destroyed. kctx is null\n");
		return ;
	}
	else if (kctx->ctx_status != CTX_INITIALIZED) {
		printk("An uninitialized or destroyed context is tried to be destroyed\n");
		printk("kctx: 0x%p, kctx->osctx->tgid: %d, kctx->ctx_status: 0x%x\n", kctx, kctx ? kctx->osctx.tgid:0, kctx ? kctx->ctx_status:0);
		return ;
	}
#endif
	KBASE_DEBUG_ASSERT(NULL != kctx);

	kbdev = kctx->kbdev;
	KBASE_DEBUG_ASSERT(NULL != kbdev);

	KBASE_TRACE_ADD(kbdev, CORE_CTX_DESTROY, kctx, NULL, 0u, 0u);

	/* Ensure the core is powered up for the destroy process */
	/* A suspend won't happen here, because we're in a syscall from a userspace
	 * thread. */
	kbase_pm_context_active(kbdev);
#if SLSI_INTEGRATION
	if (kbdev->hwcnt.kctx == kctx || kbdev->hwcnt.suspended_kctx == kctx) {
#else
	if (kbdev->hwcnt.kctx == kctx) {
#endif
		/* disable the use of the hw counters if the app didn't use the API correctly or crashed */
		KBASE_TRACE_ADD(kbdev, CORE_CTX_HWINSTR_TERM, kctx, NULL, 0u, 0u);
		KBASE_DEBUG_PRINT_WARN(KBASE_CTX, "The privileged process asking for instrumentation forgot to disable it " "before exiting. Will end instrumentation for them");

#if SLSI_INTEGRATION
		if (kbdev->hwcnt.prev_mm) {
			struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

			kbdev->hwcnt.triggered = 1;
			kbdev->hwcnt.trig_exception = 1;
			wake_up(&kbdev->hwcnt.wait);

			mutex_lock(&kbdev->hwcnt.mlock);

			kbdev->hwcnt.condition_to_dump = FALSE;
			kbdev->hwcnt.enable_for_utilization = FALSE;
			kbdev->hwcnt.enable_for_gpr = FALSE;
			kbdev->hwcnt.cnt_for_stop = 0;
			kbdev->hwcnt.cnt_for_bt_start = 0;
			kbdev->hwcnt.cnt_for_bt_stop = 0;
			platform->hwcnt_bt_clk = FALSE;

			if (kbdev->hwcnt.kspace_addr) {
				kbdev->hwcnt.state = KBASE_INSTR_STATE_IDLE;
				kbase_instr_hwcnt_disable(kctx);
			}

			kbase_pm_policy_change(kbdev, 2);

			kbdev->hwcnt.suspended_kctx = NULL;

			mutex_unlock(&kbdev->hwcnt.mlock);
		} else
#endif

		kbase_instr_hwcnt_disable(kctx);
	}

#if SLSI_INTEGRATION
	else if (kbdev->hwcnt.kctx_gpr == kctx) {
		if (kbdev->hwcnt.prev_mm) {
			kbdev->hwcnt.triggered = 1;
			kbdev->hwcnt.trig_exception = 1;
			wake_up(&kbdev->hwcnt.wait);

			mutex_lock(&kbdev->hwcnt.mlock);

			if (kbdev->hwcnt.kspace_addr) {
				kbdev->hwcnt.state = KBASE_INSTR_STATE_IDLE;
				kbase_instr_hwcnt_stop(kbdev);
			}

			kbase_pm_policy_change(kbdev, 2);

			kbdev->hwcnt.condition_to_dump = FALSE;
			kbdev->hwcnt.enable_for_gpr = FALSE;
			kbdev->hwcnt.enable_for_utilization = TRUE;
			kbdev->hwcnt.kctx_gpr = NULL;

			mutex_unlock(&kbdev->hwcnt.mlock);
		}
	}
#endif

	kbase_jd_zap_context(kctx);
	kbase_event_cleanup(kctx);

	kbase_gpu_vm_lock(kctx);

	/* MMU is disabled as part of scheduling out the context */
	kbase_mmu_free_pgd(kctx);

	/* free pending region setups */
	kbase_mem_allocator_free(&kctx->osalloc, 1, &kctx->aliasing_sink_page, 0);

	/* free pending region setups */
	pending_regions_to_clean = (~kctx->cookies) & KBASE_COOKIE_MASK;
	while (pending_regions_to_clean) {
		unsigned int cookie = __ffs(pending_regions_to_clean);
		BUG_ON(!kctx->pending_regions[cookie]);

		kbase_reg_pending_dtor(kctx->pending_regions[cookie]);

		kctx->pending_regions[cookie] = NULL;
		pending_regions_to_clean &= ~(1UL << cookie);
	}

	kbase_region_tracker_term(kctx);
	kbase_destroy_os_context(&kctx->osctx);
	kbase_gpu_vm_unlock(kctx);

	/* Safe to call this one even when didn't initialize (assuming kctx was sufficiently zeroed) */
	kbasep_js_kctx_term(kctx);

	kbase_jd_exit(kctx);

	kbase_pm_context_idle(kbdev);

	kbase_mmu_term(kctx);

	pages = atomic_read(&kctx->used_pages);
	if (pages != 0)
		dev_warn(kbdev->osdev.dev, "%s: %d pages in use!\n", __func__, pages);

	if (kctx->keep_gpu_powered) {
		atomic_dec(&kbdev->keep_gpu_powered_count);
		kbase_pm_context_idle(kbdev);
	}

	kbase_mem_allocator_term(&kctx->osalloc);
	WARN_ON(atomic_read(&kctx->nonmapped_pages) != 0);
#ifdef SLSI_INTEGRATION
	kctx->ctx_status = CTX_DESTROYED;
	if (kctx->ctx_need_qos) {
		kctx->ctx_need_qos = false;
		set_hmp_boost(0);
		set_hmp_aggressive_up_migration(false);
		set_hmp_aggressive_yield(false);
#if defined(SET_MINLOCK)
		platform = (struct exynos_context *)kbdev->platform_context;
		platform->custom_cpu_max_lock = 0;
		platform->target_lock_type = BOOST_LOCK;
		gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_MIN_UNLOCK, 0);
		pm_qos_update_request(&exynos5_g3d_cpu_egl_min_qos, 0);
#endif
	}
#endif
	vfree(kctx);
#ifdef SLSI_INTEGRATION
	kctx = NULL;
#endif
}
KBASE_EXPORT_SYMBOL(kbase_destroy_context)

/**
 * Set creation flags on a context
 */
mali_error kbase_context_set_create_flags(kbase_context *kctx, u32 flags)
{
	mali_error err = MALI_ERROR_NONE;
	kbasep_js_kctx_info *js_kctx_info;
	KBASE_DEBUG_ASSERT(NULL != kctx);

	js_kctx_info = &kctx->jctx.sched_info;

	/* Validate flags */
	if (flags != (flags & BASE_CONTEXT_CREATE_KERNEL_FLAGS)) {
		err = MALI_ERROR_FUNCTION_FAILED;
		goto out;
	}

	mutex_lock(&js_kctx_info->ctx.jsctx_mutex);

	/* Translate the flags */
	if ((flags & BASE_CONTEXT_SYSTEM_MONITOR_SUBMIT_DISABLED) == 0)
		js_kctx_info->ctx.flags &= ~((u32) KBASE_CTX_FLAG_SUBMIT_DISABLED);

	if ((flags & BASE_CONTEXT_HINT_ONLY_COMPUTE) != 0)
		js_kctx_info->ctx.flags |= (u32) KBASE_CTX_FLAG_HINT_ONLY_COMPUTE;

	/* Latch the initial attributes into the Job Scheduler */
	kbasep_js_ctx_attr_set_initial_attrs(kctx->kbdev, kctx);

	mutex_unlock(&js_kctx_info->ctx.jsctx_mutex);
 out:
	return err;
}
KBASE_EXPORT_SYMBOL(kbase_context_set_create_flags)
