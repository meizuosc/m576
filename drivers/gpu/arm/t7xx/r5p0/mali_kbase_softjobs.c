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





#include <mali_kbase.h>

#include <linux/dma-mapping.h>
#ifdef CONFIG_SYNC
#ifdef MALI_SEC_FENCE_INTEGRATION
#include <../../../../staging/android/sync.h>
#endif
#include <linux/syscalls.h>
#include "mali_kbase_sync.h"
#endif


/* Mask to check cache alignment of data structures */
#define KBASE_CACHE_ALIGNMENT_MASK		((1<<L1_CACHE_SHIFT)-1)

/**
 * @file mali_kbase_softjobs.c
 *
 * This file implements the logic behind software only jobs that are
 * executed within the driver rather than being handed over to the GPU.
 */

#ifdef MALI_SEC_FENCE_INTEGRATION
static void kbase_fence_timer_init(struct kbase_jd_atom *katom);
static void kbase_fence_del_timer(struct kbase_jd_atom *katom);
#endif
static int kbase_dump_cpu_gpu_time(struct kbase_jd_atom *katom)
{
	struct kbase_va_region *reg;
	phys_addr_t addr = 0;
	u64 pfn;
	u32 offset;
	char *page;
	struct timespec ts;
	struct base_dump_cpu_gpu_counters data;
	u64 system_time;
	u64 cycle_counter;
	mali_addr64 jc = katom->jc;
	struct kbase_context *kctx = katom->kctx;
	int pm_active_err;

	u32 hi1, hi2;

	memset(&data, 0, sizeof(data));

	/* Take the PM active reference as late as possible - otherwise, it could
	 * delay suspend until we process the atom (which may be at the end of a
	 * long chain of dependencies */
	pm_active_err = kbase_pm_context_active_handle_suspend(kctx->kbdev, KBASE_PM_SUSPEND_HANDLER_DONT_REACTIVATE);
	if (pm_active_err) {
		struct kbasep_js_device_data *js_devdata = &kctx->kbdev->js_data;

		/* We're suspended - queue this on the list of suspended jobs
		 * Use dep_item[1], because dep_item[0] is in use for 'waiting_soft_jobs' */
		mutex_lock(&js_devdata->runpool_mutex);
		list_add_tail(&katom->dep_item[1], &js_devdata->suspended_soft_jobs_list);
		mutex_unlock(&js_devdata->runpool_mutex);

		return pm_active_err;
	}

	kbase_pm_request_gpu_cycle_counter(kctx->kbdev);

	/* Read hi, lo, hi to ensure that overflow from lo to hi is handled correctly */
	do {
		hi1 = kbase_reg_read(kctx->kbdev, GPU_CONTROL_REG(CYCLE_COUNT_HI), NULL);
		cycle_counter = kbase_reg_read(kctx->kbdev, GPU_CONTROL_REG(CYCLE_COUNT_LO), NULL);
		hi2 = kbase_reg_read(kctx->kbdev, GPU_CONTROL_REG(CYCLE_COUNT_HI), NULL);
		cycle_counter |= (((u64) hi1) << 32);
	} while (hi1 != hi2);

	/* Read hi, lo, hi to ensure that overflow from lo to hi is handled correctly */
	do {
		hi1 = kbase_reg_read(kctx->kbdev, GPU_CONTROL_REG(TIMESTAMP_HI), NULL);
		system_time = kbase_reg_read(kctx->kbdev, GPU_CONTROL_REG(TIMESTAMP_LO), NULL);
		hi2 = kbase_reg_read(kctx->kbdev, GPU_CONTROL_REG(TIMESTAMP_HI), NULL);
		system_time |= (((u64) hi1) << 32);
	} while (hi1 != hi2);

	/* Record the CPU's idea of current time */
	getrawmonotonic(&ts);

	kbase_pm_release_gpu_cycle_counter(kctx->kbdev);

	kbase_pm_context_idle(kctx->kbdev);

	data.sec = ts.tv_sec;
	data.usec = ts.tv_nsec / 1000;
	data.system_time = system_time;
	data.cycle_counter = cycle_counter;

	pfn = jc >> PAGE_SHIFT;
	offset = jc & ~PAGE_MASK;

	/* Assume this atom will be cancelled until we know otherwise */
	katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;
	if (offset > 0x1000 - sizeof(data)) {
		/* Wouldn't fit in the page */
		return 0;
	}

	kbase_gpu_vm_lock(kctx);
	reg = kbase_region_tracker_find_region_enclosing_address(kctx, jc);
	if (reg &&
	    (reg->flags & KBASE_REG_GPU_WR) &&
	    reg->alloc && reg->alloc->pages)
		addr = reg->alloc->pages[pfn - reg->start_pfn];

	kbase_gpu_vm_unlock(kctx);
	if (!addr)
		return 0;

	page = kmap(pfn_to_page(PFN_DOWN(addr)));
	if (!page)
		return 0;

	dma_sync_single_for_cpu(katom->kctx->kbdev->dev,
			kbase_dma_addr(pfn_to_page(PFN_DOWN(addr))) +
			offset, sizeof(data),
			DMA_BIDIRECTIONAL);
	memcpy(page + offset, &data, sizeof(data));
	dma_sync_single_for_device(katom->kctx->kbdev->dev,
			kbase_dma_addr(pfn_to_page(PFN_DOWN(addr))) +
			offset, sizeof(data),
			DMA_BIDIRECTIONAL);
	kunmap(pfn_to_page(PFN_DOWN(addr)));

	/* Atom was fine - mark it as done */
	katom->event_code = BASE_JD_EVENT_DONE;

	return 0;
}

