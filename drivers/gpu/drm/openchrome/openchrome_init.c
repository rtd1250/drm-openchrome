/*
 * Copyright 2017 Kevin Brace. All Rights Reserved.
 * Copyright 2012 James Simmons <jsimmons@infradead.org>. All Rights Reserved.
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
 * THE AUTHOR(S) OR COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/module.h>
#include <linux/console.h>

#include <drm/drmP.h>
#include "openchrome_drv.h"


int openchrome_mmio_init(
			struct openchrome_drm_private *dev_private)
{
	struct drm_device *dev = dev_private->dev;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/*
	 * PCI BAR1 is the MMIO memory window for all
	 * VIA Technologies Chrome IGPs.
	 * Obtain the starting base address and size, and
	 * map it to the OS for use.
	 */
	dev_private->mmio_base = pci_resource_start(dev->pdev, 1);
	dev_private->mmio_size = pci_resource_len(dev->pdev, 1);
	dev_private->mmio = ioremap(dev_private->mmio_base,
					dev_private->mmio_size);
	if (!dev_private->mmio) {
		ret = -ENOMEM;
		goto exit;
	}

	DRM_INFO("VIA Technologies Chrome IGP MMIO Physical Address: "
			"0x%08llx\n",
			dev_private->mmio_base);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void openchrome_mmio_fini(struct openchrome_drm_private *dev_private)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (dev_private->mmio) {
		iounmap(dev_private->mmio);
		dev_private->mmio = NULL;
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

void openchrome_graphics_unlock(
			struct openchrome_drm_private *dev_private)
{
	uint8_t temp;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/*
	 * Enable VGA subsystem.
	 */
	temp = vga_io_r(0x03C3);
	vga_io_w(0x03C3, temp | 0x01);
	svga_wmisc_mask(VGABASE, BIT(0), BIT(0));

	/*
	 * Unlock VIA Technologies Chrome IGP extended
	 * registers.
	 */
	svga_wseq_mask(VGABASE, 0x10, BIT(0), BIT(0));

	/*
	 * Unlock VIA Technologies Chrome IGP extended
	 * graphics functionality.
	 */
	svga_wseq_mask(VGABASE, 0x1a, BIT(3), BIT(3));

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

void chip_revision_info(struct openchrome_drm_private *dev_private)
{
	struct drm_device *dev = dev_private->dev;
	struct pci_bus *bus = NULL;
	u16 device_id, subsystem_vendor_id, subsystem_device_id;
	u8 tmp;
	int pci_bus;
	u8 pci_device, pci_function;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/*
	 * VX800, VX855, and VX900 chipsets have Chrome IGP
	 * connected as Bus 0, Device 1 PCI device.
	 */
	if ((dev->pdev->device == PCI_DEVICE_ID_VIA_VT1122) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_VX875) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_VX900_VGA)) {

		pci_bus = 0;
		pci_device = 1;
		pci_function = 0;

	/*
	 * For all other devices, Chrome IGP is connected as
	 * Bus 1, Device 0 PCI Device.
	 */
	} else {
		pci_bus = 1;
		pci_device = 0;
		pci_function = 0;
	}

	bus = pci_find_bus(0, pci_bus);
	if (!bus) {
		goto pci_error;
	}

	ret = pci_bus_read_config_word(bus, PCI_DEVFN(pci_device,
							pci_function),
					PCI_DEVICE_ID,
					&device_id);
	if (ret) {
		goto pci_error;
	}

	ret = pci_bus_read_config_word(bus, PCI_DEVFN(pci_device,
							pci_function),
					PCI_SUBSYSTEM_VENDOR_ID,
					&subsystem_vendor_id);
	if (ret) {
		goto pci_error;
	}

	ret = pci_bus_read_config_word(bus, PCI_DEVFN(pci_device,
							pci_function),
					PCI_SUBSYSTEM_ID,
					&subsystem_device_id);
	if (ret) {
		goto pci_error;
	}

	DRM_DEBUG_KMS("DRM Device ID: "
			"0x%04x\n", dev->pdev->device);
	DRM_DEBUG_KMS("Chrome IGP Device ID: "
			"0x%04x\n", device_id);
	DRM_DEBUG_KMS("Chrome IGP Subsystem Vendor ID: "
			"0x%04x\n", subsystem_vendor_id);
	DRM_DEBUG_KMS("Chrome IGP Subsystem Device ID: "
			"0x%04x\n", subsystem_device_id);

	switch (dev->pdev->device) {
	/* CLE266 Chipset */
	case PCI_DEVICE_ID_VIA_CLE266:
		/* CR4F only defined in CLE266.CX chipset. */
		tmp = vga_rcrt(VGABASE, 0x4F);
		vga_wcrt(VGABASE, 0x4F, 0x55);
		if (vga_rcrt(VGABASE, 0x4F) != 0x55) {
			dev_private->revision = CLE266_REVISION_AX;
		} else {
			dev_private->revision = CLE266_REVISION_CX;
		}

		/* Restore original CR4F value. */
		vga_wcrt(VGABASE, 0x4F, tmp);
		break;
	/* CX700 / VX700 Chipset */
	case PCI_DEVICE_ID_VIA_VT3157:
		tmp = vga_rseq(VGABASE, 0x43);
		if (tmp & 0x02) {
			dev_private->revision = CX700_REVISION_700M2;
		} else if (tmp & 0x40) {
			dev_private->revision = CX700_REVISION_700M;
		} else {
			dev_private->revision = CX700_REVISION_700;
		}

		/* Check for VIA Technologies NanoBook reference
		 * design. This is necessary due to its strapping
		 * resistors not being set to indicate the
		 * availability of DVI. */
		if ((subsystem_vendor_id == 0x1509) &&
			(subsystem_device_id == 0x2d30)) {
			dev_private->is_via_nanobook = true;
		} else {
			dev_private->is_via_nanobook = false;
		}

		break;
	/* VX800 / VX820 Chipset */
	case PCI_DEVICE_ID_VIA_VT1122:

		/* Check for Quanta IL1 netbook. This is necessary
		 * due to its flat panel connected to DVP1 (Digital
		 * Video Port 1) rather than its LVDS channel. */
		if ((subsystem_vendor_id == 0x152d) &&
			(subsystem_device_id == 0x0771)) {
			dev_private->is_quanta_il1 = true;
		} else {
			dev_private->is_quanta_il1 = false;
		}

		/* Samsung NC20 netbook has its FP connected to LVDS2
		 * rather than the more logical LVDS1, hence, a special
		 * flag register is needed for properly controlling its
		 * FP. */
		if ((subsystem_vendor_id == 0x144d) &&
			(subsystem_device_id == 0xc04e)) {
			dev_private->is_samsung_nc20 = true;
		} else {
			dev_private->is_samsung_nc20 = false;
		}

		break;
	/* VX855 / VX875 Chipset */
	case PCI_DEVICE_ID_VIA_VX875:
	/* VX900 Chipset */
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		dev_private->revision = vga_rseq(VGABASE, 0x3B);
		break;
	default:
		break;
	}

	goto exit;
pci_error:
	DRM_ERROR("PCI bus related error.");
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

void openchrome_flag_init(struct openchrome_drm_private *dev_private)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/*
	 * Special handling flags for a few special models.
	 */
	dev_private->is_via_nanobook = false;
	dev_private->is_quanta_il1 = false;

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

int openchrome_device_init(struct openchrome_drm_private *dev_private)
{
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	openchrome_flag_init(dev_private);

	ret = via_vram_detect(dev_private);
	if (ret) {
		DRM_ERROR("Failed to detect video RAM.\n");
		goto exit;
	}

	/*
	 * Map VRAM.
	 */
	ret = openchrome_vram_init(dev_private);
	if (ret) {
		DRM_ERROR("Failed to initialize video RAM.\n");
		goto exit;
	}

	ret = openchrome_mmio_init(dev_private);
	if (ret) {
		DRM_ERROR("Failed to initialize MMIO.\n");
		goto exit;
	}

	openchrome_graphics_unlock(dev_private);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void via_init_2d(
			struct openchrome_drm_private *dev_private,
			int pci_device)
{
	int i;

	for (i = 0x04; i < 0x5c; i += 4)
		VIA_WRITE(i, 0x0);

	/* For 410 chip*/
	if (pci_device == PCI_DEVICE_ID_VIA_VX900_VGA)
		VIA_WRITE(0x60, 0x0);
}

static void via_init_3d(
		struct openchrome_drm_private *dev_private)
{
	unsigned long texture_stage;
	int i;

	VIA_WRITE(VIA_REG_TRANSET, 0x00010000);
	for (i = 0; i <= 0x9A; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, i << 24);

	/* guardband clipping default setting */
	VIA_WRITE(VIA_REG_TRANSPACE, (0x88 << 24) | 0x00001ed0);
	VIA_WRITE(VIA_REG_TRANSPACE, (0x89 << 24) | 0x00000800);

	/* Initial Texture Stage Setting */
	for (texture_stage = 0; texture_stage <= 0xf; texture_stage++) {
		VIA_WRITE(VIA_REG_TRANSET, (0x00020000 | 0x00000000 |
					(texture_stage & 0xf) << 24));
		for (i = 0 ; i <= 0x30 ; i++)
			VIA_WRITE(VIA_REG_TRANSPACE, i << 24);
	}

	/* Initial Texture Sampler Setting */
	for (texture_stage = 0; texture_stage <= 0xf; texture_stage++) {
		VIA_WRITE(VIA_REG_TRANSET, (0x00020000 | 0x20000000 |
					(texture_stage & 0x10) << 24));
		for (i = 0; i <= 0x36; i++)
			VIA_WRITE(VIA_REG_TRANSPACE, i << 24);
	}

	VIA_WRITE(VIA_REG_TRANSET, (0x00020000 | 0xfe000000));
	for (i = 0 ; i <= 0x13 ; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, i << 24);

	/* degamma table */
	VIA_WRITE(VIA_REG_TRANSET, (0x00030000 | 0x15000000));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (30 << 20) | (15 << 10) | (5)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((119 << 20) | (81 << 10) | (52)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((283 << 20) | (219 << 10) | (165)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((535 << 20) | (441 << 10) | (357)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((119 << 20) | (884 << 20) | (757 << 10) | (640)));

	/* gamma table */
	VIA_WRITE(VIA_REG_TRANSET, (0x00030000 | 0x17000000));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (13 << 20) | (13 << 10) | (13)));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (26 << 20) | (26 << 10) | (26)));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (39 << 20) | (39 << 10) | (39)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((51 << 20) | (51 << 10) | (51)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((71 << 20) | (71 << 10) | (71)));
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

	/* Initialize INV_ParaSubType_TexPa */
	VIA_WRITE(VIA_REG_TRANSET, (0x00030000 | 0x00000000));
	for (i = 0; i < 16; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);

	/* Initialize INV_ParaSubType_4X4Cof */
	VIA_WRITE(VIA_REG_TRANSET, (0x00030000 | 0x11000000));
	for (i = 0; i < 32; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);

	/* Initialize INV_ParaSubType_StipPal */
	VIA_WRITE(VIA_REG_TRANSET, (0x00030000 | 0x14000000));
	for (i = 0; i < (5 + 3); i++)
		VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);

	/* primitive setting & vertex format */
	VIA_WRITE(VIA_REG_TRANSET, (0x00040000));
	for (i = 0; i <= 0x62; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, (i << 24));

	/* c2s clamping value for screen coordinate */
	VIA_WRITE(VIA_REG_TRANSPACE, (0x50 << 24) | 0x00000000);
	VIA_WRITE(VIA_REG_TRANSPACE, (0x51 << 24) | 0x00000000);
	VIA_WRITE(VIA_REG_TRANSPACE, (0x52 << 24) | 0x00147fff);

	/* ParaType 0xFE - Configure and Misc Setting */
	VIA_WRITE(VIA_REG_TRANSET, (0x00fe0000));
	for (i = 0; i <= 0x47; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, (i << 24));

	/* ParaType 0x11 - Frame Buffer Auto-Swapping and Command Regulator */
	VIA_WRITE(VIA_REG_TRANSET, (0x00110000));
	for (i = 0; i <= 0x20; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, (i << 24));

	VIA_WRITE(VIA_REG_TRANSET, 0x00fe0000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x4000840f);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x47000404);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x44000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x46000005);

	/* setting Misconfig */
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

