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
#include <mali_kbase_config.h>
#include <mali_kbase_jm.h>

/*
 * Private functions follow
 */

/**
 * @brief Check whether a ctx has a certain attribute, and if so, retain that
 * attribute on the runpool.
 *
 * Requires:
 * - jsctx mutex
 * - runpool_irq spinlock
 * - ctx is scheduled on the runpool
 *
 * @return MALI_TRUE indicates a change in ctx attributes state of the runpool.
 * In this state, the scheduler might be able to submit more jobs than
 * previously, and so the caller should ensure kbasep_js_try_run_next_job_nolock()
 * or similar is called sometime later.
 * @return MALI_FALSE indicates no change in ctx attributes state of the runpool.
 */
STATIC mali_bool kbasep_js_ctx_attr_runpool_retain_attr(struct kbase_device *kbdev, struct kbase_context *kctx, enum kbasep_js_ctx_attr attribute)
{
	struct kbasep_js_device_data *js_devdata;
	struct kbasep_js_kctx_info *js_kctx_info;
	mali_bool runpool_state_changed = MALI_FALSE;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(kctx != NULL);
	KBASE_DEBUG_ASSERT(attribute < KBASEP_JS_CTX_ATTR_COUNT);
	js_devdata = &kbdev->js_data;
	js_kctx_info = &kctx->jctx.sched_info;

	BUG_ON(!mutex_is_locked(&js_kctx_info->ctx.jsctx_mutex));
	lockdep_assert_held(&kbdev->js_data.runpool_irq.lock);

	KBASE_DEBUG_ASSERT(js_kctx_info->ctx.is_scheduled != MALI_FALSE);

	if (kbasep_js_ctx_attr_is_attr_on_ctx(kctx, attribute) != MALI_FALSE) {
		KBASE_DEBUG_ASSERT(js_devdata->runpool_irq.ctx_attr_ref_count[attribute] < S8_MAX);
		++(js_devdata->runpool_irq.ctx_attr_ref_count[attribute]);

		if (js_devdata->runpool_irq.ctx_attr_ref_count[attribute] == 1) {
			/* First refcount indicates a state change */
			runpool_state_changed = MALI_TRUE;
			KBASE_TRACE_ADD(kbdev, JS_CTX_ATTR_NOW_ON_RUNPOOL, kctx, NULL, 0u, attribute);
		}
	}

	return runpool_state_changed;
}

/**
 * @brief Check whether a ctx has a certain attribute, and if so, release that
 * attribute on the runpool.
 *
 * Requires:
 * - jsctx mutex
 * - runpool_irq spinlock
 * - ctx is scheduled on the runpool
 *
 * @return MALI_TRUE indicates a change in ctx attributes state of the runpool.
 * In this state, the scheduler might be able to submit more jobs than
 * previously, and so the caller should ensure kbasep_js_try_run_next_job_nolock()
 * or similar is called sometime later.
 * @return MALI_FALSE indicates no change in ctx attributes state of the runpool.
 */
STATIC mali_bool kbasep_js_ctx_attr_runpool_release_attr(struct kbase_device *kbdev, struct kbase_context *kctx, enum kbasep_js_ctx_attr attribute)
{
	struct kbasep_js_device_data *js_devdata;
	struct kbasep_js_kctx_info *js_kctx_info;
	mali_bool runpool_state_changed = MALI_FALSE;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(kctx != NULL);
	KBASE_DEBUG_ASSERT(attribute < KBASEP_JS_CTX_ATTR_COUNT);
	js_devdata = &kbdev->js_data;
	js_kctx_info = &kctx->jctx.sched_info;

	BUG_ON(!mutex_is_locked(&js_kctx_info->ctx.jsctx_mutex));
	lockdep_assert_held(&kbdev->js_data.runpool_irq.lock);
	KBASE_DEBUG_ASSERT(js_kctx_info->ctx.is_scheduled != MALI_FALSE);

	if (kbasep_js_ctx_attr_is_attr_on_ctx(kctx, attribute) != MALI_FALSE) {
		KBASE_DEBUG_ASSERT(js_devdata->runpool_irq.ctx_attr_ref_count[attribute] > 0);
		--(js_devdata->runpool_irq.ctx_attr_ref_count[attribute]);

		if (js_devdata->runpool_irq.ctx_attr_ref_count[attribute] == 0) {
			/* Last de-refcount indicates a state change */
			runpool_state_changed = MALI_TRUE;
			KBASE_TRACE_ADD(kbdev, JS_CTX_ATTR_NOW_OFF_RUNPOOL, kctx, NULL, 0u, attribute);
		}
	}

	return runpool_state_changed;
}

