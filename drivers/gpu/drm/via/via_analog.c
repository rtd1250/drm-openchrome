/*
 * Copyright © 2012 James Simmons
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
#include "via_drv.h"

/*
 * Routines for controlling stuff on the analog port
 */
static const struct drm_encoder_funcs via_dac_enc_funcs = {
	.destroy = via_encoder_cleanup,
};

/* Manage the power state of the DAC */
static void
via_dac_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_via_private *dev_priv = encoder->dev->dev_private;
	u8 mask = 0;

	switch (mode) {
	case DRM_MODE_DPMS_SUSPEND:
		mask = BIT(5);		/* VSync off */
		break;
	case DRM_MODE_DPMS_STANDBY:
		mask = BIT(4);		/* HSync off */
		break;
	case DRM_MODE_DPMS_OFF:
		mask = (BIT(5) | BIT(4));/* HSync and VSync off */
		break;
	case DRM_MODE_DPMS_ON:
	default:
		break;
	}
	svga_wcrt_mask(VGABASE, 0x36, mask, BIT(5) | BIT(4));
}

/* Pass our mode to the connectors and the CRTC to give them a chance to
 * adjust it according to limitations or connector properties, and also
 * a chance to reject the mode entirely. Usefule for things like scaling.
 */
static bool
via_dac_mode_fixup(struct drm_encoder *encoder,
		 const struct drm_display_mode *mode,
		 struct drm_display_mode *adjusted_mode)
{
	drm_mode_set_crtcinfo(adjusted_mode, 0);
	return true;
}

static const struct drm_encoder_helper_funcs via_dac_enc_helper_funcs = {
	.dpms = via_dac_dpms,
	.mode_fixup = via_dac_mode_fixup,
	.mode_set = via_set_sync_polarity,
	.prepare = via_encoder_prepare,
	.commit = via_encoder_commit,
	.disable = via_encoder_disable,
};

static enum drm_connector_status
via_analog_detect(struct drm_connector *connector, bool force)
{
	struct via_connector *con = container_of(connector, struct via_connector, base);
	enum drm_connector_status ret = connector_status_disconnected;
	struct edid *edid = NULL;

	drm_mode_connector_update_edid_property(connector, edid);
	if (con->ddc_bus) {
		edid = drm_get_edid(connector, con->ddc_bus);
		if (edid) {
			drm_mode_connector_update_edid_property(connector, edid);
			kfree(edid);
			ret = connector_status_connected;
		}
	}
	return ret;
}

static const struct drm_connector_funcs via_analog_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = via_analog_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.set_property = via_connector_set_property,
	.destroy = via_connector_destroy,
};

static const struct drm_connector_helper_funcs via_analog_connector_helper_funcs = {
	.mode_valid = via_connector_mode_valid,
	.get_modes = via_get_edid_modes,
	.best_encoder = via_best_encoder,
};

void
via_analog_init(struct drm_device *dev)
{
	struct via_connector *con;
	struct via_encoder *enc;

	enc = kzalloc(sizeof(*enc) + sizeof(*con), GFP_KERNEL);
	if (!enc) {
		DRM_ERROR("Failed to allocate connector and encoder\n");
		return;
	}
	con = &enc->cons[0];
	INIT_LIST_HEAD(&con->props);

	/* Piece together our connector */
	drm_connector_init(dev, &con->base, &via_analog_connector_funcs,
				DRM_MODE_CONNECTOR_VGA);
	drm_connector_helper_add(&con->base, &via_analog_connector_helper_funcs);
	drm_connector_register(&con->base);

	con->ddc_bus = via_find_ddc_bus(0x26);
	con->base.doublescan_allowed = false;
	con->base.interlace_allowed = true;

	/* Setup the encoders and attach them */
	drm_encoder_init(dev, &enc->base, &via_dac_enc_funcs, DRM_MODE_ENCODER_DAC);
	drm_encoder_helper_add(&enc->base, &via_dac_enc_helper_funcs);

	enc->base.possible_crtcs = BIT(0);
	enc->base.possible_clones = 0;
	enc->diPort = DISP_DI_DAC;

	drm_mode_connector_attach_encoder(&con->base, &enc->base);
}
