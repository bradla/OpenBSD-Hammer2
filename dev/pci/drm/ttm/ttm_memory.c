/*	$OpenBSD: ttm_memory.c,v 1.4 2014/02/09 10:57:26 jsg Exp $	*/
/**************************************************************************
 *
 * Copyright (c) 2006-2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#define pr_fmt(fmt) "[TTM] " fmt

#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/refcount.h>
#include <dev/pci/drm/ttm/ttm_memory.h>
#include <dev/pci/drm/ttm/ttm_module.h>
#include <dev/pci/drm/ttm/ttm_page_alloc.h>

#define TTM_MEMORY_ALLOC_RETRIES 4

struct ttm_mem_zone {
	u_int kobj_ref;
	struct ttm_mem_global *glob;
	const char *name;
	uint64_t zone_mem;
	uint64_t emer_mem;
	uint64_t max_mem;
	uint64_t swap_limit;
	uint64_t used_mem;
};

#ifdef notyet
static struct attribute ttm_mem_sys = {
	.name = "zone_memory",
	.mode = S_IRUGO
};
static struct attribute ttm_mem_emer = {
	.name = "emergency_memory",
	.mode = S_IRUGO | S_IWUSR
};
static struct attribute ttm_mem_max = {
	.name = "available_memory",
	.mode = S_IRUGO | S_IWUSR
};
static struct attribute ttm_mem_swap = {
	.name = "swap_limit",
	.mode = S_IRUGO | S_IWUSR
};
static struct attribute ttm_mem_used = {
	.name = "used_memory",
	.mode = S_IRUGO
};
#endif

static void ttm_mem_zone_kobj_release(struct ttm_mem_zone *zone)
{

	DRM_INFO("Zone %7s: Used memory at exit: %llu kiB\n",
		zone->name, (unsigned long long)zone->used_mem >> 10);
	kfree(zone);
}

#ifdef notyet
static ssize_t ttm_mem_zone_show(struct kobject *kobj,
				 struct attribute *attr,
				 char *buffer)
{
	struct ttm_mem_zone *zone =
		container_of(kobj, struct ttm_mem_zone, kobj);
	uint64_t val = 0;

	mtx_enter(&zone->glob->lock);
	if (attr == &ttm_mem_sys)
		val = zone->zone_mem;
	else if (attr == &ttm_mem_emer)
		val = zone->emer_mem;
	else if (attr == &ttm_mem_max)
		val = zone->max_mem;
	else if (attr == &ttm_mem_swap)
		val = zone->swap_limit;
	else if (attr == &ttm_mem_used)
		val = zone->used_mem;
	mtx_leave(&zone->glob->lock);

	return snprintf(buffer, PAGE_SIZE, "%llu\n",
			(unsigned long long) val >> 10);
}

static void ttm_check_swapping(struct ttm_mem_global *glob);

static ssize_t ttm_mem_zone_store(struct kobject *kobj,
				  struct attribute *attr,
				  const char *buffer,
				  size_t size)
{
	struct ttm_mem_zone *zone =
		container_of(kobj, struct ttm_mem_zone, kobj);
	int chars;
	unsigned long val;
	uint64_t val64;

	chars = sscanf(buffer, "%lu", &val);
	if (chars == 0)
		return size;

	val64 = val;
	val64 <<= 10;

	mtx_enter(&zone->glob->lock);
	if (val64 > zone->zone_mem)
		val64 = zone->zone_mem;
	if (attr == &ttm_mem_emer) {
		zone->emer_mem = val64;
		if (zone->max_mem > val64)
			zone->max_mem = val64;
	} else if (attr == &ttm_mem_max) {
		zone->max_mem = val64;
		if (zone->emer_mem < val64)
			zone->emer_mem = val64;
	} else if (attr == &ttm_mem_swap)
		zone->swap_limit = val64;
	mtx_leave(&zone->glob->lock);

	ttm_check_swapping(zone->glob);

	return size;
}
#endif

#ifdef notyet
static struct attribute *ttm_mem_zone_attrs[] = {
	&ttm_mem_sys,
	&ttm_mem_emer,
	&ttm_mem_max,
	&ttm_mem_swap,
	&ttm_mem_used,
	NULL
};

static const struct sysfs_ops ttm_mem_zone_ops = {
	.show = &ttm_mem_zone_show,
	.store = &ttm_mem_zone_store
};

static struct kobj_type ttm_mem_zone_kobj_type = {
	.release = &ttm_mem_zone_kobj_release,
	.sysfs_ops = &ttm_mem_zone_ops,
	.default_attrs = ttm_mem_zone_attrs,
};
#endif

static void ttm_mem_global_kobj_release(struct ttm_mem_global *glob)
{

	kfree(glob);
}

#ifdef notyet
static struct kobj_type ttm_mem_glob_kobj_type = {
	.release = &ttm_mem_global_kobj_release,
};
#endif

static bool ttm_zones_above_swap_target(struct ttm_mem_global *glob,
					bool from_wq, uint64_t extra)
{
	unsigned int i;
	struct ttm_mem_zone *zone;
	uint64_t target;

	for (i = 0; i < glob->num_zones; ++i) {
		zone = glob->zones[i];

		if (from_wq)
			target = zone->swap_limit;
		else if (DRM_SUSER(curproc))
			target = zone->emer_mem;
		else
			target = zone->max_mem;

		target = (extra > target) ? 0ULL : target;

		if (zone->used_mem > target)
			return true;
	}
	return false;
}

/**
 * At this point we only support a single shrink callback.
 * Extend this if needed, perhaps using a linked list of callbacks.
 * Note that this function is reentrant:
 * many threads may try to swap out at any given time.
 */

