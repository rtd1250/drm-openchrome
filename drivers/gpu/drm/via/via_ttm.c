/*
 * Copyright 2011 James Simmons <jsimmons@infradead.org>. All Rights Reserved.
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

#if __OS_HAS_AGP
	if (drm_pci_device_is_agp(dev_priv->dev))
		return ttm_agp_backend_init(bdev, dev_priv->dev->agp->bridge);
#endif
	return via_sgdma_backend_init(bdev, dev_priv->dev);
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
#if __OS_HAS_AGP
	struct drm_via_private *dev_priv =
		container_of(bdev, struct drm_via_private, bdev);
	struct drm_device *dev = dev_priv->dev;
#endif

	switch (type) {
	case TTM_PL_SYSTEM:
		/* System memory */
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_MASK_CACHING;
		man->default_caching = TTM_PL_FLAG_CACHED;
		break;

	case TTM_PL_TT:
		man->func = &ttm_bo_manager_func;

		/* By default we handle PCI/PCIe DMA. If AGP is avaliable
		 * then we use that instead */
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE | TTM_MEMTYPE_FLAG_CMA;
		man->available_caching = TTM_PL_MASK_CACHING;
		man->default_caching = TTM_PL_FLAG_CACHED;

#if __OS_HAS_AGP
		if (drm_pci_device_is_agp(dev)) {
			if (drm_core_has_AGP(dev) && dev->agp) {
				man->flags = TTM_MEMTYPE_FLAG_MAPPABLE;
				man->available_caching = TTM_PL_FLAG_UNCACHED | TTM_PL_FLAG_WC;
				man->default_caching = TTM_PL_FLAG_WC;
			} else
				DRM_ERROR("AGP is possible but not enabled\n");
		}
#endif
		break;

	case TTM_PL_VRAM:
		/* "On-card" video ram */
		man->func = &ttm_bo_manager_func;
		man->flags = TTM_MEMTYPE_FLAG_FIXED | TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_FLAG_UNCACHED | TTM_PL_FLAG_WC;
		man->default_caching = TTM_PL_FLAG_WC;
		/* The display base address does not always equal the start of
		 * the memory region of the VRAM. In our case it is */
		man->gpu_offset = 0;
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

/* Move between GART and VRAM */
static int
via_move_blit(struct ttm_buffer_object *bo, bool evict,
		bool no_wait_reserve, bool no_wait_gpu,
		struct ttm_mem_reg *new_mem,
		struct ttm_mem_reg *old_mem)
{
	unsigned long old_start, new_start;
	void *fence = NULL;

	/* Real CPU physical address */
	old_start = (old_mem->start << PAGE_SHIFT) + old_mem->bus.base;
	new_start = (new_mem->start << PAGE_SHIFT) + new_mem->bus.base;

	//ret = via_copy(rdev, old_start, new_start, new_mem->num_pages, fence);

	return ttm_bo_move_accel_cleanup(bo, fence, NULL, evict,
					no_wait_reserve, no_wait_gpu, new_mem);
}

static int
via_move_from_vram(struct ttm_buffer_object *bo, bool interruptible,
			bool no_wait_reserve, bool no_wait_gpu,
			struct ttm_mem_reg *new_mem)
{
	struct ttm_mem_reg *old_mem = &bo->mem;
	struct ttm_mem_reg tmp_mem;
	struct ttm_placement placement;
	u32 placements;
	int ret;

	tmp_mem = *new_mem;
	tmp_mem.mm_node = NULL;
	placement.fpfn = 0;
	placement.lpfn = 0;
	placement.num_placement = 1;
	placement.placement = &placements;
	placement.num_busy_placement = 1;
	placement.busy_placement = &placements;
	placements = TTM_PL_MASK_CACHING | TTM_PL_FLAG_TT;
	ret = ttm_bo_mem_space(bo, &placement, &tmp_mem,
				interruptible, no_wait_reserve, no_wait_gpu);
	if (unlikely(ret))
		return ret;

	/* Allocate some DMA memory for the GART address space */
	ret = ttm_tt_set_placement_caching(bo->ttm, tmp_mem.placement);
	if (unlikely(ret))
		goto out_cleanup;

	ret = ttm_tt_bind(bo->ttm, &tmp_mem);
	if (unlikely(ret))
		goto out_cleanup;

	/* Move from the VRAM to GART space */
	ret = via_move_blit(bo, true, no_wait_reserve, no_wait_gpu, &tmp_mem, old_mem);
	if (unlikely(ret))
		goto out_cleanup;

