/*
 * Copyright 2012 James Simmons <jsimmons@infradead.org>. All Rights Reserved.
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
 * THE AUTHOR(S) OR COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/dma-mapping.h>
#ifdef CONFIG_SWIOTLB
#include <linux/swiotlb.h>
#endif

#include "openchrome_drv.h"


#define DRM_FILE_PAGE_OFFSET (0x100000000ULL >> PAGE_SHIFT)

static void via_ttm_bo_destroy(struct ttm_buffer_object *bo)
{
	struct ttm_heap *heap = container_of(bo, struct ttm_heap, bo);

	kfree(heap);
	heap = NULL;
}

static int via_invalidate_caches(struct ttm_bo_device *bdev,
					uint32_t flags)
{
	/*
	 * FIXME: Invalidate texture caches here.
	 */
	return 0;
}

static int via_init_mem_type(struct ttm_bo_device *bdev, uint32_t type,
				struct ttm_mem_type_manager *man)
{
	switch (type) {
	case TTM_PL_SYSTEM:
		/* System memory */
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_MASK_CACHING;
		man->default_caching = TTM_PL_FLAG_CACHED;
		break;

	case TTM_PL_TT:
		man->func = &ttm_bo_manager_func;

		/* By default we handle PCI/PCIe DMA. */
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE |
				TTM_MEMTYPE_FLAG_CMA;
		man->available_caching = TTM_PL_MASK_CACHING;
		man->default_caching = TTM_PL_FLAG_CACHED;
		break;

	case TTM_PL_VRAM:
		/* "On-card" video ram */
		man->func = &ttm_bo_manager_func;
		man->flags = TTM_MEMTYPE_FLAG_FIXED |
				TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_FLAG_UNCACHED |
						TTM_PL_FLAG_WC;
		man->default_caching = TTM_PL_FLAG_WC;
		/* The display base address does not always equal the start of
		 * the memory region of the VRAM. In our case it is */
		man->gpu_offset = 0;
		break;

	case TTM_PL_PRIV:
		/* MMIO region */
		man->func = &ttm_bo_manager_func;
		man->flags = TTM_MEMTYPE_FLAG_FIXED |
				TTM_MEMTYPE_FLAG_MAPPABLE;
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
	switch (bo->mem.mem_type) {
	case TTM_PL_VRAM:
		ttm_placement_from_domain(bo, placement,
				TTM_PL_FLAG_TT | TTM_PL_FLAG_SYSTEM,
				bo->bdev);
		break;

	case TTM_PL_TT:
	default:
		ttm_placement_from_domain(bo, placement,
						TTM_PL_FLAG_SYSTEM,
						bo->bdev);
		break;
	}
}

static int via_ttm_io_mem_reserve(struct ttm_bo_device *bdev,
					struct ttm_mem_reg *mem)
{
	struct openchrome_drm_private *dev_private =
					container_of(bdev,
					struct openchrome_drm_private,
					ttm.bdev);
	struct ttm_mem_type_manager *man = &bdev->man[mem->mem_type];
	struct drm_device *dev = dev_private->dev;

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
		break;

	case TTM_PL_PRIV:
		mem->bus.base = pci_resource_start(dev->pdev, 1);
		break;

