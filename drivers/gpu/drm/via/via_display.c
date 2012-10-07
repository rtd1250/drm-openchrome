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

#include "drmP.h"
#include "via_drv.h"

struct drm_encoder*
via_best_encoder(struct drm_connector *connector)
{
	int enc_id = connector->encoder_ids[0];
	struct drm_encoder *encoder = NULL;
	struct drm_mode_object *obj;

	/* pick the encoder ids */
	if (enc_id) {
		obj = drm_mode_object_find(connector->dev, enc_id, DRM_MODE_OBJECT_ENCODER);
		if (!obj)
			return NULL;
		encoder = obj_to_encoder(obj);
	}
	return encoder;
}

int
via_get_edid_modes(struct drm_connector *connector)
{
	struct edid *edid = NULL;

	if (connector->edid_blob_ptr)
		edid = (struct edid *) connector->edid_blob_ptr->data;

	return drm_add_edid_modes(connector, edid);
}

static void
via_i2c_reg_init(struct drm_via_private *dev_priv)
{
	vga_wseq(VGABASE, 0x31, 0x01);
	svga_wseq_mask(VGABASE, 0x31, 0x30, 0x30);
	vga_wseq(VGABASE, 0x26, 0x01);
	svga_wseq_mask(VGABASE, 0x26, 0x30, 0x30);
	vga_wseq(VGABASE, 0x2C, 0xc2);
	vga_wseq(VGABASE, 0x3D, 0xc0);
	svga_wseq_mask(VGABASE, 0x2C, 0x30, 0x30);
	svga_wseq_mask(VGABASE, 0x3D, 0x30, 0x30);
}

static void
via_hwcursor_init(struct drm_via_private *dev_priv)
{
	/* set 0 as transparent color key */
	VIA_WRITE(PRIM_HI_TRANSCOLOR, 0);
	VIA_WRITE(PRIM_HI_FIFO, 0x0D000D0F);
	VIA_WRITE(PRIM_HI_INVTCOLOR, 0X00FFFFFF);
	VIA_WRITE(V327_HI_INVTCOLOR, 0X00FFFFFF);

	/* set 0 as transparent color key */
	VIA_WRITE(HI_TRANSPARENT_COLOR, 0);
	VIA_WRITE(HI_INVTCOLOR, 0X00FFFFFF);
	VIA_WRITE(ALPHA_V3_PREFIFO_CONTROL, 0xE0000);
	VIA_WRITE(ALPHA_V3_FIFO_CONTROL, 0xE0F0000);

	/* Turn cursor off. */
	VIA_WRITE(HI_CONTROL, VIA_READ(HI_CONTROL) & 0xFFFFFFFA);
}

static void
via_init_crtc_regs(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;

        via_unlock_crt(VGABASE, dev->pdev->device);

	/* always set to 1 */
	svga_wcrt_mask(VGABASE, 0x03, BIT(7), BIT(7));
	/* bits 0 to 7 of line compare */
	vga_wcrt(VGABASE, 0x18, 0xFF);
	/* bit 8 of line compare */
	svga_wcrt_mask(VGABASE, 0x07, BIT(4), BIT(4));
	/* bit 9 of line compare */
	svga_wcrt_mask(VGABASE, 0x09, BIT(6), BIT(6));
	/* bit 10 of line compare */
	svga_wcrt_mask(VGABASE, 0x35, BIT(4), BIT(4));
	/* adjust hsync by one character - value 011 */
	svga_wcrt_mask(VGABASE, 0x33, 0x06, 0x07);
	/* extend mode always set to e3h */
	vga_wcrt(VGABASE, 0x17, 0xE3);
	/* extend mode always set to 0h */
	vga_wcrt(VGABASE, 0x08, 0x00);
	/* extend mode always set to 0h */
	vga_wcrt(VGABASE, 0x14, 0x00);

	/* If K8M800, enable Prefetch Mode. */
	if ((dev->pdev->device == PCI_DEVICE_ID_VIA_K8M800) ||
	    (dev->pdev->device == PCI_DEVICE_ID_VIA_K8M890))
		svga_wcrt_mask(VGABASE, 0x33, 0x00, BIT(3));

	if ((dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266) &&
	    (dev_priv->revision == CLE266_REVISION_AX))
		svga_wseq_mask(VGABASE, 0x1A, BIT(1), BIT(1));

        via_lock_crt(VGABASE);
}

static void
via_display_init(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;

	/* load fixed CRTC timing registers */
	via_init_crtc_regs(dev);

	/* I/O address bit to be 1. Enable access */
	/* to frame buffer at A0000-BFFFFh */
	svga_wmisc_mask(VGABASE, BIT(0), BIT(0));
}

int __devinit
via_modeset_init(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	int i;

	drm_mode_config_init(dev);

	/* What is the max ? */
	dev->mode_config.min_width = 320;
	dev->mode_config.min_height = 200;
	dev->mode_config.max_width = 2048;
	dev->mode_config.max_height = 2048;

	via_display_init(dev);
	via_i2c_reg_init(dev_priv);
	via_i2c_init(dev);
	via_hwcursor_init(dev_priv);

	for (i = 0; i < 2; i++)
		via_crtc_init(dev, i);

	via_analog_init(dev);
	/* CX700M has two analog ports */
	if ((dev->pdev->device == PCI_DEVICE_ID_VIA_VT3157) &&
	    (dev_priv->revision == CX700_REVISION_700M))
		via_analog_init(dev);

	/*
	 * Set up the framebuffer device
	 */
	return via_framebuffer_init(dev, &dev_priv->helper);
}

void via_modeset_fini(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;

	via_framebuffer_fini(dev_priv->helper);

	drm_mode_config_cleanup(dev);

	via_i2c_exit(dev);
}
