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
#include "drm_pciids.h"

static struct pci_device_id via_pci_table[] = {
	viadrv_PCI_IDS
};

static int via_driver_unload(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;

	drm_sman_takedown(&dev_priv->sman);

	ttm_global_fini(&dev_priv->mem_global_ref,
			&dev_priv->bo_global_ref,
			&dev_priv->bdev);

	kfree(dev_priv);
	return 0;
}

static int via_driver_load(struct drm_device *dev, unsigned long chipset)
{
	struct drm_via_private *dev_priv;
	int ret = 0;

	dev_priv = kzalloc(sizeof(struct drm_via_private), GFP_KERNEL);
	if (dev_priv == NULL)
		return -ENOMEM;

	dev->dev_private = (void *)dev_priv;
	dev_priv->chipset = chipset;

	ret = via_ttm_init(dev_priv);
	if (ret)
		return ret;

	ret = drm_sman_init(&dev_priv->sman, 2, 12, 8);
	if (ret) {
		kfree(dev_priv);
		return ret;
	}

	ret = via_detect_vram(dev);
	if (ret)
		goto out_err;

	ret = drm_vblank_init(dev, 1);
out_err:
	if (ret)
		via_driver_unload(dev);
	return ret;
}

static int via_final_context(struct drm_device *dev, int context)
{
	struct drm_via_private *dev_priv = dev->dev_private;

	via_release_futex(dev_priv, context);

	/* Linux specific until context tracking code gets ported to BSD */
	/* Last context, perform cleanup */
	if (dev->ctx_count == 1 && dev->dev_private) {
		DRM_DEBUG("Last Context\n");
		drm_irq_uninstall(dev);
		via_cleanup_futex(dev_priv);
		via_dma_cleanup(dev);
	}
	return 1;
}

static void via_lastclose(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;

	if (!dev_priv)
		return;

	mutex_lock(&dev->struct_mutex);
	drm_sman_cleanup(&dev_priv->sman);
	dev_priv->vram_initialized = 0;
	dev_priv->agp_initialized = 0;
	mutex_unlock(&dev->struct_mutex);
}

static void via_reclaim_buffers_locked(struct drm_device *dev,
                                struct drm_file *file_priv)
{
	struct drm_via_private *dev_priv = dev->dev_private;

	mutex_lock(&dev->struct_mutex);
	if (drm_sman_owner_clean(&dev_priv->sman, (unsigned long)file_priv)) {
		mutex_unlock(&dev->struct_mutex);
		return;
	}

	if (dev->driver->dma_quiescent)
		dev->driver->dma_quiescent(dev);

	drm_sman_owner_cleanup(&dev_priv->sman, (unsigned long)file_priv);
	mutex_unlock(&dev->struct_mutex);
	return;
}

static struct drm_driver via_driver = {
	.driver_features =
		DRIVER_USE_AGP | DRIVER_USE_MTRR | DRIVER_HAVE_IRQ |
		DRIVER_IRQ_SHARED,
	.load = via_driver_load,
	.unload = via_driver_unload,
	.context_dtor = via_final_context,
	.get_vblank_counter = via_get_vblank_counter,
	.enable_vblank = via_enable_vblank,
	.disable_vblank = via_disable_vblank,
	.irq_preinstall = via_driver_irq_preinstall,
	.irq_postinstall = via_driver_irq_postinstall,
	.irq_uninstall = via_driver_irq_uninstall,
	.irq_handler = via_driver_irq_handler,
	.dma_quiescent = via_driver_dma_quiescent,
	.reclaim_buffers = drm_core_reclaim_buffers,
	.reclaim_buffers_locked = NULL,
	.reclaim_buffers_idlelocked = via_reclaim_buffers_locked,
	.lastclose = via_lastclose,
	.ioctls = via_ioctls,
	.fops = {
		.owner = THIS_MODULE,
		.open = drm_open,
		.release = drm_release,
		.unlocked_ioctl = drm_ioctl,
		.mmap = drm_mmap,
		.poll = drm_poll,
		.fasync = drm_fasync,
		.llseek = noop_llseek,
		},
	.pci_driver = {
		.name = DRIVER_NAME,
		.id_table = via_pci_table,
	},

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

static int __init via_init(void)
{
	via_driver.num_ioctls = via_max_ioctl;
	via_init_command_verifier();
	return drm_init(&via_driver);
}

static void __exit via_exit(void)
{
	drm_exit(&via_driver);
}

module_init(via_init);
module_exit(via_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