/**
 * @brief Retain a certain attribute on a ctx, also retaining it on the runpool
 * if the context is scheduled.
 *
 * Requires:
 * - jsctx mutex
 * - If the context is scheduled, then runpool_irq spinlock must also be held
 *
 * @return MALI_TRUE indicates a change in ctx attributes state of the runpool.
 * This may allow the scheduler to submit more jobs than previously.
 * @return MALI_FALSE indicates no change in ctx attributes state of the runpool.
 */
STATIC mali_bool kbasep_js_ctx_attr_ctx_retain_attr(struct kbase_device *kbdev, struct kbase_context *kctx, enum kbasep_js_ctx_attr attribute)
{
	struct kbasep_js_kctx_info *js_kctx_info;
	mali_bool runpool_state_changed = MALI_FALSE;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(kctx != NULL);
	KBASE_DEBUG_ASSERT(attribute < KBASEP_JS_CTX_ATTR_COUNT);
	js_kctx_info = &kctx->jctx.sched_info;

	BUG_ON(!mutex_is_locked(&js_kctx_info->ctx.jsctx_mutex));
	KBASE_DEBUG_ASSERT(js_kctx_info->ctx.ctx_attr_ref_count[attribute] < U32_MAX);

	++(js_kctx_info->ctx.ctx_attr_ref_count[attribute]);

	if (js_kctx_info->ctx.is_scheduled != MALI_FALSE && js_kctx_info->ctx.ctx_attr_ref_count[attribute] == 1) {
		lockdep_assert_held(&kbdev->js_data.runpool_irq.lock);
		/* Only ref-count the attribute on the runpool for the first time this contexts sees this attribute */
		KBASE_TRACE_ADD(kbdev, JS_CTX_ATTR_NOW_ON_CTX, kctx, NULL, 0u, attribute);
		runpool_state_changed = kbasep_js_ctx_attr_runpool_retain_attr(kbdev, kctx, attribute);
	}

	return runpool_state_changed;
}

/**
 * @brief Release a certain attribute on a ctx, also releasing it from the runpool
 * if the context is scheduled.
 *
 * Requires:
 * - jsctx mutex
 * - If the context is scheduled, then runpool_irq spinlock must also be held
 *
 * @return MALI_TRUE indicates a change in ctx attributes state of the runpool.
 * This may allow the scheduler to submit more jobs than previously.
 * @return MALI_FALSE indicates no change in ctx attributes state of the runpool.
 */
STATIC mali_bool kbasep_js_ctx_attr_ctx_release_attr(struct kbase_device *kbdev, struct kbase_context *kctx, enum kbasep_js_ctx_attr attribute)
{
	struct kbasep_js_kctx_info *js_kctx_info;
	mali_bool runpool_state_changed = MALI_FALSE;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(kctx != NULL);
	KBASE_DEBUG_ASSERT(attribute < KBASEP_JS_CTX_ATTR_COUNT);
	js_kctx_info = &kctx->jctx.sched_info;

	BUG_ON(!mutex_is_locked(&js_kctx_info->ctx.jsctx_mutex));
	KBASE_DEBUG_ASSERT(js_kctx_info->ctx.ctx_attr_ref_count[attribute] > 0);

	if (js_kctx_info->ctx.is_scheduled != MALI_FALSE && js_kctx_info->ctx.ctx_attr_ref_count[attribute] == 1) {
		lockdep_assert_held(&kbdev->js_data.runpool_irq.lock);
		/* Only de-ref-count the attribute on the runpool when this is the last ctx-reference to it */
		runpool_state_changed = kbasep_js_ctx_attr_runpool_release_attr(kbdev, kctx, attribute);
		KBASE_TRACE_ADD(kbdev, JS_CTX_ATTR_NOW_OFF_CTX, kctx, NULL, 0u, attribute);
	}

	/* De-ref must happen afterwards, because kbasep_js_ctx_attr_runpool_release() needs to check it too */
	--(js_kctx_info->ctx.ctx_attr_ref_count[attribute]);

	return runpool_state_changed;
}

