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
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	James Simmons <jsimmons@infradead.org>
 */

#include "drmP.h"
#include "drm_mode.h"
#include "drm_crtc_helper.h"

#include "via_drv.h"

static int
via_iga1_cursor_set(struct drm_crtc *crtc, struct drm_file *file_priv,
			uint32_t handle, uint32_t width, uint32_t height)
{
	struct drm_via_private *dev_priv = crtc->dev->dev_private;
	int max_height = 64, max_width = 64, ret = 0;
	struct via_crtc *iga = &dev_priv->iga[0];
	struct drm_device *dev = crtc->dev;
	struct drm_gem_object *obj = NULL;
	uint32_t temp;

	if (!handle) {
		/* turn off cursor */
		temp = VIA_READ(VIA_REG_HI_CONTROL1);
		VIA_WRITE(VIA_REG_HI_CONTROL1, temp & 0xFFFFFFFA);
		obj = iga->cursor_bo;
		goto unpin;
	}

	if (dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266 ||
	    dev->pdev->device == PCI_DEVICE_ID_VIA_KM400) {
		max_height >>= 1;
		max_width >>= 1;
	}

	if ((height > max_height) || (width > max_width)) {
		DRM_ERROR("bad cursor width or height %d x %d\n", width, height);
		return -EINVAL;
	}

	obj = drm_gem_object_lookup(dev, file_priv, handle);
	if (!obj) {
		DRM_ERROR("Cannot find cursor object %x for crtc %d\n", handle, crtc->base.id);
		return -ENOENT;
	}

	/* set_cursor, show_cursor */
	iga->cursor_bo = obj;
unpin:
	if (obj)
		drm_gem_object_unreference_unlocked(obj);
	return ret;
}

static int
via_iga2_cursor_set(struct drm_crtc *crtc, struct drm_file *file_priv,
			uint32_t handle, uint32_t width, uint32_t height)
{
	return 0;
}

static int
via_iga1_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	return 0;
}

static int
via_iga2_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	return 0;
}

static void
via_iga1_gamma_set(struct drm_crtc *crtc, u16 *red, u16 *green, u16 *blue,
			uint32_t start, uint32_t size)
{
	struct drm_via_private *dev_priv = crtc->dev->dev_private;
	int end = (start + size > 256) ? 256 : start + size, i;
	u8 val;

	if (!crtc->enabled || !crtc->fb)
		return;

	if (crtc->fb->bits_per_pixel == 8) {
		u8 sr1a = vga_rseq(VGABASE, 0x1a);

		/* Prepare for initialize IGA1's LUT: */
		vga_wseq(VGABASE, 0x1a, sr1a & 0xfe);
		/* Change to Primary Display's LUT. */
		val = vga_rseq(VGABASE, 0x1b);
		vga_wseq(VGABASE, 0x1b, val);
		val = vga_rcrt(VGABASE, 0x67);
		vga_wcrt(VGABASE, 0x67, val);

		/* Fill in IGA1's LUT. */
		for (i = start; i < end; i++) {
			u16 r = *red++, g = *green++, b = *blue++;

			/* Bit mask of palette */
			vga_w(VGABASE, VGA_PEL_MSK, 0xff);
			vga_w(VGABASE, VGA_PEL_IW, i);
			vga_w(VGABASE, VGA_PEL_D, r >> 8);
			vga_w(VGABASE, VGA_PEL_D, g >> 8);
			vga_w(VGABASE, VGA_PEL_D, b >> 8);
		}
		/* enable LUT */
		vga_wseq(VGABASE, 0x1b, BIT(0));
		/*  Disable gamma in case it was enabled previously */
		vga_wcrt(VGABASE, 0x33, BIT(7));	
		/* access Primary Display's LUT. */
		vga_wseq(VGABASE, 0x1a, sr1a & 0xfe);
	} else {
		val = vga_rseq(VGABASE, 0x1a);

		/* Enable Gamma */
		vga_wcrt(VGABASE, 0x80, BIT(7));
		vga_wseq(VGABASE, 0x00, BIT(0));
		/* Fill in IGA1's gamma. */
		for (i = start; i < end; i++) {
			u16 r = *red++, g = *green++, b = *blue++;

			/* bit mask of palette */
			vga_w(VGABASE, VGA_PEL_MSK, 0xff);
			vga_w(VGABASE, VGA_PEL_IW, i);
			vga_w(VGABASE, VGA_PEL_D, r >> 8);
			vga_w(VGABASE, VGA_PEL_D, g >> 8);
			vga_w(VGABASE, VGA_PEL_D, b >> 8);
		}
		vga_wseq(VGABASE, 0x1a, val);
	}
}

