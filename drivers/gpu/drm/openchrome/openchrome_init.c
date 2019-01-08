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


#if IS_ENABLED(CONFIG_AGP)

#define VIA_AGP_MODE_MASK	0x17
#define VIA_AGPV3_MODE		0x08
#define VIA_AGPV3_8X_MODE	0x02
#define VIA_AGPV3_4X_MODE	0x01
#define VIA_AGP_4X_MODE		0x04
#define VIA_AGP_2X_MODE		0x02
#define VIA_AGP_1X_MODE		0x01
#define VIA_AGP_FW_MODE		0x10

int via_detect_agp(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct drm_agp_info agp_info;
	struct drm_agp_mode mode;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

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

	ret = ttm_bo_init_mm(&dev_private->ttm.bdev, TTM_PL_TT,
				agp_info.aperture_size >> PAGE_SHIFT);
	if (!ret) {
		DRM_INFO("Detected %lu MB of AGP Aperture at "
			"physical address 0x%08lx.\n",
			agp_info.aperture_size >> 20,
			agp_info.aperture_base);
	} else {
out_err0:
		drm_agp_release(dev);
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void via_agp_engine_init(struct openchrome_drm_private *dev_private)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

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

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}
#endif

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
