/*
 * Copyright © 2019 Kevin Brace.
 * Copyright 2012 James Simmons. All Rights Reserved.
 * Copyright 2006-2009 Luc Verhaegen.
 * Copyright 1998-2009 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2009 S3 Graphics, Inc. All Rights Reserved.
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
 *
 * Author(s):
 * Kevin Brace <kevinbrace@bracecomputerlab.com>
 * James Simmons <jsimmons@infradead.org>
 * Luc Verhaegen
 */

#include <linux/pci.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_mode_config.h>
#include <drm/drm_probe_helper.h>

#include "via_drv.h"


static int cle266_mem_type(struct drm_device *dev)
{
	struct pci_dev *gfx_dev = to_pci_dev(dev->dev);
	struct pci_dev *bridge_fn0_dev;
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	u8 fsb, freq, type;
	int ret;

	bridge_fn0_dev =
		pci_get_domain_bus_and_slot(pci_domain_nr(gfx_dev->bus), 0,
						PCI_DEVFN(0, 0));
	if (!bridge_fn0_dev) {
		ret = -ENODEV;
		drm_err(dev, "Host Bridge Function 0 not found! errno: %d\n",
			ret);
		goto exit;
	}

	ret = pci_read_config_byte(bridge_fn0_dev, 0x54, &fsb);
	if (ret) {
		goto error_pci_cfg_read;
	}

	ret = pci_read_config_byte(bridge_fn0_dev, 0x69, &freq);
	if (ret) {
		goto error_pci_cfg_read;
	}

	fsb >>= 6;
	freq >>= 6;

	/* FSB frequency */
	switch (fsb) {
	case 0x01: /* 100MHz */
		switch (freq) {
		case 0x00:
			freq = 100;
			break;
		case 0x01:
			freq = 133;
			break;
		case 0x02:
			freq = 66;
			break;
		default:
			freq = 0;
			break;
		}

		break;
	case 0x02: /* 133 MHz */
	case 0x03:
		switch (freq) {
		case 0x00:
			freq = 133;
			break;
		case 0x02:
			freq = 100;
			break;
		default:
			freq = 0;
			break;
		}

		break;
	default:
		freq = 0;
		break;
	}

	ret = pci_read_config_byte(bridge_fn0_dev, 0x60, &fsb);
	if (ret) {
		goto error_pci_cfg_read;
	}

	ret = pci_read_config_byte(bridge_fn0_dev, 0xe3, &type);
	if (ret) {
		goto error_pci_cfg_read;
	}

	/* On bank 2/3 */
	if (type & 0x02) {
		fsb >>= 2;
	}

	/* Memory type */
	switch (fsb & 0x03) {
	case 0x00: /* SDR */
		switch (freq) {
		case 66:
			dev_priv->vram_type = VIA_MEM_SDR66;
			break;
		case 100:
			dev_priv->vram_type = VIA_MEM_SDR100;
			break;
		case 133:
			dev_priv->vram_type = VIA_MEM_SDR133;
			break;
		default:
			break;
		}

		break;
	case 0x02: /* DDR */
		switch (freq) {
		case 100:
			dev_priv->vram_type = VIA_MEM_DDR_200;
			break;
		case 133:
			dev_priv->vram_type = VIA_MEM_DDR_266;
			break;
		default:
			break;
		}

		break;
	default:
		break;
	}

	goto exit;
error_pci_cfg_read:
	drm_err(dev, "PCI configuration space read error! errno: %d\n", ret);
exit:
	return ret;
}