#ifdef CONFIG_SYNC

/* Complete an atom that has returned '1' from kbase_process_soft_job (i.e. has waited)
 *
 * @param katom     The atom to complete
 */
static void complete_soft_job(struct kbase_jd_atom *katom)
{
	struct kbase_context *kctx = katom->kctx;

/* MALI_SEC_INTEGRATION */
	struct list_head *entry = (struct list_head *)&katom->dep_item[0];

	mutex_lock(&kctx->jctx.lock);
/* MALI_SEC_INTEGRATION */
	/* Do not delete from list if item was removed already */
	if (!(entry->prev == LIST_POISON2 || entry->next == LIST_POISON1))

	list_del(&katom->dep_item[0]);
	kbase_finish_soft_job(katom);
	if (jd_done_nolock(katom))
		kbasep_js_try_schedule_head_ctx(kctx->kbdev);
	mutex_unlock(&kctx->jctx.lock);
}

static enum base_jd_event_code kbase_fence_trigger(struct kbase_jd_atom *katom, int result)
{
	struct sync_pt *pt;
	struct sync_timeline *timeline;

	if (!list_is_singular(&katom->fence->pt_list_head)) {
		/* Not exactly one item in the list - so it didn't (directly) come from us */
		return BASE_JD_EVENT_JOB_CANCELLED;
	}

	pt = list_first_entry(&katom->fence->pt_list_head, struct sync_pt, pt_list);
	timeline = pt->parent;

	if (!kbase_sync_timeline_is_ours(timeline)) {
		/* Fence has a sync_pt which isn't ours! */
		return BASE_JD_EVENT_JOB_CANCELLED;
	}

	/* MALI_SEC_INTEGRATION */
	kbase_sync_signal_pt(pt, 0);

	sync_timeline_signal(timeline);

	return (result < 0) ? BASE_JD_EVENT_JOB_CANCELLED : BASE_JD_EVENT_DONE;
}

