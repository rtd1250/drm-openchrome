/*
 * Copyright © 2011 James Simmons 
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
 *	James Simmons <jsimmons@infradead.org>
 */

#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"

#include "via_drv.h"

static void
disableInternalLvds(struct via_encoder *enc)
{
	struct drm_device *dev = enc->base.dev;
	struct drm_via_private *dev_priv = dev->dev_private;
	u8 orig;

	/* 1. Turn off LCD panel. VX800/CX700 is handled differently */
	if (dev->pdev->device == PCI_DEVICE_ID_VIA_VT1122 ||
	    dev->pdev->device == PCI_DEVICE_ID_VIA_VT3157) {
		unsigned char reg = (enc->diPort == VIA_DI_DFPHIGH) ? 0xD3 : 0x91;

		/* Back-Light OFF */
		orig = (vga_rcrt(VGABASE, reg) & ~BIT(1));
		vga_wcrt(VGABASE, reg, orig);

		msleep(250);

		/* VEE OFF (unused on vt3353)*/
		orig = (vga_rcrt(VGABASE, reg) & ~BIT(2));
		vga_wcrt(VGABASE, reg, orig);

		/* DATA OFF */
		orig = (vga_rcrt(VGABASE, reg) & ~BIT(3));
		vga_wcrt(VGABASE, reg, orig);

		msleep(25);

		/* VDD OFF */
		orig = (vga_rcrt(VGABASE, reg) & ~BIT(4));
		vga_wcrt(VGABASE, reg, orig);
	} else {
		if (enc->diPort == VIA_DI_DFPHIGH) {
			/* Use second power sequence control: */
			/* Turn off power sequence. */
			orig = (vga_rcrt(VGABASE, 0xD4) & ~BIT(1));
			vga_wcrt(VGABASE, 0xD4, orig);

			/* Turn off back light and panel path. */
			orig = (vga_rcrt(VGABASE, 0xD3) & ~(BIT(6) | BIT(7)));
			vga_wcrt(VGABASE, 0xD3, (orig | 0xC0));
		} else {
			/* Use first power sequence control: */
			/* Turn off power sequence. */
			orig = (vga_rcrt(VGABASE, 0x6A) & ~BIT(3));
			vga_wcrt(VGABASE, 0x6A, orig);

			/* Turn off back light and panel path. */
			orig = (vga_rcrt(VGABASE, 0x91) & ~(BIT(6) | BIT(7)));
			vga_wcrt(VGABASE, 0x91, (orig | 0xC0));
		}
	}
}

static void
enableInternalLvds(struct via_encoder *enc)
{
	struct drm_device *dev = enc->base.dev;
	struct drm_via_private *dev_priv = dev->dev_private;
	unsigned char reg = (enc->diPort == VIA_DI_DFPHIGH) ? 0xD3 : 0x91;
	u8 orig;

	/* 1. Turn off LCD panel. VX800/CX700 is handled differently */
	if (dev->pdev->device == PCI_DEVICE_ID_VIA_VT1122 ||
	    dev->pdev->device == PCI_DEVICE_ID_VIA_VT3157) {
		/* Secondary power hardware power sequence enable */
		if (enc->diPort == VIA_DI_DFPHIGH) {
			orig = (vga_rcrt(VGABASE, reg+1) & ~BIT(1));
			vga_wcrt(VGABASE, reg+1, orig);
		} else {
			orig = (vga_rcrt(VGABASE, reg) & ~BIT(7));
			vga_wcrt(VGABASE, reg, orig);
		}

		/* Software control power sequence ON*/		
		orig = (vga_rcrt(VGABASE, reg) & BIT(1));
		vga_wcrt(VGABASE, reg, orig);

		msleep(200);

		/* VDD ON */
		orig = (vga_rcrt(VGABASE, reg) & BIT(4));
		vga_wcrt(VGABASE, reg, orig);

		msleep(25);

		/* DATA ON */
		orig = (vga_rcrt(VGABASE, reg) & BIT(3));
		vga_wcrt(VGABASE, reg, orig);

		/* VEE ON (unused on vt3353)*/
		orig = (vga_rcrt(VGABASE, reg) & BIT(2));
		vga_wcrt(VGABASE, reg, orig);

		msleep(250);

		/* Back-Light ON */
		orig = (vga_rcrt(VGABASE, reg) & BIT(1));
		vga_wcrt(VGABASE, reg, orig);
	} else {
		/* Use hardware control power sequence. */
		orig = (vga_rcrt(VGABASE, reg) & ~BIT(0));
		vga_wcrt(VGABASE, reg, orig);

		/* Turn on back light and panel path. */
		orig = (vga_rcrt(VGABASE, reg) & ~(BIT(6) | BIT(7)));
		vga_wcrt(VGABASE, reg, orig);

		/* Turn on hardware power sequence. */
		if (enc->diPort == VIA_DI_DFPHIGH) {
			/* Turn on hardware power sequence. */
			orig = (vga_rcrt(VGABASE, 0xD4) & BIT(1));
			vga_wcrt(VGABASE, 0xD4, orig);
		} else {
			/* Turn on hardware power sequence. */
			orig = (vga_rcrt(VGABASE, 0x6A) & BIT(3));
			vga_wcrt(VGABASE, 0x6A, orig);
		}
	}
}