static int km400_mem_type(struct drm_device *dev)
{
	struct pci_dev *gfx_dev = to_pci_dev(dev->dev);
	struct pci_dev *bridge_fn0_dev;
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	u8 fsb, freq, rev;
	int ret;

	bridge_fn0_dev =
		pci_get_domain_bus_and_slot(pci_domain_nr(gfx_dev->bus), 0,
						PCI_DEVFN(0, 0));
	if (!bridge_fn0_dev) {
		ret = -ENODEV;
		drm_err(dev, "Host Bridge Function 0 not found! errno: %d\n",
			ret);
		goto exit;
	}

	ret = pci_read_config_byte(bridge_fn0_dev, 0x54, &fsb);
	if (ret) {
		goto error_pci_cfg_read;
	}

	ret = pci_read_config_byte(bridge_fn0_dev, 0x69, &freq);
	if (ret) {
		goto error_pci_cfg_read;
	}

	fsb >>= 6;
	freq >>= 6;

	ret = pci_read_config_byte(bridge_fn0_dev, 0xf6, &rev);
	if (ret) {
		goto error_pci_cfg_read;
	}

	/* KM400 */
	if (rev < 0x80) {
		/* FSB frequency */
		switch (fsb) {
		case 0x00:
			switch (freq) {
			case 0x00:
				dev_priv->vram_type = VIA_MEM_DDR_200;
				break;
			case 0x01:
				dev_priv->vram_type = VIA_MEM_DDR_266;
				break;
			case 0x02:
				dev_priv->vram_type = VIA_MEM_DDR_400;
				break;
			case 0x03:
				dev_priv->vram_type = VIA_MEM_DDR_333;
				break;
			default:
				break;
			}

			break;
		case 0x01:
			switch (freq) {
			case 0x00:
				dev_priv->vram_type = VIA_MEM_DDR_266;
				break;
			case 0x01:
				dev_priv->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x02:
				dev_priv->vram_type = VIA_MEM_DDR_400;
				break;
			default:
				break;
			}

			break;
		case 0x02:
		case 0x03:
			switch (freq) {
			case 0x00:
				dev_priv->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x02:
				dev_priv->vram_type = VIA_MEM_DDR_400;
				break;
			case 0x03:
				dev_priv->vram_type = VIA_MEM_DDR_266;
				break;
			default:
				break;
			}

			break;
		default:
			break;
		}
	} else {
		/* KM400A */
		ret = pci_read_config_byte(bridge_fn0_dev, 0x67, &rev);
		if (ret) {
			goto error_pci_cfg_read;
		}

		if (rev & 0x80)
			freq |= 0x04;

		switch (fsb) {
		case 0x00:
			switch (freq) {
			case 0x00:
				dev_priv->vram_type = VIA_MEM_DDR_200;
				break;
			case 0x01:
				dev_priv->vram_type = VIA_MEM_DDR_266;
				break;
			case 0x03:
				dev_priv->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x07:
				dev_priv->vram_type = VIA_MEM_DDR_400;
				break;
			default:
				break;
			}

			break;
		case 0x01:
			switch (freq) {
			case 0x00:
				dev_priv->vram_type = VIA_MEM_DDR_266;
				break;
			case 0x01:
				dev_priv->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x03:
				dev_priv->vram_type = VIA_MEM_DDR_400;
				break;
			default:
				break;
			}

			break;
		case 0x02:
			switch (freq) {
			case 0x00:
				dev_priv->vram_type = VIA_MEM_DDR_400;
				break;
			case 0x04:
				dev_priv->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x06:
				dev_priv->vram_type = VIA_MEM_DDR_266;
				break;
			default:
				break;
			}

			break;
		case 0x03:
			switch (freq) {
			case 0x00:
				dev_priv->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x01:
				dev_priv->vram_type = VIA_MEM_DDR_400;
				break;
			case 0x04:
				dev_priv->vram_type = VIA_MEM_DDR_266;
				break;
			default:
				break;
			}

			break;
		default:
			break;
		}
	}

	goto exit;
error_pci_cfg_read:
	drm_err(dev, "PCI configuration space read error! errno: %d\n", ret);
exit:
	return ret;
}

static int p4m800_mem_type(struct drm_device *dev)
{
	struct pci_dev *gfx_dev = to_pci_dev(dev->dev);
	struct pci_dev *bridge_fn3_dev, *bridge_fn4_dev;
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	int freq = 0;
	u8 fsb, type;
	int ret;

	bridge_fn3_dev =
		pci_get_domain_bus_and_slot(pci_domain_nr(gfx_dev->bus), 0,
						PCI_DEVFN(0, 3));
	if (!bridge_fn3_dev) {
		ret = -ENODEV;
		drm_err(dev, "Host Bridge Function 3 not found! errno: %d\n",
			ret);
		goto exit;
	}

	bridge_fn4_dev =
		pci_get_domain_bus_and_slot(pci_domain_nr(gfx_dev->bus), 0,
						PCI_DEVFN(0, 4));
	if (!bridge_fn4_dev) {
		ret = -ENODEV;
		drm_err(dev, "Host Bridge Function 4 not found! errno: %d\n",
			ret);
		goto exit;
	}

	/* VIA Scratch region */
	ret = pci_read_config_byte(bridge_fn4_dev, 0xf3, &fsb);
	if (ret) {
		goto error_pci_cfg_read;
	}

	fsb >>= 5;
	switch (fsb) {
	case 0x00:
		freq = 0x03; /* 100 MHz */
		break;
	case 0x01:
		freq = 0x04; /* 133 MHz */
		break;
	case 0x02:
		freq = 0x06; /* 200 MHz */
		break;
	case 0x04:
		freq = 0x07; /* 233 MHz */
		break;
	default:
		break;
	}

	ret = pci_read_config_byte(bridge_fn3_dev, 0x68, &type);
	if (ret) {
		goto error_pci_cfg_read;
	}

	type &= 0x0f;
	if (type & 0x02) {
		freq -= type >> 2;
	} else {
		freq += type >> 2;
		if (type & 0x01)
			freq++;
	}

	switch (freq) {
	case 0x03:
		dev_priv->vram_type = VIA_MEM_DDR_200;
		break;
	case 0x04:
		dev_priv->vram_type = VIA_MEM_DDR_266;
		break;
	case 0x05:
		dev_priv->vram_type = VIA_MEM_DDR_333;
		break;
	case 0x06:
		dev_priv->vram_type = VIA_MEM_DDR_400;
		break;
	default:
		break;
	}

	goto exit;
error_pci_cfg_read:
	drm_err(dev, "PCI configuration space read error! errno: %d\n", ret);
exit:
	return ret;
}