static void
via_iga2_gamma_set(struct drm_crtc *crtc, u16 *red, u16 *green,
			u16 *blue, uint32_t start, uint32_t size)
{
}

static void
via_crtc_destroy(struct drm_crtc *crtc)
{
	struct drm_via_private *dev_priv = crtc->dev->dev_private;
	struct via_crtc *iga = NULL;
	int index = 0;

	if (iga->iga1 != crtc->base.id)
		index = 1;
	iga = &dev_priv->iga[index];

	drm_crtc_cleanup(crtc);
	crtc->dev->driver->gem_free_object(iga->cursor_bo);
	if (crtc) kfree(crtc);
}

static const struct drm_crtc_funcs via_iga1_funcs = {
	.cursor_set = via_iga1_cursor_set,
	.cursor_move = via_iga1_cursor_move,
	.gamma_set = via_iga1_gamma_set,
	.set_config = drm_crtc_helper_set_config,
	.destroy = via_crtc_destroy,
};

static const struct drm_crtc_funcs via_iga2_funcs = {
	.cursor_set = via_iga2_cursor_set,
	.cursor_move = via_iga2_cursor_move,
	.gamma_set = via_iga2_gamma_set,
	.set_config = drm_crtc_helper_set_config,
	.destroy = via_crtc_destroy,
};

static void
via_iga1_dpms(struct drm_crtc *crtc, int mode)
{
	struct drm_via_private *dev_priv = crtc->dev->dev_private;

	/* Setup IGA path */
	switch (mode) {
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_OFF:
		vga_wseq(VGABASE, 0x01, BIT(5));
		break;
	case DRM_MODE_DPMS_ON:
		vga_wseq(VGABASE, 0x00, BIT(5));
		break;
	}
}

static void
via_iga2_dpms(struct drm_crtc *crtc, int mode)
{
	struct drm_via_private *dev_priv = crtc->dev->dev_private;

	/* Setup IGA path */
	switch (mode) {
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_OFF:
		vga_wseq(VGABASE, 0x00, BIT(2));
		break;
	case DRM_MODE_DPMS_ON:
		vga_wseq(VGABASE, 0x6b, BIT(2));
		break;
	}
}

static void
via_crtc_prepare(struct drm_crtc *crtc)
{
	struct drm_crtc_helper_funcs *crtc_funcs;

	/* Turn off the cursor */

	/* Blank the screen */
	if (crtc->enabled) {
		crtc_funcs = crtc->helper_private;
		crtc_funcs->dpms(crtc, DRM_MODE_DPMS_OFF);
	}

	/* Once VQ is working it will have to be disabled here */
}

static void
via_crtc_commit(struct drm_crtc *crtc)
{
	struct drm_crtc_helper_funcs *crtc_funcs;

	/* Turn on the cursor */

	/* Turn on the monitor */
	if (crtc->enabled) {
		crtc_funcs = crtc->helper_private;

		crtc_funcs->dpms(crtc, DRM_MODE_DPMS_ON);
	}
}

static bool
via_crtc_mode_fixup(struct drm_crtc *crtc, struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode)
{
	return true;
}