static void via_init_vq(struct openchrome_drm_private *dev_private)
{
	unsigned long vq_start_addr, vq_end_addr, vqlen;
	unsigned long vqstartl, vqendl, vqstart_endh;
	struct ttm_buffer_object *bo = dev_private->vq.bo;

	if (!bo)
		return;

	vq_start_addr = bo->offset;
	vq_end_addr = vq_start_addr + bo->mem.size - 1;
	vqstartl = 0x70000000 | (vq_start_addr & 0xFFFFFF);
	vqendl = 0x71000000 | (vq_end_addr & 0xFFFFFF);
	vqstart_endh = 0x72000000 | ((vq_start_addr & 0xFF000000) >> 24) |
			((vq_end_addr & 0xFF000000) >> 16);
	vqlen = 0x73000000 | (bo->mem.size >> 3);

	VIA_WRITE(0x41c, 0x00100000);
	VIA_WRITE(0x420, vqstart_endh);
	VIA_WRITE(0x420, vqstartl);
	VIA_WRITE(0x420, vqendl);
	VIA_WRITE(0x420, vqlen);
	VIA_WRITE(0x420, 0x74301001);
	VIA_WRITE(0x420, 0x00000000);
}

static void via_init_pcie_gart_table(
			struct openchrome_drm_private *dev_private,
			struct pci_dev *pdev)
{
	struct ttm_buffer_object *bo = dev_private->gart.bo;
	u8 value;

