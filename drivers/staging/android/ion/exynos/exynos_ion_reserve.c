#ifdef CONFIG_OF_RESERVED_MEM

#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/exynos_ion.h>

#include "../ion.h"
#include "../ion_priv.h"

int ion_exynos_region_cursor;

struct carveout_id {
	int id;
	const char *name;
};

static struct carveout_id carveout_id_table[] = {
	{ION_EXYNOS_ID_COMMON,	"common"},
	{ION_EXYNOS_ID_MFC_SH,	"mfc_sh"},
	{ION_EXYNOS_ID_G2D_WFD,	"g2d_wfd"},
	{ION_EXYNOS_ID_VIDEO,	"video"},
	{ION_EXYNOS_ID_SECTBL,	"sectbl"},
	{ION_EXYNOS_ID_MFC_FW,	"mfc_fw"},
	{ION_EXYNOS_ID_MFC_NFW,	"mfc_nfw"},
	{ION_EXYNOS_ID_SECDMA,	"secdma"},
};

#define NR_RESERVE_REGIONS	ARRAY_SIZE(carveout_id_table)

struct ion_platform_heap contig_heaps[NR_RESERVE_REGIONS];

static void exynos_ion_rmem_device_init(struct reserved_mem *rmem,
						struct device *dev)
{
	/* Nothing to do */
}

static void exynos_ion_rmem_device_release(struct reserved_mem *rmem,
						struct device *dev)
{
	/* Nothing to do */
}

static const struct reserved_mem_ops exynos_ion_rmem_ops = {
	.device_init	= exynos_ion_rmem_device_init,
	.device_release	= exynos_ion_rmem_device_release,
};

static int exynos_ion_make_carveout_id(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(carveout_id_table); i++) {
		if (!strcmp(name, carveout_id_table[i].name))
			break;
	}

	BUG_ON(i == ARRAY_SIZE(carveout_id_table));

	return MAKE_CARVEOUT_ID(carveout_id_table[i].id);
}

static int __init exynos_ion_reserved_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &exynos_ion_rmem_ops;

	BUG_ON(ion_exynos_region_cursor > (NR_RESERVE_REGIONS - 1));

	contig_heaps[ion_exynos_region_cursor].type = ION_HEAP_TYPE_CARVEOUT;
	contig_heaps[ion_exynos_region_cursor].id =
		exynos_ion_make_carveout_id(rmem->name);
	contig_heaps[ion_exynos_region_cursor].name = rmem->name;
	contig_heaps[ion_exynos_region_cursor].base = rmem->base;
	contig_heaps[ion_exynos_region_cursor].size = rmem->size;
	contig_heaps[ion_exynos_region_cursor].align = PAGE_SIZE;
	contig_heaps[ion_exynos_region_cursor].priv = rmem;
	ion_exynos_region_cursor++;

	pr_info("Reserved memory: %s:%#lx\n",
			rmem->name, (unsigned long) rmem->size);

	return 0;
}

int ion_exynos_contig_heap_info(int region_id, phys_addr_t *phys, size_t *size)
{
	const char *name;
	int i;

	for (i = 0; i < ARRAY_SIZE(carveout_id_table); i++) {
		if (region_id == carveout_id_table[i].id) {
			name = carveout_id_table[i].name;
			break;
		}
	}
	BUG_ON(i == ARRAY_SIZE(carveout_id_table));

	for (i = 0; i < ion_exynos_region_cursor; i++) {
		if (!strcmp(name, contig_heaps[i].name)) {
			if (phys)
				*phys = contig_heaps[i].base;
			if (size)
				*size = contig_heaps[i].size;
			break;
		}
	}
	BUG_ON(i == ion_exynos_region_cursor);

	return 0;
}
EXPORT_SYMBOL(ion_exynos_contig_heap_info);

#define DECLARE_EXYNOS_ION_RESERVED_REGION(name) \
RESERVEDMEM_OF_DECLARE(name, "exynos5433-ion,"#name, exynos_ion_reserved_mem_setup)

DECLARE_EXYNOS_ION_RESERVED_REGION(common);
DECLARE_EXYNOS_ION_RESERVED_REGION(mfc_sh);
DECLARE_EXYNOS_ION_RESERVED_REGION(g2d_wfd);
DECLARE_EXYNOS_ION_RESERVED_REGION(video);
DECLARE_EXYNOS_ION_RESERVED_REGION(sectbl);
DECLARE_EXYNOS_ION_RESERVED_REGION(mfc_fw);
DECLARE_EXYNOS_ION_RESERVED_REGION(mfc_nfw);
DECLARE_EXYNOS_ION_RESERVED_REGION(secdma);

#endif
