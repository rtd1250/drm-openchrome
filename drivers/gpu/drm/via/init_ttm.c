/*
 * Copyright (c) 2010 James Simmons
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
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
 */

#include "ttm_heap.h"

#define DRM_FILE_PAGE_OFFSET (0x100000000ULL >> PAGE_SHIFT)

static int
ttm_global_mem_init(struct drm_global_reference *ref)
{
	return ttm_mem_global_init(ref->object);
}

static void
ttm_global_mem_release(struct drm_global_reference *ref)
{
	ttm_mem_global_release(ref->object);
}

void
ttm_global_fini(struct drm_global_reference *global_ref,
			struct ttm_bo_global_ref *global_bo,
			struct ttm_bo_device *bdev)
{
	if (global_ref->release == NULL)
		return;

	if (bdev)
		ttm_bo_device_release(bdev);
	drm_global_item_unref(&global_bo->ref);
	drm_global_item_unref(global_ref);
	global_ref->release = NULL;
}

int
ttm_global_init(struct drm_global_reference *global_ref,
			struct ttm_bo_global_ref *global_bo,
			struct ttm_bo_driver *driver,
			struct ttm_bo_device *bdev,
			bool dma32)
{
	struct drm_global_reference *bo_ref;
	int rc;

	global_ref->global_type = DRM_GLOBAL_TTM_MEM;
	global_ref->size = sizeof(struct ttm_mem_global);
	global_ref->init = &ttm_global_mem_init;
	global_ref->release = &ttm_global_mem_release;

	rc = drm_global_item_ref(global_ref);
	if (unlikely(rc != 0)) {
		DRM_ERROR("Failed setting up TTM memory accounting\n");
		global_ref->release = NULL;
		return rc;
	}

	global_bo->mem_glob = global_ref->object;
	bo_ref = &global_bo->ref;
	bo_ref->global_type = DRM_GLOBAL_TTM_BO;
	bo_ref->size = sizeof(struct ttm_bo_global);
	bo_ref->init = &ttm_bo_global_init;
	bo_ref->release = &ttm_bo_global_release;

	rc = drm_global_item_ref(bo_ref);
	if (unlikely(rc != 0)) {
		DRM_ERROR("Failed setting up TTM BO subsystem\n");
		drm_global_item_unref(global_ref);
		global_ref->release = NULL;
		return rc;
	}

	rc = ttm_bo_device_init(bdev, bo_ref->object, driver,
				DRM_FILE_PAGE_OFFSET, dma32);
	if (rc) {
		DRM_ERROR("Error initialising bo driver: %d\n", rc);
		ttm_global_fini(global_ref, global_bo, NULL);
	}
	return rc;
}

int
ttm_bo_allocate(struct ttm_bo_device *bdev,
		unsigned long size,
		enum ttm_bo_type origin,
		int types,
		uint32_t byte_align,
		uint32_t page_align,
		unsigned long buffer_start,
		bool interruptible,
		void (*destroy) (struct ttm_buffer_object *),
		struct file *persistant_swap_storage,
		struct ttm_buffer_object **p_bo)
{
	uint32_t flags[TTM_NUM_MEM_TYPES], *ptr;
	struct ttm_buffer_object *bo = NULL;
	struct ttm_placement *placement;
	int cnt = 0, i = 0, ret;
	struct ttm_heap *heap;
	char *p;

	size = roundup(size, byte_align);
	size = ALIGN(size, page_align);

	do {
		int type = (types & (1 << i));

		if (type)
			flags[cnt++] = (type | bdev->man[i].available_caching);
	} while (i++ < TTM_NUM_MEM_TYPES);

	p = kzalloc(sizeof(struct ttm_heap) + cnt * sizeof(uint32_t), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	heap = (struct ttm_heap *) p;
	bo = &heap->pbo;
	placement = &heap->placement;
	ptr = (uint32_t *) (p + sizeof(struct ttm_heap));

	/* Special work around for old driver's api */
	if (buffer_start && cnt == 1 && origin != ttm_bo_type_user) {
		placement->fpfn = rounddown(buffer_start, page_align) >> PAGE_SHIFT;
		placement->lpfn = placement->fpfn + (size >> PAGE_SHIFT);
		buffer_start = 0;
	}
	placement->num_placement = placement->num_busy_placement = cnt;
	placement->busy_placement = ptr;
	placement->placement = ptr;

	for (i = 0; i < cnt; i++)
		*ptr++ = flags[i];

	ret = ttm_bo_init(bdev, bo, size, origin, placement,
				page_align >> PAGE_SHIFT, buffer_start,
				interruptible, persistant_swap_storage,
				size, destroy);
	if (!ret)
		*p_bo = bo;
	else
		kfree(p);
	return ret;
}

int
ttm_bo_pin(struct ttm_buffer_object *bo, struct ttm_bo_kmap_obj *kmap)
{
	struct ttm_heap *heap = container_of(bo, struct ttm_heap, pbo);
	struct ttm_placement *placement = &heap->placement;
	uint32_t *placements;
	int ret, i;

	ret = ttm_bo_reserve(bo, true, false, false, 0);
	if (!ret) {
		placements = (uint32_t *) placement->placement;
		for (i = 0; i < placement->num_placement; i++)
			placements[i] |= TTM_PL_FLAG_NO_EVICT;
		ret = ttm_bo_validate(bo, placement, false, false, false);
		if (!ret && kmap)
			ret = ttm_bo_kmap(bo, 0, bo->num_pages, kmap);
		ttm_bo_unreserve(bo);
	}
	return ret;
}

int
ttm_bo_unpin(struct ttm_buffer_object *bo, struct ttm_bo_kmap_obj *kmap)
{
	struct ttm_heap *heap = container_of(bo, struct ttm_heap, pbo);
	struct ttm_placement *placement = &heap->placement;
	uint32_t *placements;
	int ret, i;

	ret = ttm_bo_reserve(bo, true, false, false, 0);
	if (!ret) {
		if (kmap)
			ttm_bo_kunmap(kmap);
		placements = (uint32_t *) placement->placement;
		for (i = 0; i < placement->num_placement; i++)
			placements[i] &= ~TTM_PL_FLAG_NO_EVICT;
		ret = ttm_bo_validate(bo, placement, false, false, false);
		ttm_bo_unreserve(bo);
	}
	return ret;
}
