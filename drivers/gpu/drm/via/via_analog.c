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

/*
 * Routines for controlling stuff on the analog port
 */
static const struct drm_encoder_funcs via_analog_enc_funcs = {
	.destroy = drm_encoder_cleanup,
};

/* Manage the power state of the DAC */
static void
via_analog_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_via_private *dev_priv = encoder->dev->dev_private;
	u8 mask = BIT(5) + BIT(4), orig;

	orig = (vga_rcrt(VGABASE, 0x36) & ~mask);
	switch (mode) {
	case DRM_MODE_DPMS_SUSPEND:
		orig |= BIT(5);		// VSync off
		break;
	case DRM_MODE_DPMS_STANDBY:
		orig |= BIT(4);		// HSync off
		break;
	case DRM_MODE_DPMS_OFF:
		orig |= mask;		// HSync and VSync off
		break;
	case DRM_MODE_DPMS_ON:
	default:
		break;
	}
	vga_wcrt(VGABASE, 0x36, orig);
}

/* Pass our mode to the connectors and the CRTC to give them a chance to
 * adjust it according to limitations or connector properties, and also
 * a chance to reject the mode entirely. Usefule for things like scaling.
 */
static bool
via_analog_mode_fixup(struct drm_encoder *encoder,
		 struct drm_display_mode *mode,
		 struct drm_display_mode *adjusted_mode)
{
	drm_mode_set_crtcinfo(adjusted_mode, 0);
	return true;
}

static void
via_analog_prepare(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;

	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void
via_analog_commit(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;

	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_ON);
}

static void
via_analog_mode_set(struct drm_encoder *encoder,
		       struct drm_display_mode *mode,
		       struct drm_display_mode *adjusted_mode)
{
	struct drm_via_private *dev_priv = encoder->dev->dev_private;
	u8 polarity = 0, orig;

	if (adjusted_mode->flags & DRM_MODE_FLAG_NHSYNC)
		polarity |= BIT(6);
	if (adjusted_mode->flags & DRM_MODE_FLAG_NVSYNC)
		polarity |= BIT(7);

	orig = (vga_r(VGABASE, VGA_MIS_R) & ~0xC0);
	vga_w(VGABASE, VGA_MIS_W, ((polarity & 0xC0) | orig));

	/* Select the proper IGA */
	via_diport_set_source(encoder);
}

static const struct drm_encoder_helper_funcs via_analog_enc_helper_funcs = {
	.dpms = via_analog_dpms,
	.mode_fixup = via_analog_mode_fixup,
	.prepare = via_analog_prepare,
	.commit = via_analog_commit,
	.mode_set = via_analog_mode_set,
};

static enum drm_connector_status
via_analog_detect(struct drm_connector *connector, bool force)
{
	struct drm_device *dev = connector->dev;
	struct drm_via_private *dev_priv = dev->dev_private;
	struct via_i2c *i2c = &dev_priv->i2c_par[0];
	struct i2c_adapter *adapter = NULL;
	struct edid *edid = NULL;

	drm_mode_connector_update_edid_property(connector, edid);

	adapter = &i2c->adapter;
	if (adapter) {
		edid = drm_get_edid(connector, adapter);
		if (edid) {
			drm_mode_connector_update_edid_property(connector, edid);
			kfree(edid);
			return connector_status_connected;
		}
	}
	return connector_status_unknown;
}

static void
via_analog_destroy(struct drm_connector *connector)
{
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

static int
via_analog_set_property(struct drm_connector *connector,
		  struct drm_property *property,
		  uint64_t value)
{
	struct drm_device *dev = connector->dev;

	if (property == dev->mode_config.dpms_property && connector->encoder)
		via_analog_dpms(connector->encoder, (uint32_t)(value & 0xf));
	return 0;
}

static const struct drm_connector_funcs via_analog_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = via_analog_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = via_analog_destroy,
	.set_property = via_analog_set_property,
};

/* This function test if the drm_display_modes generated by a display
 * monitor that is connected is supported by the graphics card.
 */
static int
via_analog_mode_valid(struct drm_connector *connector,
			struct drm_display_mode *mode)
{
	if ((mode->flags & DRM_MODE_FLAG_INTERLACE) &&
	    !connector->interlace_allowed)
		return MODE_NO_INTERLACE;

	if ((mode->flags & DRM_MODE_FLAG_DBLSCAN) &&
	    !connector->doublescan_allowed)
		return MODE_NO_DBLESCAN;

	/* Check Clock Range */
	if (mode->clock > 400000)
		return MODE_CLOCK_HIGH;
	if (mode->clock < 25000)
		return MODE_CLOCK_LOW;

	return MODE_OK;
}

static const struct drm_connector_helper_funcs via_analog_connector_helper_funcs = {
	.mode_valid = via_analog_mode_valid,
	.get_modes = via_get_edid_modes,
	.best_encoder = via_best_encoder,
};

void via_analog_init(struct drm_device *dev)
{
	struct drm_connector *connector;
	struct via_encoder *enc;
	void *par;

	par = kzalloc(sizeof(*enc) + sizeof(*connector), GFP_KERNEL);
	if (!par) {
		DRM_ERROR("Failed to allocate connector and encoder\n");
		return;
	}
	connector = par + sizeof(*enc);
	enc = par;

	/* Piece together our connector */
	drm_connector_init(dev, connector, &via_analog_connector_funcs,
				DRM_MODE_CONNECTOR_VGA);
	drm_connector_helper_add(connector, &via_analog_connector_helper_funcs);

	connector->interlace_allowed = true;
	connector->doublescan_allowed = false;

	/* Setup the encoders and attach them */
	drm_encoder_init(dev, &enc->base, &via_analog_enc_funcs, DRM_MODE_ENCODER_DAC);
	drm_encoder_helper_add(&enc->base, &via_analog_enc_helper_funcs);
	enc->base.possible_clones = 0;
	enc->base.possible_crtcs = BIT(1) | BIT(0);
	enc->diPort = DISP_DI_NONE;

	drm_mode_connector_attach_encoder(connector, &enc->base);

	drm_sysfs_connector_add(connector);
}
