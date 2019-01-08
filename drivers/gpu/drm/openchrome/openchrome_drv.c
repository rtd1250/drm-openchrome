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
#include <linux/console.h>

#include <drm/drmP.h>
#include <drm/via_drm.h>
#include <drm/drm_pciids.h>

#include "openchrome_drv.h"

int via_modeset = 1;

MODULE_PARM_DESC(modeset, "Disable/Enable modesetting");
module_param_named(modeset, via_modeset, int, 0400);

int via_hdmi_audio = 0;

MODULE_PARM_DESC(audio, "HDMI Audio enable (1 = enable)");
module_param_named(audio, via_hdmi_audio, int, 0444);

static struct pci_device_id via_pci_table[] = {
	viadrv_PCI_IDS,
};
MODULE_DEVICE_TABLE(pci, via_pci_table);

#define SGDMA_MEMORY (256*1024)
#define VQ_MEMORY (256*1024)

static int via_dumb_create(struct drm_file *filp,
				struct drm_device *dev,
				struct drm_mode_create_dumb *args)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct drm_gem_object *obj;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	args->pitch = round_up(args->width * (args->bpp >> 3), 16);
	args->size = args->pitch * args->height;
	obj = ttm_gem_create(dev, &dev_private->ttm.bdev, args->size,
				ttm_bo_type_device, TTM_PL_FLAG_VRAM,
				16, PAGE_SIZE, false);
	if (IS_ERR(obj))
		return PTR_ERR(obj);

	ret = drm_gem_handle_create(filp, obj, &args->handle);
	/* drop reference from allocate - handle holds it now */
	drm_gem_object_unreference_unlocked(obj);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int via_dumb_mmap(struct drm_file *filp, struct drm_device *dev,
				uint32_t handle, uint64_t *offset_p)
{
	struct ttm_buffer_object *bo;
	struct drm_gem_object *obj;
	int rc = -ENOENT;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	obj = drm_gem_object_lookup(filp, handle);
	if (obj == NULL)
		return rc;

	bo = ttm_gem_mapping(obj);
	if (bo != NULL) {
		*offset_p = drm_vma_node_offset_addr(&bo->vma_node);
		rc = 0;
	}
	drm_gem_object_unreference_unlocked(obj);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return rc;
}

