/*
 * Copyright (C) 2016-2018 Kevin Brace. All Rights Reserved.
 * Copyright © 2013 James Simmons
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *  Kevin Brace <kevinbrace@gmx.com>
 *	James Simmons <jsimmons@infradead.org>
 */

#include <linux/pci.h>

#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_probe_helper.h>

#include "openchrome_drv.h"


static void via_tmds_power(struct openchrome_drm_private *dev_private,
				bool power_state)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (power_state) {
		via_lvds1_set_soft_display_period(VGABASE, true);
		via_lvds1_set_soft_data(VGABASE, true);
		via_tmds_set_power(VGABASE, true);
	} else {
		via_tmds_set_power(VGABASE, false);
		via_lvds1_set_soft_data(VGABASE, false);
		via_lvds1_set_soft_display_period(VGABASE, false);
	}

	DRM_INFO("DVI Power: %s\n",
			power_state ? "On" : "Off");

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void via_tmds_io_pad_setting(
			struct openchrome_drm_private *dev_private,
			u32 di_port, bool io_pad_on)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	switch(di_port) {
	case VIA_DI_PORT_TMDS:
		via_lvds1_set_io_pad_setting(VGABASE,
				io_pad_on ? 0x03 : 0x00);
		break;
	default:
		break;
	}

	DRM_DEBUG_KMS("DVI I/O Pad: %s\n", io_pad_on ? "On": "Off");

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

/*
 * Initializes most registers related to VIA Technologies IGP
 * integrated TMDS transmitter. Synchronization polarity and
 * display output source need to be set separately.
 */
static void via_tmds_init_reg(
			struct openchrome_drm_private *dev_private)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Turn off hardware controlled FP power on / off circuit. */
	via_fp_set_primary_hard_power(VGABASE, false);

	/* Use software FP power sequence control. */
	via_fp_set_primary_power_seq_type(VGABASE, false);

	/* Turn off software controlled primary FP power rails. */
	via_fp_set_primary_soft_vdd(VGABASE, false);
	via_fp_set_primary_soft_vee(VGABASE, false);

	/* Turn off software controlled primary FP back light
	* control. */
	via_fp_set_primary_soft_back_light(VGABASE, false);

	/* Turn off direct control of FP back light. */
	via_fp_set_primary_direct_back_light_ctrl(VGABASE, false);

	/* Activate DVI + LVDS2 mode. */
	/* 3X5.D2[5:4] - Display Channel Select
	 *               00: LVDS1 + LVDS2
	 *               01: DVI + LVDS2
	 *               10: One Dual LVDS Channel (High Resolution Pannel)
	 *               11: Single Channel DVI */
	svga_wcrt_mask(VGABASE, 0xd2, 0x10, 0x30);

	/* Various DVI PLL settings should be set to default settings. */
	/* 3X5.D1[7]   - PLL2 Reference Clock Edge Select Bit
	 *               0: PLLCK lock to rising edge of reference clock
	 *               1: PLLCK lock to falling edge of reference clock
	 * 3X5.D1[6:5] - PLL2 Charge Pump Current Set Bits
	 *               00: ICH = 12.5 uA
	 *               01: ICH = 25.0 uA
	 *               10: ICH = 37.5 uA
	 *               11: ICH = 50.0 uA
	 * 3X5.D1[4:1] - Reserved
	 * 3X5.D1[0]   - PLL2 Control Voltage Measurement Enable Bit */
	svga_wcrt_mask(VGABASE, 0xd1, 0x00, 0xe1);

	/* Disable DVI test mode. */
	/* 3X5.D5[7] - PD1 Enable Selection
	 *             1: Select by power flag
	 *             0: By register
	 * 3X5.D5[5] - DVI Testing Mode Enable
	 * 3X5.D5[4] - DVI Testing Format Selection
	 *             0: Half cycle
	 *             1: LFSR mode */
	svga_wcrt_mask(VGABASE, 0xd5, 0x00, 0xb0);

	/* Disable DVI sense interrupt. */
	/* 3C5.2B[7] - DVI Sense Interrupt Enable
	 *             0: Disable
	 *             1: Enable */
	svga_wseq_mask(VGABASE, 0x2b, 0x00, 0x80);

	/* Clear DVI sense interrupt status. */
	/* 3C5.2B[6] - DVI Sense Interrupt Status
	 *             (This bit has a RW1C attribute.) */
	svga_wseq_mask(VGABASE, 0x2b, 0x40, 0x40);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

/*
 * Set TMDS (DVI) sync polarity.
 */
