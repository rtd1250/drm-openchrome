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

int via_modeset = 0;

MODULE_PARM_DESC(modeset, "Disable/Enable modesetting");
module_param_named(modeset, via_modeset, int, 0400);

static struct pci_device_id via_pci_table[] __devinitdata = {
	viadrv_PCI_IDS,
};
MODULE_DEVICE_TABLE(pci, via_pci_table);

static struct drm_driver via_driver;

#if __OS_HAS_AGP

#define VIA_AGP_MODE_MASK	0x17
#define VIA_AGPV3_MODE		0x08
#define VIA_AGPV3_8X_MODE	0x02
#define VIA_AGPV3_4X_MODE	0x01
#define VIA_AGP_4X_MODE		0x04
#define VIA_AGP_2X_MODE		0x02
#define VIA_AGP_1X_MODE		0x01
#define VIA_AGP_FW_MODE		0x10

static int __devinit
via_detect_agp(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	struct drm_agp_info agp_info;
	struct drm_agp_mode mode;
	int ret = 0;

	ret = drm_agp_acquire(dev);
	if (ret) {
		DRM_ERROR("Failed acquiring AGP device.\n");
		return ret;
	}

	ret = drm_agp_info(dev, &agp_info);
	if (ret) {
		DRM_ERROR("Failed detecting AGP aperture size.\n");
		goto out_err0;
	}

	mode.mode = agp_info.mode & ~VIA_AGP_MODE_MASK;
	if (mode.mode & VIA_AGPV3_MODE)
		mode.mode |= VIA_AGPV3_8X_MODE;
	else
		mode.mode |= VIA_AGP_4X_MODE;

	mode.mode |= VIA_AGP_FW_MODE;
	ret = drm_agp_enable(dev, mode);
	if (ret) {
		DRM_ERROR("Failed to enable the AGP bus.\n");
		goto out_err0;
	}

	ret = ttm_bo_init_mm(&dev_priv->bdev, TTM_PL_TT, agp_info.aperture_size >> PAGE_SHIFT);
	if (!ret) {
		DRM_INFO("Detected %lu MB of AGP Aperture at "
			"physical address 0x%08lx.\n",
			agp_info.aperture_size >> 20,
			agp_info.aperture_base);
	} else {
out_err0:
		drm_agp_release(dev);
	}
	return ret;
}

static void via_agp_engine_init(struct drm_via_private *dev_priv)
{
	VIA_WRITE(VIA_REG_TRANSET, 0x00100000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x00333004);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x60000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x61000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x62000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x63000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x64000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x7D000000);

	VIA_WRITE(VIA_REG_TRANSET, 0xfe020000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);
}
#endif

#define SGDMA_MEMORY (256*1024)

static int __devinit
via_allocate_pcie_gart_table(struct drm_via_private *dev_priv)
{
	struct ttm_buffer_object *bo;
	int size = SGDMA_MEMORY, ret;
	u8 orig;

	ret = ttm_bo_allocate(&dev_priv->bdev, size, ttm_bo_type_kernel,
				TTM_PL_FLAG_VRAM | TTM_PL_FLAG_NO_EVICT,
				1, PAGE_SIZE, 0, false, via_ttm_bo_destroy,
				NULL, &bo);
	if (unlikely(ret))
		goto err;

	/* allocate the gart table */
	ret = ttm_bo_pin(bo, &dev_priv->gart);
	if (ret)
		goto err;

	/* enable gti write */
	orig = (vga_rseq(VGABASE, 0x6C) & 0x7F);
	vga_wseq(VGABASE, 0x6C, orig);

	/* set the base address of gart table */
	orig = (bo->offset & 0xff000) >> 12;
	vga_wseq(VGABASE, 0x6A, orig);

	orig = (bo->offset & 0xff000) >> 20;
	vga_wseq(VGABASE, 0x6B, orig);

	orig = vga_rseq(VGABASE, 0x6C);
	orig |= ((bo->offset >> 28) & 0x01);
	vga_wseq(VGABASE, 0x6C, orig);

	/* flush the gti cache */
	orig = (vga_rseq(VGABASE, 0x6F) | 0x80);
	vga_wseq(VGABASE, 0x6F, orig);

	/* disable the gti write */
	orig = (vga_rseq(VGABASE, 0x6C) | 0x80);
	vga_wseq(VGABASE, 0x6C, orig);
	DRM_INFO("Allocated %d KB of DMA memory\n", size >> 10);
err:
	return ret;
}

