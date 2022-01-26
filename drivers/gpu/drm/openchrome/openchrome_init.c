/*
 * Copyright 2017 Kevin Brace. All Rights Reserved.
 * Copyright 2012 James Simmons <jsimmons@infradead.org>. All Rights Reserved.
 * Copyright 2006-2009 Luc Verhaegen.
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

#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_mode_config.h>

#include "openchrome_drv.h"


static int cle266_mem_type(struct openchrome_drm_private *dev_private,
				struct pci_dev *bridge)
{
	u8 type, fsb, freq;
	int ret;

	ret = pci_read_config_byte(bridge, 0x54, &fsb);
	if (ret)
		return ret;
	ret = pci_read_config_byte(bridge, 0x69, &freq);
	if (ret)
		return ret;

	freq >>= 6;
	fsb >>= 6;

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

	ret = pci_read_config_byte(bridge, 0x60, &fsb);
	if (ret)
		return ret;
	ret = pci_read_config_byte(bridge, 0xE3, &type);
	if (ret)
		return ret;

	/* On bank 2/3 */
	if (type & 0x02)
		fsb >>= 2;

	/* Memory type */
	switch (fsb & 0x03) {
	case 0x00: /* SDR */
		switch (freq) {
		case 66:
			dev_private->vram_type = VIA_MEM_SDR66;
			break;
		case 100:
			dev_private->vram_type = VIA_MEM_SDR100;
			break;
		case 133:
			dev_private->vram_type = VIA_MEM_SDR133;
			break;
		default:
			break;
		}
		break;

	case 0x02: /* DDR */
		switch (freq) {
		case 100:
			dev_private->vram_type = VIA_MEM_DDR_200;
			break;
		case 133:
			dev_private->vram_type = VIA_MEM_DDR_266;
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

static int km400_mem_type(struct openchrome_drm_private *dev_private,
				struct pci_dev *bridge)
{
	u8 fsb, freq, rev;
	int ret;

	ret = pci_read_config_byte(bridge, 0xF6, &rev);
	if (ret)
		return ret;
	ret = pci_read_config_byte(bridge, 0x54, &fsb);
	if (ret)
		return ret;
	ret = pci_read_config_byte(bridge, 0x69, &freq);
	if (ret)
		return ret;

	freq >>= 6;
	fsb >>= 6;

	/* KM400 */
	if (rev < 0x80) {
		/* FSB frequency */
		switch (fsb) {
		case 0x00:
			switch (freq) {
			case 0x00:
				dev_private->vram_type =
						VIA_MEM_DDR_200;
				break;
			case 0x01:
				dev_private->vram_type =
						VIA_MEM_DDR_266;
				break;
			case 0x02:
				dev_private->vram_type =
						VIA_MEM_DDR_400;
				break;
			case 0x03:
				dev_private->vram_type =
						VIA_MEM_DDR_333;
				break;
			default:
				break;
			}
			break;

		case 0x01:
			switch (freq) {
			case 0x00:
				dev_private->vram_type =
						VIA_MEM_DDR_266;
				break;
			case 0x01:
				dev_private->vram_type =
						VIA_MEM_DDR_333;
				break;
			case 0x02:
				dev_private->vram_type =
						VIA_MEM_DDR_400;
				break;
			default:
				break;
			}
			break;

		case 0x02:
		case 0x03:
			switch (freq) {
			case 0x00:
				dev_private->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x02:
				dev_private->vram_type =
						VIA_MEM_DDR_400;
				break;
			case 0x03:
				dev_private->vram_type =
						VIA_MEM_DDR_266;
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
		ret = pci_read_config_byte(bridge, 0x67, &rev);
		if (ret)
			return ret;
		if (rev & 0x80)
			freq |= 0x04;

		switch (fsb) {
		case 0x00:
			switch (freq) {
			case 0x00:
				dev_private->vram_type =
						VIA_MEM_DDR_200;
				break;
			case 0x01:
				dev_private->vram_type =
						VIA_MEM_DDR_266;
				break;
			case 0x03:
				dev_private->vram_type =
						VIA_MEM_DDR_333;
				break;
			case 0x07:
				dev_private->vram_type =
						VIA_MEM_DDR_400;
				break;
			default:
				dev_private->vram_type = VIA_MEM_NONE;
				break;
			}
			break;

		case 0x01:
			switch (freq) {
			case 0x00:
				dev_private->vram_type =
						VIA_MEM_DDR_266;
				break;
			case 0x01:
				dev_private->vram_type =
						VIA_MEM_DDR_333;
				break;
			case 0x03:
				dev_private->vram_type =
						VIA_MEM_DDR_400;
				break;
			default:
				break;
			}
			break;

		case 0x02:
			switch (freq) {
			case 0x00:
				dev_private->vram_type =
						VIA_MEM_DDR_400;
				break;
			case 0x04:
				dev_private->vram_type =
						VIA_MEM_DDR_333;
				break;
			case 0x06:
				dev_private->vram_type =
						VIA_MEM_DDR_266;
				break;
			default:
				break;
			}
			break;

		case 0x03:
			switch (freq) {
			case 0x00:
				dev_private->vram_type =
						VIA_MEM_DDR_333;
				break;
			case 0x01:
				dev_private->vram_type =
						VIA_MEM_DDR_400;
				break;
			case 0x04:
				dev_private->vram_type =
						VIA_MEM_DDR_266;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	return ret;
}

static int p4m800_mem_type(struct openchrome_drm_private *dev_private,
				struct pci_bus *bus,
				struct pci_dev *fn3)
{
	struct pci_dev *fn4 = pci_get_slot(bus, PCI_DEVFN(0, 4));
	int ret, freq = 0;
	u8 type, fsb;

	/* VIA Scratch region */
	ret = pci_read_config_byte(fn4, 0xF3, &fsb);
	if (ret) {
		pci_dev_put(fn4);
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
	pci_dev_put(fn4);

	ret = pci_read_config_byte(fn3, 0x68, &type);
	if (ret)
		return ret;
	type &= 0x0f;

	if (type & 0x02)
		freq -= type >> 2;
	else {
		freq += type >> 2;
		if (type & 0x01)
			freq++;
	}

	switch (freq) {
	case 0x03:
		dev_private->vram_type = VIA_MEM_DDR_200;
		break;
	case 0x04:
		dev_private->vram_type = VIA_MEM_DDR_266;
		break;
	case 0x05:
		dev_private->vram_type = VIA_MEM_DDR_333;
		break;
	case 0x06:
		dev_private->vram_type = VIA_MEM_DDR_400;
		break;
	default:
		break;
	}
	return ret;
}

static int km8xx_mem_type(struct openchrome_drm_private *dev_private)
{
	struct pci_dev *dram, *misc = NULL;
	int ret = -ENXIO;
	u8 type, tmp;

	dram = pci_get_device(PCI_VENDOR_ID_AMD,
			PCI_DEVICE_ID_AMD_K8_NB_MEMCTL, NULL);
	if (dram) {
		misc = pci_get_device(PCI_VENDOR_ID_AMD,
				PCI_DEVICE_ID_AMD_K8_NB_MISC, NULL);

		ret = pci_read_config_byte(misc, 0xFD, &type);
		if (type) {
			pci_read_config_byte(dram, 0x94, &type);
			switch (type & 0x03) {
			case 0x00:
				dev_private->vram_type =
						VIA_MEM_DDR2_400;
				break;
			case 0x01:
				dev_private->vram_type =
						VIA_MEM_DDR2_533;
				break;
			case 0x02:
				dev_private->vram_type =
						VIA_MEM_DDR2_667;
				break;
			case 0x03:
				dev_private->vram_type =
						VIA_MEM_DDR2_800;
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
				dev_private->vram_type =
						VIA_MEM_DDR_200;
				break;
			case 0x02:
				dev_private->vram_type =
						VIA_MEM_DDR_266;
				break;
			case 0x05:
				dev_private->vram_type =
						VIA_MEM_DDR_333;
				break;
			case 0x07:
				dev_private->vram_type =
						VIA_MEM_DDR_400;
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
				dev_private->vram_type =
						VIA_MEM_DDR3_800;
				break;
			case 0x04:
				dev_private->vram_type =
						VIA_MEM_DDR3_1066;
				break;
			case 0x05:
				dev_private->vram_type =
						VIA_MEM_DDR3_1333;
				break;
			case 0x06:
				dev_private->vram_type =
						VIA_MEM_DDR3_1600;
				break;
			default:
				break;
			}
		} else {		/* DDR2 */
			switch (tmp & 0x07) {
			case 0x00:
				dev_private->vram_type =
						VIA_MEM_DDR2_400;
				break;
			case 0x01:
				dev_private->vram_type =
						VIA_MEM_DDR2_533;
				break;
			case 0x02:
				dev_private->vram_type =
						VIA_MEM_DDR2_667;
				break;
			case 0x03:
				dev_private->vram_type =
						VIA_MEM_DDR2_800;
				break;
			case 0x04:
				dev_private->vram_type =
						VIA_MEM_DDR2_1066;
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
			dev_private->vram_type = VIA_MEM_DDR2_533;
			break;
		case 0x02:
			dev_private->vram_type = VIA_MEM_DDR2_667;
			break;
		case 0x03:
			dev_private->vram_type = VIA_MEM_DDR2_800;
			break;
		default:
			break;
		}
	}
	return ret;
}

static int cn400_mem_type(struct openchrome_drm_private *dev_private,
				struct pci_bus *bus,
				struct pci_dev *fn3)
{
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
		dev_private->vram_type = VIA_MEM_DDR_200;
		break;
	case 0x04:
		dev_private->vram_type = VIA_MEM_DDR_266;
		break;
	case 0x05:
		dev_private->vram_type = VIA_MEM_DDR_333;
		break;
	case 0x06:
		dev_private->vram_type = VIA_MEM_DDR_400;
		break;
	default:
		break;
	}
	return ret;
}

static int cn700_mem_type(struct openchrome_drm_private *dev_private,
				struct pci_dev *fn3)
{
	int ret;
	u8 tmp;

	ret = pci_read_config_byte(fn3, 0x90, &tmp);
	if (!ret) {
		switch (tmp & 0x07) {
		case 0x00:
			dev_private->vram_type = VIA_MEM_DDR_200;
			break;
		case 0x01:
			dev_private->vram_type = VIA_MEM_DDR_266;
			break;
		case 0x02:
			dev_private->vram_type = VIA_MEM_DDR_333;
			break;
		case 0x03:
			dev_private->vram_type = VIA_MEM_DDR_400;
			break;
		case 0x04:
			dev_private->vram_type = VIA_MEM_DDR2_400;
			break;
		case 0x05:
			dev_private->vram_type = VIA_MEM_DDR2_533;
			break;
		default:
			break;
		}
	}
	return ret;
}

static int cx700_mem_type(struct openchrome_drm_private *dev_private,
				struct pci_dev *fn3)
{
	u8 type, clock;
	int ret;

	ret = pci_read_config_byte(fn3, 0x90, &clock);
	if (ret)
		return ret;
	ret = pci_read_config_byte(fn3, 0x6C, &type);
	if (ret)
		return ret;
	type &= 0x40;
	type >>= 6;

	switch (type) {
	case 0:
		switch (clock & 0x07) {
		case 0:
			dev_private->vram_type = VIA_MEM_DDR_200;
			break;
		case 1:
			dev_private->vram_type = VIA_MEM_DDR_266;
			break;
		case 2:
			dev_private->vram_type = VIA_MEM_DDR_333;
			break;
		case 3:
			dev_private->vram_type = VIA_MEM_DDR_400;
			break;
		default:
			break;
		}
		break;

	case 1:
		switch (clock & 0x07) {
		case 3:
			dev_private->vram_type = VIA_MEM_DDR2_400;
			break;
		case 4:
			dev_private->vram_type = VIA_MEM_DDR2_533;
			break;
		case 5:
			dev_private->vram_type = VIA_MEM_DDR2_667;
			break;
		case 6:
			dev_private->vram_type = VIA_MEM_DDR2_800;
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

static int vx900_mem_type(struct openchrome_drm_private *dev_private,
				struct pci_dev *fn3)
{
	int ret;
	u8 clock, type, volt;

	ret = pci_read_config_byte(fn3, 0x90, &clock);
	if (ret)
		return ret;
	ret = pci_read_config_byte(fn3, 0x6C, &type);
	if (ret)
		return ret;
	volt = type;
	type &= 0xC0;
	type >>= 6;
	volt &= 0x20;
	volt >>= 5;

	switch (type) {
	case 1:
		switch (clock & 0x0F) {
		case 0:
			if (volt)
				dev_private->vram_type =
						VIA_MEM_DDR2_800;
			else
				dev_private->vram_type =
						VIA_MEM_DDR2_533;
			break;
		case 4:
			dev_private->vram_type = VIA_MEM_DDR2_533;
			break;
		case 5:
			dev_private->vram_type = VIA_MEM_DDR2_667;
			break;
		case 6:
			dev_private->vram_type = VIA_MEM_DDR2_800;
			break;
		case 7:
			dev_private->vram_type = VIA_MEM_DDR2_1066;
			break;
		default:
			break;
		}
		break;
	case 2:
		switch (clock & 0x0F) {
		case 0:
			if (volt)
				dev_private->vram_type =
						VIA_MEM_DDR3_800;
			else
				dev_private->vram_type =
						VIA_MEM_DDR3_533;
			break;
		case 4:
			dev_private->vram_type = VIA_MEM_DDR3_533;
			break;
		case 5:
			dev_private->vram_type = VIA_MEM_DDR3_667;
			break;
		case 6:
			dev_private->vram_type = VIA_MEM_DDR3_800;
			break;
		case 7:
			dev_private->vram_type = VIA_MEM_DDR3_1066;
			break;
		default:
			break;
		}
		break;
	}
	return ret;
}

int openchrome_vram_detect(struct openchrome_drm_private *dev_private)
{
	struct drm_device *dev = dev_private->dev;
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	struct pci_dev *bridge = NULL;
	struct pci_dev *fn3 = NULL;
	char *name = "unknown";
	struct pci_bus *bus;
	u8 size;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	bus = pci_find_bus(0, 0);
	if (bus == NULL) {
		ret = -EINVAL;
		goto out_err;
	}

	bridge = pci_get_slot(bus, PCI_DEVFN(0, 0));
	fn3 = pci_get_slot(bus, PCI_DEVFN(0, 3));

	if (!bridge) {
		ret = -EINVAL;
		DRM_ERROR("No host bridge found...\n");
		goto out_err;
	}

	if (!fn3 && pdev->device != PCI_DEVICE_ID_VIA_CLE266
		&& pdev->device != PCI_DEVICE_ID_VIA_KM400) {
		ret = -EINVAL;
		DRM_ERROR("No function 3 on host bridge...\n");
		goto out_err;
	}

	if (pdev->device == PCI_DEVICE_ID_VIA_VX900_VGA) {
		dev_private->vram_start =
				pci_resource_start(pdev, 2);
	} else {
		dev_private->vram_start =
				pci_resource_start(pdev, 0);
	}

	switch (bridge->device) {

	/* CLE266 */
	case PCI_DEVICE_ID_VIA_862X_0:
		ret = cle266_mem_type(dev_private, bridge);
		if (ret)
			goto out_err;

		ret = pci_read_config_byte(bridge, 0xE1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size =
				(1 << ((size & 0x70) >> 4)) << 20;
		break;

	/* KM400 / KN400 / KM400A / KN400A */
	case PCI_DEVICE_ID_VIA_8378_0:
		ret = km400_mem_type(dev_private, bridge);

		ret = pci_read_config_byte(bridge, 0xE1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size =
				(1 << ((size & 0x70) >> 4)) << 20;
		break;

	/* P4M800 */
	case PCI_DEVICE_ID_VIA_3296_0:
		ret = p4m800_mem_type(dev_private, bus, fn3);

		ret = pci_read_config_byte(fn3, 0xA1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size =
				(1 << ((size & 0x70) >> 4)) << 20;
		break;

	/* K8M800 / K8N800 */
	case PCI_DEVICE_ID_VIA_8380_0:
	/* K8M890 */
	case PCI_DEVICE_ID_VIA_VT3336:
		ret = pci_read_config_byte(fn3, 0xA1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size =
				(1 << ((size & 0x70) >> 4)) << 20;

		if (bridge->device == PCI_DEVICE_ID_VIA_VT3336)
			dev_private->vram_size <<= 2;

		ret = km8xx_mem_type(dev_private);
		if (ret)
			goto out_err;
		break;

	/* CN400 / PM800 / PM880 */
	case PCI_DEVICE_ID_VIA_PX8X0_0:
		ret = pci_read_config_byte(fn3, 0xA1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size =
				(1 << ((size & 0x70) >> 4)) << 20;

		ret = cn400_mem_type(dev_private, bus, fn3);
		if (ret)
			goto out_err;
		break;

	/* P4M800CE / P4M800 Pro / VN800 / CN700 */
	case PCI_DEVICE_ID_VIA_P4M800CE:
	/* P4M900 / VN896 / CN896 */
	case PCI_DEVICE_ID_VIA_VT3364:
		ret = pci_read_config_byte(fn3, 0xA1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size =
				(1 << ((size & 0x70) >> 4)) << 20;

		if (bridge->device != PCI_DEVICE_ID_VIA_P4M800CE)
			dev_private->vram_size <<= 2;

		ret = cn700_mem_type(dev_private, fn3);
		if  (ret)
			goto out_err;
		break;

	/* CX700 / VX700 */
	case PCI_DEVICE_ID_VIA_VT3324:
	/* P4M890 / VN890 */
	case PCI_DEVICE_ID_VIA_P4M890:
	/* VX800 / VX820 */
	case PCI_DEVICE_ID_VIA_VT3353:
	/* VX855 / VX875 */
	case PCI_DEVICE_ID_VIA_VT3409:
		ret = pci_read_config_byte(fn3, 0xA1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size =
				(1 << ((size & 0x70) >> 4)) << 22;

		ret = cx700_mem_type(dev_private, fn3);
		if (ret)
			goto out_err;
		break;

	/* VX900 */
	case PCI_DEVICE_ID_VIA_VT3410:
		ret = pci_read_config_byte(fn3, 0xA1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size =
				(1 << ((size & 0x70) >> 4)) << 22;

		ret = vx900_mem_type(dev_private, fn3);
		if (ret)
			goto out_err;
		break;

	default:
		DRM_ERROR("Unknown north bridge device: 0x%04x\n",
				bridge->device);
		goto out_err;
	}

	switch (dev_private->vram_type) {
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

	DRM_INFO("Found %s video RAM.\n", name);
out_err:
	if (bridge)
		pci_dev_put(bridge);
	if (fn3)
		pci_dev_put(fn3);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void openchrome_flag_init(struct openchrome_drm_private *dev_private)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Set this flag for ttm_bo_device_init. */
	dev_private->need_dma32 = true;

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void openchrome_quirks_init(
			struct openchrome_drm_private *dev_private)
{
	struct drm_device *dev = dev_private->dev;
	struct pci_dev *pdev = to_pci_dev(dev->dev);

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/*
	 * Checking for VIA Technologies NanoBook reference design.
	 * Examples include Everex CloudBook and Sylvania g netbook.
	 * It is also called FIC CE260 or CE261 by its ODM (Original
	 * Design Manufacturer) name.
	 * This device has its strapping resistors set to a wrong
	 * setting to handle DVI. As a result, the code needs to know
	 * this in order to support DVI properly.
	 */
	if ((pdev->device == PCI_DEVICE_ID_VIA_VT3157) &&
		(pdev->subsystem_vendor == 0x1509) &&
		(pdev->subsystem_device == 0x2d30)) {
		dev_private->is_via_nanobook = true;
	} else {
		dev_private->is_via_nanobook = false;
	}

	/*
	 * Check for Quanta IL1 netbook. This is necessary
	 * due to its flat panel connected to DVP1 (Digital
	 * Video Port 1) rather than its LVDS channel.
	 */
	if ((pdev->device == PCI_DEVICE_ID_VIA_VT1122) &&
		(pdev->subsystem_vendor == 0x152d) &&
		(pdev->subsystem_device == 0x0771)) {
		dev_private->is_quanta_il1 = true;
	} else {
		dev_private->is_quanta_il1 = false;
	}

	/*
	 * Samsung NC20 netbook has its FP connected to LVDS2
	 * rather than the more logical LVDS1, hence, a special
	 * flag register is needed for properly controlling its
	 * FP.
	 */
	if ((pdev->device == PCI_DEVICE_ID_VIA_VT1122) &&
		(pdev->subsystem_vendor == 0x144d) &&
		(pdev->subsystem_device == 0xc04e)) {
		dev_private->is_samsung_nc20 = true;
	} else {
		dev_private->is_samsung_nc20 = false;
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

int openchrome_vram_init(struct openchrome_drm_private *dev_private)
{
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Add an MTRR for the video RAM. */
	dev_private->vram_mtrr = arch_phys_wc_add(
					dev_private->vram_start,
					dev_private->vram_size);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void openchrome_vram_fini(struct openchrome_drm_private *dev_private)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (dev_private->vram_mtrr) {
		arch_phys_wc_del(dev_private->vram_mtrr);
		arch_io_free_memtype_wc(dev_private->vram_start,
					dev_private->vram_size);
		dev_private->vram_mtrr = 0;
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

int openchrome_mmio_init(
			struct openchrome_drm_private *dev_private)
{
	struct drm_device *dev = dev_private->dev;
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/*
	 * PCI BAR1 is the MMIO memory window for all
	 * VIA Technologies Chrome IGPs.
	 * Obtain the starting base address and size, and
	 * map it to the OS for use.
	 */
	dev_private->mmio_base = pci_resource_start(pdev, 1);
	dev_private->mmio_size = pci_resource_len(pdev, 1);
	dev_private->mmio = ioremap(dev_private->mmio_base,
					dev_private->mmio_size);
	if (!dev_private->mmio) {
		ret = -ENOMEM;
		goto exit;
	}

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
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	u8 tmp;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	switch (pdev->device) {
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

		break;
	/* VX800 / VX820 Chipset */
	case PCI_DEVICE_ID_VIA_VT1122:
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

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

int openchrome_device_init(struct openchrome_drm_private *dev_private)
{
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	openchrome_flag_init(dev_private);
	openchrome_quirks_init(dev_private);

	ret = openchrome_vram_detect(dev_private);
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

void openchrome_device_fini(struct openchrome_drm_private *dev_private)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	openchrome_mmio_fini(dev_private);
	openchrome_vram_fini(dev_private);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static const struct
drm_mode_config_funcs openchrome_drm_mode_config_funcs = {
	.fb_create		= drm_gem_fb_create,
	.atomic_check		= drm_atomic_helper_check,
	.atomic_commit		= drm_atomic_helper_commit,
};

void openchrome_mode_config_init(
			struct openchrome_drm_private *dev_private)
{
	struct drm_device *dev = dev_private->dev;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	drm_mode_config_init(dev);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;
	dev->mode_config.max_width = 2044;
	dev->mode_config.max_height = 4096;

	dev->mode_config.funcs = &openchrome_drm_mode_config_funcs;

	dev->mode_config.fb_base = dev_private->vram_start;

	dev->mode_config.preferred_depth = 24;
	dev->mode_config.prefer_shadow_fbdev = true;

	dev->mode_config.cursor_width =
	dev->mode_config.cursor_height = OPENCHROME_CURSOR_SIZE;

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

int openchrome_drm_init(struct drm_device *dev)
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
	dev_private->dev = dev;
	dev_private->vram_mtrr = -ENXIO;

	ret = openchrome_device_init(dev_private);
	if (ret) {
		DRM_ERROR("Failed to initialize Chrome IGP.\n");
		goto error_free_private;
	}

	ret = openchrome_mm_init(dev_private);
	if (ret) {
		DRM_ERROR("Failed to initialize TTM.\n");
		goto error_free_private;
	}

	chip_revision_info(dev_private);

	ret = via_modeset_init(dev);
	if (ret) {
		DRM_ERROR("Failed to initialize mode setting.\n");
		goto error_modeset;
	}

	goto exit;
error_modeset:
	via_modeset_fini(dev);
	openchrome_mm_fini(dev_private);
	openchrome_mmio_fini(dev_private);
	openchrome_vram_fini(dev_private);
error_free_private:
	kfree(dev_private);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void openchrome_drm_fini(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	via_modeset_fini(dev);
	openchrome_mm_fini(dev_private);
	openchrome_mmio_fini(dev_private);
	openchrome_vram_fini(dev_private);
	kfree(dev_private);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}