static void kbase_fence_wait_worker(struct work_struct *data)
{
	struct kbase_jd_atom *katom;
	struct kbase_context *kctx;

	katom = container_of(data, struct kbase_jd_atom, work);
	kctx = katom->kctx;

#ifdef MALI_SEC_FENCE_INTEGRATION
#ifdef KBASE_FENCE_TIMEOUT_FAKE_SIGNAL
	if (katom->fence && katom->fence->status == 0) {
		/* means that it comes via kbase_fence_timeout*/
		pr_info("kbase_fence_wait_worker cancel async fence[%p]\n", katom->fence);
		if (sync_fence_cancel_async(katom->fence, &katom->sync_waiter) != 0) {
			mutex_unlock(&katom->fence_mt);
			return;
		}
	}
#endif
#endif
	complete_soft_job(katom);
}

static void kbase_fence_wait_callback(struct sync_fence *fence, struct sync_fence_waiter *waiter)
{
	struct kbase_jd_atom *katom = container_of(waiter, struct kbase_jd_atom, sync_waiter);
	struct kbase_context *kctx;

	KBASE_DEBUG_ASSERT(NULL != katom);

	kctx = katom->kctx;

	KBASE_DEBUG_ASSERT(NULL != kctx);

	/* Propagate the fence status to the atom.
	 * If negative then cancel this atom and its dependencies.
	 */
	if (fence->status < 0)
		katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;

	/* To prevent a potential deadlock we schedule the work onto the job_done_wq workqueue
	 *
	 * The issue is that we may signal the timeline while holding kctx->jctx.lock and
	 * the callbacks are run synchronously from sync_timeline_signal. So we simply defer the work.
	 */

	KBASE_DEBUG_ASSERT(0 == object_is_on_stack(&katom->work));
	INIT_WORK(&katom->work, kbase_fence_wait_worker);
	queue_work(kctx->jctx.job_done_wq, &katom->work);
}

static int kbase_fence_wait(struct kbase_jd_atom *katom)
{
	int ret;

	KBASE_DEBUG_ASSERT(NULL != katom);
	KBASE_DEBUG_ASSERT(NULL != katom->kctx);

	sync_fence_waiter_init(&katom->sync_waiter, kbase_fence_wait_callback);

	ret = sync_fence_wait_async(katom->fence, &katom->sync_waiter);

	if (ret == 1) {
		/* Already signalled */
		return 0;
	} else if (ret < 0) {
		goto cancel_atom;
	}
#ifdef MALI_SEC_FENCE_INTEGRATION
	kbase_fence_timer_init(katom);
#endif
	return 1;

 cancel_atom:
	katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;
	/* We should cause the dependant jobs in the bag to be failed,
	 * to do this we schedule the work queue to complete this job */
	KBASE_DEBUG_ASSERT(0 == object_is_on_stack(&katom->work));
	INIT_WORK(&katom->work, kbase_fence_wait_worker);
	queue_work(katom->kctx->jctx.job_done_wq, &katom->work);
	return 1;
}

static void kbase_fence_cancel_wait(struct kbase_jd_atom *katom)
{
	if (sync_fence_cancel_async(katom->fence, &katom->sync_waiter) != 0) {
		/* The wait wasn't cancelled - leave the cleanup for kbase_fence_wait_callback */
		return;
	}

	/* Wait was cancelled - zap the atoms */
	katom->event_code = BASE_JD_EVENT_JOB_CANCELLED;

	kbase_finish_soft_job(katom);

	if (jd_done_nolock(katom))
		kbasep_js_try_schedule_head_ctx(katom->kctx->kbdev);
}
#endif /* CONFIG_SYNC */

int kbase_process_soft_job(struct kbase_jd_atom *katom)
{
	switch (katom->core_req & BASEP_JD_REQ_ATOM_TYPE) {
	case BASE_JD_REQ_SOFT_DUMP_CPU_GPU_TIME:
		return kbase_dump_cpu_gpu_time(katom);
#ifdef CONFIG_SYNC
	case BASE_JD_REQ_SOFT_FENCE_TRIGGER:
		KBASE_DEBUG_ASSERT(katom->fence != NULL);
		katom->event_code = kbase_fence_trigger(katom, katom->event_code == BASE_JD_EVENT_DONE ? 0 : -EFAULT);
		/* Release the reference as we don't need it any more */
		sync_fence_put(katom->fence);
		katom->fence = NULL;
		break;
	case BASE_JD_REQ_SOFT_FENCE_WAIT:
		return kbase_fence_wait(katom);
#endif				/* CONFIG_SYNC */
	case BASE_JD_REQ_SOFT_REPLAY:
		return kbase_replay_process(katom);
	}

	/* Atom is complete */
	return 0;
}

