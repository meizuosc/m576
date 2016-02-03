/*
 * OF idle states parsing code.
 *
 * Copyright (C) 2014 ARM Ltd.
 * Author: Lorenzo Pieralisi <lorenzo.pieralisi@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "of_idle_states.h"

struct state_elem {
	struct list_head list;
	struct device_node *node;
	int val;
};

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
static struct list_head head[2] __initdata;
static u32 drv_count;

static int init_idle_state_list(struct device_node *idle_states)
{
	u32 i;

	if (of_property_read_u32(idle_states, "multiple_drv_count", &drv_count))
		return -EINVAL;

	for (i = 0; i < drv_count; i++)
		INIT_LIST_HEAD(head + i);

	return 0;
}
#else
static struct list_head head __initdata = LIST_HEAD_INIT(head);
#endif

static bool __init state_cpu_valid(struct device_node *state_node,
				   struct device_node *cpu_node)
{
	int i = 0;
	struct device_node *cpu_state;

	while ((cpu_state = of_parse_phandle(cpu_node,
					     "cpu-idle-states", i++))) {
		if (cpu_state && state_node == cpu_state) {
			of_node_put(cpu_state);
			return true;
		}
		of_node_put(cpu_state);
	}
	return false;
}

static bool __init state_cpus_valid(const cpumask_t *cpus,
				    struct device_node *state_node)
{
	int cpu;
	struct device_node *cpu_node;

	/*
	 * Check if state is valid on driver cpumask cpus
	 */
	for_each_cpu(cpu, cpus) {
		cpu_node = of_get_cpu_node(cpu, NULL);

		if (!cpu_node) {
			pr_err("Missing device node for CPU %d\n", cpu);
			return false;
		}

		if (!state_cpu_valid(state_node, cpu_node))
			continue;
	}

	return true;
}

static int __init state_cmp(void *priv, struct list_head *a,
			    struct list_head *b)
{
	struct state_elem *ela, *elb;

	ela = container_of(a, struct state_elem, list);
	elb = container_of(b, struct state_elem, list);

	return ela->val - elb->val;
}

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
static int __init add_state_node(cpumask_t *cpumask,
				 struct device_node *state_node)
{
	struct state_elem *el;
	struct device_node *np;
	u32 val;

	pr_debug(" * %s...\n", state_node->full_name);

	if (!state_cpus_valid(cpumask, state_node))
		return -EINVAL;

	/*
	 * For supporting multi-cluster, each sleep mode(C1, C2, CPD, LPM)
	 * has parameters more two in DT. For example,
	 * 1) one parmeter
	 * CPU_SLEEP_1: C1 {
	 *	compatible = "arm,idle-state";
	 *	CLUSTER0_C1 {
	 *		...
	 *	};
	 * };
	 * 2) two parameters
	 * CPU_SLEEP_1: C1 {
	 *	compatible = "arm,idle-state";
	 *	CLUSTER0_C1 {
	 *		...
	 *	};
	 *	CLUSTER1_C1 {
	 *		...
	 *	};
	 * };
	 */
	for_each_child_of_node(state_node, np) {
		u32 cluster;

		if (of_property_read_u32(np, "min-residency-us", &val))
			return -EINVAL;

		if (of_property_read_u32(np, "cluster", &cluster))
			return -EINVAL;

		el = kmalloc(sizeof(struct state_elem), GFP_KERNEL);
		if (!el) {
			pr_err("%s failed to allocate memory\n", __func__);
			return -ENOMEM;
		}

		el->node = np;
		el->val = val;
		/* TODO: head1 -> head */
		list_add_tail(&el->list, &head[cluster]);
	}

	return 0;
}

#else
static int __init add_state_node(cpumask_t *cpumask,
				 struct device_node *state_node)
{
	struct state_elem *el;
	u32 val;

	pr_debug(" * %s...\n", state_node->full_name);

	if (!state_cpus_valid(cpumask, state_node))
		return -EINVAL;
	/*
	 * Parse just the value required to sort the states.
	 */
	if (of_property_read_u32(state_node, "min-residency-us",
				 &val)) {
		pr_debug(" * %s missing min-residency-us property\n",
			 state_node->full_name);
		return -EINVAL;
	}

