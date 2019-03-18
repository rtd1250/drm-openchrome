/*
 * Copyright 2012 James Simmons <jsimmons@infradead.org>. All Rights Reserved.
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

#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_crtc_helper.h>

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
		default:
			break;
		}
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
				dev_private->vram_type = VIA_MEM_DDR_200;
				break;
			case 0x01:
				dev_private->vram_type = VIA_MEM_DDR_266;
				break;
			case 0x02:
				dev_private->vram_type = VIA_MEM_DDR_400;
				break;
			case 0x03:
				dev_private->vram_type = VIA_MEM_DDR_333;
			default:
				break;
			}
			break;

		case 0x01:
			switch (freq) {
			case 0x00:
				dev_private->vram_type = VIA_MEM_DDR_266;
				break;
			case 0x01:
				dev_private->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x02:
				dev_private->vram_type = VIA_MEM_DDR_400;
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
				dev_private->vram_type = VIA_MEM_DDR_400;
				break;
			case 0x03:
				dev_private->vram_type = VIA_MEM_DDR_266;
			default:
				break;
			}
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
				dev_private->vram_type = VIA_MEM_DDR_200;
				break;
			case 0x01:
				dev_private->vram_type = VIA_MEM_DDR_266;
				break;
			case 0x03:
				dev_private->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x07:
				dev_private->vram_type = VIA_MEM_DDR_400;
				break;
			default:
				dev_private->vram_type = VIA_MEM_NONE;
				break;
			}
			break;

		case 0x01:
			switch (freq) {
			case 0x00:
				dev_private->vram_type = VIA_MEM_DDR_266;
				break;
			case 0x01:
				dev_private->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x03:
				dev_private->vram_type = VIA_MEM_DDR_400;
			default:
				break;
			}
			break;

		case 0x02:
			switch (freq) {
			case 0x00:
				dev_private->vram_type = VIA_MEM_DDR_400;
				break;
			case 0x04:
				dev_private->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x06:
				dev_private->vram_type = VIA_MEM_DDR_266;
			default:
				break;
			}
			break;

		case 0x03:
			switch (freq) {
			case 0x00:
				dev_private->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x01:
				dev_private->vram_type = VIA_MEM_DDR_400;
				break;
			case 0x04:
				dev_private->vram_type = VIA_MEM_DDR_266;
			default:
				break;
			}
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

	dram = pci_get_device(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_K8_NB_MEMCTL, NULL);
	if (dram) {
		misc = pci_get_device(PCI_VENDOR_ID_AMD,
					PCI_DEVICE_ID_AMD_K8_NB_MISC, NULL);

		ret = pci_read_config_byte(misc, 0xFD, &type);
		if (type) {
			pci_read_config_byte(dram, 0x94, &type);
			switch (type & 0x03) {
			case 0x00:
				dev_private->vram_type = VIA_MEM_DDR2_400;
				break;
			case 0x01:
				dev_private->vram_type = VIA_MEM_DDR2_533;
				break;
			case 0x02:
				dev_private->vram_type = VIA_MEM_DDR2_667;
				break;
			case 0x03:
				dev_private->vram_type = VIA_MEM_DDR2_800;
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
				dev_private->vram_type = VIA_MEM_DDR_200;
				break;
			case 0x02:
				dev_private->vram_type = VIA_MEM_DDR_266;
				break;
			case 0x05:
				dev_private->vram_type = VIA_MEM_DDR_333;
				break;
			case 0x07:
				dev_private->vram_type = VIA_MEM_DDR_400;
			default:
				break;
			}
		}
	}

	/* AMD 10h DRAM Controller */
	dram = pci_get_device(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_10H_NB_DRAM, NULL);
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
				dev_private->vram_type = VIA_MEM_DDR3_800;
				break;
			case 0x04:
				dev_private->vram_type = VIA_MEM_DDR3_1066;
				break;
			case 0x05:
				dev_private->vram_type = VIA_MEM_DDR3_1333;
				break;
			case 0x06:
				dev_private->vram_type = VIA_MEM_DDR3_1600;
			default:
				break;
			}
		} else {		/* DDR2 */
			switch (tmp & 0x07) {
			case 0x00:
				dev_private->vram_type = VIA_MEM_DDR2_400;
				break;
			case 0x01:
				dev_private->vram_type = VIA_MEM_DDR2_533;
				break;
			case 0x02:
				dev_private->vram_type = VIA_MEM_DDR2_667;
				break;
			case 0x03:
				dev_private->vram_type = VIA_MEM_DDR2_800;
				break;
			case 0x04:
				dev_private->vram_type = VIA_MEM_DDR2_1066;
			default:
				break;
			}
		}
	}

	/* AMD 11h DRAM Controller */
	dram = pci_get_device(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_11H_NB_DRAM, NULL);
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
		default:
			break;
		}
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
				dev_private->vram_type = VIA_MEM_DDR2_800;
			else
				dev_private->vram_type = VIA_MEM_DDR2_533;
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
		default:
			break;
		}
		break;
	case 2:
		switch (clock & 0x0F) {
		case 0:
			if (volt)
				dev_private->vram_type = VIA_MEM_DDR3_800;
			else
				dev_private->vram_type = VIA_MEM_DDR3_533;
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
		default:
			break;
		}
		break;
	}
	return ret;
}

