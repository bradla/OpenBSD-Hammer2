/*	$OpenBSD: ttm_bo_manager.c,v 1.2 2014/02/09 10:57:26 jsg Exp $	*/
/**************************************************************************
 *
 * Copyright (c) 2007-2010 VMware, Inc., Palo Alto, CA., USA
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
/*
 * Authors: Thomas Hellstrom <thellstrom-at-vmware-dot-com>
 */

#include <dev/pci/drm/ttm/ttm_module.h>
#include <dev/pci/drm/ttm/ttm_bo_driver.h>
#include <dev/pci/drm/ttm/ttm_placement.h>
#include <dev/pci/drm/drm_mm.h>

/**
 * Currently we use a mutex for the lock, but a rwlock *may* be
 * more appropriate to reduce scheduling latency if the range manager
 * ends up with very fragmented allocation patterns.
 */

struct ttm_range_manager {
	struct drm_mm mm;
	struct mutex lock;
};

static int ttm_bo_man_get_node(struct ttm_mem_type_manager *man,
			       struct ttm_buffer_object *bo,
			       struct ttm_placement *placement,
			       struct ttm_mem_reg *mem)
{
	struct ttm_range_manager *rman = (struct ttm_range_manager *) man->priv;
	struct drm_mm *mm = &rman->mm;
	struct drm_mm_node *node = NULL;
	unsigned long lpfn;
	int ret;

	lpfn = placement->lpfn;
	if (!lpfn)
		lpfn = man->size;
	do {
		ret = drm_mm_pre_get(mm);
		if (unlikely(ret))
			return ret;

		mtx_enter(&rman->lock);
		node = drm_mm_search_free_in_range(mm,
					mem->num_pages, mem->page_alignment,
					placement->fpfn, lpfn, 1);
		if (unlikely(node == NULL)) {
			mtx_leave(&rman->lock);
			return 0;
		}
		node = drm_mm_get_block_atomic_range(node, mem->num_pages,
						     mem->page_alignment,
						     placement->fpfn,
						     lpfn);
		mtx_leave(&rman->lock);
	} while (node == NULL);

	mem->mm_node = node;
	mem->start = node->start;
	return 0;
}

static void ttm_bo_man_put_node(struct ttm_mem_type_manager *man,
				struct ttm_mem_reg *mem)
{
	struct ttm_range_manager *rman = (struct ttm_range_manager *) man->priv;

	if (mem->mm_node) {
		mtx_enter(&rman->lock);
		drm_mm_put_block(mem->mm_node);
		mtx_leave(&rman->lock);
		mem->mm_node = NULL;
	}
}

static int ttm_bo_man_init(struct ttm_mem_type_manager *man,
			   unsigned long p_size)
{
	struct ttm_range_manager *rman;
	int ret;

	rman = kzalloc(sizeof(*rman), GFP_KERNEL);
	if (!rman)
		return -ENOMEM;

	ret = drm_mm_init(&rman->mm, 0, p_size);
	if (ret) {
		kfree(rman);
		return ret;
	}

	mtx_init(&rman->lock, IPL_NONE);
	man->priv = rman;
	return 0;
}

static int ttm_bo_man_takedown(struct ttm_mem_type_manager *man)
{
	struct ttm_range_manager *rman = (struct ttm_range_manager *) man->priv;
	struct drm_mm *mm = &rman->mm;

	mtx_enter(&rman->lock);
	if (drm_mm_clean(mm)) {
		drm_mm_takedown(mm);
		mtx_leave(&rman->lock);
		kfree(rman);
		man->priv = NULL;
		return 0;
	}
	mtx_leave(&rman->lock);
	return -EBUSY;
}

static void ttm_bo_man_debug(struct ttm_mem_type_manager *man,
			     const char *prefix)
{
	printf("%s stub\n", __func__);
#ifdef notyet
	struct ttm_range_manager *rman = (struct ttm_range_manager *) man->priv;

	mtx_enter(&rman->lock);
	drm_mm_debug_table(&rman->mm, prefix);
	mtx_leave(&rman->lock);
#endif
}

const struct ttm_mem_type_manager_func ttm_bo_manager_func = {
	ttm_bo_man_init,
	ttm_bo_man_takedown,
	ttm_bo_man_get_node,
	ttm_bo_man_put_node,
	ttm_bo_man_debug
};
EXPORT_SYMBOL(ttm_bo_manager_func);