STATIC void atom_prio_fast_start_callback(struct kbase_device *kbdev,
		struct kbase_jd_atom *enumerated_katom, int slot, void *private)
{
	struct kbase_jd_atom *target_katom = (struct kbase_jd_atom *)private;
	base_jd_core_req target_frag_bit = target_katom->core_req & BASE_JD_REQ_FS;
	base_jd_core_req enumerated_frag_bit = enumerated_katom->core_req & BASE_JD_REQ_FS;

	KBASE_DEBUG_ASSERT(target_katom);

	/* Only stopping atoms from the same context as the target */
	if (target_katom->kctx != enumerated_katom->kctx)
		return;

	/* Only stopping atoms of the same type */
	if (!DEFAULT_ATOM_PRIORITY_BLOCKS_ENTIRE_GPU &&
			target_frag_bit != enumerated_frag_bit)
		return;

	/* Check target has higher prio than enumerated */
	if (target_katom->sched_priority < enumerated_katom->sched_priority)
		kbase_job_slot_softstop(kbdev, slot, enumerated_katom);
}

/**
 * Handle priority of atoms within a context: ensure atoms at a lower priority
 * level are soft-stopped.
 */
STATIC void kbasep_js_ctx_attr_try_fast_start_atom(struct kbase_device *kbdev,
		struct kbase_context *kctx, struct kbase_jd_atom *katom)
{
	struct kbasep_js_kctx_info *js_kctx_info;
	enum kbasep_js_ctx_attr priority_ctx_attr;
	int prio_test_level;

	KBASE_DEBUG_ASSERT(kbdev);
	KBASE_DEBUG_ASSERT(kctx);
	KBASE_DEBUG_ASSERT(katom);

	js_kctx_info = &kctx->jctx.sched_info;

	lockdep_assert_held(&kbdev->js_data.runpool_irq.lock);

	/* We don't need to do any soft-stopping when the context is not
	 * already scheduled - none of its atoms are running (lower priority or
	 * otherwise). As soon as it is scheduled we'll make sure to pick the
	 * highest priority atoms anyway. */
	if (!js_kctx_info->ctx.is_scheduled)
		return;

	priority_ctx_attr = kbasep_js_ctx_attr_sched_prio_to_attr(katom->core_req,
			katom->sched_priority);

	/* We only need to soft-stop atoms (of this type) at a lower priority
	 * level when the first new higher priority atom (of this type) is
	 * added. This is because we can then guarantee future atoms (of this
	 * type) of this higher priority level will always run next until
	 * they're all complete.
	 *
	 * If an atom with an even higher priority level occurs later, it might
	 * need to soft-stop running atoms, but again, only when the first new
	 * atom of the highest priority level being added. */
	if (kbasep_js_ctx_attr_count_on_ctx(kctx, priority_ctx_attr) != 1)
		return;

	/* Check priorities highest to lowest for this atom type, starting at
	 * the next lowest level after this atom's priority */
	for (prio_test_level = katom->sched_priority + 1;
	     prio_test_level <= KBASE_JS_ATOM_SCHED_PRIO_MAX;
	     ++prio_test_level) {
		enum kbasep_js_ctx_attr ctx_attr_test_level = kbasep_js_ctx_attr_sched_prio_to_attr(katom->core_req,
				prio_test_level);

		/* Try next priority level if no atoms of this type at the test level */
		if (!kbasep_js_ctx_attr_is_attr_on_ctx(kctx, ctx_attr_test_level))
			continue;

		/* Sweep the currently running atoms for those:
		 * a) from this context
		 * b) of this type (frag vs non-frag)
		 * c) that are lower priority than the current atom */
		kbase_jm_enumerate_running_atoms_locked(kbdev,
				&atom_prio_fast_start_callback, katom);

		/* No need to enumerate lower levels still, we'll have
		 * already stopped them if they're there */
		break;
	}
}


/*
 * More commonly used public functions
 */