	if (!pci_is_pcie(pdev) || !bo)
		return;

	/* enable gtt write */
	svga_wseq_mask(VGABASE, 0x6C, 0x00, BIT(7));

	/* set the base address of gart table */
	value = (bo->offset & 0xff000) >> 12;
	vga_wseq(VGABASE, 0x6A, value);

	value = (bo->offset & 0xff000) >> 20;
	vga_wseq(VGABASE, 0x6B, value);

	value = vga_rseq(VGABASE, 0x6C);
	value |= ((bo->offset >> 28) & 0x01);
	vga_wseq(VGABASE, 0x6C, value);

	/* flush the gtt cache */
	svga_wseq_mask(VGABASE, 0x6F, BIT(7), BIT(7));

	/* disable the gtt write */
	svga_wseq_mask(VGABASE, 0x6C, BIT(7), BIT(7));
}

/* This function does:
 * 1. Command buffer allocation
 * 2. hw engine intialization:2D;3D;VQ
 * 3. Ring Buffer mechanism setup
 */
void via_engine_init(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;

	/* initial engines */
	via_init_2d(dev_private, dev->pdev->device);
	via_init_3d(dev_private);
	via_init_vq(dev_private);

	/* pcie gart table setup */
	via_init_pcie_gart_table(dev_private, dev->pdev);
}
