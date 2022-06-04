/*
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
 * Authors:
 *	James Simmons <jsimmons@infradead.org>
 */

#include <linux/pci.h>

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>

#include "openchrome_drv.h"

void via_encoder_cleanup(struct drm_encoder *encoder)
{
	struct via_encoder *enc = container_of(encoder, struct via_encoder, base);

	drm_encoder_cleanup(encoder);
	kfree(enc);
}

int via_connector_mode_valid(struct drm_connector *connector,
			struct drm_display_mode *mode)
{
	/* Check Clock Range */
	if (mode->clock > 400000)
		return MODE_CLOCK_HIGH;

	if (mode->clock < 25000)
		return MODE_CLOCK_LOW;

	return MODE_OK;
}

void via_connector_destroy(struct drm_connector *connector)
{
	struct via_connector *con = container_of(connector, struct via_connector, base);
	struct drm_property *property, *tmp;

	list_for_each_entry_safe(property, tmp, &con->props, head)
		drm_property_destroy(connector->dev, property);
	list_del(&con->props);

	drm_connector_update_edid_property(connector, NULL);
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

/* Power sequence relations */
struct td_timer {
	struct vga_regset tdRegs[2];
};

static struct td_timer td_timer_regs[] = {
	/* td_timer0 */
	{ { { VGA_CRT_IC, 0x8B, 0, 7 }, { VGA_CRT_IC, 0x8F, 0, 3 } } },
	/* td_timer1 */
	{ { { VGA_CRT_IC, 0x8C, 0, 7 }, { VGA_CRT_IC, 0x8F, 4, 7 } } },
	/* td_timer2 */
	{ { { VGA_CRT_IC, 0x8D, 0, 7 }, { VGA_CRT_IC, 0x90, 0, 3 } } },
	/* td_timer3 */
	{ { { VGA_CRT_IC, 0x8E, 0, 7 }, { VGA_CRT_IC, 0x90, 4, 7 } } }
};

/*
 * Function Name:  via_init_td_timing_regs
 * Description: Init TD timing register (power sequence)
 */
static void via_init_td_timing_regs(struct drm_device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	unsigned int td_timer[4] = { 500, 50, 0, 510 }, i;
	struct vga_registers timings;
	u32 reg_value;

	/* Fill primary power sequence */
	for (i = 0; i < 4; i++) {
		/* Calculate TD Timer, every step is 572.1uSec */
		reg_value = td_timer[i] * 10000 / 5721;

		timings.count = ARRAY_SIZE(td_timer_regs[i].tdRegs);
		timings.regs = td_timer_regs[i].tdRegs;
		load_value_to_registers(VGABASE, &timings, reg_value);
	}

	/* Note: VT3353 have two hardware power sequences
	 * other chips only have one hardware power sequence */
	if (pdev->device == PCI_DEVICE_ID_VIA_VT1122) {
		/* set CRD4[0] to "1" to select 2nd LCD power sequence. */
		svga_wcrt_mask(VGABASE, 0xD4, BIT(0), BIT(0));
		/* Fill secondary power sequence */
		for (i = 0; i < 4; i++) {
			/* Calculate TD Timer, every step is 572.1uSec */
			reg_value = td_timer[i] * 10000 / 5721;

			timings.count = ARRAY_SIZE(td_timer_regs[i].tdRegs);
			timings.regs = td_timer_regs[i].tdRegs;
			load_value_to_registers(VGABASE, &timings, reg_value);
		}
	}
}

int via_modeset_init(struct drm_device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	struct via_drm_priv *dev_priv = to_via_drm_priv(dev);
	uint32_t i;
	int ret = 0;

	openchrome_mode_config_init(dev_priv);

	/* Initialize the number of display connectors. */
	dev_priv->number_fp = 0;
	dev_priv->number_dvi = 0;

	/* Init TD timing register (power sequence) */
	via_init_td_timing_regs(dev);

	via_i2c_reg_init(dev_priv);
	ret = via_i2c_init(dev);
	if (ret) {
		DRM_ERROR("Failed to initialize I2C bus!\n");
		goto exit;
	}

	for (i = 0; i < VIA_MAX_CRTC; i++) {
		ret = openchrome_crtc_init(dev_priv, i);
		if (ret) {
			goto exit;
		}
	}

	openchrome_ext_dvi_probe(dev);
	via_tmds_probe(dev);

	via_lvds_probe(dev);

	via_dac_probe(dev);


	openchrome_ext_dvi_init(dev);
	via_tmds_init(dev);

	via_dac_init(dev);

	via_lvds_init(dev);

	switch (pdev->device) {
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		via_hdmi_init(dev, VIA_DI_PORT_NONE);
		break;
	default:
		break;
	}

	drm_mode_config_reset(dev);

	drm_kms_helper_poll_init(dev);
exit:
	return ret;
}

void via_modeset_fini(struct drm_device *dev)
{
	drm_kms_helper_poll_fini(dev);

	drm_helper_force_disable_all(dev);

	drm_mode_config_cleanup(dev);

	via_i2c_exit();
}