static int gem_dumb_destroy(struct drm_file *filp,
				struct drm_device *dev, uint32_t handle)
{
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ret = drm_gem_handle_delete(filp, handle);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void via_driver_unload(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct ttm_buffer_object *bo;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		via_modeset_fini(dev);

	bo = dev_private->vq.bo;
	if (bo) {
		via_bo_unpin(bo, &dev_private->vq);
		ttm_bo_unref(&bo);
	}

	bo = dev_private->gart.bo;
	if (bo) {
		/* enable gtt write */
		if (pci_is_pcie(dev->pdev))
			svga_wseq_mask(VGABASE, 0x6C, 0, BIT(7));
		via_bo_unpin(bo, &dev_private->gart);
		ttm_bo_unref(&bo);
	}

	via_mm_fini(dev);

	/*
	 * Unmap VRAM.
	 */
	openchrome_vram_fini(dev_private);

	openchrome_mmio_fini(dev_private);

#if IS_ENABLED(CONFIG_AGP)
	if (dev->agp && dev->agp->acquired)
		drm_agp_release(dev);
#endif
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

	ret = via_mm_init(dev_private);
	if (ret) {
		DRM_ERROR("Failed to initialize TTM.\n");
		goto init_error;
	}

	chip_revision_info(dev_private);

#if IS_ENABLED(CONFIG_AGP)
	if ((dev_private->engine_type <= VIA_ENG_H2) ||
		(dev->agp &&
		pci_find_capability(dev->pdev, PCI_CAP_ID_AGP))) {
		ret = via_detect_agp(dev);
		if (!ret)
			via_agp_engine_init(dev_private);
		else
			DRM_ERROR("Failed to allocate AGP.\n");
	}
#endif
	if (pci_is_pcie(dev->pdev)) {
		/* Allocate GART. */
		ret = via_ttm_allocate_kernel_buffer(
						&dev_private->ttm.bdev,
						SGDMA_MEMORY, 16,
						TTM_PL_FLAG_VRAM,
						&dev_private->gart);
		if (likely(!ret)) {
			DRM_INFO("Allocated %u KB of DMA memory.\n",
					SGDMA_MEMORY >> 10);
		} else {
			DRM_ERROR("Failed to allocate DMA memory.\n");
			goto init_error;
		}
	}

	/* Allocate VQ. (Virtual Queue) */
	ret = via_ttm_allocate_kernel_buffer(&dev_private->ttm.bdev,
					VQ_MEMORY, 16,
					TTM_PL_FLAG_VRAM,
					&dev_private->vq);
	if (likely(!ret)) {
		DRM_INFO("Allocated %u KB of VQ (Virtual Queue) "
				"memory.\n", VQ_MEMORY >> 10);
	} else {
		DRM_ERROR("Failed to allocate VQ (Virtual Queue) "
				"memory.\n");
		goto init_error;
	}

	via_engine_init(dev);

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

static int via_pm_ops_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	struct openchrome_drm_private *dev_private = drm_dev->dev_private;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	console_lock();
	drm_fb_helper_set_suspend(&dev_private->via_fbdev->helper,
					true);

	/*
	 * Frame Buffer Size Control register (SR14) and GTI registers
	 * (SR66 through SR6F) need to be saved and restored upon standby
	 * resume or can lead to a display corruption issue. These registers
	 * are only available on VX800, VX855, and VX900 chipsets. This bug
	 * was observed on VIA EPIA-M830 mainboard.
	 */
	if ((drm_dev->pdev->device == PCI_DEVICE_ID_VIA_VT1122) ||
		(drm_dev->pdev->device == PCI_DEVICE_ID_VIA_VX875) ||
		(drm_dev->pdev->device == PCI_DEVICE_ID_VIA_VX900_VGA)) {
		dev_private->saved_sr14 = vga_rseq(VGABASE, 0x14);

		dev_private->saved_sr66 = vga_rseq(VGABASE, 0x66);
		dev_private->saved_sr67 = vga_rseq(VGABASE, 0x67);
		dev_private->saved_sr68 = vga_rseq(VGABASE, 0x68);
		dev_private->saved_sr69 = vga_rseq(VGABASE, 0x69);
		dev_private->saved_sr6a = vga_rseq(VGABASE, 0x6a);
		dev_private->saved_sr6b = vga_rseq(VGABASE, 0x6b);
		dev_private->saved_sr6c = vga_rseq(VGABASE, 0x6c);
		dev_private->saved_sr6d = vga_rseq(VGABASE, 0x6d);
		dev_private->saved_sr6e = vga_rseq(VGABASE, 0x6e);
		dev_private->saved_sr6f = vga_rseq(VGABASE, 0x6f);
	}

	/* 3X5.3B through 3X5.3F are scratch pad registers.
	 * They are important for FP detection.
	 * Their values need to be saved because they get lost
	 * when resuming from standby. */
	dev_private->saved_cr3b = vga_rcrt(VGABASE, 0x3b);
	dev_private->saved_cr3c = vga_rcrt(VGABASE, 0x3c);
	dev_private->saved_cr3d = vga_rcrt(VGABASE, 0x3d);
	dev_private->saved_cr3e = vga_rcrt(VGABASE, 0x3e);
	dev_private->saved_cr3f = vga_rcrt(VGABASE, 0x3f);

	console_unlock();

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return 0;
}

static int via_pm_ops_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *drm_dev = pci_get_drvdata(pdev);
	struct openchrome_drm_private *dev_private =
						drm_dev->dev_private;
	void __iomem *regs = ioport_map(0x3c0, 100);
	u8 val;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	console_lock();

	val = ioread8(regs + 0x03);
	iowrite8(val | 0x1, regs + 0x03);
	val = ioread8(regs + 0x0C);
	iowrite8(val | 0x1, regs + 0x02);

	/* Unlock Extended IO Space. */
	iowrite8(0x10, regs + 0x04);
	iowrite8(0x01, regs + 0x05);
	/* Unlock CRTC register protect. */
	iowrite8(0x47, regs + 0x14);