#define VQ_MEMORY (256*1024)
static int __devinit
via_init_vq(struct drm_via_private *dev_priv)
{
	unsigned long  vq_start_addr, vq_end_addr, vqlen;
	unsigned long  vqstartl, vqendl, vqstart_endh;
	struct ttm_buffer_object *bo;
	int size = VQ_MEMORY, ret;

	/* allocate vq bo */
	ret = ttm_bo_allocate(&dev_priv->bdev, size, ttm_bo_type_kernel,
				TTM_PL_FLAG_VRAM | TTM_PL_FLAG_NO_EVICT,
				1, PAGE_SIZE, 0, false, via_ttm_bo_destroy,
				NULL, &bo);
	if (unlikely(ret))
		goto err;

	ret = ttm_bo_pin(bo, &dev_priv->vq);
	if (ret)
		goto err;

	vq_start_addr = bo->offset;
	vq_end_addr = vq_start_addr + VQ_MEMORY - 1;
	vqstartl = 0x70000000 | (vq_start_addr & 0xFFFFFF);
	vqendl = 0x71000000 | (vq_end_addr & 0xFFFFFF);
	vqstart_endh = 0x72000000 | ((vq_start_addr & 0xFF000000) >> 24) |
			((vq_end_addr & 0xFF000000) >> 16);
	vqlen = 0x73000000 | (VQ_MEMORY >> 3);

	VIA_WRITE(0x41c, 0x00100000);
	VIA_WRITE(0x420, vqstart_endh);
	VIA_WRITE(0x420, vqstartl);
	VIA_WRITE(0x420, vqendl);
	VIA_WRITE(0x420, vqlen);
	VIA_WRITE(0x420, 0x74301001);
	VIA_WRITE(0x420, 0x00000000);
	DRM_INFO("Allocated %d KB of memory for VQ\n", size >> 10);
err:
	return ret;
}

static int __devinit
via_mmio_setup(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	int ret, len = pci_resource_len(dev->pdev, 1);
	void __iomem *regs = ioport_map(0x3c0, 100);
	struct ttm_buffer_object *bo;
	u8 val;

	val = ioread8(regs + 0x03);
	iowrite8(val | 0x1, regs + 0x03);
	val = ioread8(regs + 0x0C);
	iowrite8(val | 0x1, regs + 0x02);

	/* Unlock Extended IO Space */
	iowrite8(0x10, regs + 0x04);
	iowrite8(0x01, regs + 0x05);
	/* Unlock CRTC register protect */
	iowrite8(0x47, regs + 0x14);

	/* Enable MMIO */
	iowrite8(0x1a, regs + 0x04);
	val = ioread8(regs + 0x05);
	iowrite8(val | 0x38, regs + 0x05);

	ret = ttm_bo_init_mm(&dev_priv->bdev, TTM_PL_PRIV0,
				len >> PAGE_SHIFT);
	if (ret)
		return ret;

	ret = ttm_bo_allocate(&dev_priv->bdev, VIA_MMIO_REGSIZE, ttm_bo_type_kernel,
				TTM_PL_FLAG_PRIV0, 1, PAGE_SIZE,
				0, false, via_ttm_bo_destroy,
				NULL, &bo);
	if (ret)
		goto err;

	ret = ttm_bo_pin(bo, &dev_priv->mmio);
err:
	if (!ret)
		DRM_INFO("Detected MMIO at physical address 0x%08llx.\n",
			(unsigned long long) pci_resource_start(dev->pdev, 1));
	else
		ttm_bo_clean_mm(&dev_priv->bdev, TTM_PL_PRIV0);
	return ret;
}