	case TTM_PL_VRAM:
		if (dev->pdev->device == PCI_DEVICE_ID_VIA_VX900_VGA)
			mem->bus.base = pci_resource_start(dev->pdev, 2);
		else
			mem->bus.base = pci_resource_start(dev->pdev, 0);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static void via_ttm_io_mem_free(struct ttm_bo_device *bdev,
				struct ttm_mem_reg *mem)
{
}

static int via_verify_access(struct ttm_buffer_object *bo,
				struct file *filp)
{
	return 0;
}

static struct ttm_bo_driver via_bo_driver = {
	.invalidate_caches	= via_invalidate_caches,
	.init_mem_type		= via_init_mem_type,
	.evict_flags		= via_evict_flags,
	.verify_access		= via_verify_access,
	.io_mem_reserve		= via_ttm_io_mem_reserve,
	.io_mem_free		= via_ttm_io_mem_free,
};

int via_mm_init(struct openchrome_drm_private *dev_private)
{
	struct drm_device *dev = dev_private->dev;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	dev_private->ttm.bdev.dev_mapping = dev->anon_inode->i_mapping;

	ret = ttm_bo_device_init(&dev_private->ttm.bdev,
				&via_bo_driver,
				dev->anon_inode->i_mapping,
				DRM_FILE_PAGE_OFFSET,
				false);
	if (ret) {
		DRM_ERROR("Error initialising bo driver: %d\n", ret);
		goto exit;
	}

	ret = ttm_bo_init_mm(&dev_private->ttm.bdev, TTM_PL_VRAM,
				dev_private->vram_size >> PAGE_SHIFT);
	if (ret) {
		DRM_ERROR("Failed to map video RAM: %d\n", ret);
		goto exit;
	}

exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void via_mm_fini(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ttm_bo_device_release(&dev_private->ttm.bdev);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

/*
 * the buffer object domain
 */
void ttm_placement_from_domain(struct ttm_buffer_object *bo,
				struct ttm_placement *placement,
				u32 domains,
				struct ttm_bo_device *bdev)
{
	struct ttm_heap *heap = container_of(bo, struct ttm_heap, bo);
	int cnt = 0, i = 0;

	if (!(domains & TTM_PL_MASK_MEM))
		domains = TTM_PL_FLAG_SYSTEM;

	do {
		int domain = (domains & (1 << i));

		if (domain) {
			heap->busy_placements[cnt].flags =
				(domain | bdev->man[i].default_caching);
			heap->busy_placements[cnt].fpfn =
				heap->busy_placements[cnt].lpfn = 0;
			heap->placements[cnt].flags =
				(domain | bdev->man[i].available_caching);
			heap->placements[cnt].fpfn =
				heap->placements[cnt].lpfn = 0;
			cnt++;
		}
	} while (i++ < TTM_NUM_MEM_TYPES);

	placement->num_busy_placement = placement->num_placement = cnt;
	placement->busy_placement = heap->busy_placements;
	placement->placement = heap->placements;
}

int via_bo_create(struct ttm_bo_device *bdev,
			struct ttm_buffer_object **p_bo,
			unsigned long size,
			enum ttm_bo_type type,
			uint32_t domains,
			uint32_t byte_alignment,
			uint32_t page_alignment,
			bool interruptible,
			struct sg_table *sg,
			struct reservation_object *resv)
{
	struct ttm_buffer_object *bo = NULL;
	struct ttm_placement placement;
	struct ttm_heap *heap;
	size_t acc_size;
	int ret = -ENOMEM;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	size = round_up(size, byte_alignment);
	size = ALIGN(size, page_alignment);

	heap = kzalloc(sizeof(struct ttm_heap), GFP_KERNEL);
	if (unlikely(!heap)) {
		DRM_ERROR("Failed to allocate kernel memory.");
		goto exit;
	}

	bo = &heap->bo;

	ttm_placement_from_domain(bo, &placement, domains, bdev);

	acc_size = ttm_bo_dma_acc_size(bdev, size,
					sizeof(struct ttm_heap));

	ret = ttm_bo_init(bdev, bo, size, type, &placement,
				page_alignment >> PAGE_SHIFT,
				interruptible, acc_size,
				sg, NULL, via_ttm_bo_destroy);

	if (unlikely(ret)) {
		DRM_ERROR("Failed to initialize a TTM Buffer Object.");
		goto error;
	}

	*p_bo = bo;
	goto exit;
error:
	kfree(heap);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

int via_bo_pin(struct ttm_buffer_object *bo,
		struct ttm_bo_kmap_obj *kmap)
{
	struct ttm_heap *heap = container_of(bo, struct ttm_heap, bo);
	struct ttm_placement placement;
	struct ttm_operation_ctx ctx = {
					.interruptible = false,
					.no_wait_gpu = false
					};
	int ret;

	ret = ttm_bo_reserve(bo, true, false, NULL);
	if (!ret) {
		placement.placement = heap->placements;
		placement.num_placement = 1;

		heap->placements[0].flags = (bo->mem.placement | TTM_PL_FLAG_NO_EVICT);
		ret = ttm_bo_validate(bo, &placement, &ctx);
		if (!ret && kmap)
			ret = ttm_bo_kmap(bo, 0, bo->num_pages, kmap);
		ttm_bo_unreserve(bo);
	}
	return ret;
}

int via_bo_unpin(struct ttm_buffer_object *bo,
			struct ttm_bo_kmap_obj *kmap)
{
	struct ttm_heap *heap = container_of(bo, struct ttm_heap, bo);
	struct ttm_placement placement;
	struct ttm_operation_ctx ctx = {
					.interruptible = false,
					.no_wait_gpu = false
					};
	int ret;

	ret = ttm_bo_reserve(bo, true, false, NULL);
	if (!ret) {
		if (kmap)
			ttm_bo_kunmap(kmap);

		placement.placement = heap->placements;
		placement.num_placement = 1;

		heap->placements[0].flags = (bo->mem.placement & ~TTM_PL_FLAG_NO_EVICT);
		ret = ttm_bo_validate(bo, &placement, &ctx);
		ttm_bo_unreserve(bo);
	}
	return ret;
}

int via_ttm_allocate_kernel_buffer(struct ttm_bo_device *bdev,
					unsigned long size,
					uint32_t alignment,
					uint32_t domain,
					struct ttm_bo_kmap_obj *kmap)
{
	int ret = via_bo_create(bdev, &kmap->bo, size,
				ttm_bo_type_kernel, domain,
				alignment, PAGE_SIZE,
				false, NULL, NULL);
	if (likely(!ret)) {
		ret = via_bo_pin(kmap->bo, kmap);
		if (unlikely(ret)) {
			DRM_ERROR("failed to mmap the buffer\n");
			ttm_bo_unref(&kmap->bo);
			kmap->bo = NULL;
		}
	}
	return ret;
}