	/* Enable MMIO. */
	iowrite8(0x1a, regs + 0x04);
	val = ioread8(regs + 0x05);
	iowrite8(val | 0x38, regs + 0x05);

	/*
	 * Frame Buffer Size Control register (SR14) and GTI registers
	 * (SR66 through SR6F) need to be saved and restored upon standby
	 * resume or can lead to a display corruption issue. These registers
	 * are only available on VX800, VX855, and VX900 chipsets. This bug
	 * was observed on VIA EPIA-M830 mainboard.
	 */
	if ((drm_dev->pdev->device == PCI_DEVICE_ID_VIA_VT1122) ||
		(drm_dev->pdev->device == PCI_DEVICE_ID_VIA_VX875) ||
		(drm_dev->pdev->device == PCI_DEVICE_ID_VIA_VX900_VGA)) {
		vga_wseq(VGABASE, 0x14, dev_private->saved_sr14);

		vga_wseq(VGABASE, 0x66, dev_private->saved_sr66);
		vga_wseq(VGABASE, 0x67, dev_private->saved_sr67);
		vga_wseq(VGABASE, 0x68, dev_private->saved_sr68);
		vga_wseq(VGABASE, 0x69, dev_private->saved_sr69);
		vga_wseq(VGABASE, 0x6a, dev_private->saved_sr6a);
		vga_wseq(VGABASE, 0x6b, dev_private->saved_sr6b);
		vga_wseq(VGABASE, 0x6c, dev_private->saved_sr6c);
		vga_wseq(VGABASE, 0x6d, dev_private->saved_sr6d);
		vga_wseq(VGABASE, 0x6e, dev_private->saved_sr6e);
		vga_wseq(VGABASE, 0x6f, dev_private->saved_sr6f);
	}

	/* 3X5.3B through 3X5.3F are scratch pad registers.
	 * They are important for FP detection.
	 * Their values need to be restored because they are undefined
	 * after resuming from standby. */
	vga_wcrt(VGABASE, 0x3b, dev_private->saved_cr3b);
	vga_wcrt(VGABASE, 0x3c, dev_private->saved_cr3c);
	vga_wcrt(VGABASE, 0x3d, dev_private->saved_cr3d);
	vga_wcrt(VGABASE, 0x3e, dev_private->saved_cr3e);
	vga_wcrt(VGABASE, 0x3f, dev_private->saved_cr3f);

	drm_helper_resume_force_mode(drm_dev);
	drm_fb_helper_set_suspend(&dev_private->via_fbdev->helper, false);
	console_unlock();

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return 0;
}

static const struct dev_pm_ops via_dev_pm_ops = {
	.suspend = via_pm_ops_suspend,
	.resume = via_pm_ops_resume,
};

static const struct file_operations via_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap		= ttm_mmap,
	.poll		= drm_poll,
	.llseek		= noop_llseek,
};

static struct drm_driver via_driver = {
	.driver_features = DRIVER_USE_AGP | DRIVER_HAVE_IRQ |
				DRIVER_IRQ_SHARED | DRIVER_GEM,
	.load = via_driver_load,
	.unload = via_driver_unload,
	.lastclose = via_driver_lastclose,
	.gem_open_object = ttm_gem_open_object,
	.gem_free_object = ttm_gem_free_object,
	.dumb_create = via_dumb_create,
	.dumb_map_offset = via_dumb_mmap,
	.dumb_destroy = gem_dumb_destroy,
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
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ret = drm_get_pci_dev(pdev, ent, &via_driver);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void via_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	drm_put_dev(dev);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static struct pci_driver via_pci_driver = {
	.name		= DRIVER_NAME,
	.id_table	= via_pci_table,
	.probe		= via_pci_probe,
	.remove		= via_pci_remove,
	.driver.pm	= &via_dev_pm_ops,
};

static int __init via_init(void)
{
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	via_driver.num_ioctls = via_max_ioctl;

	if (via_modeset) {
		via_driver.driver_features |= DRIVER_MODESET;
	}

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