static void __devinit
chip_revision_info(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	u8 tmp;

	/* Check revision of CLE266 Chip */
	if (dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266) {
		/* CR4F only define in CLE266.CX chip */
		tmp = vga_rcrt(VGABASE, 0x4F);
		vga_wcrt(VGABASE, 0x4F, 0x55);
		if (vga_rcrt(VGABASE, 0x4F) != 0x55)
			dev_priv->revision = CLE266_REVISION_AX;
		else
			dev_priv->revision = CLE266_REVISION_CX;
		/* restore orignal CR4F value */
		vga_wcrt(VGABASE, 0x4F, tmp);
	}

	if (dev->pdev->device == PCI_DEVICE_ID_VIA_VT3157) {
		tmp = vga_rseq(VGABASE, 0x43);
		if (tmp & 0x02) {
			dev_priv->revision = CX700_REVISION_700M2;
		} else if (tmp & 0x40) {
			dev_priv->revision = CX700_REVISION_700M;
		} else {
			dev_priv->revision = CX700_REVISION_700;
		}
	}
}

static void via_init_2d(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	int i;

	for (i = 0x04; i < 0x5c; i += 4)
		VIA_WRITE(i, 0x00);

	/* For 410 chip*/
	if (dev->pci_device == 0x7122)
		VIA_WRITE(0x60, 0x00);
}