	/* Expose the GART region to the system memory */
	ret = ttm_bo_move_ttm(bo, true, no_wait_reserve, no_wait_gpu, new_mem);
out_cleanup:
	ttm_bo_mem_put(bo, &tmp_mem);
	return ret;
}

static int
via_move_to_vram(struct ttm_buffer_object *bo, bool interruptible,
		bool no_wait_reserve, bool no_wait_gpu,
		struct ttm_mem_reg *new_mem)
{
	struct ttm_mem_reg *old_mem = &bo->mem;
	struct ttm_mem_reg tmp_mem;
	struct ttm_placement placement;
	u32 placements;
	int ret;

	tmp_mem = *new_mem;
	tmp_mem.mm_node = NULL;
	placement.fpfn = 0;
	placement.lpfn = 0;
	placement.num_placement = 1;
	placement.placement = &placements;
	placement.num_busy_placement = 1;
	placement.busy_placement = &placements;
	placements = TTM_PL_MASK_CACHING | TTM_PL_FLAG_TT;
	ret = ttm_bo_mem_space(bo, &placement, &tmp_mem,
				interruptible, no_wait_reserve, no_wait_gpu);
	if (unlikely(ret))
		return ret;

	/* Expose the GART region to the system memory */
	ret = ttm_bo_move_ttm(bo, true, no_wait_reserve, no_wait_gpu, &tmp_mem);
	if (unlikely(ret))
		goto out_cleanup;

	/* Move from the GART to VRAM */
	ret = via_move_blit(bo, true, no_wait_reserve, no_wait_gpu, new_mem, old_mem);
out_cleanup:
	ttm_bo_mem_put(bo, &tmp_mem);
	return ret;
}

static int via_bo_move(struct ttm_buffer_object *bo,
			bool evict, bool interruptible,
			bool no_wait_reserve, bool no_wait_gpu,
			struct ttm_mem_reg *new_mem)
{
	struct ttm_mem_reg *old_mem = &bo->mem;
	int ret = 0;

	/* No real memory copy. Just use the new_mem
	 * directly. */
	if ((old_mem->mem_type == TTM_PL_SYSTEM &&
	    (new_mem->mem_type == TTM_PL_TT || !bo->ttm)) ||
	    (old_mem->mem_type == TTM_PL_TT &&
	     new_mem->mem_type == TTM_PL_SYSTEM) ||
	    (new_mem->mem_type == TTM_PL_PRIV0)) {
		BUG_ON(old_mem->mm_node != NULL);
		*old_mem = *new_mem;
		new_mem->mm_node = NULL;
		return ret;
	}

	/* Accelerated copy involving the VRAM. */
	if (new_mem->mem_type == TTM_PL_SYSTEM) {
		ret = via_move_from_vram(bo, interruptible, no_wait_reserve,
					no_wait_gpu, new_mem);
	} else if (old_mem->mem_type == TTM_PL_SYSTEM) {
		ret = via_move_to_vram(bo, interruptible, no_wait_reserve,
					no_wait_gpu, new_mem);
	} else {
		ret = via_move_blit(bo, evict, no_wait_reserve,
					no_wait_gpu, new_mem, old_mem);
	}

	if (ret) {
		ret = ttm_bo_move_memcpy(bo, evict, no_wait_reserve,
					no_wait_gpu, new_mem);
	}
	return ret;
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
#if __OS_HAS_AGP
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
		if (dev->pci_device == 0x7122)
			mem->bus.base = pci_resource_start(dev->pdev, 2);
		else
			mem->bus.base = pci_resource_start(dev->pdev, 0);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static void via_ttm_io_mem_free(struct ttm_bo_device *bdev, struct ttm_mem_reg *mem)
{
}

static int via_verify_access(struct ttm_buffer_object *bo, struct file *filp)
{
	return 0;
}

static struct ttm_bo_driver via_bo_driver = {
	.create_ttm_backend_entry	= via_create_ttm_backend_entry,
	.invalidate_caches		= via_invalidate_caches,
	.init_mem_type			= via_init_mem_type,
	.evict_flags			= via_evict_flags,
	.move				= via_bo_move,
	.verify_access			= via_verify_access,
	/*.sync_obj_signaled		= via_fence_signalled,
	.sync_obj_wait			= via_fence_wait,
	.sync_obj_flush			= via_fence_flush,
	.sync_obj_unref			= via_fence_unref,
	.sync_obj_ref			= via_fence_ref,*/
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
