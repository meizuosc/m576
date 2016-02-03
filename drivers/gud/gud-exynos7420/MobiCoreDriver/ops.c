/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * MobiCore Driver Kernel Module.
 *
 * This module is written as a Linux device driver.
 * This driver represents the command proxy on the lowest layer, from the
 * secure world to the non secure world, and vice versa.
 * This driver is located in the non secure world (Linux).
 * This driver offers IOCTL commands, for access to the secure world, and has
 * the interface from the secure world to the normal world.
 * The access to the driver is possible with a file descriptor,
 * which has to be created by the fd = open(/dev/mobicore) command.
 */
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/moduleparam.h>

#include <linux/interrupt.h>
#include <linux/irq.h>

#include "main.h"
#include "fastcall.h"
#include "ops.h"
#include "mem.h"
#include "pm.h"
#include "debug.h"

/* MobiCore context data */
static struct mc_context *ctx;
static int disable_local_timer;

#ifdef CONFIG_SECURE_OS_BOOSTER_API
int mc_timer(void);
#endif

#ifdef TBASE_CORE_SWITCHER
static uint32_t active_cpu;

#ifdef TEST
	/*
	 * Normal world <t-base core info for testing.
	 */

	module_param(active_cpu, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	MODULE_PARM_DESC(active_cpu, "Active <t-base Core");
#endif


static int mobicore_cpu_callback(struct notifier_block *nfb,
				 unsigned long action, void *hcpu);
static struct notifier_block mobicore_cpu_notifer = {
	.notifier_call = mobicore_cpu_callback,
};
#endif

static inline long smc(union fc_generic *fc)
{
	/* If we request sleep yields must be filtered out as they
	 * make no sense */
	if (ctx->mcp)
		if (ctx->mcp->flags.sleep_mode.sleep_req) {
			if (fc->as_in.cmd == MC_SMC_N_YIELD)
				return MC_FC_RET_ERR_INVALID;
		}
	return _smc(fc);
}

struct fastcall_work {
#ifdef MC_FASTCALL_WORKER_THREAD
	struct kthread_work work;
#else
	struct work_struct work;
#endif
	void *data;
};

#ifdef MC_FASTCALL_WORKER_THREAD
static void fastcall_work_func(struct kthread_work *work);
#else
static void fastcall_work_func(struct work_struct *work);
#endif


#ifdef MC_FASTCALL_WORKER_THREAD

static struct task_struct *fastcall_thread;
static DEFINE_KTHREAD_WORKER(fastcall_worker);

void mc_set_schedule_policy(int core)
{
	if (core == DEFAULT_BIG_CORE)
		disable_local_timer = 1;
	else
		disable_local_timer = 0;

	return;
}

bool mc_fastcall(void *data)
{
	struct fastcall_work fc_work = {
		KTHREAD_WORK_INIT(fc_work.work, fastcall_work_func),
		.data = data,
	};

	if (!queue_kthread_work(&fastcall_worker, &fc_work.work))
		return false;
	flush_kthread_work(&fc_work.work);
	return true;
}

int mc_fastcall_init(struct mc_context *context)
{
	int ret = 0;
	ctx = context;

	fastcall_thread = kthread_create(kthread_worker_fn, &fastcall_worker,
					 "mc_fastcall");
	if (IS_ERR(fastcall_thread)) {
		ret = PTR_ERR(fastcall_thread);
		fastcall_thread = NULL;
		MCDRV_DBG_ERROR(mcd, "cannot create fastcall wq (%d)", ret);
		return ret;
	}

	wake_up_process(fastcall_thread);

	/* this thread MUST run on CPU 0 at startup */
	set_cpus_allowed(fastcall_thread, CPU_MASK_CPU0);
#ifdef TBASE_CORE_SWITCHER
	register_cpu_notifier(&mobicore_cpu_notifer);
#endif
	return 0;
}

void mc_fastcall_destroy(void)
{
	if (!IS_ERR_OR_NULL(fastcall_thread)) {
		kthread_stop(fastcall_thread);
		fastcall_thread = NULL;
	}
}
#else

bool mc_fastcall(void *data)
{
	struct fastcall_work work = {
		.data = data,
	};
	INIT_WORK(&work.work, fastcall_work_func);
	if (!schedule_work_on(0, &work.work))
		return false;
	flush_work(&work.work);
	return true;
}

int mc_fastcall_init(struct mc_context *context)
{
	ctx = context;
	return 0;
};

void mc_fastcall_destroy(void) {};
#endif

#ifdef MC_FASTCALL_WORKER_THREAD
static void fastcall_work_func(struct kthread_work *work)
#else
static void fastcall_work_func(struct work_struct *work)
#endif
{
	int irq_check_cnt = 0;
	struct fastcall_work *fc_work =
		container_of(work, struct fastcall_work, work);
	union fc_generic *fc_generic = fc_work->data;
#ifdef TBASE_CORE_SWITCHER
	uint32_t cpu_swap = 0, new_cpu;
	uint32_t cpu_id[] = CPU_IDS;
#endif
	struct irq_desc *desc = irq_to_desc(MC_INTR_LOCAL_TIMER);

#ifdef MC_CRYPTO_CLOCK_MANAGEMENT
	mc_pm_clock_enable();
#endif


	if (fc_generic == NULL)
		return;
#ifdef TBASE_CORE_SWITCHER
	if (fc_generic->as_in.cmd == MC_FC_SWITCH_CORE) {
		cpu_swap = 1;
		new_cpu = fc_generic->as_in.param[0];
		fc_generic->as_in.param[0] = cpu_id[fc_generic->as_in.param[0]];
	}

	if (disable_local_timer) {
		irq_check_cnt++;
		disable_irq(MC_INTR_LOCAL_TIMER);
#ifdef CONFIG_SECURE_OS_BOOSTER_API
		mc_timer();
#endif
	}
#endif
	smc(fc_work->data);
#ifdef TBASE_CORE_SWITCHER
	if (irq_check_cnt) {
		if (desc->depth != 0)
			enable_irq(MC_INTR_LOCAL_TIMER);
	}

	if (cpu_swap) {
		if (fc_generic->as_out.ret == 0) {
			cpumask_t cpu;
			active_cpu = new_cpu;
			MCDRV_DBG(mcd, "CoreSwap ok %d -> %d\n",
				  raw_smp_processor_id(), active_cpu);
			cpumask_clear(&cpu);
			cpumask_set_cpu(active_cpu, &cpu);
#ifdef MC_FASTCALL_WORKER_THREAD
			set_cpus_allowed(fastcall_thread, cpu);
#endif
		} else {
			MCDRV_DBG(mcd, "CoreSwap failed %d -> %d\n",
				  raw_smp_processor_id(),
				  fc_generic->as_in.param[0]);
		}
	}
#endif
#ifdef MC_CRYPTO_CLOCK_MANAGEMENT
	mc_pm_clock_disable();
#endif
}

#ifdef DUMP_TBASE_HALT_STATUS
static void mc_info_ext(uint32_t ext_info_id, uint32_t *ext_info)
{
	union mc_fc_info fc_info;

	memset(&fc_info, 0, sizeof(fc_info));
	fc_info.as_in.cmd = MC_FC_INFO;
	fc_info.as_in.ext_info_id = ext_info_id;

	MCDRV_DBG(mcd, "<- cmd=0x%08x, ext_info_id=0x%08x",
		  fc_info.as_in.cmd, fc_info.as_in.ext_info_id);

	mc_fastcall(&(fc_info.as_generic));

	*ext_info = fc_info.as_out.ext_info;
}

static void mc_dump_halt_status(uint32_t *flag, uint32_t *halt_code,
		uint32_t *fault_thread, uint8_t *uuid)
{
	uint32_t ext_info;
	uint8_t ext_info_uuid[UUID_LENGTH] = {0, };

	dev_info(mcd, "Dump <t-base internal status:\n");
	mc_info_ext(MC_EXT_INFO_ID_FLAGS, &ext_info);
	dev_info(mcd, "Flag = 0x%08x\n", ext_info);
	*flag = ext_info;
	mc_info_ext(MC_EXT_INFO_ID_HALT_CODE, &ext_info);
	dev_info(mcd, "Halt code = 0x%08x\n", ext_info);
	*halt_code = ext_info;
	mc_info_ext(MC_EXT_INFO_ID_HALT_IP, &ext_info);
	dev_info(mcd, "Halt IP = 0x%08x\n", ext_info);

	mc_info_ext(MC_EXT_INFO_ID_FAULT_CNT, &ext_info);
	dev_info(mcd, "Fault counter = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_FAULT_CAUSE, &ext_info);
	dev_info(mcd, "Fault cause = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_FAULT_META, &ext_info);
	dev_info(mcd, "Fault meta = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_FAULT_THREAD, &ext_info);
	dev_info(mcd, "Fault thread = 0x%08x\n", ext_info);
	*fault_thread = ext_info;
	mc_info_ext(MC_EXT_INFO_ID_FAULT_IP, &ext_info);
	dev_info(mcd, "Fault IP = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_FAULT_SP, &ext_info);
	dev_info(mcd, "Fault SP = 0x%08x\n", ext_info);

	mc_info_ext(MC_EXT_INFO_ID_FAULT_ARCH_DFSR, &ext_info);
	dev_info(mcd, "Fault ARCH DFSR = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_FAULT_ARCH_ADFSR, &ext_info);
	dev_info(mcd, "Fault ARCH ADFSR = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_FAULT_ARCH_DFAR, &ext_info);
	dev_info(mcd, "Fault ARCH DFAR = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_FAULT_ARCH_IFSR, &ext_info);
	dev_info(mcd, "Fault ARCH IFSR = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_FAULT_ARCH_AIFSR, &ext_info);
	dev_info(mcd, "Fault ARCH AIFSR = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_FAULT_ARCH_IFAR, &ext_info);
	dev_info(mcd, "Fault ARCH IFAR = 0x%08x\n", ext_info);

	mc_info_ext(MC_EXT_INFO_ID_MC_EXC_PARTNER, &ext_info);
	dev_info(mcd, "ExcH partner = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_MC_EXC_IPCPEER, &ext_info);
	dev_info(mcd, "ExcH peer = 0x%08x\n", ext_info);
	mc_info_ext(MC_EXT_INFO_ID_MC_EXC_IPCMSG, &ext_info);
	dev_info(mcd, "ExcH msg = 0x%08x\n", ext_info);

	mc_info_ext(MC_EXT_INFO_ID_MC_EXC_UUID_0, (uint32_t *)&ext_info_uuid[0]);
	mc_info_ext(MC_EXT_INFO_ID_MC_EXC_UUID_1, (uint32_t *)&ext_info_uuid[4]);
	mc_info_ext(MC_EXT_INFO_ID_MC_EXC_UUID_2, (uint32_t *)&ext_info_uuid[8]);
	mc_info_ext(MC_EXT_INFO_ID_MC_EXC_UUID_3, (uint32_t *)&ext_info_uuid[12]);
	dev_info(mcd, "ExcH UUID = 0x%02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x%02x%02x%02x\n",
			ext_info_uuid[0], ext_info_uuid[1], ext_info_uuid[2],
			ext_info_uuid[3], ext_info_uuid[4], ext_info_uuid[5],
			ext_info_uuid[6], ext_info_uuid[7], ext_info_uuid[8],
			ext_info_uuid[9], ext_info_uuid[10], ext_info_uuid[11],
			ext_info_uuid[12], ext_info_uuid[13], ext_info_uuid[14],
			ext_info_uuid[15]);
	memcpy(uuid, ext_info_uuid, UUID_LENGTH);

	mc_info_ext(MC_EXT_INFO_ID_MC_EXC_IPCDATA, &ext_info);
	dev_info(mcd, "ExcH data = 0x%08x\n", ext_info);
}
#endif

int mc_info(uint32_t ext_info_id, uint32_t *state, uint32_t *ext_info)
{
	int ret = 0;
	union mc_fc_info fc_info;
#ifdef DUMP_TBASE_HALT_STATUS
	uint32_t flag = 0;
	uint32_t halt_code = 0;
	uint32_t fault_thread = 0;
	uint8_t uuid[UUID_LENGTH] = {0, };
#endif

	MCDRV_DBG_VERBOSE(mcd, "enter");

	memset(&fc_info, 0, sizeof(fc_info));
	fc_info.as_in.cmd = MC_FC_INFO;
	fc_info.as_in.ext_info_id = ext_info_id;

	MCDRV_DBG(mcd, "<- cmd=0x%08x, ext_info_id=0x%08x",
		  fc_info.as_in.cmd, fc_info.as_in.ext_info_id);

	mc_fastcall(&(fc_info.as_generic));

	MCDRV_DBG(mcd,
		  "-> r=0x%08x ret=0x%08x state=0x%08x ext_info=0x%08x",
		  fc_info.as_out.resp,
		  fc_info.as_out.ret,
		  fc_info.as_out.state,
		  fc_info.as_out.ext_info);

	ret = convert_fc_ret(fc_info.as_out.ret);

	*state  = fc_info.as_out.state;
	*ext_info = fc_info.as_out.ext_info;

#ifdef DUMP_TBASE_HALT_STATUS
	if ((*state == MC_STATUS_HALT) || ((ext_info_id == 1) && (*ext_info & SYS_STATE_HALT))) {
		MCDRV_DBG_ERROR(mcd, "<t-base detects a system crash at secure world.");
		mc_dump_halt_status(&flag, &halt_code, &fault_thread, uuid);
		panic("<t-base detects a system crash at secure world: "
				"Flag(0x%08x), Halt code(0x%08x), Fault thread(0x%08x), "
				"UUID(0x%02x%02x%02x%02x%02x%02x%02x%02x"
				"%02x%02x%02x%02x%02x%02x%02x%02x)\n",
				flag, halt_code, fault_thread,
				uuid[0], uuid[1], uuid[2], uuid[3],
				uuid[4], uuid[5], uuid[6], uuid[7],
				uuid[8], uuid[9], uuid[10], uuid[11],
				uuid[12], uuid[13], uuid[14], uuid[15]);
	}
#endif

	MCDRV_DBG_VERBOSE(mcd, "exit with %d/0x%08X", ret, ret);

	return ret;
}

#ifdef TBASE_CORE_SWITCHER

uint32_t mc_active_core(void)
{
	return active_cpu;
}

int mc_switch_core(uint32_t core_num)
{
	int32_t ret = 0;
	union mc_fc_swich_core fc_switch_core;

	if (!cpu_online(core_num))
		return 1;

	MCDRV_DBG_VERBOSE(mcd, "enter\n");

	memset(&fc_switch_core, 0, sizeof(fc_switch_core));
	fc_switch_core.as_in.cmd = MC_FC_SWITCH_CORE;

	if (core_num < COUNT_OF_CPUS)
		fc_switch_core.as_in.core_id = core_num;
	else
		fc_switch_core.as_in.core_id = 0;

	MCDRV_DBG(mcd,
		  "<- cmd=0x%08x, core_id=0x%08x\n",
		 fc_switch_core.as_in.cmd,
		 fc_switch_core.as_in.core_id);
	MCDRV_DBG(mcd,
		  "<- core_num=0x%08x, active_cpu=0x%08x\n",
		 core_num, active_cpu);
	mc_fastcall(&(fc_switch_core.as_generic));

	ret = convert_fc_ret(fc_switch_core.as_out.ret);

	MCDRV_DBG_VERBOSE(mcd, "exit with %d/0x%08X\n", ret, ret);

	return ret;
}

void mc_cpu_offfline(int cpu)
{
	if (active_cpu == cpu) {
		int i;
		/* Chose the first online CPU and switch! */
		for_each_online_cpu(i) {
			if (i == cpu) {
				MCDRV_DBG(mcd, "Skipping CPU %d\n", cpu);
				continue;
			}
			MCDRV_DBG(mcd, "CPU %d is dying, switching to %d\n",
				  cpu, i);
			mc_switch_core(i);
			break;
		}
	} else {
		MCDRV_DBG(mcd, "not active CPU, no action taken\n");
	}
}

static int mobicore_cpu_callback(struct notifier_block *nfb,
				unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		mc_cpu_offfline(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		break;
	}
	return NOTIFY_OK;
}
#endif

/* Yield to MobiCore */
int mc_yield(void)
{
	int ret = 0;
	union fc_generic yield;

	/* MCDRV_DBG_VERBOSE(mcd, "enter"); */
	memset(&yield, 0, sizeof(yield));
	yield.as_in.cmd = MC_SMC_N_YIELD;
	mc_fastcall(&yield);
	ret = convert_fc_ret(yield.as_out.ret);

	return ret;
}

/* call common notify */
int mc_nsiq(void)
{
	int ret = 0;
	union fc_generic nsiq;
	MCDRV_DBG_VERBOSE(mcd, "enter");
	memset(&nsiq, 0, sizeof(nsiq));
	nsiq.as_in.cmd = MC_SMC_N_SIQ;
	mc_fastcall(&nsiq);
	ret = convert_fc_ret(nsiq.as_out.ret);
	return ret;
}

/* call common notify */
int _nsiq(void)
{
	int ret = 0;
	union fc_generic nsiq;
	MCDRV_DBG_VERBOSE(mcd, "enter");
	memset(&nsiq, 0, sizeof(nsiq));
	nsiq.as_in.cmd = MC_SMC_N_SIQ;
	_smc(&nsiq);
	ret = convert_fc_ret(nsiq.as_out.ret);
	return ret;
}

/* Call the INIT fastcall to setup MobiCore initialization */
int mc_init(phys_addr_t base, uint32_t nq_length,
	uint32_t mcp_offset, uint32_t mcp_length)
{
	int ret = 0;
	union mc_fc_init fc_init;
	uint64_t base_addr = (uint64_t)base;
	uint32_t base_high = (uint32_t)(base_addr >> 32);

	MCDRV_DBG_VERBOSE(mcd, "enter");

	memset(&fc_init, 0, sizeof(fc_init));

	fc_init.as_in.cmd = MC_FC_INIT;
	/* base address of mci buffer 4KB aligned */
	fc_init.as_in.base = (uint32_t)base_addr;
	/* notification buffer start/length [16:16] [start, length] */
	fc_init.as_in.nq_info = ((base_high & 0xFFFF) << 16) |
				(nq_length & 0xFFFF);
	/* mcp buffer start/length [16:16] [start, length] */
	fc_init.as_in.mcp_info = (mcp_offset << 16) | (mcp_length & 0xFFFF);

	/*
	 * Set KMOD notification queue to start of MCI
	 * mciInfo was already set up in mmap
	 */
	MCDRV_DBG(mcd,
		  "cmd=0x%08x, base=0x%08x,nq_info=0x%08x, mcp_info=0x%08x",
		  fc_init.as_in.cmd, fc_init.as_in.base, fc_init.as_in.nq_info,
		  fc_init.as_in.mcp_info);
	mc_fastcall(&fc_init.as_generic);
	MCDRV_DBG(mcd, "out cmd=0x%08x, ret=0x%08x", fc_init.as_out.resp,
		  fc_init.as_out.ret);

	ret = convert_fc_ret(fc_init.as_out.ret);

	MCDRV_DBG_VERBOSE(mcd, "exit with %d/0x%08X", ret, ret);

	return ret;
}

/* Return MobiCore driver version */
uint32_t mc_get_version(void)
{
	MCDRV_DBG(mcd, "MobiCore driver version is %i.%i",
		  MCDRVMODULEAPI_VERSION_MAJOR,
		  MCDRVMODULEAPI_VERSION_MINOR);

	return MC_VERSION(MCDRVMODULEAPI_VERSION_MAJOR,
					MCDRVMODULEAPI_VERSION_MINOR);
}