/*
 * Routines for controlling stuff on the lvds port
 */
static const struct drm_encoder_funcs via_lvds_enc_funcs = {
	.destroy = drm_encoder_cleanup,
};

/* Manage the power state of the LVDS */
static void
via_lvds_dpms(struct drm_encoder *encoder, int mode)
{
	struct via_encoder *enc = container_of(encoder, struct via_encoder, base);
	struct drm_via_private *dev_priv = encoder->dev->dev_private;
	struct drm_crtc *crtc = encoder->crtc;

	/* Not attached to any crtc */
	if (!crtc)
		return;

/*	switch (mode) {
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_OFF:
		disableInternalLvds(enc);
		viaDIPortPadOn(dev_priv, enc->diPort, false);
		break;

	case DRM_MODE_DPMS_ON:
	default:
		enableInternalLvds(enc);
		viaDIPortPadOn(dev_priv, enc->diPort, true);
		break;
	}*/
}

/* Pass our mode to the connectors and the CRTC to give them a chance to
 * adjust it according to limitations or connector properties, and also
 * a chance to reject the mode entirely. Usefule for things like scaling.
 */
static bool
via_lvds_mode_fixup(struct drm_encoder *encoder,
		 struct drm_display_mode *mode,
		 struct drm_display_mode *adjusted_mode)
{
	drm_mode_set_crtcinfo(adjusted_mode, 0);
	return true;
}

static void
via_lvds_prepare(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
	struct drm_crtc *crtc = encoder->crtc;

	/* Not attached to any crtc */
	if (!crtc)
		return;

	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_ON);
}

static void
via_lvds_commit(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
	struct drm_crtc *crtc = encoder->crtc;

	/* Not attached to any crtc */
	if (!crtc)
		return;

	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_ON);
}

static void
via_lvds_mode_set(struct drm_encoder *encoder,
		       struct drm_display_mode *mode,
		       struct drm_display_mode *adjusted_mode)
{
}

static const struct drm_encoder_helper_funcs via_lvds_enc_helper_funcs = {
	.dpms = via_lvds_dpms,
	.mode_fixup = via_lvds_mode_fixup,
	.prepare = via_lvds_prepare,
	.commit = via_lvds_commit,
	.mode_set = via_lvds_mode_set,
};

static enum drm_connector_status
via_lvds_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
	//return connector_status_unknown;
}