static int
via_crtc_mode_set(struct drm_crtc *crtc, struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode,
		int x, int y, struct drm_framebuffer *old_fb)
{
	struct drm_via_private *dev_priv = crtc->dev->dev_private;
	u8 orig;

	/* unlock extended registers */
	vga_wseq(VGABASE, 0x10, 0x01);	

	/* unlock CRT registers */
	orig = vga_rcrt(VGABASE, 0x47);
	vga_wcrt(VGABASE, 0x47, (orig & 0x01));

	regs_init(VGABASE);

	crtc_set_regs(mode, VGABASE);
	return 0;
}

static int
via_iga1_mode_set_base(struct drm_crtc *crtc, int x, int y,
			struct drm_framebuffer *old_fb)
{
	if (!crtc->fb)
		return 0;
	return 0;
}

static int
via_iga2_mode_set_base(struct drm_crtc *crtc, int x, int y,
			struct drm_framebuffer *old_fb)
{
	return 0;
}

static int
via_iga1_mode_set_base_atomic(struct drm_crtc *crtc, struct drm_framebuffer *fb,
				int x, int y, enum mode_set_atomic state)
{
	return 0;
}

static int
via_iga2_mode_set_base_atomic(struct drm_crtc *crtc, struct drm_framebuffer *fb,
				int x, int y, enum mode_set_atomic state)
{
	return 0;
}

static void 
drm_mode_crtc_load_lut(struct drm_crtc *crtc)
{
	int size = crtc->gamma_size * sizeof(uint16_t);
	void *r_base, *g_base, *b_base;

	if (size) {
		r_base = crtc->gamma_store;
		g_base = r_base + size;
		b_base = g_base + size;
		crtc->funcs->gamma_set(crtc, r_base, g_base, b_base,
					0, crtc->gamma_size);
	}
}

static const struct drm_crtc_helper_funcs via_iga1_helper_funcs = {
	.dpms = via_iga1_dpms,
	.prepare = via_crtc_prepare,
	.commit = via_crtc_commit,
	.mode_fixup = via_crtc_mode_fixup,
	.mode_set = via_crtc_mode_set,
	.mode_set_base = via_iga1_mode_set_base,
	.mode_set_base_atomic = via_iga1_mode_set_base_atomic,
	.load_lut = drm_mode_crtc_load_lut,
};

static const struct drm_crtc_helper_funcs via_iga2_helper_funcs = {
	.dpms = via_iga2_dpms,
	.prepare = via_crtc_prepare,
	.commit = via_crtc_commit,
	.mode_fixup = via_crtc_mode_fixup,
	.mode_set = via_crtc_mode_set,
	.mode_set_base = via_iga2_mode_set_base,
	.mode_set_base_atomic = via_iga2_mode_set_base_atomic,
	.load_lut = drm_mode_crtc_load_lut,
};

static void 
via_crtc_init(struct drm_device *dev, int index)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	struct via_crtc *iga = &dev_priv->iga[index];
	struct drm_crtc *crtc = &iga->crtc;

	if (index) {
		drm_crtc_init(dev, crtc, &via_iga2_funcs);
		drm_crtc_helper_add(crtc, &via_iga2_helper_funcs);
	} else {
		drm_crtc_init(dev, crtc, &via_iga1_funcs);
		drm_crtc_helper_add(crtc, &via_iga1_helper_funcs);
		iga->iga1 = crtc->base.id;
	}
	drm_mode_crtc_set_gamma_size(crtc, 256);
}

int via_modeset_init(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	int i;

	drm_mode_config_init(dev);

	/* What is the max ? */
	dev->mode_config.min_width = 320;
	dev->mode_config.min_height = 200;
	dev->mode_config.max_width = 2048;
	dev->mode_config.max_height = 2048;

	via_i2c_init(dev);

	for (i = 0; i < 2; i++)
		via_crtc_init(dev, i);

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
