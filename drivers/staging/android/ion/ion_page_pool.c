/*
 * drivers/staging/android/ion/ion_page_pool.c
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

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include "ion_priv.h"

void *ion_page_pool_alloc_pages(struct ion_page_pool *pool)
{
	struct page *page = alloc_pages(pool->gfp_mask, pool->order);

	if (!page)
		return NULL;

#ifndef CONFIG_ION_EXYNOS
	ion_pages_sync_for_device(NULL, page, PAGE_SIZE << pool->order,
						DMA_BIDIRECTIONAL);
#endif
	return page;
}

static void ion_page_pool_free_pages(struct ion_page_pool *pool,
				     struct page *page)
{
	ion_clear_page_clean(page);
	__free_pages(page, pool->order);
}

static int ion_page_pool_add(struct ion_page_pool *pool, struct page *page)
{
#ifdef CONFIG_DEBUG_LIST
	BUG_ON(page->lru.next != LIST_POISON1 ||
			page->lru.prev != LIST_POISON2);
#endif
	if (pool->cached)
		ion_clear_page_clean(page);

	mutex_lock(&pool->mutex);
	if (PageHighMem(page)) {
		list_add_tail(&page->lru, &pool->high_items);
		pool->high_count++;
	} else {
		list_add_tail(&page->lru, &pool->low_items);
		pool->low_count++;
	}
	mutex_unlock(&pool->mutex);
	return 0;
}

static struct page *ion_page_pool_remove(struct ion_page_pool *pool, bool high)
{
	struct page *page;

	if (high) {
		BUG_ON(!pool->high_count);
		page = list_first_entry(&pool->high_items, struct page, lru);
		pool->high_count--;
	} else {
		BUG_ON(!pool->low_count);
		page = list_first_entry(&pool->low_items, struct page, lru);
		pool->low_count--;
	}

	list_del(&page->lru);
	return page;
}

struct page *ion_page_pool_alloc(struct ion_page_pool *pool)
{
	struct page *page = NULL;

	BUG_ON(!pool);

	mutex_lock(&pool->mutex);
	if (pool->high_count)
		page = ion_page_pool_remove(pool, true);
	else if (pool->low_count)
		page = ion_page_pool_remove(pool, false);
	mutex_unlock(&pool->mutex);

	return page;
}

void ion_page_pool_free(struct ion_page_pool *pool, struct page *page)
{
	int ret;

	BUG_ON(pool->order != compound_order(page));

	ret = ion_page_pool_add(pool, page);
	if (ret)
		ion_page_pool_free_pages(pool, page);
}

static int ion_page_pool_total(struct ion_page_pool *pool, bool high)
{
	int count = pool->low_count;

	if (high)
		count += pool->high_count;

	return count << pool->order;
}

static bool __init_pages_for_preload(struct page *page, int order,
				     bool zero, bool flush)
{
	int n_pages = 1 << order;
	struct page **pages = NULL;
	int i;
	void *va;
	bool ret = true;

	if (!zero && !flush)
		return true;

	if (PageHighMem(page)) {
		pages = kmalloc(sizeof(*pages) * n_pages, GFP_KERNEL);
		if (!pages) {
			pr_warn("%s: aborted due to nomemory (order %d)\n",
				__func__, order);
			return false;
		}

		for (i = 0; i < n_pages; i++)
			pages[i] = &page[i];

		va = vm_map_ram(pages, n_pages, 0, PAGE_KERNEL);
		if (!va) {
			pr_warn("%s: aborted due to novm (order %d)\n",
				__func__, order);
			ret = false;
			goto err_vm_map;
		}
	} else {
		va = page_address(page);
	}

	if (zero)
		memset(va, 0, n_pages * PAGE_SIZE);
	if (flush)
		__flush_dcache_area(va, n_pages * PAGE_SIZE);

	if (PageHighMem(page))
		vm_unmap_ram(va, n_pages);
err_vm_map:
	if (PageHighMem(page))
		kfree(pages);

	return ret;
}

/*
 * This function is called for order-0 page preloading to prevent
 * holding too many order-0 pages in the pool and to relieve memory
 * fragmentation to make the larger order page population successful later
 */
void ion_page_pool_preload_prepare(struct ion_page_pool *pool, long num_pages)
{
	long pages_in_pool = pool->high_count + pool->low_count;
	long freed = 0;

	BUG_ON(pool->order != 0);

	while (pages_in_pool-- > num_pages) {
		struct page *page;
		mutex_lock(&pool->mutex);
		page = ion_page_pool_remove(pool, !pool->low_count);
		mutex_unlock(&pool->mutex);
		if (!page)
			break;
		ion_page_pool_free_pages(pool, page);
		freed++;
	}

	if (freed)
		pr_info("%s: %ld order-0 pages are shrinked\n",
			__func__, freed);
}

