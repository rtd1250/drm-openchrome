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
 * openchrome_ttm.c
 *
 * TTM code as part of the TTM memory allocator.
 * Currently a basic implementation with no DMA support.
 *
 */


#include "openchrome_drv.h"


static int openchrome_bo_init_mem_type(struct ttm_bo_device *bdev,
				uint32_t type,
				struct ttm_mem_type_manager *man)
{
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	switch (type) {
	case TTM_PL_SYSTEM:
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_FLAG_CACHED;
		man->default_caching = TTM_PL_FLAG_CACHED;
		break;
	case TTM_PL_VRAM:
		man->flags = TTM_MEMTYPE_FLAG_FIXED |
				TTM_MEMTYPE_FLAG_MAPPABLE;
		man->gpu_offset = 0;
		man->available_caching = TTM_PL_FLAG_UNCACHED |
						TTM_PL_FLAG_WC;
		man->default_caching = TTM_PL_FLAG_WC;
		man->func = &ttm_bo_manager_func;
		break;
	default:
		DRM_ERROR("Unsupported TTM memory type.\n");
		ret = -EINVAL;
		break;
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void openchrome_bo_evict_flags(struct ttm_buffer_object *bo,
					struct ttm_placement *placement)
{
	struct openchrome_bo *driver_bo = container_of(bo,
					struct openchrome_bo, ttm_bo);

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (bo->destroy == &openchrome_ttm_bo_destroy) {
		goto exit;
	}

	switch (bo->mem.mem_type) {
	case TTM_PL_VRAM:
		openchrome_ttm_domain_to_placement(driver_bo,
						TTM_PL_FLAG_VRAM);
		break;
	default:
		openchrome_ttm_domain_to_placement(driver_bo,
						TTM_PL_FLAG_SYSTEM);
		break;
	}

	*placement = driver_bo->placement;
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static int openchrome_bo_verify_access(struct ttm_buffer_object *bo,
					struct file *filp)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return 0;
}

static int openchrome_bo_io_mem_reserve(struct ttm_bo_device *bdev,
					struct ttm_mem_reg *mem)
{
	struct openchrome_drm_private *dev_private = container_of(bdev,
					struct openchrome_drm_private, bdev);
	struct drm_device *dev = dev_private->dev;
	struct ttm_mem_type_manager *man = &bdev->man[mem->mem_type];
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (!(man->flags & TTM_MEMTYPE_FLAG_MAPPABLE)) {
		ret= -EINVAL;
		goto exit;
	}

	switch (mem->mem_type) {
	case TTM_PL_SYSTEM:
		mem->bus.addr = NULL;
		mem->bus.base = 0;
		mem->bus.size = mem->num_pages << PAGE_SHIFT;
		mem->bus.offset = 0;
		mem->bus.is_iomem = false;
		break;
	case TTM_PL_VRAM:
		mem->bus.addr = NULL;
		if (dev->pdev->device == PCI_DEVICE_ID_VIA_VX900_VGA) {
			mem->bus.base = pci_resource_start(dev->pdev, 2);
		} else {
			mem->bus.base = pci_resource_start(dev->pdev, 0);
		}

		mem->bus.size = mem->num_pages << PAGE_SHIFT;
		mem->bus.offset = mem->start << PAGE_SHIFT;
		mem->bus.is_iomem = true;
		break;
	default:
		ret = -EINVAL;
		break;
	}

exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void openchrome_bo_io_mem_free(struct ttm_bo_device *bdev,
					struct ttm_mem_reg *mem)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

struct ttm_bo_driver openchrome_bo_driver = {
	.init_mem_type = openchrome_bo_init_mem_type,
	.eviction_valuable = ttm_bo_eviction_valuable,
	.evict_flags = openchrome_bo_evict_flags,
	.verify_access = openchrome_bo_verify_access,
	.io_mem_reserve = openchrome_bo_io_mem_reserve,
	.io_mem_free = openchrome_bo_io_mem_free,
};
