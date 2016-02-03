/*
 * drivers/gpu/ion/ion_system_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/swap.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <asm/tlbflush.h>
#include "ion_priv.h"

static unsigned int high_order_gfp_flags = (GFP_HIGHUSER | __GFP_NOWARN |
					   __GFP_NORETRY | __GFP_NO_KSWAPD) & ~__GFP_WAIT;
static unsigned int low_order_gfp_flags  = (GFP_HIGHUSER | __GFP_NOWARN);
static const unsigned int orders[] = {8, 4, 0};
static const int num_orders = ARRAY_SIZE(orders);

#define PG_ion_frompool		(__NR_PAGEFLAGS + 1)
#define ion_page_order(page)	(page)->private
#define ion_set_from_pool(page)	set_bit(PG_ion_frompool, &(page)->flags)
#define ion_get_from_pool(page)	test_bit(PG_ion_frompool, &(page)->flags)
#define ion_clear_from_pool(page)	clear_bit(PG_ion_frompool, &(page)->flags)

static int order_to_index(unsigned int order)
{
	int i;
	for (i = 0; i < num_orders; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool **pools;
};

static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order,
				      bool *from_pool)
{
	int idx = order_to_index(order);
	struct ion_page_pool *pool;
	struct page *page;

	if (ion_buffer_fault_user_mappings(buffer)) {
		gfp_t gfp_flags = low_order_gfp_flags;

		if (order > 0)
			gfp_flags = high_order_gfp_flags;
		page = ion_heap_alloc_pages(buffer, gfp_flags, order);

		return page;
	}

	if (!ion_buffer_cached(buffer))
		idx += num_orders;

	pool = heap->pools[idx];

	page = ion_page_pool_alloc(pool, false, from_pool);
	if (!page) {
		/* try with alternative pool */
		if (ion_buffer_cached(buffer))
			pool = heap->pools[idx + num_orders];
		else
			pool = heap->pools[idx - num_orders];

		page = ion_page_pool_alloc(pool, true, from_pool);
		/*
		 * allocation from cached pool for nocached page allocation
		 * request must be treated as 'not from pool' because it
		 * requires cache flush.
		 */
		if (!ion_buffer_cached(buffer))
			*from_pool = false;
	}

	return page;
}

static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *page,
			     unsigned int order)
{
	int uncached = ion_buffer_cached(buffer) ? 0 : 1;
	int idx = order_to_index(order);
	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	int i;
	struct ion_page_pool *pool = heap->pools[idx + num_orders * uncached];

	if (split_pages) {
		for (i = 0; i < (1 << order); i++)
			__free_page(page + i);
	} else if (!!(buffer->flags & ION_FLAG_SHRINKER_FREE)) {
		__free_pages(page, order);
	} else {
		ion_page_pool_free(pool, page);
	}
}


static struct page *alloc_largest_available(struct ion_system_heap *heap,
						 struct ion_buffer *buffer,
						 unsigned long size,
						 unsigned int max_order)
{
	struct page *page;
	int i;
	bool from_pool = false;

	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i], &from_pool);
		if (!page)
			continue;

		ion_page_order(page) = orders[i];
		if(from_pool)
			ion_set_from_pool(page);

		return page;
	}
	return NULL;
}

static int ion_system_heap_allocate(struct ion_heap *heap,
				     struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table;
	struct scatterlist *sg;
	int ret;
	struct list_head pages;
	struct page *page, *tmp_page;
	int i = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];
	bool all_pages_from_pool = true;

	INIT_LIST_HEAD(&pages);
	while (size_remaining > 0) {
		page = alloc_largest_available(sys_heap, buffer, size_remaining, max_order);
		if (!page)
			goto err;
		list_add_tail(&page->lru, &pages);
		size_remaining -= (1 << ion_page_order(page)) * PAGE_SIZE;
		max_order = ion_page_order(page);
		i++;
	}

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		goto err;

	ret = sg_alloc_table(table, i, GFP_KERNEL);
	if (ret)
		goto err1;

	sg = table->sgl;

	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, (1 << ion_page_order(page)) * PAGE_SIZE, 0);
		sg = sg_next(sg);
		if (all_pages_from_pool && !ion_get_from_pool(page))
			all_pages_from_pool = false;
		list_del(&page->lru);
		ion_clear_from_pool(page);
		ion_page_order(page) = 0;
	}

	if (all_pages_from_pool)
		ion_buffer_set_ready(buffer);

	buffer->priv_virt = table;
	return 0;
