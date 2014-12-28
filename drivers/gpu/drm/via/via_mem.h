/*
 * Copyright 2014 James Simmons <jsimmons@infradead.org>
 *
 * Influenced by sample code from VIA Technologies and the radeon driver.
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
#ifndef _VIA_MEM_H_
#define _VIA_MEM_H_

#include "drmP.h"
#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_placement.h"

struct ttm_heap {
	uint32_t busy_placements[TTM_NUM_MEM_TYPES];
	uint32_t placements[TTM_NUM_MEM_TYPES];
	struct ttm_buffer_object pbo;
};

extern int via_ttm_init(struct drm_device *dev);
extern struct ttm_tt *via_sgdma_backend_init(struct ttm_bo_device *bdev, unsigned long size,
                                        uint32_t page_flags, struct page *dummy_read_page);

extern int ttm_global_init(struct drm_global_reference *global_ref,
                                struct ttm_bo_global_ref *global_bo,
                                struct ttm_bo_driver *driver,
                                struct ttm_bo_device *bdev,
                                bool dma32);
extern void ttm_global_fini(struct drm_global_reference *global_ref,
                                struct ttm_bo_global_ref *global_bo,
                                struct ttm_bo_device *bdev);

extern int ttm_bo_allocate(struct ttm_bo_device *bdev, unsigned long size,
                                enum ttm_bo_type origin, uint32_t domains,
                                uint32_t byte_align, uint32_t page_align,
                                bool interruptible, struct sg_table *sg,
                                struct ttm_buffer_object **p_bo);
extern void ttm_placement_from_domain(struct ttm_buffer_object *bo,
                                struct ttm_placement *placement, u32 domains,
                                struct ttm_bo_device *bdev);
extern int ttm_bo_unpin(struct ttm_buffer_object *bo, struct ttm_bo_kmap_obj *kmap);
extern int ttm_bo_pin(struct ttm_buffer_object *bo, struct ttm_bo_kmap_obj *kmap);
extern int ttm_allocate_kernel_buffer(struct ttm_bo_device *bdev, unsigned long size,
                                uint32_t alignment, uint32_t domain,
                                struct ttm_bo_kmap_obj *kmap);

extern int ttm_mmap(struct file *filp, struct vm_area_struct *vma);

extern int ttm_gem_open_object(struct drm_gem_object *obj, struct drm_file *file_priv);
extern void ttm_gem_free_object(struct drm_gem_object *obj);
extern struct drm_gem_object *ttm_gem_create(struct drm_device *dev,
					     struct ttm_bo_device *bdev,
					     enum ttm_bo_type origin,
					     int type, bool interruptible,
					     int byte_align, int page_align,
					     unsigned long size);
extern struct ttm_buffer_object *ttm_gem_mapping(struct drm_gem_object *obj);

#endif /* _VIA_MEM_H_ */