static int km8xx_mem_type(struct drm_device *dev)
{
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	struct pci_dev *dram, *misc = NULL;
	int ret = -ENXIO;
	u8 type, tmp;

	dram = pci_get_device(PCI_VENDOR_ID_AMD,
			PCI_DEVICE_ID_AMD_K8_NB_MEMCTL, NULL);
	if (dram) {
		misc = pci_get_device(PCI_VENDOR_ID_AMD,
				PCI_DEVICE_ID_AMD_K8_NB_MISC, NULL);

		ret = pci_read_config_byte(misc, 0xfd, &type);
		if (type) {
			pci_read_config_byte(dram, 0x94, &type);
			switch (type & 0x03) {
			case 0x00:
				dev_priv->vram_type = VIA_MEM_DDR2_400;
				break;
			case 0x01:
				dev_priv->vram_type = VIA_MEM_DDR2_533;
				break;
			case 0x02:
				dev_priv->vram_type = VIA_MEM_DDR2_667;
				break;
			case 0x03:
				dev_priv->vram_type = VIA_MEM_DDR2_800;
				break;
			default:
				break;
			}
		} else {
			ret = pci_read_config_byte(dram, 0x96, &type);
			if (ret)
				return ret;
			type >>= 4;
			type &= 0x07;

			switch (type) {
			case 0x00:
				dev_priv->vram_type = VIA_MEM_DDR_200;
				break;
			case 0x02:
				dev_priv->vram_type = VIA_MEM_DDR_266;
				break;
			case 0x05:
				dev_priv->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x07:
				dev_priv->vram_type = VIA_MEM_DDR_400;
				break;
			default:
				break;
			}
		}
	}

	/* AMD 10h DRAM Controller */
	dram = pci_get_device(PCI_VENDOR_ID_AMD,
			PCI_DEVICE_ID_AMD_10H_NB_DRAM, NULL);
	if (dram) {
		ret = pci_read_config_byte(misc, 0x94, &tmp);
		if (ret)
			return ret;
		ret = pci_read_config_byte(misc, 0x95, &type);
		if (ret)
			return ret;

		if (type & 0x01) {	/* DDR3 */
			switch (tmp & 0x07) {
			case 0x03:
				dev_priv->vram_type = VIA_MEM_DDR3_800;
				break;
			case 0x04:
				dev_priv->vram_type = VIA_MEM_DDR3_1066;
				break;
			case 0x05:
				dev_priv->vram_type = VIA_MEM_DDR3_1333;
				break;
			case 0x06:
				dev_priv->vram_type = VIA_MEM_DDR3_1600;
				break;
			default:
				break;
			}
		} else {		/* DDR2 */
			switch (tmp & 0x07) {
			case 0x00:
				dev_priv->vram_type = VIA_MEM_DDR2_400;
				break;
			case 0x01:
				dev_priv->vram_type = VIA_MEM_DDR2_533;
				break;
			case 0x02:
				dev_priv->vram_type = VIA_MEM_DDR2_667;
				break;
			case 0x03:
				dev_priv->vram_type = VIA_MEM_DDR2_800;
				break;
			case 0x04:
				dev_priv->vram_type = VIA_MEM_DDR2_1066;
				break;
			default:
				break;
			}
		}
	}

	/* AMD 11h DRAM Controller */
	dram = pci_get_device(PCI_VENDOR_ID_AMD,
				PCI_DEVICE_ID_AMD_11H_NB_DRAM, NULL);
	if (dram) {
		ret = pci_read_config_byte(misc, 0x94, &type);
		if (ret)
			return ret;

		switch (tmp & 0x07) {
		case 0x01:
			dev_priv->vram_type = VIA_MEM_DDR2_533;
			break;
		case 0x02:
			dev_priv->vram_type = VIA_MEM_DDR2_667;
			break;
		case 0x03:
			dev_priv->vram_type = VIA_MEM_DDR2_800;
			break;
		default:
			break;
		}
	}
	return ret;
}