void kbasep_js_ctx_attr_set_initial_attrs(struct kbase_device *kbdev, struct kbase_context *kctx)
{
	struct kbasep_js_kctx_info *js_kctx_info;
	mali_bool runpool_state_changed = MALI_FALSE;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(kctx != NULL);
	js_kctx_info = &kctx->jctx.sched_info;

	if ((js_kctx_info->ctx.flags & KBASE_CTX_FLAG_SUBMIT_DISABLED) != MALI_FALSE) {
		/* This context never submits, so don't track any scheduling attributes */
		return;
	}

	/* Transfer attributes held in the context flags for contexts that have submit enabled */

	if ((js_kctx_info->ctx.flags & KBASE_CTX_FLAG_HINT_ONLY_COMPUTE) != MALI_FALSE) {
		/* Compute context */
		runpool_state_changed |= kbasep_js_ctx_attr_ctx_retain_attr(kbdev, kctx, KBASEP_JS_CTX_ATTR_COMPUTE);
	}
	/* NOTE: Whether this is a non-compute context depends on the jobs being
	 * run, e.g. it might be submitting jobs with BASE_JD_REQ_ONLY_COMPUTE */

	/* ... More attributes can be added here ... */

	/* The context should not have been scheduled yet, so ASSERT if this caused
	 * runpool state changes (note that other threads *can't* affect the value
	 * of runpool_state_changed, due to how it's calculated) */
	KBASE_DEBUG_ASSERT(runpool_state_changed == MALI_FALSE);
	CSTD_UNUSED(runpool_state_changed);
}

void kbasep_js_ctx_attr_runpool_retain_ctx(struct kbase_device *kbdev, struct kbase_context *kctx)
{
	mali_bool runpool_state_changed;
	int i;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(kctx != NULL);

	lockdep_assert_held(&kctx->jctx.sched_info.ctx.jsctx_mutex);
	lockdep_assert_held(&kbdev->js_data.runpool_mutex);
	lockdep_assert_held(&kbdev->js_data.runpool_irq.lock);

	/* Retain any existing attributes */
	for (i = 0; i < KBASEP_JS_CTX_ATTR_COUNT; ++i) {
		if (kbasep_js_ctx_attr_is_attr_on_ctx(kctx, (enum kbasep_js_ctx_attr) i) != MALI_FALSE) {
			/* The context is being scheduled in, so update the runpool with the new attributes */
			runpool_state_changed = kbasep_js_ctx_attr_runpool_retain_attr(kbdev, kctx, (enum kbasep_js_ctx_attr) i);

			/* We don't need to know about state changed, because retaining a
			 * context occurs on scheduling it, and that itself will also try
			 * to run new atoms */
			CSTD_UNUSED(runpool_state_changed);
		}
	}
}

mali_bool kbasep_js_ctx_attr_runpool_release_ctx(struct kbase_device *kbdev, struct kbase_context *kctx)
{
	mali_bool runpool_state_changed = MALI_FALSE;
	int i;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(kctx != NULL);

	lockdep_assert_held(&kctx->jctx.sched_info.ctx.jsctx_mutex);
	lockdep_assert_held(&kbdev->js_data.runpool_mutex);
	lockdep_assert_held(&kbdev->js_data.runpool_irq.lock);

	/* Release any existing attributes */
	for (i = 0; i < KBASEP_JS_CTX_ATTR_COUNT; ++i) {
		if (kbasep_js_ctx_attr_is_attr_on_ctx(kctx, (enum kbasep_js_ctx_attr) i) != MALI_FALSE) {
			/* The context is being scheduled out, so update the runpool on the removed attributes */
			runpool_state_changed |= kbasep_js_ctx_attr_runpool_release_attr(kbdev, kctx, (enum kbasep_js_ctx_attr) i);
		}
	}

	return runpool_state_changed;
}

void kbasep_js_ctx_attr_ctx_retain_atom(struct kbase_device *kbdev, struct kbase_context *kctx, struct kbase_jd_atom *katom)
{
	mali_bool runpool_state_changed = MALI_FALSE;
	base_jd_core_req core_req;
	enum kbasep_js_ctx_attr prio_attr;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(kctx != NULL);

	lockdep_assert_held(&kctx->jctx.sched_info.ctx.jsctx_mutex);
	lockdep_assert_held(&kbdev->js_data.runpool_mutex);
	lockdep_assert_held(&kbdev->js_data.runpool_irq.lock);

	KBASE_DEBUG_ASSERT(katom);
	core_req = katom->core_req;

	if (core_req & BASE_JD_REQ_ONLY_COMPUTE)
		runpool_state_changed |= kbasep_js_ctx_attr_ctx_retain_attr(kbdev, kctx, KBASEP_JS_CTX_ATTR_COMPUTE);
	else
		runpool_state_changed |= kbasep_js_ctx_attr_ctx_retain_attr(kbdev, kctx, KBASEP_JS_CTX_ATTR_NON_COMPUTE);

	if ((core_req & (BASE_JD_REQ_CS | BASE_JD_REQ_ONLY_COMPUTE | BASE_JD_REQ_T)) != 0 && (core_req & (BASE_JD_REQ_COHERENT_GROUP | BASE_JD_REQ_SPECIFIC_COHERENT_GROUP)) == 0) {
		/* Atom that can run on slot1 or slot2, and can use all cores */
		runpool_state_changed |= kbasep_js_ctx_attr_ctx_retain_attr(kbdev, kctx, KBASEP_JS_CTX_ATTR_COMPUTE_ALL_CORES);
	}

	/* Atom priority for frag/non-frag propagated to ctx attr */
	prio_attr = kbasep_js_ctx_attr_sched_prio_to_attr(core_req,
			katom->sched_priority);
	runpool_state_changed |= kbasep_js_ctx_attr_ctx_retain_attr(kbdev, kctx,
			prio_attr);

	/* Attempt fast start when this atom is higher priority than others */
	kbasep_js_ctx_attr_try_fast_start_atom(kbdev, kctx, katom);

	/* We don't need to know about state changed, because retaining an
	 * atom occurs on adding it, and that itself will also try to run
	 * new atoms */
	CSTD_UNUSED(runpool_state_changed);
}

