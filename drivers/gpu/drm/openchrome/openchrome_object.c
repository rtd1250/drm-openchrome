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

#include <drm/ttm/ttm_bo_api.h>
#include <drm/ttm/ttm_bo_driver.h>

#include "openchrome_drv.h"


void openchrome_ttm_domain_to_placement(struct openchrome_bo *bo,
					uint32_t ttm_domain)
{
	unsigned i = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	bo->placement.placement = bo->placements;
	bo->placement.busy_placement = bo->placements;

	if (ttm_domain & TTM_PL_FLAG_SYSTEM) {
		bo->placements[i].fpfn = 0;
		bo->placements[i].lpfn = 0;
		bo->placements[i].flags = TTM_PL_FLAG_CACHED |
						TTM_PL_FLAG_SYSTEM;
		i++;
	}

	if (ttm_domain & TTM_PL_FLAG_TT) {
		bo->placements[i].fpfn = 0;
		bo->placements[i].lpfn = 0;
		bo->placements[i].flags = TTM_PL_FLAG_CACHED |
						TTM_PL_FLAG_TT;
		i++;
	}

	if (ttm_domain & TTM_PL_FLAG_VRAM) {
		bo->placements[i].fpfn = 0;
		bo->placements[i].lpfn = 0;
		bo->placements[i].flags = TTM_PL_FLAG_WC |
						TTM_PL_FLAG_UNCACHED |
						TTM_PL_FLAG_VRAM;
		i++;
	}

	bo->placement.num_placement = i;
	bo->placement.num_busy_placement = i;

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

void openchrome_ttm_bo_destroy(struct ttm_buffer_object *tbo)
{
	struct openchrome_bo *bo = container_of(tbo,
					struct openchrome_bo, ttm_bo);

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	drm_gem_object_release(&bo->gem);
	kfree(bo);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

int openchrome_bo_pin(struct openchrome_bo *bo,
			uint32_t ttm_domain)
{
	struct ttm_operation_ctx ctx = {false, false};
	uint32_t i;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	openchrome_ttm_domain_to_placement(bo, ttm_domain);
	for (i = 0; i < bo->placement.num_placement; i++) {
		bo->placements[i].flags |= TTM_PL_FLAG_NO_EVICT;
	}

	ret = ttm_bo_validate(&bo->ttm_bo, &bo->placement, &ctx);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

int openchrome_bo_unpin(struct openchrome_bo *bo)
{
	struct ttm_operation_ctx ctx = {false, false};
	uint32_t i;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	for (i = 0; i < bo->placement.num_placement; i++) {
		bo->placements[i].flags &= ~TTM_PL_FLAG_NO_EVICT;
	}

	ret = ttm_bo_validate(&bo->ttm_bo, &bo->placement, &ctx);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

int openchrome_bo_create(struct drm_device *dev,
				struct ttm_bo_device *bdev,
				uint64_t size,
				enum ttm_bo_type type,
				uint32_t ttm_domain,
				bool kmap,
				struct openchrome_bo **bo_ptr)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct openchrome_bo *bo;
	size_t acc_size;
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

	ret = drm_gem_object_init(dev, &bo->gem, size);
	if (ret) {
		DRM_ERROR("Cannot initialize a GEM object.\n");
		goto error;
	}

	openchrome_ttm_domain_to_placement(bo, ttm_domain);
	acc_size = ttm_bo_dma_acc_size(&dev_private->bdev, size,
					sizeof(struct openchrome_bo));
	ret = ttm_bo_init(&dev_private->bdev,
				&bo->ttm_bo,
				size,
				type,
				&bo->placement,
				PAGE_SIZE >> PAGE_SHIFT,
				false, acc_size,
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
					bo->ttm_bo.num_pages,
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

		ret = openchrome_bo_unpin(bo);
		ttm_bo_unreserve(&bo->ttm_bo);
		if (ret) {
			goto exit;
		}
	}

	ttm_bo_put(&bo->ttm_bo);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

int openchrome_mm_init(struct openchrome_drm_private *dev_private)
{
	struct drm_device *dev = dev_private->dev;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	drm_vma_offset_manager_init(&dev_private->vma_manager,
					DRM_FILE_PAGE_OFFSET_START,
					DRM_FILE_PAGE_OFFSET_SIZE);

	/*
	 * Initialize bdev ttm_bo_device struct.
	 */
	ret = ttm_bo_device_init(&dev_private->bdev,
				&openchrome_bo_driver,
				dev->anon_inode->i_mapping,
				&dev_private->vma_manager,
				dev_private->need_dma32);
	if (ret) {
		DRM_ERROR("Failed initializing buffer object driver.\n");
		goto exit;
	}

	/*
	 * Initialize TTM memory manager for VRAM management.
	 */
	ret = ttm_bo_init_mm(&dev_private->bdev, TTM_PL_VRAM,
				dev_private->vram_size >> PAGE_SHIFT);
	if (ret) {
		DRM_ERROR("Failed initializing TTM VRAM memory manager.\n");
		goto exit;
	}

exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void openchrome_mm_fini(struct openchrome_drm_private *dev_private)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ttm_bo_device_release(&dev_private->bdev);
	drm_vma_offset_manager_destroy(&dev_private->vma_manager);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}