static int cn400_mem_type(struct drm_device *dev,
				struct pci_bus *bus,
				struct pci_dev *fn3)
{
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	struct pci_dev *fn2 = pci_get_slot(bus, PCI_DEVFN(0, 2));
	int ret, freq = 0;
	u8 type, fsb;

	ret = pci_read_config_byte(fn2, 0x54, &fsb);
	if (ret) {
		pci_dev_put(fn2);
		return ret;
	}

	switch (fsb >> 5) {
	case 0:
		freq = 3; /* 100 MHz */
		break;
	case 1:
		freq = 4; /* 133 MHz */
		break;
	case 3:
		freq = 5; /* 166 MHz */
		break;
	case 2:
		freq = 6; /* 200 MHz */
		break;
	case 4:
		freq = 7; /* 233 MHz */
		break;
	default:
		break;
	}
	pci_dev_put(fn2);

	ret = pci_read_config_byte(fn3, 0x68, &type);
	if (ret)
		return ret;
	type &= 0x0f;

	if (type & 0x01)
		freq += 1 + (type >> 2);
	else
		freq -= 1 + (type >> 2);

	switch (freq) {
	case 0x03:
		dev_priv->vram_type = VIA_MEM_DDR_200;
		break;
	case 0x04:
		dev_priv->vram_type = VIA_MEM_DDR_266;
		break;
	case 0x05:
		dev_priv->vram_type = VIA_MEM_DDR_333;
		break;
	case 0x06:
		dev_priv->vram_type = VIA_MEM_DDR_400;
		break;
	default:
		break;
	}
	return ret;
}

static int cn700_mem_type(struct drm_device *dev,
				struct pci_dev *fn3)
{
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	int ret;
	u8 tmp;

	ret = pci_read_config_byte(fn3, 0x90, &tmp);
	if (!ret) {
		switch (tmp & 0x07) {
		case 0x00:
			dev_priv->vram_type = VIA_MEM_DDR_200;
			break;
		case 0x01:
			dev_priv->vram_type = VIA_MEM_DDR_266;
			break;
		case 0x02:
			dev_priv->vram_type = VIA_MEM_DDR_333;
			break;
		case 0x03:
			dev_priv->vram_type = VIA_MEM_DDR_400;
			break;
		case 0x04:
			dev_priv->vram_type = VIA_MEM_DDR2_400;
			break;
		case 0x05:
			dev_priv->vram_type = VIA_MEM_DDR2_533;
			break;
		default:
			break;
		}
	}
	return ret;
}

static int cx700_mem_type(struct drm_device *dev,
				struct pci_dev *fn3)
{
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	u8 type, clock;
	int ret;

	ret = pci_read_config_byte(fn3, 0x90, &clock);
	if (ret)
		return ret;
	ret = pci_read_config_byte(fn3, 0x6c, &type);
	if (ret)
		return ret;
	type &= 0x40;
	type >>= 6;