mali_bool kbasep_js_ctx_attr_ctx_release_atom(struct kbase_device *kbdev, struct kbase_context *kctx, struct kbasep_js_atom_retained_state *katom_retained_state)
{
	mali_bool runpool_state_changed = MALI_FALSE;
	base_jd_core_req core_req;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(kctx != NULL);

	lockdep_assert_held(&kctx->jctx.sched_info.ctx.jsctx_mutex);
	lockdep_assert_held(&kbdev->js_data.runpool_mutex);
	lockdep_assert_held(&kbdev->js_data.runpool_irq.lock);

	KBASE_DEBUG_ASSERT(katom_retained_state);
	core_req = katom_retained_state->core_req;

	/* No-op for invalid atoms */
	if (kbasep_js_atom_retained_state_is_valid(katom_retained_state) == MALI_FALSE)
		return MALI_FALSE;

	if (core_req & BASE_JD_REQ_ONLY_COMPUTE) {
#if KBASE_PM_EN
		unsigned long flags;
		int device_nr = (core_req & BASE_JD_REQ_SPECIFIC_COHERENT_GROUP) ? katom_retained_state->device_nr : 0;
		KBASE_DEBUG_ASSERT(device_nr < 2);

		spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
		kbasep_pm_record_job_status(kbdev);
		kbdev->pm.metrics.active_cl_ctx[device_nr]--;
		spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
#endif
		runpool_state_changed |= kbasep_js_ctx_attr_ctx_release_attr(kbdev, kctx, KBASEP_JS_CTX_ATTR_COMPUTE);
	} else {
#if KBASE_PM_EN
		unsigned long flags;

		spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
		kbasep_pm_record_job_status(kbdev);
		kbdev->pm.metrics.active_gl_ctx--;
		spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
#endif
		runpool_state_changed |= kbasep_js_ctx_attr_ctx_release_attr(kbdev, kctx, KBASEP_JS_CTX_ATTR_NON_COMPUTE);
	}

	if ((core_req & (BASE_JD_REQ_CS | BASE_JD_REQ_ONLY_COMPUTE | BASE_JD_REQ_T)) != 0 && (core_req & (BASE_JD_REQ_COHERENT_GROUP | BASE_JD_REQ_SPECIFIC_COHERENT_GROUP)) == 0) {
		/* Atom that can run on slot1 or slot2, and can use all cores */
		runpool_state_changed |= kbasep_js_ctx_attr_ctx_release_attr(kbdev, kctx, KBASEP_JS_CTX_ATTR_COMPUTE_ALL_CORES);
	}

	/* Atom priority removed from ctx attr */
	{
		enum kbasep_js_ctx_attr priority_attr;
		priority_attr = kbasep_js_ctx_attr_sched_prio_to_attr(katom_retained_state->core_req,
				katom_retained_state->sched_priority );
		runpool_state_changed |= kbasep_js_ctx_attr_ctx_release_attr(kbdev, kctx,
				priority_attr);

		/* Special case state changed: if this is the last atom
		 * priority level in use on *this context* - not just when this
		 * is the last atom priority level in use throughout the
		 * runpool.
		 *
		 * This can be used by the scheduler to check whether it should
		 * start atoms from *this* context* at the next lowest
		 * priority */
		if (!kbasep_js_ctx_attr_is_attr_on_ctx(kctx, priority_attr))
			runpool_state_changed |= MALI_TRUE;
	}

	return runpool_state_changed;
}
