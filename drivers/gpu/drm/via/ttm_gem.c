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

int ttm_gem_init_object(struct drm_gem_object *obj)
{
	return 0;
}

void ttm_gem_free_object(struct drm_gem_object *obj)
{
	struct ttm_buffer_object *bo = obj->driver_private;

	if (bo) {
		obj->driver_private = NULL;
		ttm_bo_unref(&bo);
	}
	drm_gem_object_release(obj);
	kfree(obj);
}

void ttm_gem_vm_open(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct ttm_buffer_object *bo = obj->driver_private;

	if (bo)
		(void)ttm_bo_reference(bo);
	drm_gem_vm_open(vma);
}

void ttm_gem_vm_close(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct ttm_buffer_object *bo = obj->driver_private;

	if (bo)
		ttm_bo_unref(&bo);
	drm_gem_vm_close(vma);
	vma->vm_private_data = NULL;
}

static const struct vm_operations_struct *ttm_vm_ops = NULL;

int ttm_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct ttm_bo_object *bo = obj->driver_private;
	int ret;

	vma->vm_private_data = bo;
	ret =  ttm_vm_ops->fault(vma, vmf);
	vma->vm_private_data = obj;
	return ret;
}

int ttm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_via_private *dev_priv;
	struct drm_file *file_priv;
	int ret = -EINVAL;

	if (unlikely(vma->vm_pgoff < DRM_FILE_PAGE_OFFSET))
		return drm_mmap(filp, vma);

	file_priv = filp->private_data;
	dev_priv = file_priv->minor->dev->dev_private;
	if (!dev_priv)
		return ret;

	ret = ttm_bo_mmap(filp, vma, &dev_priv->bdev);
	if (unlikely(ret != 0))
		return ret;

	if (unlikely(ttm_vm_ops == NULL))
		ttm_vm_ops = vma->vm_ops;
	return drm_gem_mmap(filp, vma);
}

struct drm_gem_object *
ttm_gem_create(struct drm_device *dev, struct ttm_bo_device *bdev, int types,
		bool interruptible, int byte_align, int page_align,
		unsigned long start, unsigned long size,
                void (*destroy) (struct ttm_buffer_object *))
{
	struct ttm_buffer_object *bo = NULL;
	struct drm_gem_object *obj;
	int ret;

	size = roundup(size, byte_align);
	size = ALIGN(size, page_align);

	obj = drm_gem_object_alloc(dev, size);
	if (!obj)
		return NULL;

	ret = ttm_bo_allocate(bdev, size, ttm_bo_type_device, types,
				byte_align, page_align, start, interruptible,
				destroy, obj->filp, &bo);
	if (ret) {
		DRM_ERROR("Failed to create buffer object\n");
		kfree(obj);
		return NULL;
	}
	obj->driver_private = bo;
	return obj;
}