	switch (type) {
	case 0:
		switch (clock & 0x07) {
		case 0:
			dev_priv->vram_type = VIA_MEM_DDR_200;
			break;
		case 1:
			dev_priv->vram_type = VIA_MEM_DDR_266;
			break;
		case 2:
			dev_priv->vram_type = VIA_MEM_DDR_333;
			break;
		case 3:
			dev_priv->vram_type = VIA_MEM_DDR_400;
			break;
		default:
			break;
		}
		break;

	case 1:
		switch (clock & 0x07) {
		case 3:
			dev_priv->vram_type = VIA_MEM_DDR2_400;
			break;
		case 4:
			dev_priv->vram_type = VIA_MEM_DDR2_533;
			break;
		case 5:
			dev_priv->vram_type = VIA_MEM_DDR2_667;
			break;
		case 6:
			dev_priv->vram_type = VIA_MEM_DDR2_800;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return ret;
}

static int vx900_mem_type(struct drm_device *dev,
				struct pci_dev *fn3)
{
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	int ret;
	u8 clock, type, volt;

	ret = pci_read_config_byte(fn3, 0x90, &clock);
	if (ret)
		return ret;
	ret = pci_read_config_byte(fn3, 0x6c, &type);
	if (ret)
		return ret;
	volt = type;
	type &= 0xc0;
	type >>= 6;
	volt &= 0x20;
	volt >>= 5;

	switch (type) {
	case 1:
		switch (clock & 0x0f) {
		case 0:
			if (volt)
				dev_priv->vram_type = VIA_MEM_DDR2_800;
			else
				dev_priv->vram_type = VIA_MEM_DDR2_533;
			break;
		case 4:
			dev_priv->vram_type = VIA_MEM_DDR2_533;
			break;
		case 5:
			dev_priv->vram_type = VIA_MEM_DDR2_667;
			break;
		case 6:
			dev_priv->vram_type = VIA_MEM_DDR2_800;
			break;
		case 7:
			dev_priv->vram_type = VIA_MEM_DDR2_1066;
			break;
		default:
			break;
		}
		break;
	case 2:
		switch (clock & 0x0f) {
		case 0:
			if (volt)
				dev_priv->vram_type = VIA_MEM_DDR3_800;
			else
				dev_priv->vram_type = VIA_MEM_DDR3_533;
			break;
		case 4:
			dev_priv->vram_type = VIA_MEM_DDR3_533;
			break;
		case 5:
			dev_priv->vram_type = VIA_MEM_DDR3_667;
			break;
		case 6:
			dev_priv->vram_type = VIA_MEM_DDR3_800;
			break;
		case 7:
			dev_priv->vram_type = VIA_MEM_DDR3_1066;
			break;
		default:
			break;
		}
		break;
	}
	return ret;
}

static void via_quirks_init(struct drm_device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);

	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	/*
	 * Checking for VIA Technologies NanoBook reference design.
	 * Examples include Everex CloudBook and Sylvania g netbook.
	 * It is also called FIC CE260 or CE261 by its ODM (Original
	 * Design Manufacturer) name.
	 * This device has its strapping resistors set to a wrong
	 * setting to handle DVI. As a result, the code needs to know
	 * this in order to support DVI properly.
	 */
	if ((pdev->device == PCI_DEVICE_ID_VIA_UNICHROME_PRO_II) &&
		(pdev->subsystem_vendor == 0x1509) &&
		(pdev->subsystem_device == 0x2d30)) {
		dev_priv->is_via_nanobook = true;
	} else {
		dev_priv->is_via_nanobook = false;
	}

	/*
	 * Check for Quanta IL1 netbook. This is necessary
	 * due to its flat panel connected to DVP1 (Digital
	 * Video Port 1) rather than its LVDS channel.
	 */
	if ((pdev->device == PCI_DEVICE_ID_VIA_CHROME9_HC3) &&
		(pdev->subsystem_vendor == 0x152d) &&
		(pdev->subsystem_device == 0x0771)) {
		dev_priv->is_quanta_il1 = true;
	} else {
		dev_priv->is_quanta_il1 = false;
	}

	/*
	 * Samsung NC20 netbook has its FP connected to LVDS2
	 * rather than the more logical LVDS1, hence, a special
	 * flag register is needed for properly controlling its
	 * FP.
	 */
	if ((pdev->device == PCI_DEVICE_ID_VIA_CHROME9_HC3) &&
		(pdev->subsystem_vendor == 0x144d) &&
		(pdev->subsystem_device == 0xc04e)) {
		dev_priv->is_samsung_nc20 = true;
	} else {
		dev_priv->is_samsung_nc20 = false;
	}

	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
}

static int via_vram_init(struct drm_device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	struct pci_bus *bus;
	struct pci_dev *hb_fn0 = NULL;
	struct pci_dev *hb_fn3 = NULL;
	char *name = "unknown";
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	u8 size;
	int ret = 0;

	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	bus = pci_find_bus(0, 0);
	if (!bus) {
		ret = -ENODEV;
		drm_err(dev, "PCI bus not found!\n");
		goto exit;
	}

	hb_fn0 = pci_get_slot(bus, PCI_DEVFN(0, 0));
	if (!hb_fn0) {
		ret = -ENODEV;
		drm_err(dev, "Host Bridge Function 0 not found!\n");
		goto exit;
	}

	if ((pdev->device != PCI_DEVICE_ID_VIA_CLE266_GFX) ||
		(pdev->device != PCI_DEVICE_ID_VIA_KM400_GFX)) {
		hb_fn3 = pci_get_slot(bus, PCI_DEVFN(0, 3));
		if (!hb_fn3) {
			ret = -ENODEV;
			drm_err(dev, "Host Bridge Function 3 not found!\n");
			goto error_hb_fn0;
		}
	}

	if (pdev->device == PCI_DEVICE_ID_VIA_CHROME9_HD) {
		dev_priv->vram_start = pci_resource_start(pdev, 2);
	} else {
		dev_priv->vram_start = pci_resource_start(pdev, 0);
	}

	switch (hb_fn0->device) {

	/* CLE266 */
	case PCI_DEVICE_ID_VIA_862X_0:
		ret = cle266_mem_type(dev);
		if (ret)
			goto error_hb_fn0;

		ret = pci_read_config_byte(hb_fn0, 0xe1, &size);
		if (ret)
			goto error_hb_fn0;
		dev_priv->vram_size = (1 << ((size & 0x70) >> 4)) << 20;
		break;

	/* KM400(A) / KN400(A) */
	case PCI_DEVICE_ID_VIA_8378_0:
		ret = km400_mem_type(dev);
		if (ret)
			goto error_hb_fn0;

		ret = pci_read_config_byte(hb_fn0, 0xe1, &size);
		if (ret)
			goto error_hb_fn0;
		dev_priv->vram_size = (1 << ((size & 0x70) >> 4)) << 20;
		break;

	/* P4M800 */
	case PCI_DEVICE_ID_VIA_3296_0:
		ret = p4m800_mem_type(dev);

		ret = pci_read_config_byte(hb_fn3, 0xa1, &size);
		if (ret)
			goto error_hb_fn3;
		dev_priv->vram_size = (1 << ((size & 0x70) >> 4)) << 20;
		break;

	/* K8M800 / K8N800 */
	case PCI_DEVICE_ID_VIA_8380_0:
	/* K8M890 */
	case PCI_DEVICE_ID_VIA_VT3336:
		ret = pci_read_config_byte(hb_fn3, 0xa1, &size);
		if (ret)
			goto error_hb_fn3;
		dev_priv->vram_size = (1 << ((size & 0x70) >> 4)) << 20;

		if (hb_fn0->device == PCI_DEVICE_ID_VIA_VT3336)
			dev_priv->vram_size <<= 2;

		ret = km8xx_mem_type(dev);
		if (ret)
			goto error_hb_fn3;
		break;

	/* CN400 / PM800 / PM880 */
	case PCI_DEVICE_ID_VIA_PX8X0_0:
		ret = pci_read_config_byte(hb_fn3, 0xa1, &size);
		if (ret)
			goto error_hb_fn3;
		dev_priv->vram_size = (1 << ((size & 0x70) >> 4)) << 20;

		ret = cn400_mem_type(dev, bus, hb_fn3);
		if (ret)
			goto error_hb_fn3;
		break;

	/* P4M800CE / P4M800 Pro / VN800 / CN700 */
	case PCI_DEVICE_ID_VIA_P4M800CE:
	/* P4M900 / VN896 / CN896 */
	case PCI_DEVICE_ID_VIA_VT3364:
		ret = pci_read_config_byte(hb_fn3, 0xa1, &size);
		if (ret)
			goto error_hb_fn3;
		dev_priv->vram_size = (1 << ((size & 0x70) >> 4)) << 20;

		if (hb_fn0->device != PCI_DEVICE_ID_VIA_P4M800CE)
			dev_priv->vram_size <<= 2;

		ret = cn700_mem_type(dev, hb_fn3);
		if (ret)
			goto error_hb_fn3;
		break;

	/* CX700 / VX700 */
	case PCI_DEVICE_ID_VIA_VT3324:
	/* P4M890 / VN890 */
	case PCI_DEVICE_ID_VIA_P4M890:
	/* VX800 / VX820 */
	case PCI_DEVICE_ID_VIA_VX800_HB:
	/* VX855 / VX875 */
	case PCI_DEVICE_ID_VIA_VX855_HB:
		ret = pci_read_config_byte(hb_fn3, 0xa1, &size);
		if (ret)
			goto error_hb_fn3;
		dev_priv->vram_size = (1 << ((size & 0x70) >> 4)) << 22;

		ret = cx700_mem_type(dev, hb_fn3);
		if (ret)
			goto error_hb_fn3;
		break;

	/* VX900 */
	case PCI_DEVICE_ID_VIA_VX900_HB:
		ret = pci_read_config_byte(hb_fn3, 0xa1, &size);
		if (ret)
			goto error_hb_fn3;
		dev_priv->vram_size = (1 << ((size & 0x70) >> 4)) << 22;

		ret = vx900_mem_type(dev, hb_fn3);
		if (ret)
			goto error_hb_fn3;
		break;

	default:
		ret = -ENODEV;
		drm_err(dev, "Unknown Host Bridge device: 0x%04x\n",
				hb_fn0->device);
		goto error_hb_fn3;
	}

	switch (dev_priv->vram_type) {
	case VIA_MEM_SDR66:
		name = "SDR 66";
		break;
	case VIA_MEM_SDR100:
		name = "SDR 100";
		break;
	case VIA_MEM_SDR133:
		name = "SDR 133";
		break;
	case VIA_MEM_DDR_200:
		name = "DDR 200";
		break;
	case VIA_MEM_DDR_266:
		name = "DDR 266";
		break;
	case VIA_MEM_DDR_333:
		name = "DDR 333";
		break;
	case VIA_MEM_DDR_400:
		name = "DDR 400";
		break;
	case VIA_MEM_DDR2_400:
		name = "DDR2 400";
		break;
	case VIA_MEM_DDR2_533:
		name = "DDR2 533";
		break;
	case VIA_MEM_DDR2_667:
		name = "DDR2 667";
		break;
	case VIA_MEM_DDR2_800:
		name = "DDR2 800";
		break;
	case VIA_MEM_DDR2_1066:
		name = "DDR2 1066";
		break;
	case VIA_MEM_DDR3_533:
		name = "DDR3 533";
		break;
	case VIA_MEM_DDR3_667:
		name = "DDR3 667";
		break;
	case VIA_MEM_DDR3_800:
		name = "DDR3 800";
		break;
	case VIA_MEM_DDR3_1066:
		name = "DDR3 1066";
		break;
	case VIA_MEM_DDR3_1333:
		name = "DDR3 1333";
		break;
	case VIA_MEM_DDR3_1600:
		name = "DDR3 1600";
		break;
	default:
		break;
	}

	drm_dbg_driver(dev, "Found %s video RAM.\n", name);

	/* Add an MTRR for the video RAM. */
	dev_priv->vram_mtrr = arch_phys_wc_add(dev_priv->vram_start,
						dev_priv->vram_size);
	goto exit;
error_hb_fn3:
	if (hb_fn3)
		pci_dev_put(hb_fn3);
error_hb_fn0:
	if (hb_fn0)
		pci_dev_put(hb_fn0);
exit:
	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
	return ret;
}

static void via_vram_fini(struct drm_device *dev)
{
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);

	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	if (dev_priv->vram_mtrr) {
		arch_phys_wc_del(dev_priv->vram_mtrr);
		arch_io_free_memtype_wc(dev_priv->vram_start,
					dev_priv->vram_size);
		dev_priv->vram_mtrr = 0;
	}

	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
}