long ion_page_pool_preload(struct ion_page_pool *pool,
			   struct ion_page_pool *alt_pool,
			   unsigned int alloc_flags, long num_pages)
{
	long pages_required;

	BUG_ON(pool->order != alt_pool->order);

	/*
	 * the number of pages to preload needs to be tuned because the
	 * preloaded can be allocated to another thread that did not request
	 * preloading. Because we have no better idea about the optimal number
	 * of pages to preload currently, this function just tries that the pool
	 * has enough pages for the preload request.
	 */
	pages_required = num_pages - (pool->high_count + pool->low_count);
	pr_info("%s: order %d pages requested - %ld, to preload - %ld\n",
		__func__, pool->order, num_pages, pages_required);
	if (pages_required <= 0)
		return num_pages;

	/* enlarge the target pool first */
	while (pages_required > 0) {
		struct page *page;

		page = alloc_pages(pool->gfp_mask, pool->order);
		if (!page)
			break;

		if (!__init_pages_for_preload(page, pool->order,
				true, !(alloc_flags & ION_FLAG_CACHED))) {
			ion_clear_page_clean(page);
			__free_pages(page, pool->order);
			return pages_required;
		}

		if (ion_page_pool_add(pool, page)) {
			ion_clear_page_clean(page);
			__free_pages(page, pool->order);
			pr_warn("%s: aborted due to nomemory (order %d)\n",
				__func__, pool->order);
			return pages_required;
		}

		pages_required--;
	}

	/* take pages from alternative pool if the page allocator fails */
	while (pages_required > 0) {
		struct page *page;

		page = ion_page_pool_alloc(alt_pool);
		if (!page) {
			pr_warn("%s: aborted due to no page (order %d)\n",
				__func__, pool->order);
			break;
		}

		if (!__init_pages_for_preload(page, pool->order,
				false, !(alloc_flags & ION_FLAG_CACHED))) {
			/*
			 * instead of returning the page to the pool,
			 * just free the page due to lack of memory.
			 */
			ion_clear_page_clean(page);
			__free_pages(page, pool->order);
			pages_required++; /* to be decremented below */
		}

		if (ion_page_pool_add(pool, page)) {
			ion_clear_page_clean(page);
			__free_pages(page, pool->order);
			pr_warn("%s: aborted due to nomemory (order %d)\n",
				__func__, pool->order);
			return pages_required;
		}

		pages_required--;
	}

	if (pages_required > 0)
		pr_warn("%s: %ld pages are not preloaded for order %d\n",
			__func__, pages_required, pool->order);

	return num_pages - pages_required;
}

int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
				int nr_to_scan)
{
	int i;
	bool high;

	if (current_is_kswapd())
		high = 1;
	else
		high = !!(gfp_mask & __GFP_HIGHMEM);

	for (i = 0; i < nr_to_scan; i += (1 << pool->order)) {
		struct page *page;

		mutex_lock(&pool->mutex);
		if (pool->low_count) {
			page = ion_page_pool_remove(pool, false);
		} else if (high && pool->high_count) {
			page = ion_page_pool_remove(pool, true);
		} else {
			mutex_unlock(&pool->mutex);
			break;
		}
		mutex_unlock(&pool->mutex);
		ion_page_pool_free_pages(pool, page);
	}

	return ion_page_pool_total(pool, high);
}

struct ion_page_pool *ion_page_pool_create(gfp_t gfp_mask, unsigned int order)
{
	struct ion_page_pool *pool = kmalloc(sizeof(struct ion_page_pool),
					     GFP_KERNEL);
	if (!pool)
		return NULL;
	pool->high_count = 0;
	pool->low_count = 0;
	INIT_LIST_HEAD(&pool->low_items);
	INIT_LIST_HEAD(&pool->high_items);
	pool->gfp_mask = gfp_mask | __GFP_COMP;
	pool->order = order;
	mutex_init(&pool->mutex);
	plist_node_init(&pool->list, order);

	return pool;
}

void ion_page_pool_destroy(struct ion_page_pool *pool)
{
	kfree(pool);
}

static int __init ion_page_pool_init(void)
{
	return 0;
}

static void __exit ion_page_pool_exit(void)
{
}

module_init(ion_page_pool_init);
module_exit(ion_page_pool_exit);
