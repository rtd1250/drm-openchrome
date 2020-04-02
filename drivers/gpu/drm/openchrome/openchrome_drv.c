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

#include <linux/module.h>

#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_pci.h>
#include <drm/drm_pciids.h>

#include <drm/via_drm.h>

#include "openchrome_drv.h"


int via_hdmi_audio = 0;

MODULE_PARM_DESC(audio, "HDMI Audio enable (1 = enable)");
module_param_named(audio, via_hdmi_audio, int, 0444);

static struct pci_device_id via_pci_table[] = {
	viadrv_PCI_IDS,
};
MODULE_DEVICE_TABLE(pci, via_pci_table);


void openchrome_drm_driver_gem_free_object_unlocked (
					struct drm_gem_object *obj)
{
	struct openchrome_bo *bo = container_of(obj,
					struct openchrome_bo, gem);

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ttm_bo_put(&bo->ttm_bo);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static int openchrome_drm_driver_dumb_create(
				struct drm_file *file_priv,
				struct drm_device *dev,
				struct drm_mode_create_dumb *args)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct openchrome_bo *bo;
	u32 handle;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/*
	 * Calculate the parameters for the dumb buffer.
	 */
	args->pitch = args->width * ((args->bpp + 7) >> 3);
	args->size = args->pitch * args->height;

	ret = openchrome_bo_create(dev,
					&dev_private->bdev,
					args->size,
					ttm_bo_type_device,
					TTM_PL_FLAG_VRAM,
					false,
					&bo);
	if (ret) {
		goto exit;
	}

	ret = drm_gem_handle_create(file_priv, &bo->gem, &handle);
	drm_gem_object_put_unlocked(&bo->gem);
	if (ret) {
		goto exit;
	}

	args->handle = handle;
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int openchrome_drm_driver_dumb_map_offset(
				struct drm_file *file_priv,
				struct drm_device *dev,
				uint32_t handle,
				uint64_t *offset)
{
	struct drm_gem_object *gem;
	struct openchrome_bo *bo;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	gem = drm_gem_object_lookup(file_priv, handle);
	if (!gem) {
		ret = -ENOENT;
		goto exit;
	}

	bo = container_of(gem, struct openchrome_bo, gem);
	*offset = drm_vma_node_offset_addr(&bo->ttm_bo.base.vma_node);

	drm_gem_object_put_unlocked(gem);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void via_driver_unload(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		via_modeset_fini(dev);

	openchrome_mm_fini(dev_private);

	/*
	 * Unmap VRAM.
	 */
	openchrome_vram_fini(dev_private);

	openchrome_mmio_fini(dev_private);

	kfree(dev_private);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return;
}

static int via_driver_load(struct drm_device *dev,
				unsigned long chipset)
{
	struct openchrome_drm_private *dev_private;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	dev_private = kzalloc(sizeof(struct openchrome_drm_private),
				GFP_KERNEL);
	if (!dev_private) {
		ret = -ENOMEM;
		DRM_ERROR("Failed to allocate private "
				"storage memory.\n");
		goto exit;
	}

	dev->dev_private = (void *) dev_private;
	dev_private->engine_type = chipset;
	dev_private->vram_mtrr = -ENXIO;
	dev_private->dev = dev;

	ret = openchrome_device_init(dev_private);
	if (ret) {
		DRM_ERROR("Failed to initialize Chrome IGP.\n");
		goto init_error;
	}

	ret = openchrome_mm_init(dev_private);
	if (ret) {
		DRM_ERROR("Failed to initialize TTM.\n");
		goto init_error;
	}

	chip_revision_info(dev_private);

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		ret = via_modeset_init(dev);
		if (ret) {
		DRM_ERROR("Failed to initialize mode setting.\n");
		goto init_error;
		}
	}

	goto exit;
init_error:
	if (ret)
		via_driver_unload(dev);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void via_driver_lastclose(struct drm_device *dev)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (drm_core_check_feature(dev, DRIVER_MODESET) &&
		dev->mode_config.funcs->output_poll_changed)
		dev->mode_config.funcs->output_poll_changed(dev);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static int openchrome_drm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *file_priv = filp->private_data;
	struct openchrome_drm_private *dev_private =
				file_priv->minor->dev->dev_private;
	int ret = -EINVAL;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (!dev_private) {
		DRM_DEBUG_KMS("No device private data.\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = ttm_bo_mmap(filp, vma, &dev_private->bdev);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static const struct dev_pm_ops openchrome_dev_pm_ops = {
	.suspend	= openchrome_dev_pm_ops_suspend,
	.resume		= openchrome_dev_pm_ops_resume,
};

static const struct file_operations via_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap		= openchrome_drm_mmap,
	.poll		= drm_poll,
	.llseek		= noop_llseek,
};

static struct drm_driver via_driver = {
	.driver_features = DRIVER_HAVE_IRQ |
				DRIVER_GEM |
				DRIVER_MODESET,
	.load = via_driver_load,
	.unload = via_driver_unload,
	.lastclose = via_driver_lastclose,
	.gem_free_object_unlocked =
		openchrome_drm_driver_gem_free_object_unlocked,
	.dumb_create = openchrome_drm_driver_dumb_create,
	.dumb_map_offset =
				openchrome_drm_driver_dumb_map_offset,
	.ioctls = via_ioctls,
	.fops = &via_driver_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

static int via_pci_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	struct drm_device *dev;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ret = pci_enable_device(pdev);
	if (ret) {
		goto exit;
	}

	dev = drm_dev_alloc(&via_driver, &pdev->dev);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto err_pci_disable_device;
	}

	dev->pdev = pdev;
	pci_set_drvdata(pdev, dev);

	ret = drm_dev_register(dev, ent->driver_data);
	if (ret) {
		goto err_drm_dev_put;
	}

	goto exit;
err_drm_dev_put:
	drm_dev_put(dev);
err_pci_disable_device:
	pci_disable_device(pdev);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void via_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	drm_dev_unregister(dev);
	drm_dev_put(dev);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static struct pci_driver via_pci_driver = {
	.name		= DRIVER_NAME,
	.id_table	= via_pci_table,
	.probe		= via_pci_probe,
	.remove		= via_pci_remove,
	.driver.pm	= &openchrome_dev_pm_ops,
};

static int __init via_init(void)
{
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	via_driver.num_ioctls = via_max_ioctl;

	ret = pci_register_driver(&via_pci_driver);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void __exit via_exit(void)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	pci_unregister_driver(&via_pci_driver);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

module_init(via_init);
module_exit(via_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