static void via_tmds_sync_polarity(
			struct openchrome_drm_private *dev_private,
			unsigned int flags)
{
	u8 syncPolarity = 0x00;

	DRM_DEBUG_KMS("Entered via_tmds_sync_polarity.\n");

	if (flags & DRM_MODE_FLAG_NHSYNC) {
		syncPolarity |= BIT(0);
	}

	if (flags & DRM_MODE_FLAG_NVSYNC) {
		syncPolarity |= BIT(1);
	}

	via_tmds_set_sync_polarity(VGABASE, syncPolarity);
	DRM_INFO("TMDS (DVI) Horizontal Sync Polarity: %s\n",
		(syncPolarity & BIT(0)) ? "-" : "+");
	DRM_INFO("TMDS (DVI) Vertical Sync Polarity: %s\n",
		(syncPolarity & BIT(1)) ? "-" : "+");

	DRM_DEBUG_KMS("Exiting via_tmds_sync_polarity.\n");
}

/*
 * Sets TMDS (DVI) display source.
 */
static void via_tmds_display_source(
			struct openchrome_drm_private *dev_private,
			int index)
{
	u8 displaySource = index;

	DRM_DEBUG_KMS("Entered via_tmds_display_source.\n");

	via_tmds_set_display_source(VGABASE, displaySource & 0x01);
	DRM_INFO("TMDS (DVI) Display Source: IGA%d\n",
			(displaySource & 0x01) + 1);

	DRM_DEBUG_KMS("Exiting via_tmds_display_source.\n");
}

/*
 * Routines for controlling stuff on the TMDS port
 */
static const struct drm_encoder_funcs via_tmds_enc_funcs = {
	.destroy = via_encoder_cleanup,
};