static int via_mmio_init(struct drm_device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	int ret = 0;

	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	/*
	 * PCI BAR1 is the MMIO memory window for all
	 * VIA Technologies Chrome IGPs.
	 * Obtain the starting base address and size, and
	 * map it to the OS for use.
	 */
	dev_priv->mmio_base = pci_resource_start(pdev, 1);
	dev_priv->mmio_size = pci_resource_len(pdev, 1);
	dev_priv->mmio = ioremap(dev_priv->mmio_base,
					dev_priv->mmio_size);
	if (!dev_priv->mmio) {
		ret = -ENOMEM;
		goto exit;
	}

exit:
	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
	return ret;
}

static void via_mmio_fini(struct drm_device *dev)
{
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);

	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	if (dev_priv->mmio) {
		iounmap(dev_priv->mmio);
		dev_priv->mmio = NULL;
	}

	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
}

static void via_graphics_unlock(struct drm_device *dev)
{
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	uint8_t temp;

	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	/*
	 * Enable VGA subsystem.
	 */
	temp = vga_io_r(0x03c3);
	vga_io_w(0x03c3, temp | 0x01);
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

	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
}

static void via_chip_revision_info(struct drm_device *dev)
{
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	u8 tmp;

	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	switch (pdev->device) {
	/* CLE266 Chipset */
	case PCI_DEVICE_ID_VIA_CLE266_GFX:
		/* CR4F only defined in CLE266.CX chipset. */
		tmp = vga_rcrt(VGABASE, 0x4f);
		vga_wcrt(VGABASE, 0x4f, 0x55);
		if (vga_rcrt(VGABASE, 0x4f) != 0x55) {
			dev_priv->revision = CLE266_REVISION_AX;
		} else {
			dev_priv->revision = CLE266_REVISION_CX;
		}

		/* Restore original CR4F value. */
		vga_wcrt(VGABASE, 0x4f, tmp);
		break;
	/* CX700 / VX700 Chipset */
	case PCI_DEVICE_ID_VIA_UNICHROME_PRO_II:
		tmp = vga_rseq(VGABASE, 0x43);
		if (tmp & 0x02) {
			dev_priv->revision = CX700_REVISION_700M2;
		} else if (tmp & 0x40) {
			dev_priv->revision = CX700_REVISION_700M;
		} else {
			dev_priv->revision = CX700_REVISION_700;
		}

		break;
	/* VX800 / VX820 Chipset */
	case PCI_DEVICE_ID_VIA_CHROME9_HC3:
		break;
	/* VX855 / VX875 Chipset */
	case PCI_DEVICE_ID_VIA_CHROME9_HCM:
	/* VX900 Chipset */
	case PCI_DEVICE_ID_VIA_CHROME9_HD:
		dev_priv->revision = vga_rseq(VGABASE, 0x3b);
		break;
	default:
		break;
	}

	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
}

