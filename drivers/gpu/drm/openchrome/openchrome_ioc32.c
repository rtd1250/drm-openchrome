/*
 * Copyright 2006 Tungsten Graphics Inc., Bismarck, ND., USA.
 * All rights reserved.
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
 * THE AUTHORS OR COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/*
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#include <drm/drm_ioctl.h>

#include "openchrome_drv.h"


static int
via_getparam(struct drm_device *dev, void *data,
		struct drm_file *filp)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct drm_via_param *args = data;
	int ret = 0;

	switch (args->param) {
	case VIA_PARAM_CHIPSET_ID:
		args->value = dev->pdev->device;
		break;
	case VIA_PARAM_REVISION_ID:
		args->value = dev_private->revision;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

/* Not yet supported */
static int
via_setparam(struct drm_device *dev, void *data,
		struct drm_file *filp)
{
	return -EINVAL;
}

static int
via_gem_alloc(struct drm_device *dev, void *data,
		struct drm_file *filp)
{
	struct drm_via_gem_object *args = data;
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct openchrome_bo *bo;
	uint32_t handle;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ret = openchrome_bo_create(dev,
					&dev_private->bdev,
					args->size,
					ttm_bo_type_device,
					args->domains,
					false,
					&bo);

	if (ret) {
		goto exit;
	}

	ret = drm_gem_handle_create(filp, &bo->gem,
					&handle);

	/* Drop reference from allocate; handle holds it now. */
	drm_gem_object_put_unlocked(&bo->gem);

	if (ret) {
		openchrome_bo_destroy(bo, false);
		goto exit;
	}

	args->size		= bo->ttm_bo.mem.size;
	args->domains		= bo->ttm_bo.mem.placement &
						TTM_PL_MASK_MEM;
	args->offset		= bo->ttm_bo.offset;
	args->map_handle	= drm_vma_node_offset_addr(
						&bo->ttm_bo.vma_node);
	args->handle		= handle;
	args->version		= 1;

exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int
via_gem_state(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct ttm_operation_ctx ctx = {.interruptible = false,
					.no_wait_gpu = false};
	struct drm_gem_object *gem;
	struct drm_via_gem_object *args = data;
	struct openchrome_bo *bo;
	int ret = -EINVAL;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	gem = drm_gem_object_lookup(file_priv, args->handle);
	if (!gem) {
		goto exit;
	}

	bo = container_of(gem, struct openchrome_bo, gem);

	/* Don't bother to migrate to same domain */
	args->domains &= ~(bo->ttm_bo.mem.placement & TTM_PL_MASK_MEM);
	if (args->domains) {
		ret = ttm_bo_reserve(&bo->ttm_bo, true, false, NULL);
		if (ret) {
			goto exit;
		}

		openchrome_ttm_domain_to_placement(bo, args->domains);
		ret = ttm_bo_validate(&bo->ttm_bo, &bo->placement,
					&ctx);
		ttm_bo_unreserve(&bo->ttm_bo);

		if (!ret) {
			args->size = bo->ttm_bo.mem.size;
			args->domains = bo->ttm_bo.mem.placement &
						TTM_PL_MASK_MEM;
			args->offset = bo->ttm_bo.offset;
			args->map_handle = drm_vma_node_offset_addr(
						&bo->ttm_bo.vma_node);
		}
	}

	mutex_lock(&dev->struct_mutex);
	drm_gem_object_put(gem);
	mutex_unlock(&dev->struct_mutex);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int
via_gem_wait(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_gem_object *gem;
	struct drm_via_gem_wait *args = data;
	struct openchrome_bo *bo;
	int ret = -EINVAL;
	bool no_wait;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	gem = drm_gem_object_lookup(file_priv, args->handle);
	if (!gem) {
		goto exit;
	}

	bo = container_of(gem, struct openchrome_bo, gem);

	no_wait = (args->no_wait != 0);
	ret = ttm_bo_reserve(&bo->ttm_bo, true, no_wait, NULL);
	if (ret) {
		goto exit;
	}

	ret = ttm_bo_wait(&bo->ttm_bo, true, no_wait);
	ttm_bo_unreserve(&bo->ttm_bo);

	mutex_lock(&dev->struct_mutex);
	drm_gem_object_put(gem);
	mutex_unlock(&dev->struct_mutex);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

#define KMS_INVALID_IOCTL(name)						\
int name(struct drm_device *dev, void *data, struct drm_file *file_priv)\
{									\
	DRM_ERROR("invalid ioctl with kms %s\n", __func__);		\
	return -EINVAL;							\
}

KMS_INVALID_IOCTL(via_mem_alloc)
KMS_INVALID_IOCTL(via_mem_free)
KMS_INVALID_IOCTL(via_agp_init)
KMS_INVALID_IOCTL(via_fb_init)
KMS_INVALID_IOCTL(via_map_init)
KMS_INVALID_IOCTL(via_decoder_futex)

const struct drm_ioctl_desc via_ioctls[] = {
	DRM_IOCTL_DEF_DRV(VIA_ALLOCMEM, via_mem_alloc, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(VIA_FREEMEM, via_mem_free, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(VIA_FB_INIT, via_fb_init, DRM_AUTH | DRM_MASTER),
	DRM_IOCTL_DEF_DRV(VIA_MAP_INIT, via_map_init, DRM_AUTH | DRM_MASTER),
	DRM_IOCTL_DEF_DRV(VIA_DEC_FUTEX, via_decoder_futex, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(VIA_OLD_GEM_CREATE, via_gem_alloc, DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(VIA_GETPARAM, via_getparam, DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(VIA_SETPARAM, via_setparam, DRM_AUTH | DRM_MASTER | DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(VIA_GEM_CREATE, via_gem_alloc, DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(VIA_GEM_WAIT, via_gem_wait, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(VIA_GEM_STATE, via_gem_state, DRM_AUTH),
};

int via_max_ioctl = ARRAY_SIZE(via_ioctls);