void kbase_cancel_soft_job(struct kbase_jd_atom *katom)
{
/* MALI_SEC_INTEGRATION */
	pgd_t *pgd;
	struct mm_struct *mm = katom->kctx->process_mm;

	pgd = pgd_offset(mm, (unsigned long)katom);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		printk("Abnormal katom\n");
		printk("katom->kctx: 0x%p, katom->kctx->tgid: %d, katom->kctx->process_mm: 0x%p, pgd: 0x%px\n", katom->kctx, katom->kctx->tgid, katom->kctx->process_mm, pgd);
		return;
	}

	switch (katom->core_req & BASEP_JD_REQ_ATOM_TYPE) {
#ifdef CONFIG_SYNC
	case BASE_JD_REQ_SOFT_FENCE_WAIT:
		kbase_fence_cancel_wait(katom);
		break;
#endif
	default:
		/* This soft-job doesn't support cancellation! */
		KBASE_DEBUG_ASSERT(0);
	}
}

mali_error kbase_prepare_soft_job(struct kbase_jd_atom *katom)
{
	switch (katom->core_req & BASEP_JD_REQ_ATOM_TYPE) {
	case BASE_JD_REQ_SOFT_DUMP_CPU_GPU_TIME:
		{
			if (0 != (katom->jc & KBASE_CACHE_ALIGNMENT_MASK))
				return MALI_ERROR_FUNCTION_FAILED;
		}
		break;
#ifdef CONFIG_SYNC
	case BASE_JD_REQ_SOFT_FENCE_TRIGGER:
		{
			struct base_fence fence;
			int fd;

			if (0 != copy_from_user(&fence, (__user void *)(uintptr_t) katom->jc, sizeof(fence)))
				return MALI_ERROR_FUNCTION_FAILED;

			fd = kbase_stream_create_fence(fence.basep.stream_fd);
			if (fd < 0)
				return MALI_ERROR_FUNCTION_FAILED;

			katom->fence = sync_fence_fdget(fd);

			if (katom->fence == NULL) {
				/* The only way the fence can be NULL is if userspace closed it for us.
				 * So we don't need to clear it up */
				return MALI_ERROR_FUNCTION_FAILED;
			}
			fence.basep.fd = fd;
			if (0 != copy_to_user((__user void *)(uintptr_t) katom->jc, &fence, sizeof(fence))) {
				katom->fence = NULL;
				sys_close(fd);
				return MALI_ERROR_FUNCTION_FAILED;
			}
		}
		break;
	case BASE_JD_REQ_SOFT_FENCE_WAIT:
		{
			struct base_fence fence;

			if (0 != copy_from_user(&fence, (__user void *)(uintptr_t) katom->jc, sizeof(fence)))
				return MALI_ERROR_FUNCTION_FAILED;

			/* Get a reference to the fence object */
			katom->fence = sync_fence_fdget(fence.basep.fd);
			if (katom->fence == NULL)
				return MALI_ERROR_FUNCTION_FAILED;
		}
		break;
#endif				/* CONFIG_SYNC */
	case BASE_JD_REQ_SOFT_REPLAY:
		break;
	default:
		/* Unsupported soft-job */
		return MALI_ERROR_FUNCTION_FAILED;
	}
	return MALI_ERROR_NONE;
}