err1:
	kfree(table);
err:
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		int order = ion_page_order(page);
		list_del(&page->lru);
		ion_page_order(page) = 0;
		ion_clear_from_pool(page);
		free_buffer_page(sys_heap, buffer, page, order);
	}
	return -ENOMEM;
}

void ion_system_heap_free(struct ion_buffer *buffer) {
	struct ion_heap *heap = buffer->heap;
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	int i;

	/* pages come from the page pools, zero them before returning
	   for security purposes (other allocations are zerod at alloc time */
	if (!ion_buffer_fault_user_mappings(buffer))
		ion_heap_buffer_zero(buffer);

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				get_order(sg_dma_len(sg)));
	sg_free_table(table);
	kfree(table);
}

struct sg_table *ion_system_heap_map_dma(struct ion_heap *heap,
					 struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

void ion_system_heap_unmap_dma(struct ion_heap *heap,
			       struct ion_buffer *buffer)
{
	return;
}

struct ion_system_heap_prealod_object {
	size_t len;
	unsigned int count;
};

struct ion_system_heap_preload_data {
	struct ion_system_heap *heap;
	unsigned int flags;
	unsigned int count;
	struct ion_system_heap_prealod_object objs[0];
};

#define ION_FLAG_CACHED_POOL (ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC)
static int ion_system_heap_preloader(void *p)
{
	struct ion_system_heap_preload_data *data = p;
	long num_1m = 0, num_64k = 0, num_4k = 0;
	long loaded_pages;
	int idx = 0, alt_idx = num_orders;
	int i;

	for (i = 0; i < data->count; i++) {
		size_t len = data->objs[i].len / PAGE_SIZE;

		num_4k += (len & 0xF) * data->objs[i].count;
		len >>= 4;
		num_64k += (len & 0xF) * data->objs[i].count;
		len >>= 4;
		num_1m += len * data->objs[i].count;
	}

	if ((num_4k + num_64k * 16 + num_1m * 256) > (totalram_pages / 4)) {
		/* too many pages requested */
		long max_pages = totalram_pages / 4;
		long exceeded_pages = num_4k + num_64k * 16 + num_1m * 256;
		exceeded_pages -= max_pages;

		if (num_4k < exceeded_pages) {
			num_4k = 0;
			exceeded_pages -= num_4k;
		} else {
			num_4k -= exceeded_pages;
			exceeded_pages = 0;
		}

		exceeded_pages /= 16;

		if (num_64k < exceeded_pages) {
			num_64k = 0;
			exceeded_pages -= num_64k;
		} else {
			num_64k -= exceeded_pages;
			exceeded_pages = 0;
		}

		exceeded_pages /= 16;

		if (num_1m < exceeded_pages) {
			num_1m = 0;
			exceeded_pages -= num_1m;
		} else {
			num_1m -= exceeded_pages;
			exceeded_pages = 0;
		}
	}

	ion_heap_freelist_drain(&data->heap->heap, 0);

	if ((data->flags & ION_FLAG_CACHED_POOL) != ION_FLAG_CACHED_POOL) {
		idx = num_orders; /* non cached */
		alt_idx = 0;
	}

	ion_page_pool_preload_prepare(data->heap->pools[idx + 2], num_4k);
	/* populates order-0 pages first to invoke page reclamation */
	loaded_pages = ion_page_pool_preload(data->heap->pools[idx + 2],
			data->heap->pools[alt_idx + 2], data->flags, num_4k);
	if (loaded_pages < num_4k)
		/* kernel is really unable to allocate page */
		goto finish;

	loaded_pages = ion_page_pool_preload(data->heap->pools[idx + 1],
			data->heap->pools[alt_idx + 1], data->flags, num_64k);
	num_64k -= loaded_pages;

	loaded_pages = ion_page_pool_preload(data->heap->pools[idx],
			data->heap->pools[alt_idx], data->flags, num_1m);
	num_1m -= loaded_pages;

	if (num_1m || num_64k) {
		/* try again with lower order free list */
		loaded_pages = ion_page_pool_preload(data->heap->pools[idx + 1],
				data->heap->pools[alt_idx + 1], data->flags,
				num_64k + num_1m * 16);
		if (num_1m > (loaded_pages / 16)) {
			num_1m -= loaded_pages / 16;
			loaded_pages &= 0xF; /* remiander of loaded_pages/16 */
		} else {
			loaded_pages -= num_1m * 16;
			num_1m = 0;
		}
		num_64k -= loaded_pages;
		/*
		 * half of order-8 pages won't be tried with order-0 free list
		 * for memory utilization because populating too much low order
		 * pages causes memory fragmentation seriously.
		 */
		num_64k += num_1m * 8;
		num_4k += num_64k * 16;

		loaded_pages = ion_page_pool_preload(data->heap->pools[idx + 2],
				data->heap->pools[alt_idx + 2], data->flags,
				num_4k + num_64k * 16);

		if (((num_4k - loaded_pages) + num_1m) > 0)
			pr_info("%s: %ld pages are not populated to the pool\n",
				__func__, loaded_pages + num_1m * 256);
	}

finish:
	kfree(data); /* allocated in ion_system_heap_preload_allocate() */

	if (!signal_pending(current))
		do_exit(0);
	return 0;
}

static void ion_system_heap_preload_allocate(struct ion_heap *heap,
					     unsigned int flags,
					     unsigned int count,
					     struct ion_preload_object obj[])
{
	struct sched_param param = { .sched_priority = 0 };
	struct ion_system_heap_preload_data *data;
	struct task_struct *ret;

	data = kmalloc(sizeof(*data) + sizeof(data->objs) * count, GFP_KERNEL);
	if (!data) {
		pr_info("%s: preload request failed due to nomem\n", __func__);
		return;
	}

	data->heap = container_of(heap, struct ion_system_heap, heap);
	data->flags = flags;
	data->count = count;
	for (count = 0; count < data->count; count++) {
		data->objs[count].count = obj[count].count;
		data->objs[count].len = obj[count].len;
	}

	ret = kthread_run(ion_system_heap_preloader, data,
				"ion_system_heap_preloader_%d", count);
	if (IS_ERR(ret)) {
		kfree(data);
		pr_info("%s: failed to create preload thread(%ld)\n",
				__func__, PTR_ERR(ret));
	} else {
		sched_setscheduler(ret, SCHED_NORMAL, &param);
	}
}

static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.map_dma = ion_system_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.preload = ion_system_heap_preload_allocate,
};

