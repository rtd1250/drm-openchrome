/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
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
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "drmP.h"
#include "via_drv.h"

struct ttm_backend *via_create_ttm_backend_entry(struct ttm_bo_device *bdev)
{
	struct drm_via_private *dev_priv =
		container_of(bdev, struct drm_via_private, bdev);

#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
	if (drm_pci_device_is_agp(dev_priv->dev))
		return ttm_agp_backend_init(bdev, dev_priv->dev->agp->bridge);
#endif
	return ttm_pci_backend_init(bdev, dev_priv->dev);
}

int via_invalidate_caches(struct ttm_bo_device *bdev, uint32_t flags)
{
	/*
	 * FIXME: Invalidate texture caches here.
	 */
	return 0;
}

int via_init_mem_type(struct ttm_bo_device *bdev, uint32_t type,
                      struct ttm_mem_type_manager *man)
{
	struct drm_via_private *dev_priv = 
		container_of(bdev, struct drm_via_private, bdev);
	struct drm_device *dev = dev_priv->dev;

	switch (type) {
	case TTM_PL_SYSTEM:
		/* System memory */
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_MASK_CACHING;
		man->default_caching = TTM_PL_FLAG_CACHED;
		break;

	case TTM_PL_TT:
		man->func = &ttm_bo_manager_func;
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_FLAG_UNCACHED | TTM_PL_FLAG_WC;
		man->default_caching = TTM_PL_FLAG_WC;

#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
		/* In the future support PCI DMA */
		if (drm_pci_device_is_agp(dev)) {
			if (!(drm_core_has_AGP(dev) && dev->agp)) {
				DRM_ERROR("AGP is possible but not enabled\n");	
				return -EINVAL;
			}
		}
#endif
		break;

	case TTM_PL_VRAM:
		/* "On-card" video ram */
		man->func = &ttm_bo_manager_func;
		man->flags = TTM_MEMTYPE_FLAG_FIXED | TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_FLAG_UNCACHED | TTM_PL_FLAG_WC;
		man->default_caching = TTM_PL_FLAG_WC;
		break;

	case TTM_PL_PRIV0:
		/* MMIO region */
		man->func = &ttm_bo_manager_func;
		man->flags = TTM_MEMTYPE_FLAG_FIXED | TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_FLAG_UNCACHED;
		man->default_caching = TTM_PL_FLAG_UNCACHED;
		break;

	default:
		DRM_ERROR("Unsupported memory type %u\n", (unsigned)type);
		return -EINVAL;
	}
	return 0;
}

static void via_evict_flags(struct ttm_buffer_object *bo,
				struct ttm_placement *placement)
{
	static uint32_t flags;

	placement->num_busy_placement = 1;
	placement->num_placement = 1;

	switch (bo->mem.mem_type) {
	case TTM_PL_VRAM:
		flags = TTM_PL_MASK_CACHING | TTM_PL_FLAG_TT;
		break;

	case TTM_PL_TT:
	default:
		flags = TTM_PL_MASK_CACHING | TTM_PL_FLAG_SYSTEM;
		break;
	}
	placement->busy_placement = placement->placement = &flags;
}

static int via_bo_move(struct ttm_buffer_object *bo,
			bool evict, bool interruptible,
			bool no_wait_reserve, bool no_wait_gpu,
			struct ttm_mem_reg *new_mem)
{
	return 0;
}

static int via_ttm_io_mem_reserve(struct ttm_bo_device *bdev, struct ttm_mem_reg *mem)
{
	struct drm_via_private *dev_priv =
		container_of(bdev, struct drm_via_private, bdev);
	struct ttm_mem_type_manager *man = &bdev->man[mem->mem_type];
	struct drm_device *dev = dev_priv->dev;

	mem->bus.base = 0;
	mem->bus.addr = NULL;
	mem->bus.offset = mem->start << PAGE_SHIFT;
	mem->bus.size = mem->num_pages << PAGE_SHIFT;
	mem->bus.is_iomem = true;
	if (!(man->flags & TTM_MEMTYPE_FLAG_MAPPABLE))
		return -EINVAL;

	switch (mem->mem_type) {
	case TTM_PL_SYSTEM:
		/* system memory */
		mem->bus.is_iomem = false;
		mem->bus.offset = 0;
		return 0;

	case TTM_PL_TT:
		mem->bus.offset = mem->start << PAGE_SHIFT;
#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
		if (drm_pci_device_is_agp(dev)) {
			mem->bus.is_iomem = !dev->agp->cant_use_aperture;
			mem->bus.base = dev->agp->base;
		}
#endif
		break;

	case TTM_PL_PRIV0:
		mem->bus.base = pci_resource_start(dev->pdev, 1);
		break;

	case TTM_PL_VRAM:
		mem->bus.base = dev_priv->vram_start;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static void via_ttm_io_mem_free(struct ttm_bo_device *bdev, struct ttm_mem_reg *mem)
{
}

static struct ttm_bo_driver via_bo_driver = {
	.create_ttm_backend_entry 	= via_create_ttm_backend_entry,
	.invalidate_caches 		= via_invalidate_caches,
	.init_mem_type			= via_init_mem_type,
	.evict_flags			= via_evict_flags,
	.move				= via_bo_move,
	/*.verify_access		= via_verify_access,
	.sync_obj_signaled		= via_fence_signalled,
	.sync_obj_wait			= via_fence_wait,
	.sync_obj_flush			= via_fence_flush,
	.sync_obj_unref			= via_fence_unref,
	.sync_obj_ref			= via_fence_ref,*/
	/*.fault_reserve_notify 	= &via_ttm_fault_reserve_notify,*/
	.io_mem_reserve			= &via_ttm_io_mem_reserve,
	.io_mem_free			= &via_ttm_io_mem_free,
};

void
via_ttm_bo_destroy(struct ttm_buffer_object *bo)
{
	kfree(bo);
}

int via_ttm_init(struct drm_via_private *dev_priv)
{
	return ttm_global_init(&dev_priv->mem_global_ref,
				&dev_priv->bo_global_ref,
				&via_bo_driver,
				&dev_priv->bdev, false);
}
