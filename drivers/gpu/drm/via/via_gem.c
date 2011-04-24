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

int via_gem_init_object(struct drm_gem_object *obj)
{
	return 0;
}

void via_gem_free_object(struct drm_gem_object *obj)
{
	struct ttm_buffer_object *bo = obj->driver_private;

	if (bo) {
		obj->driver_private = NULL;
		ttm_bo_unref(&bo);
	}
	drm_gem_object_release(obj);
	kfree(obj);
}

struct drm_gem_object *
via_gem_create(struct drm_device *dev, struct ttm_bo_device *bdev, int types,
		int page_align, unsigned long start, unsigned long size)
{
	struct ttm_buffer_object *bo = NULL;
	struct drm_gem_object *gem;
	int ret;

	/*
	 * VIA hardware access is 128 bits boundries. Modify size 
	 * to be in unites of 128 bit access. For the TTM/GEM layer 
	 * the size needs to rounded to the nearest page. The user
	 * might ask for a offset that is not aligned. In that case
	 * we find the start of the page for this offset and allocate
	 * from there.
	 */
	size = roundup(size, VIA_MM_ALIGN_SIZE);
	size = ALIGN(size, PAGE_SIZE);

	gem = drm_gem_object_alloc(dev, size);
	if (!gem)
		return NULL;

	ret = ttm_bo_allocate(bdev, size, ttm_bo_type_device, types,
				VIA_MM_ALIGN_SIZE, PAGE_SIZE, start,
				false, via_ttm_bo_destroy, gem->filp, &bo);
	if (ret) {
		DRM_ERROR("Failed to create buffer object\n");
		kfree(gem);
		return NULL;
	}
	gem->driver_private = bo;
	return gem;
}