#define MAX_POOL_SHRINK_SHIFT		8
#define DEFAULT_POOL_SHRINK_SHIFT	7
#define MIN_POOL_SHRINK_SHIFT		1
static int pool_shrink_shift = DEFAULT_POOL_SHRINK_SHIFT;
module_param_named(shrink_shift, pool_shrink_shift, int, S_IRUGO | S_IWUSR);

static int ion_system_heap_shrink(struct shrinker *shrinker,
				  struct shrink_control *sc)
{

	struct ion_heap *heap = container_of(shrinker, struct ion_heap,
					     shrinker);
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int nr_total = 0;
	int nr_freed, nr_freelist, nr_to_scan, other_avail;
	int shift;
	int shrink_shift = pool_shrink_shift;
	gfp_t gfp_mask = sc->gfp_mask | __GFP_HIGHMEM;
	int i;

	for (i = 0; i < num_orders * 2; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		nr_total += ion_page_pool_shrink(pool, gfp_mask, 0);
	}
	nr_freelist = ion_heap_freelist_size(heap) / PAGE_SIZE;
	nr_total += nr_freelist;

	if ((sc->nr_to_scan == 0) || !nr_total)
		return nr_total;
	/*
	 * shrink shift: aging the rate of page returned by shrinker
	 * -DEFAULT_POOL_SHRINK_SHIFT <= shift <= pool_shrink_shift
	 */
	if ((shrink_shift < 0) || (shrink_shift > MAX_POOL_SHRINK_SHIFT))
		shrink_shift = MAX_POOL_SHRINK_SHIFT;
	other_avail = global_page_state(NR_FREE_PAGES)
			+ global_page_state(NR_FILE_PAGES)
			- nr_total
			- totalreserve_pages
			- global_page_state(NR_SHMEM);
	shift = (other_avail < 0) ? 0 : other_avail / nr_total;
	shift = shrink_shift - shift;
	if (shift < MIN_POOL_SHRINK_SHIFT)
		shift = MIN_POOL_SHRINK_SHIFT;
	nr_to_scan = sc->nr_to_scan << shift;