static void via_init_3d(struct drm_via_private *dev_priv)
{
	unsigned long texture_stage;
	int i;

	VIA_WRITE(VIA_REG_TRANSET, 0x00010000);
	for (i = 0; i <= 0x9A; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, i << 24);

	/* Initial Texture Stage Setting*/
	for (texture_stage = 0; texture_stage <= 0xf; texture_stage++) {
		VIA_WRITE(VIA_REG_TRANSET, (0x00020000 | 0x00000000 |
					(texture_stage & 0xf) << 24));
		for (i = 0 ; i <= 0x30 ; i++)
			VIA_WRITE(VIA_REG_TRANSPACE, i << 24);
	}

	/* Initial Texture Sampler Setting*/
	for (texture_stage = 0; texture_stage <= 0xf; texture_stage++) {
		VIA_WRITE(VIA_REG_TRANSET, (0x00020000 | 0x20000000 |
					(texture_stage & 0xf) << 24));
		for ( i = 0 ; i <= 0x36 ; i++)
			VIA_WRITE(VIA_REG_TRANSPACE, i << 24);
	}

	VIA_WRITE(VIA_REG_TRANSET, (0x00020000 | 0xfe000000));
	for ( i = 0 ; i <= 0x13 ; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, i << 24);

	/* degamma table*/
	VIA_WRITE(VIA_REG_TRANSET, ( 0x00030000 | 0x15000000));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (30 << 20) | (15 << 10) | (5)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((119 << 20) | (81 << 10) | (52)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((283 << 20) | (219 << 10) | (165)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((535 << 20) | (441 << 10) | (357)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((119 << 20) | (884 << 20) | (757 << 10) | (640)));

	/* gamma table*/
	VIA_WRITE(VIA_REG_TRANSET, ( 0x00030000 | 0x17000000));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (13 << 20) | (13 << 10) | (13)));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (26 << 20) | (26 << 10) | (26)));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (39 << 20) | (39 << 10) | (39)));
	VIA_WRITE(VIA_REG_TRANSPACE, (51 << 20) | (51 << 10) | (51));
	VIA_WRITE(VIA_REG_TRANSPACE, (71 << 20) | (71 << 10) | (71));
	VIA_WRITE(VIA_REG_TRANSPACE, (87 << 20) | (87 << 10) | (87));
	VIA_WRITE(VIA_REG_TRANSPACE, (113 << 20) | (113 << 10) | (113));
	VIA_WRITE(VIA_REG_TRANSPACE, (135 << 20) | (135 << 10) | (135));
	VIA_WRITE(VIA_REG_TRANSPACE, (170 << 20) | (170 << 10) | (170));
	VIA_WRITE(VIA_REG_TRANSPACE, (199 << 20) | (199 << 10) | (199));
	VIA_WRITE(VIA_REG_TRANSPACE, (246 << 20) | (246 << 10) | (246));
	VIA_WRITE(VIA_REG_TRANSPACE, (284 << 20) | (284 << 10) | (284));
	VIA_WRITE(VIA_REG_TRANSPACE, (317 << 20) | (317 << 10) | (317));
	VIA_WRITE(VIA_REG_TRANSPACE, (347 << 20) | (347 << 10) | (347));
	VIA_WRITE(VIA_REG_TRANSPACE, (373 << 20) | (373 << 10) | (373));
	VIA_WRITE(VIA_REG_TRANSPACE, (398 << 20) | (398 << 10) | (398));
	VIA_WRITE(VIA_REG_TRANSPACE, (442 << 20) | (442 << 10) | (442));
	VIA_WRITE(VIA_REG_TRANSPACE, (481 << 20) | (481 << 10) | (481));
	VIA_WRITE(VIA_REG_TRANSPACE, (517 << 20) | (517 << 10) | (517));
	VIA_WRITE(VIA_REG_TRANSPACE, (550 << 20) | (550 << 10) | (550));
	VIA_WRITE(VIA_REG_TRANSPACE, (609 << 20) | (609 << 10) | (609));
	VIA_WRITE(VIA_REG_TRANSPACE, (662 << 20) | (662 << 10) | (662));
	VIA_WRITE(VIA_REG_TRANSPACE, (709 << 20) | (709 << 10) | (709));
	VIA_WRITE(VIA_REG_TRANSPACE, (753 << 20) | (753 << 10) | (753));
	VIA_WRITE(VIA_REG_TRANSPACE, (794 << 20) | (794 << 10) | (794));
	VIA_WRITE(VIA_REG_TRANSPACE, (832 << 20) | (832 << 10) | (832));
	VIA_WRITE(VIA_REG_TRANSPACE, (868 << 20) | (868 << 10) | (868));
	VIA_WRITE(VIA_REG_TRANSPACE, (902 << 20) | (902 << 10) | (902));
	VIA_WRITE(VIA_REG_TRANSPACE, (934 << 20) | (934 << 10) | (934));
	VIA_WRITE(VIA_REG_TRANSPACE, (966 << 20) | (966 << 10) | (966));
	VIA_WRITE(VIA_REG_TRANSPACE, (996 << 20) | (996 << 10) | (996));

	/* Initialize INV_ParaSubType_TexPa*/
	VIA_WRITE(VIA_REG_TRANSET, ( 0x00030000 | 0x00000000));
	for (i = 0; i < 16; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);

	/* Initialize INV_ParaSubType_4X4Cof */
	VIA_WRITE(VIA_REG_TRANSET, ( 0x00030000 | 0x11000000));
	for (i = 0; i < 32; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);

	/* Initialize INV_ParaSubType_StipPal */
	VIA_WRITE(VIA_REG_TRANSET, ( 0x00030000 | 0x14000000));
	for (i = 0; i < (5 + 3); i++)
		VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);

	/* primitive setting & vertex format*/
	VIA_WRITE(VIA_REG_TRANSET, ( 0x00040000));
	for ( i = 0; i <= 0x62; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, (i << 24));

	/* ParaType 0xFE - Configure and Misc Setting*/
	VIA_WRITE(VIA_REG_TRANSET, ( 0x00fe0000));
	for ( i = 0; i <= 0x47; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, (i << 24));

	/* ParaType 0x11 - Frame Buffer Auto-Swapping and Command Regulator*/
	VIA_WRITE(VIA_REG_TRANSET, ( 0x00110000));
	for ( i = 0; i <= 0x20; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, (i << 24));

	VIA_WRITE(VIA_REG_TRANSET, 0x00fe0000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x4000840f);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x47000404);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x44000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x46000005);

	/* setting Misconfig*/
	VIA_WRITE(VIA_REG_TRANSET, 0x00fe0000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x00001004);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x08000249);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x0a0002c9);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x0b0002fb);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x0c000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x0d0002cb);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x0e000009);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x10000049);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x110002ff);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x12000008);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x130002db);
}