int via_vram_detect(struct openchrome_drm_private *dev_private)
{
	struct drm_device *dev = dev_private->dev;
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

	if (!fn3 && dev->pdev->device != PCI_DEVICE_ID_VIA_CLE266
		&& dev->pdev->device != PCI_DEVICE_ID_VIA_KM400) {
		ret = -EINVAL;
		DRM_ERROR("No function 3 on host bridge...\n");
		goto out_err;
	}
	dev_private->vram_start = pci_resource_start(dev->pdev, 0);

	switch (bridge->device) {

	/* CLE266 */
	case PCI_DEVICE_ID_VIA_862X_0:
		ret = cle266_mem_type(dev_private, bridge);
		if (ret)
			goto out_err;

		ret = pci_read_config_byte(bridge, 0xE1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size = (1 << ((size & 0x70) >> 4)) << 20;
		break;

	/* KM400 / KN400 / KM400A / KN400A */
	case PCI_DEVICE_ID_VIA_8378_0:
		ret = km400_mem_type(dev_private, bridge);

		ret = pci_read_config_byte(bridge, 0xE1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size = (1 << ((size & 0x70) >> 4)) << 20;
		break;

	/* P4M800 */
	case PCI_DEVICE_ID_VIA_3296_0:
		ret = p4m800_mem_type(dev_private, bus, fn3);

		ret = pci_read_config_byte(fn3, 0xA1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size = (1 << ((size & 0x70) >> 4)) << 20;
		break;

	/* K8M800 / K8N800 */
	case PCI_DEVICE_ID_VIA_8380_0:
	/* K8M890 */
	case PCI_DEVICE_ID_VIA_VT3336:
		ret = pci_read_config_byte(fn3, 0xA1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size = (1 << ((size & 0x70) >> 4)) << 20;

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
		dev_private->vram_size = (1 << ((size & 0x70) >> 4)) << 20;

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
		dev_private->vram_size = (1 << ((size & 0x70) >> 4)) << 20;

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
		dev_private->vram_size = (1 << ((size & 0x70) >> 4)) << 22;

		ret = cx700_mem_type(dev_private, fn3);
		if (ret)
			goto out_err;
		break;

	/* VX900 */
	case PCI_DEVICE_ID_VIA_VT3410:
		dev_private->vram_start = pci_resource_start(dev->pdev, 2);

		ret = pci_read_config_byte(fn3, 0xA1, &size);
		if (ret)
			goto out_err;
		dev_private->vram_size = (1 << ((size & 0x70) >> 4)) << 22;

		ret = vx900_mem_type(dev_private, fn3);
		if (ret)
			goto out_err;
		break;

	default:
		DRM_ERROR("Unknown north bridge device: 0x%04x\n", bridge->device);
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

    DRM_INFO("Found %s video RAM.\n",
                name);
out_err:
	if (bridge)
		pci_dev_put(bridge);
	if (fn3)
		pci_dev_put(fn3);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

int openchrome_vram_init(struct openchrome_drm_private *dev_private)
{
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Add an MTRR for the video RAM. */
	dev_private->vram_mtrr = arch_phys_wc_add(
					dev_private->vram_start,
					dev_private->vram_size);

	DRM_INFO("VIA Technologies Chrome IGP VRAM "
			"Physical Address: 0x%08llx\n",
			dev_private->vram_start);
	DRM_INFO("VIA Technologies Chrome IGP VRAM "
			"Size: %llu\n",
			(unsigned long long) dev_private->vram_size >> 20);

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

static int
via_user_framebuffer_create_handle(struct drm_framebuffer *fb,
					struct drm_file *file_priv,
					unsigned int *handle)
{
	struct via_framebuffer *via_fb = container_of(fb,
					struct via_framebuffer, fb);
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ret = drm_gem_handle_create(file_priv, via_fb->gem, handle);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void
via_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct via_framebuffer *via_fb = container_of(fb,
					struct via_framebuffer, fb);

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (via_fb->gem) {
		drm_gem_object_put_unlocked(via_fb->gem);
		via_fb->gem = NULL;
	}

	drm_framebuffer_cleanup(fb);
	kfree(via_fb);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static const struct drm_framebuffer_funcs via_fb_funcs = {
	.create_handle	= via_user_framebuffer_create_handle,
	.destroy	= via_user_framebuffer_destroy,
};

static void
via_output_poll_changed(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;

	drm_fb_helper_hotplug_event(&dev_private->via_fbdev->helper);
}

static struct drm_framebuffer *
via_user_framebuffer_create(struct drm_device *dev,
				struct drm_file *file_priv,
				const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct via_framebuffer *via_fb;
	struct drm_gem_object *gem;
	int ret;

	gem = drm_gem_object_lookup(file_priv, mode_cmd->handles[0]);
	if (!gem) {
		DRM_ERROR("No GEM object found for handle 0x%08X\n",
				mode_cmd->handles[0]);
		return ERR_PTR(-ENOENT);
	}

	via_fb = kzalloc(sizeof(struct via_framebuffer), GFP_KERNEL);
	if (!via_fb) {
		return ERR_PTR(-ENOMEM);
	}

	via_fb->gem = gem;

	drm_helper_mode_fill_fb_struct(dev, &via_fb->fb, mode_cmd);
	ret = drm_framebuffer_init(dev, &via_fb->fb, &via_fb_funcs);
	if (ret) {
		drm_gem_object_put_unlocked(via_fb->gem);
		via_fb->gem = NULL;
		kfree(via_fb);
		return ERR_PTR(ret);
	}

	return &via_fb->fb;
}

static const struct drm_mode_config_funcs via_mode_funcs = {
	.fb_create		= via_user_framebuffer_create,
	.output_poll_changed	= via_output_poll_changed
};

int drmfb_helper_pan_display(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *dev = fb_helper->dev;
	struct drm_mode_set *modeset;
	struct drm_crtc *crtc;
	int ret = -ENXIO, i;

	mutex_lock(&dev->mode_config.mutex);
	for (i = 0; i < fb_helper->crtc_count; i++) {
		crtc = fb_helper->crtc_info[i].mode_set.crtc;

		if (!crtc->helper_private->mode_set_base)
			continue;

		modeset = &fb_helper->crtc_info[i].mode_set;
		modeset->x = var->xoffset;
		modeset->y = var->yoffset;

		if (modeset->num_connectors) {
			ret = crtc->helper_private->mode_set_base(
						crtc,
						modeset->x, modeset->y,
						crtc->primary->fb);
			if (!ret) {
				info->flags |= FBINFO_HWACCEL_YPAN;
				info->var.xoffset = var->xoffset;
				info->var.yoffset = var->yoffset;
			}
		}
	}
	if (ret)
		info->flags &= ~FBINFO_HWACCEL_YPAN;
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

static struct fb_ops via_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= drm_fb_helper_check_var,
	.fb_set_par	= drm_fb_helper_set_par,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_pan_display	= drmfb_helper_pan_display,
	.fb_blank	= drm_fb_helper_blank,
	.fb_setcmap	= drm_fb_helper_setcmap,
	.fb_debug_enter	= drm_fb_helper_debug_enter,
	.fb_debug_leave	= drm_fb_helper_debug_leave,
};

static int
via_fb_probe(struct drm_fb_helper *helper,
		struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *dev = helper->dev;
	struct openchrome_drm_private *dev_private =
					helper->dev->dev_private;
	struct via_framebuffer_device *via_fbdev = container_of(helper,
				struct via_framebuffer_device, helper);
	struct via_framebuffer *via_fb = &via_fbdev->via_fb;
	struct drm_framebuffer *fb = &via_fbdev->via_fb.fb;
	struct fb_info *info = helper->fbdev;
	struct openchrome_bo *bo;
	struct drm_mode_fb_cmd2 mode_cmd;
	struct apertures_struct *ap;
	int size, cpp;
	int ret = 0;
	int fake_ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(
						sizes->surface_bpp,
						sizes->surface_depth);
	cpp = drm_format_plane_cpp(mode_cmd.pixel_format, 0);
	mode_cmd.pitches[0] = (mode_cmd.width * cpp);
	mode_cmd.pitches[0] = round_up(mode_cmd.pitches[0], 16);
	size = mode_cmd.pitches[0] * mode_cmd.height;
	size = ALIGN(size, PAGE_SIZE);

	ret = openchrome_bo_create(dev,
					&dev_private->bdev,
					size,
					ttm_bo_type_kernel,
					TTM_PL_FLAG_VRAM,
					&bo);
	if (ret) {
		goto exit;
	}

	ret = ttm_bo_reserve(&bo->ttm_bo, true, false, NULL);
	if (ret) {
		goto out_err;
	}

	ret = openchrome_bo_pin(bo, TTM_PL_FLAG_VRAM);
	if (ret) {
		ttm_bo_unreserve(&bo->ttm_bo);
		goto out_err;
	}

	ret = ttm_bo_kmap(&bo->ttm_bo, 0, bo->ttm_bo.num_pages,
				&bo->kmap);
	ttm_bo_unreserve(&bo->ttm_bo);
	if (ret) {
		goto out_err;
	}

	via_fbdev->bo = bo;

	info = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(info)) {
		ret = PTR_ERR(info);
		goto out_err;
	}

	info->par = via_fbdev;
	info->skip_vt_switch = true;

	drm_helper_mode_fill_fb_struct(dev, fb, &mode_cmd);
	ret = drm_framebuffer_init(helper->dev, fb, &via_fb_funcs);
	if (unlikely(ret)) {
		goto out_err;
	}

	via_fb->gem = &bo->gem;
	via_fbdev->helper.fb = fb;
	via_fbdev->helper.fbdev = info;

	strcpy(info->fix.id, dev->driver->name);
	strcat(info->fix.id, "drmfb");

	info->fbops = &via_fb_ops;

	info->fix.smem_start = bo->kmap.bo->mem.bus.base +
				bo->kmap.bo->mem.bus.offset;
	info->fix.smem_len = size;
	info->screen_base = bo->kmap.virtual;
	info->screen_size = size;

	/* Setup aperture base / size for takeover (i.e., vesafb). */
	ap = alloc_apertures(1);
	if (!ap) {
		drm_framebuffer_cleanup(fb);
		goto out_err;
	}

	ap->ranges[0].size = bo->kmap.bo->bdev->
				man[bo->kmap.bo->mem.mem_type].size;
	ap->ranges[0].base = bo->kmap.bo->mem.bus.base;
	info->apertures = ap;

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->format->depth);
	drm_fb_helper_fill_var(info, helper,
				sizes->fb_width, sizes->fb_height);
	goto exit;
out_err:
	if (bo->kmap.bo) {
		fake_ret = ttm_bo_reserve(&bo->ttm_bo, true, false, NULL);
		if (fake_ret) {
			goto exit;
		}

		ttm_bo_kunmap(&bo->kmap);

		fake_ret = openchrome_bo_unpin(bo);
		if (fake_ret) {
			ttm_bo_unreserve(&bo->ttm_bo);
			goto exit;
		}

		ttm_bo_put(&bo->ttm_bo);
	}

	if (via_fb->gem) {
		drm_gem_object_put_unlocked(via_fb->gem);
		via_fb->gem = NULL;
	}
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static struct drm_fb_helper_funcs via_drm_fb_helper_funcs = {
	.fb_probe = via_fb_probe,
};

int via_fbdev_init(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct via_framebuffer_device *via_fbdev;
	int bpp_sel = 32;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	dev->mode_config.funcs = &via_mode_funcs;

	via_fbdev = kzalloc(sizeof(struct via_framebuffer_device),
				GFP_KERNEL);
	if (!via_fbdev) {
		ret = -ENOMEM;
		goto exit;
	}

	dev_private->via_fbdev = via_fbdev;

	drm_fb_helper_prepare(dev, &via_fbdev->helper,
				&via_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, &via_fbdev->helper,
				dev->mode_config.num_connector);
	if (ret) {
		goto free_fbdev;
	}

	ret = drm_fb_helper_single_add_all_connectors(&via_fbdev->helper);
	if (ret) {
		goto free_fb_helper;
	}

	drm_helper_disable_unused_functions(dev);
	ret = drm_fb_helper_initial_config(&via_fbdev->helper, bpp_sel);
	if (ret) {
		goto free_fb_helper;
	}

	goto exit;
free_fb_helper:
	drm_fb_helper_fini(&via_fbdev->helper);
free_fbdev:
	kfree(via_fbdev);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void via_fbdev_fini(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct drm_fb_helper *fb_helper = &dev_private->via_fbdev->helper;
	struct via_framebuffer *via_fb = &dev_private->via_fbdev->via_fb;
	struct fb_info *info;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (!fb_helper) {
		goto exit;
	}

	info = fb_helper->fbdev;
	if (info) {
		unregister_framebuffer(info);
		kfree(info->apertures);
		framebuffer_release(info);
		fb_helper->fbdev = NULL;
	}

	if (via_fb->gem) {
		drm_gem_object_put_unlocked(via_fb->gem);
		via_fb->gem = NULL;
	}

	drm_fb_helper_fini(&dev_private->via_fbdev->helper);
	drm_framebuffer_cleanup(&dev_private->via_fbdev->via_fb.fb);
	if (dev_private->via_fbdev) {
		kfree(dev_private->via_fbdev);
		dev_private->via_fbdev = NULL;
	}
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}