	/* shrink the free list first, no point in zeroing the memory if
	   we're just going to reclaim it */
	nr_freed = ion_heap_freelist_drain(heap, nr_to_scan * PAGE_SIZE) /
		PAGE_SIZE;

	if (nr_freed >= nr_to_scan)
		goto end;

	/* shrink order: cached pool first, low order pages first */
	for (i = num_orders - 1; i >= 0; i--) {
		struct ion_page_pool *pool = sys_heap->pools[i];

		nr_freed += ion_page_pool_shrink(pool, gfp_mask,
						 nr_to_scan);
		if (nr_freed >= nr_to_scan)
			break;

		pool = sys_heap->pools[i + num_orders];
		nr_freed += ion_page_pool_shrink(pool, gfp_mask,
						 nr_to_scan);
		if (nr_freed >= nr_to_scan)
			break;
	}

	if (nr_to_scan > 0)
		ION_EVENT_SHRINK(heap->dev, nr_freed * PAGE_SIZE);

end:
	/* total number of items is whatever the page pools are holding
	   plus whatever's in the freelist */
	nr_total = 0;
	for (i = 0; i < num_orders * 2; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		nr_total += ion_page_pool_shrink(pool, gfp_mask, 0);
	}
	nr_total += ion_heap_freelist_size(heap) / PAGE_SIZE;
	return nr_total;

}

static int ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{

	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		seq_printf(s, "%d order %u highmem pages in cached pool = %lu total\n",
			   pool->high_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in cached pool = %lu total\n",
			   pool->low_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->low_count);
	}

	for (i = num_orders; i < (num_orders * 2); i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		seq_printf(s, "%d order %u highmem pages in uncached pool = %lu total\n",
			   pool->high_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in uncached pool = %lu total\n",
			   pool->low_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->low_count);
	}
	return 0;
}

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_heap *heap;
	int i;

	compiletime_assert((ZONES_PGSHIFT - __NR_PAGEFLAGS) > 0,
			"No space in pageflags for ion_system_heap");

	heap = kzalloc(sizeof(struct ion_system_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	heap->pools = kzalloc(sizeof(struct ion_page_pool *) * num_orders * 2,
			      GFP_KERNEL);
	if (!heap->pools)
		goto err_alloc_pools;
	for (i = 0; i < num_orders * 2; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i % num_orders] > 0)
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i % num_orders]);
		if (!pool)
			goto err_create_pool;
		heap->pools[i] = pool;
	}

	heap->heap.shrinker.shrink = ion_system_heap_shrink;
	heap->heap.shrinker.seeks = DEFAULT_SEEKS;
	heap->heap.shrinker.batch = 0;
	register_shrinker(&heap->heap.shrinker);
	heap->heap.debug_show = ion_system_heap_debug_show;
	return &heap->heap;
err_create_pool:
	for (i = 0; i < num_orders * 2; i++)
		if (heap->pools[i])
			ion_page_pool_destroy(heap->pools[i]);
	kfree(heap->pools);
err_alloc_pools:
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}

void ion_system_heap_destroy(struct ion_heap *heap)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;

	for (i = 0; i < num_orders; i++)
		ion_page_pool_destroy(sys_heap->pools[i]);
	kfree(sys_heap->pools);
	kfree(sys_heap);
}

static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	buffer->priv_virt = kzalloc(len, GFP_KERNEL);
	if (!buffer->priv_virt)
		return -ENOMEM;
	return 0;
}

void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	kfree(buffer->priv_virt);
}

static int ion_system_contig_heap_phys(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       ion_phys_addr_t *addr, size_t *len)
{
	*addr = virt_to_phys(buffer->priv_virt);
	*len = buffer->size;
	return 0;
}

struct sg_table *ion_system_contig_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl, virt_to_page(buffer->priv_virt), buffer->size,
		    0);
	return table;
}

void ion_system_contig_heap_unmap_dma(struct ion_heap *heap,
				      struct ion_buffer *buffer)
{
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

int ion_system_contig_heap_map_user(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    struct vm_area_struct *vma)
{
	unsigned long pfn = __phys_to_pfn(virt_to_phys(buffer->priv_virt));
	return remap_pfn_range(vma, vma->vm_start, pfn + vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);

}

static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
	.phys = ion_system_contig_heap_phys,
	.map_dma = ion_system_contig_heap_map_dma,
	.unmap_dma = ion_system_contig_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_system_contig_heap_map_user,
};

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	return heap;
}

void ion_system_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