static int via_dumb_create(struct drm_file *filp, struct drm_device *dev,
				struct drm_mode_create_dumb *args)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	struct drm_gem_object *obj;
	int ret = -ENOMEM;

	args->pitch = roundup(args->width * (args->bpp >> 3), 16);
	args->size = args->pitch * args->height;
	obj = ttm_gem_create(dev, &dev_priv->bdev, TTM_PL_FLAG_VRAM,
				false, 16, PAGE_SIZE, 0, args->size,
				via_ttm_bo_destroy);
	if (obj && obj->driver_private) {
		ret = drm_gem_handle_create(filp, obj, &args->handle);
		/* drop reference from allocate - handle holds it now */
		drm_gem_object_unreference_unlocked(obj);
	}

	if (ret) {
		if (obj) drm_gem_object_free((struct kref *)obj);
	} else
		args->size = obj->size;
	return ret;
}

static int via_dumb_mmap(struct drm_file *filp, struct drm_device *dev,
			uint32_t handle, uint64_t *offset_p)
{
	struct ttm_buffer_object *bo;
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(dev, filp, handle);
	if (!obj || !obj->driver_private)
		return -ENOENT;

	bo = obj->driver_private;
	*offset_p = bo->addr_space_offset;
	drm_gem_object_unreference_unlocked(obj);
	return 0;
}

static int gem_dumb_destroy(struct drm_file *filp, struct drm_device *dev,
				uint32_t handle)
{
	return drm_gem_handle_delete(filp, handle);
}

static int via_driver_unload(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	struct ttm_buffer_object *bo;
	int ret = 0;

	ret = via_dma_cleanup(dev);
	if (ret)
		return ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		via_modeset_fini(dev);

	drm_vblank_cleanup(dev);

	drm_irq_uninstall(dev);

	bo = dev_priv->vq.bo;
	if (bo) {
		ttm_bo_unpin(bo, &dev_priv->vq);
		ttm_bo_unref(&bo);
	}

	bo = dev_priv->gart.bo;
	if (bo) {
		/* enable gti write */
		if (pci_is_pcie(dev->pdev)) {
			u8 orig = (vga_rseq(VGABASE, 0x6C) & 0x7F);

			vga_wseq(VGABASE, 0x6C, orig);
		}
		ttm_bo_unpin(bo, &dev_priv->gart);
		ttm_bo_unref(&bo);
	}

	bo = dev_priv->mmio.bo;
	if (bo) {
		ttm_bo_unpin(bo, &dev_priv->mmio);
		ttm_bo_unref(&bo);
	}

	ttm_bo_clean_mm(&dev_priv->bdev, TTM_PL_PRIV0);
	ttm_bo_clean_mm(&dev_priv->bdev, TTM_PL_VRAM);
	ttm_bo_clean_mm(&dev_priv->bdev, TTM_PL_TT);

	ttm_global_fini(&dev_priv->mem_global_ref,
			&dev_priv->bo_global_ref,
			&dev_priv->bdev);

#if __OS_HAS_AGP
	if (dev->agp && dev->agp->acquired)
		drm_agp_release(dev);
#endif
	kfree(dev_priv);
	return ret;
}