	el = kmalloc(sizeof(*el), GFP_KERNEL);
	if (!el) {
		pr_err("%s failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	el->node = state_node;
	el->val = val;
	list_add_tail(&el->list, &head);

	return 0;
}
#endif

static void __init init_state_node(struct cpuidle_driver *drv,
				   struct device_node *state_node,
				   int *cnt)
{
	struct cpuidle_state *idle_state;
	const char *desc, *status;

	pr_debug(" * %s...\n", state_node->full_name);

	idle_state = &drv->states[*cnt];

	if (of_property_read_string(state_node, "status", &status)) {
		pr_debug(" * %s missing status property\n",
			     state_node->full_name);
		return;
	}

	idle_state->disabled = strcmp(status, "disabled") ? 0 : 1;

	if (of_property_read_u32(state_node, "exit-latency-us",
				 &idle_state->exit_latency)) {
		pr_debug(" * %s missing exit-latency-us property\n",
			     state_node->full_name);
		return;
	}

	if (of_property_read_u32(state_node, "min-residency-us",
				 &idle_state->target_residency)) {
		pr_debug(" * %s missing min-residency-us property\n",
			     state_node->full_name);
		return;
	}

	if (of_property_read_string(state_node, "desc", &desc)) {
		pr_debug(" * %s missing desc property\n",
				state_node->full_name);
		return;
	}

	/*
	 * It is unknown to the idle driver if and when the tick_device
	 * loses context when the CPU enters the idle states. To solve
	 * this issue the tick device must be linked to a power domain
	 * so that the idle driver can check on which states the device
	 * loses its context. Current code takes the conservative choice
	 * of defining the idle state as one where the tick device always
	 * loses its context. On platforms where tick device never loses
	 * its context (ie it is not a C3STOP device) this turns into
	 * a nop. On platforms where the tick device does lose context in some
	 * states, this code can be optimized, when power domain specifications
	 * for ARM CPUs are finalized.
	 */
	idle_state->flags = CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_TIMER_STOP;

	strncpy(idle_state->name, state_node->name, sizeof(idle_state->name));
	strncpy(idle_state->desc, desc, sizeof(idle_state->desc));

	(*cnt)++;
}

static int __init init_idle_states(struct cpuidle_driver *drv,
				   struct device_node *state_nodes[],
				   unsigned int start_idx, bool init_nodes)
{
	struct state_elem *el;
	struct list_head *curr, *tmp;
	unsigned int cnt = start_idx;
	__maybe_unused int i = 0;

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
	/* TODO : how cluster?? */
	if (cpumask_first(drv->cpumask) == 0)
		i = 1;
#endif

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
	list_for_each_entry(el, &head[i], list) {
#else
	list_for_each_entry(el, &head, list) {
#endif
		/*
		 * Check if the init function has to fill the
		 * state_nodes array on behalf of the CPUidle driver.
		 */
		if (init_nodes)
			state_nodes[cnt] = el->node;
		/*
		 * cnt is updated on return if a state was added.
		 */
		init_state_node(drv, el->node, &cnt);

		if (cnt == CPUIDLE_STATE_MAX) {
			pr_warn("State index reached static CPU idle state limit\n");
			break;
		}
	}

	drv->state_count = cnt;

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
	list_for_each_safe(curr, tmp, &head[i]) {
#else
	list_for_each_safe(curr, tmp, &head) {
#endif
		list_del(curr);
		kfree(container_of(curr, struct state_elem, list));
	}

	/*
	 * If no idle states are detected, return an error and let the idle
	 * driver initialization fail accordingly.
	 */
	return (cnt > start_idx) ? 0 : -ENODATA;
}

static void __init add_idle_states(struct cpuidle_driver *drv,
				   struct device_node *idle_states)
{
	struct device_node *state_node;
	__maybe_unused int i;

	for_each_child_of_node(idle_states, state_node) {
		if ((!of_device_is_compatible(state_node, "arm,idle-state"))) {
			pr_warn(" * %s: children of /cpus/idle-states must be \"arm,idle-state\" compatible\n",
				     state_node->full_name);
			continue;
		}
		/*
		 * If memory allocation fails, better bail out.
		 * Initialized nodes are freed at initialization
		 * completion in of_init_idle_driver().
		 */
		if ((add_state_node(drv->cpumask, state_node) == -ENOMEM))
			break;
	}
	/*
	 * Sort the states list before initializing the CPUidle driver
	 * states array.
	 */
#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
	for (i = 0; i < drv_count; i++)
		list_sort(NULL, &head[i], state_cmp);
#else
	list_sort(NULL, &head, state_cmp);
#endif
}

/*
 * of_init_idle_driver - Parse the DT idle states and initialize the
 *			 idle driver states array
 *
 * @drv:	  Pointer to CPU idle driver to be initialized
 * @state_nodes:  Array of struct device_nodes to be initialized if
 *		  init_nodes == true. Must be sized CPUIDLE_STATE_MAX
 * @start_idx:    First idle state index to be initialized
 * @init_nodes:   Boolean to request device nodes initialization
 *
 * Returns:
 *	0 on success
 *	<0 on failure
 *
 *	On success the states array in the cpuidle driver contains
 *	initialized entries in the states array, starting from index start_idx.
 *	If init_nodes == true, on success the state_nodes array is initialized
 *	with idle state DT node pointers, starting from index start_idx,
 *	in a 1:1 relation with the idle driver states array.
 */
int __init of_init_idle_driver(struct cpuidle_driver *drv,
			       struct device_node *state_nodes[],
			       unsigned int start_idx, bool init_nodes)
{
	struct device_node *idle_states_node;
	int ret;

	if (start_idx >= CPUIDLE_STATE_MAX) {
		pr_warn("State index exceeds static CPU idle driver states array size\n");
		return -EINVAL;
	}

	if (WARN(init_nodes && !state_nodes,
		"Requested nodes stashing in an invalid nodes container\n"))
		return -EINVAL;

	idle_states_node = of_find_node_by_path("/cpus/idle-states");
	if (!idle_states_node)
		return -ENOENT;

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS
	if (init_idle_state_list(idle_states_node))
		return -EINVAL;
#endif

	add_idle_states(drv, idle_states_node);

	ret = init_idle_states(drv, state_nodes, start_idx, init_nodes);

	of_node_put(idle_states_node);

	return ret;
}
