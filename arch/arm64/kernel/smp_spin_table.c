/*
 * Spin Table SMP initialisation
 *
 * Copyright (C) 2013 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/smp.h>

#include <asm/cacheflush.h>
#include <asm/cpu_ops.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>

#include <asm/io.h>
#include <mach/regs-pmu.h>

extern void secondary_holding_pen(void);
volatile unsigned long secondary_holding_pen_release = INVALID_HWID;

static phys_addr_t cpu_release_addr[NR_CPUS];

/*
 * Write secondary_holding_pen_release in a way that is guaranteed to be
 * visible to all observers, irrespective of whether they're taking part
 * in coherency or not.  This is necessary for the hotplug code to work
 * reliably.
 */
static void write_pen_release(u64 val)
{
	void *start = (void *)&secondary_holding_pen_release;
	unsigned long size = sizeof(secondary_holding_pen_release);

	secondary_holding_pen_release = val;
	__flush_dcache_area(start, size);
}

#if defined(CONFIG_SOC_EXYNOS7580)
static void secondary_power_up(unsigned int cpu)
{
}
#else
static void secondary_power_up(unsigned int cpu)
{
	void __iomem *addr;
	unsigned int val = 0x000f000f;

	switch (cpu) {
		case 1: addr = EXYNOS_PMU_APOLLO_CPU1_CONFIGURATION; break;
		case 2: addr = EXYNOS_PMU_APOLLO_CPU2_CONFIGURATION; break;
		case 3: addr = EXYNOS_PMU_APOLLO_CPU3_CONFIGURATION; break;
	}
	__raw_writel(val, addr);
}
#endif
static int smp_spin_table_cpu_init(struct device_node *dn, unsigned int cpu)
{
	/*
	 * Determine the address from which the CPU is polling.
	 */
	if (of_property_read_u64(dn, "cpu-release-addr",
				 &cpu_release_addr[cpu])) {
		pr_err("CPU %d: missing or invalid cpu-release-addr property\n",
		       cpu);

		return -1;
	}

	return 0;
}

static int smp_spin_table_cpu_prepare(unsigned int cpu)
{
	void __iomem *release_addr;

	if (!cpu_release_addr[cpu])
		return -ENODEV;

	release_addr = ioremap(cpu_release_addr[cpu], SZ_4K);

	/*
	 * We write the release address as LE regardless of the native
	 * endianess of the kernel. Therefore, any boot-loaders that
	 * read this address need to convert this address to the
	 * boot-loader's endianess before jumping. This is mandated by
	 * the boot protocol.
	 */
	__raw_writel(cpu_to_le64(__pa(secondary_holding_pen)), release_addr);

	__flush_dcache_area(release_addr, SZ_4K);

	secondary_power_up(cpu);

	/*
	 * Send an event to wake up the secondary CPU.
	 */
	sev();

	iounmap(release_addr);

	return 0;
}

static int smp_spin_table_cpu_boot(unsigned int cpu)
{
	/*
	 * Update the pen release flag.
	 */
	write_pen_release(cpu_logical_map(cpu));

	/*
	 * Send an event, causing the secondaries to read pen_release.
	 */
	sev();

	return 0;
}

const struct cpu_operations smp_spin_table_ops = {
	.name		= "spin-table",
	.cpu_init	= smp_spin_table_cpu_init,
	.cpu_prepare	= smp_spin_table_cpu_prepare,
	.cpu_boot	= smp_spin_table_cpu_boot,
};