static int via_driver_load(struct drm_device *dev, unsigned long chipset)
{
	struct drm_via_private *dev_priv;
	int ret = 0;

	dev_priv = kzalloc(sizeof(struct drm_via_private), GFP_KERNEL);
	if (dev_priv == NULL)
		return -ENOMEM;

	dev->dev_private = (void *)dev_priv;
	dev_priv->engine_type = chipset;
	dev_priv->dev = dev;

	via_init_command_verifier();

	ret = via_ttm_init(dev_priv);
	if (ret)
		goto out_err;

	ret = via_detect_vram(dev);
	if (ret)
		goto out_err;

	ret = via_mmio_setup(dev);
	if (ret) {
		DRM_INFO("VIA MMIO region failed to map\n");
		goto out_err;
	}

	chip_revision_info(dev);
	via_init_2d(dev);
	via_init_3d(dev_priv);
	via_init_vq(dev_priv);

#if __OS_HAS_AGP
	if ((dev_priv->engine_type > VIA_ENG_H2) ||
	    (dev->agp && drm_pci_device_is_agp(dev))) {
		ret = via_detect_agp(dev);
		if (!ret)
			via_agp_engine_init(dev_priv);
		else
			DRM_ERROR("Failed to allocate AGP\n");
	}
#endif
	if (pci_is_pcie(dev->pdev)) {
		ret = via_allocate_pcie_gart_table(dev_priv);
		if (ret)
			DRM_ERROR("Failed to allocate DMA memory\n");
	}

	ret = drm_irq_install(dev);
	if (ret)
		goto out_err;

	ret = drm_vblank_init(dev, 2);
	if (ret)
		goto out_err;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		ret = via_modeset_init(dev);
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
}

static void via_reclaim_buffers_locked(struct drm_device *dev,
					struct drm_file *filp)
{
	mutex_lock(&dev->struct_mutex);

	if (dev->driver->dma_quiescent)
		dev->driver->dma_quiescent(dev);

	mutex_unlock(&dev->struct_mutex);
	return;
}

static const struct file_operations via_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = ttm_mmap,
	.poll = drm_poll,
	.fasync = drm_fasync,
	.llseek = noop_llseek,
};

static struct drm_driver via_driver = {
	.driver_features =
		DRIVER_USE_AGP | DRIVER_USE_MTRR | DRIVER_HAVE_IRQ |
		DRIVER_GEM | DRIVER_IRQ_SHARED,
	.load = via_driver_load,
	.unload = via_driver_unload,
	.context_dtor = via_final_context,
	.get_vblank_counter = drm_vblank_count,
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
	.gem_init_object = ttm_gem_init_object,
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

static int __devinit
via_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	return drm_get_pci_dev(pdev, ent, &via_driver);
}

static void __devexit
via_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	drm_put_dev(dev);
}

#ifdef CONFIG_PM
static int
via_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
	return 0;
}

static int
via_pci_resume(struct pci_dev *pdev)
{
	return 0;
}
#endif /* CONFIG_PM */

static struct pci_driver via_pci_driver = {
	.name		= DRIVER_NAME,
	.id_table	= via_pci_table,
};

static int __init via_init(void)
{
	via_driver.num_ioctls = via_max_ioctl;

	if (via_modeset) {
		via_pci_driver.probe	= via_pci_probe;
		via_pci_driver.remove	= __devexit_p(via_pci_remove);
#ifdef CONFIG_PM
		via_pci_driver.suspend	= via_pci_suspend;
		via_pci_driver.resume	= via_pci_resume;
#endif

		via_driver.driver_features |= DRIVER_MODESET;
		via_driver.major = 3;
		via_driver.minor = 0;
	}
	return drm_pci_init(&via_driver, &via_pci_driver);
}

static void __exit via_exit(void)
{
	drm_pci_exit(&via_driver, &via_pci_driver);
}

module_init(via_init);
module_exit(via_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
