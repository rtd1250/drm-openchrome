/*
 * Copyright (c) 2012 James Simmons
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
#include "drmP.h"
#include "via_drv.h"

int
ttm_allocate_kernel_buffer(struct ttm_bo_device *bdev, unsigned long size,
				uint32_t alignment, uint32_t domain,
				struct ttm_bo_kmap_obj *kmap)
{
	int ret = via_bo_create(bdev, size, ttm_bo_type_kernel, domain,
				  alignment, PAGE_SIZE, false, NULL,
				  NULL, &kmap->bo);
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
