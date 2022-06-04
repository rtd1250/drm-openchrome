/*
 * Copyright © 2018-2019 Kevin Brace
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including
 * the next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/*
 * Author(s):
 *
 * Kevin Brace <kevinbrace@gmx.com>
 */
/*
 * openchrome_object.c
 *
 * Manages Buffer Objects (BO) via TTM.
 * Part of the TTM memory allocator.
 *
 */

#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_ttm_helper.h>

#include <drm/ttm/ttm_bo_api.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_range_manager.h>

#include "openchrome_drv.h"


static void openchrome_gem_free(struct drm_gem_object *obj)
{
	struct ttm_buffer_object *ttm_bo;
	struct openchrome_bo *bo;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ttm_bo = container_of(obj, struct ttm_buffer_object, base);
	bo = container_of(ttm_bo, struct openchrome_bo, ttm_bo);

	ttm_bo_put(&bo->ttm_bo);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static const struct vm_operations_struct openchrome_ttm_bo_vm_ops = {
	.fault = ttm_bo_vm_fault,
	.open = ttm_bo_vm_open,
	.close = ttm_bo_vm_close,
	.access = ttm_bo_vm_access
};

static const struct drm_gem_object_funcs openchrome_gem_object_funcs = {
	.free = openchrome_gem_free,
	.vmap = drm_gem_ttm_vmap,
	.vunmap = drm_gem_ttm_vunmap,
	.mmap = drm_gem_ttm_mmap,
	.vm_ops = &openchrome_ttm_bo_vm_ops,
};

void openchrome_ttm_domain_to_placement(struct openchrome_bo *bo,
					uint32_t ttm_domain)
{
	unsigned i = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	bo->placement.placement = bo->placements;
	bo->placement.busy_placement = bo->placements;

	if (ttm_domain == TTM_PL_SYSTEM) {
		bo->placements[i].fpfn = 0;
		bo->placements[i].lpfn = 0;
		bo->placements[i].mem_type = TTM_PL_SYSTEM;
		bo->placements[i].flags = 0;
		i++;
	}

	if (ttm_domain == TTM_PL_TT) {
		bo->placements[i].fpfn = 0;
		bo->placements[i].lpfn = 0;
		bo->placements[i].mem_type = TTM_PL_TT;
		bo->placements[i].flags = 0;
		i++;
	}

	if (ttm_domain == TTM_PL_VRAM) {
		bo->placements[i].fpfn = 0;
		bo->placements[i].lpfn = 0;
		bo->placements[i].mem_type = TTM_PL_VRAM;
		bo->placements[i].flags = 0;
		i++;
	}

	bo->placement.num_placement = i;
	bo->placement.num_busy_placement = i;

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

void openchrome_ttm_bo_destroy(struct ttm_buffer_object *tbo)
{
	struct openchrome_bo *bo;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	bo = container_of(tbo, struct openchrome_bo, ttm_bo);

	drm_gem_object_release(&bo->ttm_bo.base);
	kfree(bo);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

int openchrome_bo_pin(struct openchrome_bo *bo,
			uint32_t ttm_domain)
{
	struct ttm_operation_ctx ctx = {false, false};
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (bo->ttm_bo.pin_count) {
		goto pin;
	}

	openchrome_ttm_domain_to_placement(bo, ttm_domain);
	ret = ttm_bo_validate(&bo->ttm_bo, &bo->placement, &ctx);
	if (ret) {
		goto exit;
	}

pin:
	ttm_bo_pin(&bo->ttm_bo);
exit:

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void openchrome_bo_unpin(struct openchrome_bo *bo)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ttm_bo_unpin(&bo->ttm_bo);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

int openchrome_bo_create(struct drm_device *dev,
				struct ttm_device *bdev,
				uint64_t size,
				enum ttm_bo_type type,
				uint32_t ttm_domain,
				bool kmap,
				struct openchrome_bo **bo_ptr)
{
	struct via_drm_priv *dev_private = to_via_drm_priv(dev);
	struct openchrome_bo *bo;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

//	bo = kzalloc(sizeof(struct openchrome_bo), GFP_KERNEL);
	bo = kzalloc(sizeof(*bo), GFP_KERNEL);
	if (!bo) {
		DRM_ERROR("Cannot allocate a TTM buffer object.\n");
		ret = -ENOMEM;
		goto exit;
	}

	/*
	 * It is imperative to page align the requested buffer size
	 * prior to a memory allocation request, or various memory
	 * allocation related system instabilities may occur.
	 */
	size = ALIGN(size, PAGE_SIZE);

	ret = drm_gem_object_init(dev, &bo->ttm_bo.base, size);
	if (ret) {
		DRM_ERROR("Cannot initialize a GEM object.\n");
		goto error;
	}

	bo->ttm_bo.base.funcs = &openchrome_gem_object_funcs;

	openchrome_ttm_domain_to_placement(bo, ttm_domain);
	ret = ttm_bo_init(&dev_private->bdev,
				&bo->ttm_bo,
				size,
				type,
				&bo->placement,
				PAGE_SIZE >> PAGE_SHIFT,
				false,
				NULL, NULL,
				openchrome_ttm_bo_destroy);
	if (ret) {
		DRM_ERROR("Cannot initialize a TTM object.\n");
		goto exit;
	}

	if (kmap) {
		ret = ttm_bo_reserve(&bo->ttm_bo, true, false, NULL);
		if (ret) {
			ttm_bo_put(&bo->ttm_bo);
			goto exit;
		}

		ret = openchrome_bo_pin(bo, ttm_domain);
		ttm_bo_unreserve(&bo->ttm_bo);
		if (ret) {
			ttm_bo_put(&bo->ttm_bo);
			goto exit;
		}

		ret = ttm_bo_kmap(&bo->ttm_bo, 0,
					bo->ttm_bo.resource->num_pages,
					&bo->kmap);
		if (ret) {
			ttm_bo_put(&bo->ttm_bo);
			goto exit;
		}
	}

	*bo_ptr = bo;
	goto exit;
error:
	kfree(bo);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void openchrome_bo_destroy(struct openchrome_bo *bo, bool kmap)
{
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (kmap) {
		ttm_bo_kunmap(&bo->kmap);

		ret = ttm_bo_reserve(&bo->ttm_bo, true, false, NULL);
		if (ret) {
			goto exit;
		}

		openchrome_bo_unpin(bo);
		ttm_bo_unreserve(&bo->ttm_bo);
		if (ret) {
			goto exit;
		}
	}

	ttm_bo_put(&bo->ttm_bo);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

int openchrome_mm_init(struct via_drm_priv *dev_private)
{
	struct drm_device *dev = &dev_private->dev;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/*
	 * Initialize bdev ttm_bo_device struct.
	 */
	ret = ttm_device_init(&dev_private->bdev,
				&openchrome_bo_driver,
				dev->dev,
				dev->anon_inode->i_mapping,
				dev->vma_offset_manager,
				false,
				dev_private->need_dma32);
	if (ret) {
		DRM_ERROR("Failed initializing buffer object driver.\n");
		goto exit;
	}

	/*
	 * Initialize TTM range manager for VRAM management.
	 */
	ret = ttm_range_man_init(&dev_private->bdev, TTM_PL_VRAM,
				false,
				dev_private->vram_size >> PAGE_SHIFT);
	if (ret) {
		DRM_ERROR("Failed initializing TTM VRAM memory manager.\n");
		goto exit;
	}

exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void openchrome_mm_fini(struct via_drm_priv *dev_private)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ttm_range_man_fini(&dev_private->bdev, TTM_PL_VRAM);

	ttm_device_fini(&dev_private->bdev);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}