void kbase_finish_soft_job(struct kbase_jd_atom *katom)
{
	switch (katom->core_req & BASEP_JD_REQ_ATOM_TYPE) {
	case BASE_JD_REQ_SOFT_DUMP_CPU_GPU_TIME:
		/* Nothing to do */
		break;
#ifdef CONFIG_SYNC
	case BASE_JD_REQ_SOFT_FENCE_TRIGGER:
		if (katom->fence) {
			/* The fence has not yet been signalled, so we do it now */
			kbase_fence_trigger(katom, katom->event_code == BASE_JD_EVENT_DONE ? 0 : -EFAULT);
			sync_fence_put(katom->fence);
			katom->fence = NULL;
		}
		break;
	case BASE_JD_REQ_SOFT_FENCE_WAIT:
		/* Release the reference to the fence object */
#ifdef MALI_SEC_FENCE_INTEGRATION
		if (katom->fence) {
			sync_fence_put(katom->fence);
			katom->fence = NULL;
		}
		kbase_fence_del_timer(katom);
#else
		sync_fence_put(katom->fence);
		katom->fence = NULL;
#endif
		break;
#endif				/* CONFIG_SYNC */
	}
}

void kbase_resume_suspended_soft_jobs(struct kbase_device *kbdev)
{
	LIST_HEAD(local_suspended_soft_jobs);
	struct kbase_jd_atom *tmp_iter;
	struct kbase_jd_atom *katom_iter;
	struct kbasep_js_device_data *js_devdata;
	mali_bool resched = MALI_FALSE;

	KBASE_DEBUG_ASSERT(kbdev);

	js_devdata = &kbdev->js_data;

	/* Move out the entire list */
	mutex_lock(&js_devdata->runpool_mutex);
	list_splice_init(&js_devdata->suspended_soft_jobs_list, &local_suspended_soft_jobs);
	mutex_unlock(&js_devdata->runpool_mutex);

	/* Each atom must be detached from the list and ran separately - it could
	 * be re-added to the old list, but this is unlikely */
	list_for_each_entry_safe(katom_iter, tmp_iter, &local_suspended_soft_jobs, dep_item[1])
	{
		struct kbase_context *kctx = katom_iter->kctx;

		mutex_lock(&kctx->jctx.lock);

		/* Remove from the global list */
		list_del(&katom_iter->dep_item[1]);
		/* Remove from the context's list of waiting soft jobs */
		list_del(&katom_iter->dep_item[0]);

		if (kbase_process_soft_job(katom_iter) == 0) {
			kbase_finish_soft_job(katom_iter);
			resched |= jd_done_nolock(katom_iter);
		} else {
			/* The job has not completed */
			KBASE_DEBUG_ASSERT((katom_iter->core_req & BASEP_JD_REQ_ATOM_TYPE)
					!= BASE_JD_REQ_SOFT_REPLAY);
			list_add_tail(&katom_iter->dep_item[0], &kctx->waiting_soft_jobs);
		}

		mutex_unlock(&kctx->jctx.lock);
	}

	if (resched)
		kbasep_js_try_schedule_head_ctx(kbdev);
}

#ifdef MALI_SEC_FENCE_INTEGRATION
#define KBASE_FENCE_TIMEOUT 1000
#define DUMP_CHUNK 256

#ifdef KBASE_FENCE_DUMP
static const char *kbase_sync_status_str(int status)
{
	if (status > 0)
		return "signaled";
	else if (status == 0)
		return "active";
	else
		return "error";
}

static void kbase_sync_print_pt(struct seq_file *s, struct sync_pt *pt, bool fence)
{
	int status;

	if (pt == NULL)
		return;
	status = pt->status;

	seq_printf(s, "  %s%spt %s",
		   fence ? pt->parent->name : "",
		   fence ? "_" : "",
		   kbase_sync_status_str(status));
	if (pt->status) {
		struct timeval tv = ktime_to_timeval(pt->timestamp);
		seq_printf(s, "@%ld.%06ld", tv.tv_sec, tv.tv_usec);
	}

	if (pt->parent->ops->timeline_value_str &&
	    pt->parent->ops->pt_value_str) {
		char value[64];
		pt->parent->ops->pt_value_str(pt, value, sizeof(value));
		seq_printf(s, ": %s", value);
		if (fence) {
			pt->parent->ops->timeline_value_str(pt->parent, value,
						    sizeof(value));
			seq_printf(s, " / %s", value);
		}
	} else if (pt->parent->ops->print_pt) {
		seq_printf(s, ": ");
		pt->parent->ops->print_pt(s, pt);
	}

	seq_printf(s, "\n");
}