static void
via_lvds_destroy(struct drm_connector *connector)
{
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

static int
via_lvds_set_property(struct drm_connector *connector,
		  struct drm_property *property,
		  uint64_t value)
{
	struct drm_device *dev = connector->dev;

	if (property == dev->mode_config.dpms_property && connector->encoder)
		via_lvds_dpms(connector->encoder, (uint32_t)(value & 0xf));
	return 0;
}

static const struct drm_connector_funcs via_lvds_connector_funcs = {
	.detect = via_lvds_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = via_lvds_destroy,
	.set_property = via_lvds_set_property,
};

/* This function test if the drm_display_modes generated by a display
 * monitor that is connected is supported by the graphics card.
 */
static int
via_lvds_mode_valid(struct drm_connector *connector,
			struct drm_display_mode *mode)
{
	if ((mode->flags & DRM_MODE_FLAG_INTERLACE) &&
	    !connector->interlace_allowed)
		return MODE_NO_INTERLACE;

	if ((mode->flags & DRM_MODE_FLAG_DBLSCAN) &&
	    !connector->doublescan_allowed)
		return MODE_NO_DBLESCAN;

	return MODE_OK;
}

static int
via_lvds_get_modes(struct drm_connector *connector)
{
	int count = 0; //via_get_edid_modes(connector);

	/* If no edid modes then just get the built in one */
	if (!count)
		drm_mode_connector_list_update(connector);
	return (count ? count : 1);
}

static const struct drm_connector_helper_funcs via_lvds_connector_helper_funcs = {
	.mode_valid = via_lvds_mode_valid,
	.get_modes = via_lvds_get_modes,
	.best_encoder = via_best_encoder,
};

void via_lvds_init(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	struct via_i2c *i2c = &dev_priv->i2c_par[0];
	struct i2c_adapter *adapter = NULL;
	struct drm_connector *connector;
	struct edid *edid = NULL;
	struct via_encoder *enc;
	void *par;
	int size;

	size = sizeof(*enc) + sizeof(struct drm_connector);
	par = kzalloc(size, GFP_KERNEL);
	if (!par) {
		DRM_ERROR("Failed to allocate connector and encoder\n");
		return;
	}
	connector = par + sizeof(*enc);
	enc = par;

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_VT1122:
	case PCI_DEVICE_ID_VIA_VT3157:
		enc->diPort = VIA_DI_DFPHIGH;
		break;
	default:
		enc->diPort = VIA_DI_NONE;
		break;
	}

	/* Piece together our connector */
	drm_connector_init(dev, connector, &via_lvds_connector_funcs,
				DRM_MODE_CONNECTOR_LVDS);
	drm_connector_helper_add(connector, &via_lvds_connector_helper_funcs);

	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;

	/* Setup the encoders and attach them */
	drm_encoder_init(dev, &enc->base, &via_lvds_enc_funcs, DRM_MODE_ENCODER_LVDS);
	drm_encoder_helper_add(&enc->base, &via_lvds_enc_helper_funcs);
	enc->base.possible_clones = 0;
	enc->base.possible_crtcs = BIT(1);

	/* We need an connector/encoder to exist to test if the hardware is 
	 * present. If we don't detect real device then we remove the 
	 * connector and encoder. LVDS is usually on the second i2c bus *
	adapter = &i2c->adapter;
	if (adapter) {
		edid = drm_get_edid(connector, adapter);
		drm_mode_connector_update_edid_property(connector, edid);

		* If no edid then we detect the mode using the scratch pad
		 * registers. */
		if (!edid) {
			struct drm_display_mode *fixed_mode = NULL;
			u8 index = (vga_rcrt(VGABASE, 0x3F) & 0xF);

			fixed_mode = drm_mode_create(dev);
			if (!fixed_mode)
				goto no_device;

			switch (index) {
			case 0x0A:
				fixed_mode->clock = 85860;
        			fixed_mode->hdisplay = 1366;
				fixed_mode->hsync_start = 1440;
				fixed_mode->hsync_end = 1584;
				fixed_mode->htotal = 1800;
				fixed_mode->hskew = 0;
				fixed_mode->vdisplay = 768;
				fixed_mode->vsync_start = 769;
				fixed_mode->vsync_end = 772;
				fixed_mode->vtotal = 795;
				fixed_mode->vscan = 0;
				fixed_mode->vrefresh = 60;
				fixed_mode->hsync = 48;
				break;

			case 0x0B:
				fixed_mode->clock = 56519;
        			fixed_mode->hdisplay = 1200;
				fixed_mode->hsync_start = 1211;
				fixed_mode->hsync_end = 1243;
				fixed_mode->htotal = 1264;
				fixed_mode->hskew = 0;
				fixed_mode->vdisplay = 900;
				fixed_mode->vsync_start = 901;
				fixed_mode->vsync_end = 911;
				fixed_mode->vtotal = 912;
				fixed_mode->vscan = 0;
				fixed_mode->vrefresh = 50;
				fixed_mode->hsync = 0;
				//49 56519 1200 1211 1243 1264 900 901 911 912 0x0
				break;
			default:
				drm_mode_destroy(dev, fixed_mode);
				goto no_device;
				break;
			}

			drm_mode_set_name(fixed_mode);
			drm_mode_probed_add(connector, fixed_mode);
		} else
			kfree(edid);
	//}

	drm_mode_connector_attach_encoder(connector, &enc->base);
	drm_sysfs_connector_add(connector);
	return;

no_device:
	drm_connector_cleanup(connector);
	drm_encoder_cleanup(&enc->base);
	kfree(par);
}
