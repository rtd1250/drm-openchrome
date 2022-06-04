/*
 * Copyright © 2020 Kevin Brace
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

#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>

#include <drm/ttm/ttm_bo_api.h>

#include <uapi/drm/openchrome_drm.h>

#include "openchrome_drv.h"


static int openchrome_gem_create_ioctl(struct drm_device *dev,
					void *data,
					struct drm_file *file_priv)
{
	struct drm_openchrome_gem_create *args = data;
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	struct openchrome_bo *bo;
	uint32_t handle;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ret = openchrome_bo_create(dev,
					&dev_priv->bdev,
					args->size,
					ttm_bo_type_device,
					args->domain,
					false,
					&bo);

	if (ret) {
		goto exit;
	}

	ret = drm_gem_handle_create(file_priv, &bo->ttm_bo.base,
					&handle);
	drm_gem_object_put(&bo->ttm_bo.base);
	if (ret) {
		openchrome_bo_destroy(bo, false);
		goto exit;
	}

	args->size		= bo->ttm_bo.base.size;
	args->domain		= bo->ttm_bo.resource->placement;
	args->handle		= handle;
	args->offset		= bo->ttm_bo.resource->start << PAGE_SHIFT;
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int openchrome_gem_map_ioctl(struct drm_device *dev,
					void *data,
					struct drm_file *file_priv)
{
	struct drm_openchrome_gem_map *args = data;
	struct drm_gem_object *gem;
	struct ttm_buffer_object *ttm_bo;
	struct openchrome_bo *bo;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	gem = drm_gem_object_lookup(file_priv, args->handle);
	if (!gem) {
		ret = -EINVAL;
		goto exit;
	}

	ttm_bo = container_of(gem, struct ttm_buffer_object, base);
	bo = to_ttm_bo(ttm_bo);

	args->map_offset = drm_vma_node_offset_addr(
					&bo->ttm_bo.base.vma_node);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int openchrome_gem_unmap_ioctl(struct drm_device *dev,
				void *data,
				struct drm_file *file_priv)
{
	struct drm_openchrome_gem_unmap *args = data;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ret = drm_gem_handle_delete(file_priv, args->handle);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}


const struct drm_ioctl_desc openchrome_driver_ioctls[] = {
	DRM_IOCTL_DEF_DRV(OPENCHROME_GEM_CREATE, openchrome_gem_create_ioctl, DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(OPENCHROME_GEM_MAP, openchrome_gem_map_ioctl, DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(OPENCHROME_GEM_UNMAP, openchrome_gem_unmap_ioctl, DRM_AUTH | DRM_UNLOCKED),
};


int openchrome_driver_num_ioctls = ARRAY_SIZE(openchrome_driver_ioctls);