static void ttm_shrink(struct ttm_mem_global *glob, bool from_wq,
		       uint64_t extra)
{
	int ret;
	struct ttm_mem_shrink *shrink;

	mtx_enter(&glob->lock);
	if (glob->shrink == NULL)
		goto out;

	while (ttm_zones_above_swap_target(glob, from_wq, extra)) {
		shrink = glob->shrink;
		mtx_leave(&glob->lock);
		ret = shrink->do_shrink(shrink);
		mtx_enter(&glob->lock);
		if (unlikely(ret != 0))
			goto out;
	}
out:
	glob->task_queued = false;
	mtx_leave(&glob->lock);
}

static void ttm_shrink_work(void *arg1, void *arg2)
{
	struct ttm_mem_global *glob = arg1;

	ttm_shrink(glob, true, 0ULL);
}

static int ttm_mem_init_kernel_zone(struct ttm_mem_global *glob,
    uint64_t mem)
{
	struct ttm_mem_zone *zone = kzalloc(sizeof(*zone), GFP_KERNEL);

	if (unlikely(!zone))
		return -ENOMEM;

	zone->name = "kernel";
	zone->zone_mem = mem;
	zone->max_mem = mem >> 1;
	zone->emer_mem = (mem >> 1) + (mem >> 2);
	zone->swap_limit = zone->max_mem - (mem >> 3);
	zone->used_mem = 0;
	zone->glob = glob;
	glob->zone_kernel = zone;
	refcount_init(&zone->kobj_ref, 1);
	glob->zones[glob->num_zones++] = zone;
	return 0;
}

#ifdef CONFIG_HIGHMEM
static int ttm_mem_init_highmem_zone(struct ttm_mem_global *glob,
    uint64_t mem)
{
	struct ttm_mem_zone *zone;

	if (si->totalhigh == 0)
		return 0;

	zone = kzalloc(sizeof(*zone), GFP_KERNEL);
	if (unlikely(!zone))
		return -ENOMEM;

	zone->name = "highmem";
	zone->zone_mem = mem;
	zone->max_mem = mem >> 1;
	zone->emer_mem = (mem >> 1) + (mem >> 2);
	zone->swap_limit = zone->max_mem - (mem >> 3);
	zone->used_mem = 0;
	zone->glob = glob;
	glob->zone_highmem = zone;
	refcount_init(&zone->kobj_ref, 1);
	glob->zones[glob->num_zones++] = zone;
	return 0;
}
#else
static int ttm_mem_init_dma32_zone(struct ttm_mem_global *glob,
    uint64_t mem)
{
	struct ttm_mem_zone *zone = kzalloc(sizeof(*zone), GFP_KERNEL);

	if (unlikely(!zone))
		return -ENOMEM;

	/**
	 * No special dma32 zone needed.
	 */

	if (mem <= ((uint64_t) 1ULL << 32)) {
		kfree(zone);
		return 0;
	}

	/*
	 * Limit max dma32 memory to 4GB for now
	 * until we can figure out how big this
	 * zone really is.
	 */

	mem = ((uint64_t) 1ULL << 32);
	zone->name = "dma32";
	zone->zone_mem = mem;
	zone->max_mem = mem >> 1;
	zone->emer_mem = (mem >> 1) + (mem >> 2);
	zone->swap_limit = zone->max_mem - (mem >> 3);
	zone->used_mem = 0;
	zone->glob = glob;
	glob->zone_dma32 = zone;
	refcount_init(&zone->kobj_ref, 1);
	glob->zones[glob->num_zones++] = zone;
	return 0;
}
#endif