static int via_device_init(struct drm_device *dev)
{
	int ret;

	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	via_quirks_init(dev);

	/*
	 * Map VRAM.
	 */
	ret = via_vram_init(dev);
	if (ret) {
		drm_err(dev, "Failed to initialize video RAM.\n");
		goto exit;
	}

	ret = via_mmio_init(dev);
	if (ret) {
		drm_err(dev, "Failed to initialize MMIO.\n");
		goto error_mmio_init;
	}

	via_graphics_unlock(dev);
	goto exit;
error_mmio_init:
	via_vram_fini(dev);
exit:
	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
	return ret;
}

static void via_device_fini(struct drm_device *dev)
{
	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	via_mmio_fini(dev);
	via_vram_fini(dev);

	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
}

static const struct drm_mode_config_funcs via_drm_mode_config_funcs = {
	.fb_create		= drm_gem_fb_create,
	.atomic_check		= drm_atomic_helper_check,
	.atomic_commit		= drm_atomic_helper_commit,
};

int via_modeset_init(struct drm_device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	uint32_t i;
	int ret = 0;

	/* Initialize the number of display connectors. */
	dev_priv->number_fp = 0;
	dev_priv->number_dvi = 0;

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;
	dev->mode_config.max_width = 2044;
	dev->mode_config.max_height = 4096;

	dev->mode_config.funcs = &via_drm_mode_config_funcs;

	dev->mode_config.preferred_depth = 24;

	dev->mode_config.cursor_width =
	dev->mode_config.cursor_height = VIA_CURSOR_SIZE;

	ret = drmm_mode_config_init(dev);
	if (ret) {
		drm_err(dev, "Failed to initialize mode setting "
				"configuration!\n");
		goto exit;
	}

	via_i2c_reg_init(dev_priv);
	ret = via_i2c_init(dev);
	if (ret) {
		drm_err(dev, "Failed to initialize I2C bus!\n");
		goto exit;
	}

	for (i = 0; i < VIA_MAX_CRTC; i++) {
		ret = via_crtc_init(dev_priv, i);
		if (ret) {
			drm_err(dev, "Failed to initialize CRTC %u!\n", i + 1);
			goto error_crtc_init;
		}
	}

	via_ext_dvi_probe(dev);
	via_tmds_probe(dev);

	via_lvds_probe(dev);

	via_dac_probe(dev);


	via_ext_dvi_init(dev);
	via_tmds_init(dev);

	via_dac_init(dev);

	via_lvds_init(dev);

	switch (pdev->device) {
	case PCI_DEVICE_ID_VIA_CHROME9_HD:
		via_hdmi_init(dev, VIA_DI_PORT_NONE);
		break;
	default:
		break;
	}

	drm_mode_config_reset(dev);

	drm_kms_helper_poll_init(dev);
	goto exit;
error_crtc_init:
	via_i2c_exit();
exit:
	return ret;
}

void via_modeset_fini(struct drm_device *dev)
{
	drm_kms_helper_poll_fini(dev);

	drm_helper_force_disable_all(dev);

	via_i2c_exit();
}

int via_drm_init(struct drm_device *dev)
{
	int ret = 0;

	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	ret = via_device_init(dev);
	if (ret) {
		drm_err(dev, "Failed to initialize Chrome IGP!\n");
		goto exit;
	}

	ret = via_mm_init(dev);
	if (ret) {
		drm_err(dev, "Failed to initialize TTM!\n");
		goto error_mm_init;
	}

	via_chip_revision_info(dev);

	ret = via_modeset_init(dev);
	if (ret) {
		drm_err(dev, "Failed to initialize mode setting!\n");
		goto error_modeset_init;
	}

	goto exit;
error_modeset_init:
	via_mm_fini(dev);
error_mm_init:
	via_device_fini(dev);
exit:
	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
	return ret;
}

void via_drm_fini(struct drm_device *dev)
{
	drm_dbg_driver(dev, "Entered %s.\n", __func__);

	via_modeset_fini(dev);
	via_mm_fini(dev);
	via_device_fini(dev);

	drm_dbg_driver(dev, "Exiting %s.\n", __func__);
}