static void kbase_fence_print(struct seq_file *s, struct sync_fence *fence)
{
	struct list_head *pos;
	unsigned long flags;

	seq_printf(s, "[%p] %s: %s\n", fence, fence->name,
		   kbase_sync_status_str(fence->status));

	list_for_each(pos, &fence->pt_list_head) {
		struct sync_pt *pt =
			container_of(pos, struct sync_pt, pt_list);
		kbase_sync_print_pt(s, pt, true);
	}

	spin_lock_irqsave(&fence->waiter_list_lock, flags);
	list_for_each(pos, &fence->waiter_list_head) {
		struct sync_fence_waiter *waiter =
			container_of(pos, struct sync_fence_waiter,
				     waiter_list);

		if (waiter)
			seq_printf(s, "waiter %pF\n", waiter->callback);
	}
	spin_unlock_irqrestore(&fence->waiter_list_lock, flags);
}

static char kbase_sync_dump_buf[64 * 1024];
static void kbase_fence_dump(struct sync_fence *fence)
{
	int i;
	struct seq_file s = {
		.buf = kbase_sync_dump_buf,
		.size = sizeof(kbase_sync_dump_buf) - 1,
	};

	kbase_fence_print(&s, fence);
	for (i = 0; i < s.count; i += DUMP_CHUNK) {
		if ((s.count - i) > DUMP_CHUNK) {
			char c = s.buf[i + DUMP_CHUNK];
			s.buf[i + DUMP_CHUNK] = 0;
			pr_cont("%s", s.buf + i);
			s.buf[i + DUMP_CHUNK] = c;
		} else {
			s.buf[s.count] = 0;
			pr_cont("%s", s.buf + i);
		}
	}
}
#endif

static void kbase_fence_timeout(unsigned long data)
{
	struct kbase_jd_atom *katom = (struct kbase_jd_atom *)data;
	KBASE_DEBUG_ASSERT(NULL != katom);

	if (katom == NULL || katom->fence == NULL)
		return;

	if (katom->fence->status != 0) {
		kbase_fence_del_timer(katom);
		return;
	}
	pr_info("Release fence is not signaled on [%p] for %d ms\n", katom->fence, KBASE_FENCE_TIMEOUT);

#ifdef KBASE_FENCE_DUMP
	kbase_fence_dump(katom->fence);
#endif
#ifdef KBASE_FENCE_TIMEOUT_FAKE_SIGNAL
	if (katom->fence)
		kbase_fence_wait_callback(katom->fence, &katom->sync_waiter);
#endif
	return;
}

static void kbase_fence_timer_init(struct kbase_jd_atom *katom)
{
	const u32 timeout = msecs_to_jiffies(KBASE_FENCE_TIMEOUT);

	KBASE_DEBUG_ASSERT(NULL != katom);
	if (katom == NULL)
		return;

	init_timer(&katom->fence_timer);
	katom->fence_timer.function = kbase_fence_timeout;
	katom->fence_timer.data = (unsigned long)katom;
	katom->fence_timer.expires = jiffies + timeout;

	add_timer(&katom->fence_timer);
	return;
}

static void kbase_fence_del_timer(struct kbase_jd_atom *katom)
{
	KBASE_DEBUG_ASSERT(NULL != katom);
	if (katom == NULL)
		return;

	if (katom->fence_timer.function == kbase_fence_timeout)
		del_timer(&katom->fence_timer);
	katom->fence_timer.function = NULL;
	return;
}
#endif