int ttm_mem_global_init(struct ttm_mem_global *glob)
{
	uint64_t mem;
	int ret;
#ifdef DRMDEBUG
	int i;
	struct ttm_mem_zone *zone;
#endif

	mtx_init(&glob->lock, IPL_TTY);
	glob->swap_queue = taskq_create("ttm_swap", 1, IPL_TTY);
	glob->task_queued = false;
	task_set(&glob->task, ttm_shrink_work, glob, NULL);

	refcount_init(&glob->kobj_ref, 1);

	mem = ptoa(physmem);

	ret = ttm_mem_init_kernel_zone(glob, mem);
	if (unlikely(ret != 0))
		goto out_no_zone;
#ifdef CONFIG_HIGHMEM
	ret = ttm_mem_init_highmem_zone(glob, mem);
	if (unlikely(ret != 0))
		goto out_no_zone;
#else
	ret = ttm_mem_init_dma32_zone(glob, mem);
	if (unlikely(ret != 0))
		goto out_no_zone;
#endif
#ifdef DRMDEBUG
	for (i = 0; i < glob->num_zones; ++i) {
		zone = glob->zones[i];
		DRM_INFO("Zone %7s: Available graphics memory: %llu kiB\n",
			zone->name, (unsigned long long)zone->max_mem >> 10);
	}
#endif
	ttm_page_alloc_init(glob, glob->zone_kernel->max_mem/(2*PAGE_SIZE));
	ttm_dma_page_alloc_init(glob, glob->zone_kernel->max_mem/(2*PAGE_SIZE));
	return 0;
out_no_zone:
	ttm_mem_global_release(glob);
	return ret;
}
EXPORT_SYMBOL(ttm_mem_global_init);

void ttm_mem_global_release(struct ttm_mem_global *glob)
{
	unsigned int i;
	struct ttm_mem_zone *zone;

	/* let the page allocator first stop the shrink work. */
	ttm_page_alloc_fini();
	ttm_dma_page_alloc_fini();

	taskq_destroy(glob->swap_queue);
	glob->swap_queue = NULL;
	for (i = 0; i < glob->num_zones; ++i) {
		zone = glob->zones[i];
		if (refcount_release(&zone->kobj_ref))
			ttm_mem_zone_kobj_release(zone);
	}
	if (refcount_release(&glob->kobj_ref))
		ttm_mem_global_kobj_release(glob);
}
EXPORT_SYMBOL(ttm_mem_global_release);

static void ttm_check_swapping(struct ttm_mem_global *glob)
{
	bool needs_swapping = false;
	unsigned int i;
	struct ttm_mem_zone *zone;

	mtx_enter(&glob->lock);
	for (i = 0; i < glob->num_zones; ++i) {
		zone = glob->zones[i];
		if (zone->used_mem > zone->swap_limit) {
			needs_swapping = true;
			break;
		}
	}

	if (glob->task_queued)
		needs_swapping = false;
	else
		glob->task_queued = true;
	mtx_leave(&glob->lock);

	if (unlikely(needs_swapping))
		task_add(glob->swap_queue, &glob->task);
}

static void ttm_mem_global_free_zone(struct ttm_mem_global *glob,
				     struct ttm_mem_zone *single_zone,
				     uint64_t amount)
{
	unsigned int i;
	struct ttm_mem_zone *zone;

	mtx_enter(&glob->lock);
	for (i = 0; i < glob->num_zones; ++i) {
		zone = glob->zones[i];
		if (single_zone && zone != single_zone)
			continue;
		zone->used_mem -= amount;
	}
	mtx_leave(&glob->lock);
}

