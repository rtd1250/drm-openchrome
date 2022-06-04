/*
 * Copyright 2017 Kevin Brace. All Rights Reserved.
 * Copyright 2012 James Simmons <jsimmons@infradead.org>. All Rights Reserved.
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * This DRM's standby and resume code is based on Radeon DRM's code,
 * but it was shortened and simplified.
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

#include <linux/pci.h>

#include <drm/drm_aperture.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_pciids.h>
#include <drm/drm_prime.h>

#include <drm/ttm/ttm_bo_api.h>

#include "openchrome_drv.h"


extern const struct drm_ioctl_desc openchrome_driver_ioctls[];

/*
 * For now, this device driver will be disabled, unless the
 * user decides to enable it.
 */
int openchrome_modeset = 0;

MODULE_PARM_DESC(modeset, "Enable DRM device driver "
				"(Default: Disabled, "
				"0 = Disabled,"
				"1 = Enabled)");
module_param_named(modeset, openchrome_modeset, int, 0400);

static int openchrome_driver_open(struct drm_device *dev,
					struct drm_file *file_priv)
{
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void openchrome_driver_postclose(struct drm_device *dev,
					struct drm_file *file_priv)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void openchrome_driver_lastclose(struct drm_device *dev)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	drm_fb_helper_lastclose(dev);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static int openchrome_driver_dumb_create(
				struct drm_file *file_priv,
				struct drm_device *dev,
				struct drm_mode_create_dumb *args)
{
	struct via_drm_priv *dev_private = to_via_drm_priv(dev);
	struct openchrome_bo *bo;
	uint32_t handle, pitch;
	uint64_t size;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/*
	 * Calculate the parameters for the dumb buffer.
	 */
	pitch = args->width * ((args->bpp + 7) >> 3);
	size = pitch * args->height;

	ret = openchrome_bo_create(dev,
					&dev_private->bdev,
					size,
					ttm_bo_type_device,
					TTM_PL_VRAM,
					false,
					&bo);
	if (ret) {
		goto exit;
	}

	ret = drm_gem_handle_create(file_priv, &bo->ttm_bo.base, &handle);
	drm_gem_object_put(&bo->ttm_bo.base);
	if (ret) {
		goto exit;
	}

	args->handle = handle;
	args->pitch = pitch;
	args->size = size;
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int openchrome_driver_dumb_map_offset(
				struct drm_file *file_priv,
				struct drm_device *dev,
				uint32_t handle,
				uint64_t *offset)
{
	struct drm_gem_object *gem;
	struct ttm_buffer_object *ttm_bo;
	struct openchrome_bo *bo;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	gem = drm_gem_object_lookup(file_priv, handle);
	if (!gem) {
		ret = -ENOENT;
		goto exit;
	}

	ttm_bo = container_of(gem, struct ttm_buffer_object, base);
	bo = container_of(ttm_bo, struct openchrome_bo, ttm_bo);
	*offset = drm_vma_node_offset_addr(&bo->ttm_bo.base.vma_node);

	drm_gem_object_put(gem);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static const struct file_operations openchrome_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap		= drm_gem_mmap,
	.poll		= drm_poll,
	.llseek		= noop_llseek,
};

static struct drm_driver openchrome_driver = {
	.driver_features = DRIVER_HAVE_IRQ |
				DRIVER_GEM |
				DRIVER_MODESET |
				DRIVER_ATOMIC,
	.open = openchrome_driver_open,
	.postclose = openchrome_driver_postclose,
	.lastclose = openchrome_driver_lastclose,
	.gem_prime_mmap = drm_gem_prime_mmap,
	.dumb_create = openchrome_driver_dumb_create,
	.dumb_map_offset = openchrome_driver_dumb_map_offset,
	.ioctls = openchrome_driver_ioctls,
	.fops = &openchrome_driver_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

static struct pci_device_id openchrome_pci_table[] = {
	viadrv_PCI_IDS,
};

MODULE_DEVICE_TABLE(pci, openchrome_pci_table);

static int openchrome_pci_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	struct drm_device *dev;
	struct via_drm_priv *dev_private;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ret = drm_aperture_remove_conflicting_pci_framebuffers(pdev,
						&openchrome_driver);
	if (ret) {
		goto exit;
	}

	ret = pci_enable_device(pdev);
	if (ret) {
		goto exit;
	}

	dev_private = devm_drm_dev_alloc(&pdev->dev,
					&openchrome_driver,
					struct via_drm_priv,
					dev);
	if (IS_ERR(dev_private)) {
		ret = PTR_ERR(dev_private);
		goto exit;
	}

	dev = &dev_private->dev;

	pci_set_drvdata(pdev, dev);

	ret = openchrome_drm_init(dev);
	if (ret) {
		goto error_disable_pci;
	}

	ret = drm_dev_register(dev, ent->driver_data);
	if (ret) {
		goto error_disable_pci;
	}

	drm_fbdev_generic_setup(dev, 32);
	goto exit;
error_disable_pci:
	pci_disable_device(pdev);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void openchrome_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	openchrome_drm_fini(dev);
	drm_dev_unregister(dev);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static const struct dev_pm_ops openchrome_dev_pm_ops = {
	.suspend	= openchrome_dev_pm_ops_suspend,
	.resume		= openchrome_dev_pm_ops_resume,
};

static struct pci_driver openchrome_pci_driver = {
	.name		= DRIVER_NAME,
	.id_table	= openchrome_pci_table,
	.probe		= openchrome_pci_probe,
	.remove		= openchrome_pci_remove,
	.driver.pm	= &openchrome_dev_pm_ops,
};

static int __init openchrome_init(void)
{
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if ((openchrome_modeset == -1) &&
		(drm_firmware_drivers_only())) {
		openchrome_modeset = 0;
	}

	if (!openchrome_modeset) {
		ret = -EINVAL;
		goto exit;
	}

	openchrome_driver.num_ioctls = openchrome_driver_num_ioctls;

	ret = pci_register_driver(&openchrome_pci_driver);

exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void __exit openchrome_exit(void)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (!openchrome_modeset) {
		goto exit;
	}

	pci_unregister_driver(&openchrome_pci_driver);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

module_init(openchrome_init);
module_exit(openchrome_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