static void via_tmds_dpms(struct drm_encoder *encoder, int mode)
{
	struct via_encoder *enc = container_of(encoder,
					struct via_encoder, base);
	struct openchrome_drm_private *dev_private =
					encoder->dev->dev_private;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		via_tmds_power(dev_private, true);
		via_tmds_io_pad_setting(dev_private,
						enc->di_port, true);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		via_tmds_power(dev_private, false);
		via_tmds_io_pad_setting(dev_private,
						enc->di_port, false);
		break;
	default:
		DRM_ERROR("Bad DPMS mode.");
		break;
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

/* Pass our mode to the connectors and the CRTC to give them a chance to
 * adjust it according to limitations or connector properties, and also
 * a chance to reject the mode entirely. Usefule for things like scaling.
 */
static bool
via_tmds_mode_fixup(struct drm_encoder *encoder,
		 const struct drm_display_mode *mode,
		 struct drm_display_mode *adjusted_mode)
{
	drm_mode_set_crtcinfo(adjusted_mode, 0);
	return true;
}

static void via_tmds_prepare(struct drm_encoder *encoder)
{
	struct via_encoder *enc = container_of(encoder,
					struct via_encoder, base);
	struct openchrome_drm_private *dev_private =
					encoder->dev->dev_private;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	via_tmds_power(dev_private, false);
	via_tmds_io_pad_setting(dev_private, enc->di_port, false);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void via_tmds_commit(struct drm_encoder *encoder)
{
	struct via_encoder *enc = container_of(encoder,
					struct via_encoder, base);
	struct openchrome_drm_private *dev_private =
					encoder->dev->dev_private;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	via_tmds_power(dev_private, true);
	via_tmds_io_pad_setting(dev_private, enc->di_port, true);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

/*
 * Handle CX700 / VX700 and VX800 integrated TMDS (DVI) mode setting.
 */
static void
via_tmds_mode_set(struct drm_encoder *encoder,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode)
{
	struct openchrome_drm_private *dev_private =
					encoder->dev->dev_private;
	struct via_crtc *iga = container_of(encoder->crtc,
						struct via_crtc, base);

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	via_tmds_init_reg(dev_private);
	via_tmds_sync_polarity(dev_private, adjusted_mode->flags);
	via_tmds_display_source(dev_private, iga->index);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void via_tmds_disable(struct drm_encoder *encoder)
{
	struct via_encoder *enc = container_of(encoder,
					struct via_encoder, base);
	struct openchrome_drm_private *dev_private =
					encoder->dev->dev_private;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	via_tmds_power(dev_private, false);
	via_tmds_io_pad_setting(dev_private, enc->di_port, false);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static const struct drm_encoder_helper_funcs
			via_tmds_enc_helper_funcs = {
	.dpms = via_tmds_dpms,
	.mode_fixup = via_tmds_mode_fixup,
	.prepare = via_tmds_prepare,
	.commit = via_tmds_commit,
	.mode_set = via_tmds_mode_set,
	.disable = via_tmds_disable,
};

static enum drm_connector_status
via_tmds_detect(struct drm_connector *connector, bool force)
{
	struct via_connector *con = container_of(connector, struct via_connector, base);
	enum drm_connector_status ret = connector_status_disconnected;
	struct i2c_adapter *i2c_bus;
	struct edid *edid = NULL;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (con->i2c_bus & VIA_I2C_BUS2) {
		i2c_bus = via_find_ddc_bus(0x31);
	} else if (con->i2c_bus & VIA_I2C_BUS3) {
		i2c_bus = via_find_ddc_bus(0x2c);
	} else {
		i2c_bus = NULL;
	}

	if (i2c_bus) {
		edid = drm_get_edid(&con->base, i2c_bus);
		if (edid) {
			if (edid->input & DRM_EDID_INPUT_DIGITAL) {
				drm_connector_update_edid_property(connector, edid);
				ret = connector_status_connected;
			}

			kfree(edid);
		}
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static const struct drm_connector_funcs via_dvi_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = via_tmds_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = via_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state =
			drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state =
			drm_atomic_helper_connector_destroy_state,
};

static int via_tmds_get_modes(struct drm_connector *connector)
{
	struct via_connector *con = container_of(connector, struct via_connector, base);
	struct i2c_adapter *i2c_bus;
	struct edid *edid = NULL;
	int count = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (con->i2c_bus & VIA_I2C_BUS2) {
		i2c_bus = via_find_ddc_bus(0x31);
	} else if (con->i2c_bus & VIA_I2C_BUS3) {
		i2c_bus = via_find_ddc_bus(0x2c);
	} else {
		i2c_bus = NULL;
	}

	if (i2c_bus) {
		edid = drm_get_edid(&con->base, i2c_bus);
		if (edid->input & DRM_EDID_INPUT_DIGITAL) {
			drm_connector_update_edid_property(connector,
								edid);
			count = drm_add_edid_modes(connector, edid);
			DRM_DEBUG_KMS("DVI EDID information was obtained.\n");
		}

		kfree(edid);
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return count;
}

static const struct drm_connector_helper_funcs via_dvi_connector_helper_funcs = {
	.mode_valid = via_connector_mode_valid,
	.get_modes = via_tmds_get_modes,
};

/*
 * Probe (pre-initialization detection) of integrated TMDS transmitters.
 */
void via_tmds_probe(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	u16 chipset = pdev->device;
	u8 sr13, sr5a;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Detect the presence of integrated TMDS transmitter. */
	switch (chipset) {
	case PCI_DEVICE_ID_VIA_VT3157:
	case PCI_DEVICE_ID_VIA_VT1122:
		sr5a = vga_rseq(VGABASE, 0x5a);

		/* Setting SR5A[0] to 1.
		 * This allows the reading out the alternative
		 * pin strapping information from SR12 and SR13. */
		svga_wseq_mask(VGABASE, 0x5a, BIT(0), BIT(0));

		sr13 = vga_rseq(VGABASE, 0x13);
		DRM_DEBUG_KMS("sr13: 0x%02x\n", sr13);

		vga_wseq(VGABASE, 0x5a, sr5a);

		/* 3C5.13[7:6] - Integrated LVDS / DVI Mode Select
		 *               (DVP1D15-14 pin strapping)
		 *               00: LVDS1 + LVDS2
		 *               01: DVI + LVDS2
		 *               10: Dual LVDS Channel (High Resolution Panel)
		 *               11: One DVI only (decrease the clock jitter) */
		/* Check for DVI presence using pin strappings.
		 * VIA Technologies NanoBook reference design based products
		 * have their pin strappings set to a wrong setting to communicate
		 * the presence of DVI, so it requires special handling here. */
		if (dev_private->is_via_nanobook) {
			dev_private->int_tmds_presence = true;
			dev_private->int_tmds_di_port =
						VIA_DI_PORT_TMDS;
			dev_private->int_tmds_i2c_bus = VIA_I2C_BUS2;
			dev_private->mapped_i2c_bus |= VIA_I2C_BUS2;
			DRM_DEBUG_KMS("Integrated TMDS (DVI) "
					"transmitter detected.\n");
		} else if (((!(sr13 & BIT(7))) && (sr13 & BIT(6))) ||
				((sr13 & BIT(7)) && (sr13 & BIT(6)))) {
			dev_private->int_tmds_presence = true;
			dev_private->int_tmds_di_port =
						VIA_DI_PORT_TMDS;
			dev_private->int_tmds_i2c_bus = VIA_I2C_BUS2;
			dev_private->mapped_i2c_bus |= VIA_I2C_BUS2;
			DRM_DEBUG_KMS("Integrated TMDS (DVI) "
					"transmitter detected via pin "
					"strapping.\n");
		} else {
			dev_private->int_tmds_presence = false;
			dev_private->int_tmds_di_port =
						VIA_DI_PORT_NONE;
			dev_private->int_tmds_i2c_bus = VIA_I2C_NONE;
		}

		break;
	default:
		dev_private->int_tmds_presence = false;
		dev_private->int_tmds_di_port = VIA_DI_PORT_NONE;
		dev_private->int_tmds_i2c_bus = VIA_I2C_NONE;
		break;
	}

	DRM_DEBUG_KMS("int_tmds_presence: %x\n",
			dev_private->int_tmds_presence);
	DRM_DEBUG_KMS("int_tmds_di_port: 0x%08x\n",
			dev_private->int_tmds_di_port);
	DRM_DEBUG_KMS("int_tmds_i2c_bus: 0x%08x\n",
			dev_private->int_tmds_i2c_bus);
	DRM_DEBUG_KMS("mapped_i2c_bus: 0x%08x\n",
			dev_private->mapped_i2c_bus);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

void via_tmds_init(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct via_connector *con;
	struct via_encoder *enc;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (!dev_private->int_tmds_presence) {
		goto exit;
	}

	enc = kzalloc(sizeof(*enc) + sizeof(*con), GFP_KERNEL);
	if (!enc) {
		DRM_ERROR("Failed to allocate connector "
				"and encoder.\n");
		goto exit;
	}

	/* Setup the encoders and attach them */
	drm_encoder_init(dev, &enc->base, &via_tmds_enc_funcs,
				DRM_MODE_ENCODER_TMDS, NULL);
	drm_encoder_helper_add(&enc->base, &via_tmds_enc_helper_funcs);

	enc->base.possible_crtcs = BIT(1) | BIT(0);
	enc->base.possible_clones = 0;

	enc->di_port = dev_private->int_tmds_di_port;

	/* Increment the number of DVI connectors. */
	dev_private->number_dvi++;


	con = &enc->cons[0];
	drm_connector_init(dev, &con->base, &via_dvi_connector_funcs,
				DRM_MODE_CONNECTOR_DVID);
	drm_connector_helper_add(&con->base, &via_dvi_connector_helper_funcs);
	drm_connector_register(&con->base);

	con->i2c_bus = dev_private->int_tmds_i2c_bus;
	con->base.doublescan_allowed = false;
	con->base.interlace_allowed = true;
	INIT_LIST_HEAD(&con->props);

	drm_connector_attach_encoder(&con->base, &enc->base);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

/*
 * Probe (pre-initialization detection) of external DVI transmitters.
 */
void openchrome_ext_dvi_probe(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct i2c_adapter *i2c_bus;
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	u16 chipset = pdev->device;
	u8 sr12, sr13;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	dev_private->ext_tmds_presence = false;
	dev_private->ext_tmds_i2c_bus = VIA_I2C_NONE;
	dev_private->ext_tmds_transmitter = VIA_TMDS_NONE;

	if ((!dev_private->ext_tmds_presence) &&
		(!(dev_private->mapped_i2c_bus & VIA_I2C_BUS2))) {
		i2c_bus = via_find_ddc_bus(0x31);
		if (openchrome_vt1632_probe(i2c_bus)) {
			dev_private->ext_tmds_presence = true;
			dev_private->ext_tmds_i2c_bus = VIA_I2C_BUS2;
			dev_private->ext_tmds_transmitter =
						VIA_TMDS_VT1632;
			dev_private->mapped_i2c_bus |= VIA_I2C_BUS2;
		} else if (openchrome_sii164_probe(i2c_bus)) {
			dev_private->ext_tmds_presence = true;
			dev_private->ext_tmds_i2c_bus = VIA_I2C_BUS2;
			dev_private->ext_tmds_transmitter =
						VIA_TMDS_SII164;
			dev_private->mapped_i2c_bus |= VIA_I2C_BUS2;
		}
	}

	if ((!(dev_private->ext_tmds_presence)) &&
		(!(dev_private->mapped_i2c_bus & VIA_I2C_BUS4))) {
		i2c_bus = via_find_ddc_bus(0x2c);
		if (openchrome_vt1632_probe(i2c_bus)) {
			dev_private->ext_tmds_presence = true;
			dev_private->ext_tmds_i2c_bus = VIA_I2C_BUS4;
			dev_private->ext_tmds_transmitter =
						VIA_TMDS_VT1632;
			dev_private->mapped_i2c_bus |= VIA_I2C_BUS4;
		} else if (openchrome_sii164_probe(i2c_bus)) {
			dev_private->ext_tmds_presence = true;
			dev_private->ext_tmds_i2c_bus = VIA_I2C_BUS4;
			dev_private->ext_tmds_transmitter =
						VIA_TMDS_SII164;
			dev_private->mapped_i2c_bus |= VIA_I2C_BUS4;
		}
	}

	sr12 = vga_rseq(VGABASE, 0x12);
	sr13 = vga_rseq(VGABASE, 0x13);
	DRM_DEBUG_KMS("SR12: 0x%02x\n", sr12);
	DRM_DEBUG_KMS("SR13: 0x%02x\n", sr13);

	if (dev_private->ext_tmds_presence) {
		switch (chipset) {
		case PCI_DEVICE_ID_VIA_CLE266:

			/* 3C5.12[4] - FPD17 pin strapping
			 *             0: TMDS transmitter (DVI) /
			 *                capture device
			 *             1: Flat panel */
			if (!(sr12 & BIT(4))) {
				dev_private->ext_tmds_di_port =
						VIA_DI_PORT_DIP0;

			/* 3C5.12[5] - FPD18 pin strapping
			 *             0: TMDS transmitter (DVI)
			 *             1: TV encoder */
			} else if (!(sr12 & BIT(5))) {
				dev_private->ext_tmds_di_port =
						VIA_DI_PORT_DIP1;
			} else {
				dev_private->ext_tmds_di_port =
						VIA_DI_PORT_NONE;
			}

			break;
		case PCI_DEVICE_ID_VIA_KM400:
		case PCI_DEVICE_ID_VIA_K8M800:
		case PCI_DEVICE_ID_VIA_CN700:
		case PCI_DEVICE_ID_VIA_PM800:
			/* 3C5.12[6] - DVP0D6 pin strapping
			 *             0: Disable DVP0 (Digital Video Port 0) for
			 *                DVI or TV out use
			 *             1: Enable DVP0 (Digital Video Port 0) for
			 *                DVI or TV out use
			 * 3C5.12[5] - DVP0D5 pin strapping
			 *             0: TMDS transmitter (DVI)
			 *             1: TV encoder */
			if ((sr12 & BIT(6)) && (!(sr12 & BIT(5)))) {
				dev_private->ext_tmds_di_port =
						VIA_DI_PORT_DVP0;
			} else {
				dev_private->ext_tmds_di_port =
						VIA_DI_PORT_DVP1;
			}

			break;
		case PCI_DEVICE_ID_VIA_VT3343:
		case PCI_DEVICE_ID_VIA_K8M890:
		case PCI_DEVICE_ID_VIA_P4M900:
			/* Assume DVP2 as DVP0. Hence, VIA_DI_PORT_DVP0
			 * is used. */
			/* 3C5.12[6] - DVP2D6 pin strapping
			 *             0: Disable DVP2 (Digital Video Port 2)
			 *             1: Enable DVP2 (Digital Video Port 2)
			 * 3C5.12[5] - DVP2D5 pin strapping
			 *             0: TMDS transmitter (DVI)
			 *             1: TV encoder */
			if ((sr12 & BIT(6)) && (!(sr12 & BIT(5)))) {
				dev_private->ext_tmds_di_port =
						VIA_DI_PORT_DVP0;
			} else {
				dev_private->ext_tmds_di_port =
						VIA_DI_PORT_NONE;
			}

			break;
		case PCI_DEVICE_ID_VIA_VT3157:
		case PCI_DEVICE_ID_VIA_VT1122:
		case PCI_DEVICE_ID_VIA_VX875:
		case PCI_DEVICE_ID_VIA_VX900_VGA:
			dev_private->ext_tmds_di_port =
						VIA_DI_PORT_DVP1;
			break;
		default:
			dev_private->ext_tmds_di_port =
						VIA_DI_PORT_NONE;
			break;
		}
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

void openchrome_ext_dvi_init(struct drm_device *dev)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	openchrome_vt1632_init(dev);
	openchrome_sii164_init(dev);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}