void ttm_mem_global_free(struct ttm_mem_global *glob,
			 uint64_t amount)
{
	return ttm_mem_global_free_zone(glob, NULL, amount);
}
EXPORT_SYMBOL(ttm_mem_global_free);

static int ttm_mem_global_reserve(struct ttm_mem_global *glob,
				  struct ttm_mem_zone *single_zone,
				  uint64_t amount, bool reserve)
{
	uint64_t limit;
	int ret = -ENOMEM;
	unsigned int i;
	struct ttm_mem_zone *zone;

	mtx_enter(&glob->lock);
	for (i = 0; i < glob->num_zones; ++i) {
		zone = glob->zones[i];
		if (single_zone && zone != single_zone)
			continue;

		limit = (DRM_SUSER(curproc)) ?
			zone->emer_mem : zone->max_mem;

		if (zone->used_mem > limit)
			goto out_unlock;
	}

	if (reserve) {
		for (i = 0; i < glob->num_zones; ++i) {
			zone = glob->zones[i];
			if (single_zone && zone != single_zone)
				continue;
			zone->used_mem += amount;
		}
	}

	ret = 0;
out_unlock:
	mtx_leave(&glob->lock);
	ttm_check_swapping(glob);

	return ret;
}


static int ttm_mem_global_alloc_zone(struct ttm_mem_global *glob,
				     struct ttm_mem_zone *single_zone,
				     uint64_t memory,
				     bool no_wait, bool interruptible)
{
	int count = TTM_MEMORY_ALLOC_RETRIES;

	while (unlikely(ttm_mem_global_reserve(glob,
					       single_zone,
					       memory, true)
			!= 0)) {
		if (no_wait)
			return -ENOMEM;
		if (unlikely(count-- == 0))
			return -ENOMEM;
		ttm_shrink(glob, false, memory + (memory >> 2) + 16);
	}

	return 0;
}

int ttm_mem_global_alloc(struct ttm_mem_global *glob, uint64_t memory,
			 bool no_wait, bool interruptible)
{
	/**
	 * Normal allocations of kernel memory are registered in
	 * all zones.
	 */

	return ttm_mem_global_alloc_zone(glob, NULL, memory, no_wait,
					 interruptible);
}
EXPORT_SYMBOL(ttm_mem_global_alloc);

#define page_to_pfn(pp) (VM_PAGE_TO_PHYS(pp) / PAGE_SIZE)

int ttm_mem_global_alloc_page(struct ttm_mem_global *glob,
			      struct vm_page *page,
			      bool no_wait, bool interruptible)
{

	struct ttm_mem_zone *zone = NULL;

	/**
	 * Page allocations may be registed in a single zone
	 * only if highmem or !dma32.
	 */

#ifdef CONFIG_HIGHMEM
	if (PageHighMem(page) && glob->zone_highmem != NULL)
		zone = glob->zone_highmem;
#else
	if (glob->zone_dma32 && page_to_pfn(page) > 0x00100000UL)
		zone = glob->zone_kernel;
#endif
	return ttm_mem_global_alloc_zone(glob, zone, PAGE_SIZE, no_wait,
					 interruptible);
}

void ttm_mem_global_free_page(struct ttm_mem_global *glob, struct vm_page *page)
{
	struct ttm_mem_zone *zone = NULL;

#ifdef CONFIG_HIGHMEM
	if (PageHighMem(page) && glob->zone_highmem != NULL)
		zone = glob->zone_highmem;
#else
	if (glob->zone_dma32 && page_to_pfn(page) > 0x00100000UL)
		zone = glob->zone_kernel;
#endif
	ttm_mem_global_free_zone(glob, zone, PAGE_SIZE);
}


size_t ttm_round_pot(size_t size)
{
	if ((size & (size - 1)) == 0)
		return size;
	else if (size > PAGE_SIZE)
		return PAGE_ALIGN(size);
	else {
		size_t tmp_size = 4;

		while (tmp_size < size)
			tmp_size <<= 1;

		return tmp_size;
	}
	return 0;
}
EXPORT_SYMBOL(ttm_round_pot);
