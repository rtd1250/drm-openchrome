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
#include <drm/drm_mode.h>
#include "drmP.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"

#include "openchrome_drv.h"
#include "openchrome_disp_reg.h"

static struct vga_regset vpit_table[] = {
	{VGA_SEQ_I, 0x01, 0xFF, 0x01 },
	{VGA_SEQ_I, 0x02, 0xFF, 0x0F },
	{VGA_SEQ_I, 0x03, 0xFF, 0x00 },
	{VGA_SEQ_I, 0x04, 0xFF, 0x0E },
	{VGA_GFX_I, 0x00, 0xFF, 0x00 },
	{VGA_GFX_I, 0x01, 0xFF, 0x00 },
	{VGA_GFX_I, 0x02, 0xFF, 0x00 },
	{VGA_GFX_I, 0x03, 0xFF, 0x00 },
	{VGA_GFX_I, 0x04, 0xFF, 0x00 },
	{VGA_GFX_I, 0x05, 0xFF, 0x00 },
	{VGA_GFX_I, 0x06, 0xFF, 0x05 },
	{VGA_GFX_I, 0x07, 0xFF, 0x0F },
	{VGA_GFX_I, 0x08, 0xFF, 0xFF }
};

static void via_iga_common_init(void __iomem *regs)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Be careful with 3C5.15[5] - Wrap Around Disable.
	 * It must be set to 1 for proper operation. */
	/* 3C5.15[5]   - Wrap Around Disable
	 *               0: Disable (For Mode 0-13)
	 *               1: Enable
	 * 3C5.15[1]   - Extended Display Mode Enable
	 *               0: Disable
	 *               1: Enable */
	svga_wseq_mask(regs, 0x15, BIT(5) | BIT(1), BIT(5) | BIT(1));

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void via_iga1_set_color_depth(
			struct openchrome_drm_private *dev_private,
			u8 depth)
{
	u8 value;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	value = 0x00;

	/* Set the color depth for IGA1. */
	switch (depth) {
	case 8:
		break;
	case 16:
		/* Bit 4 is for 555 (15-bit) / 565 (16-bit) color selection. */
		value |= BIT(4) | BIT(2);
		break;
	case 24:
	case 32:
		value |= BIT(3) | BIT(2);
		break;
	default:
		break;
	}

	if ((depth == 8) || (depth == 16) ||
		(depth == 24) || (depth == 32)) {
		/* 3C5.15[4]   - Hi Color Mode Select
		 *               0: 555
		 *               1: 565
		 * 3C5.15[3:2] - Display Color Depth Select
		 *               00: 8bpp
		 *               01: 16bpp
		 *               10: 30bpp
		 *               11: 32bpp */
		svga_wseq_mask(VGABASE, 0x15, value,
				BIT(4) | BIT(3) | BIT(2));
		DRM_INFO("IGA1 Color Depth: %d bit\n", depth);
	} else {
		DRM_ERROR("Unsupported IGA1 Color Depth: %d bit\n",
				depth);
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void via_iga2_set_color_depth(
			struct openchrome_drm_private *dev_private,
			u8 depth)
{
	u8 value;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	value = 0x00;

	/* Set the color depth for IGA2. */
	switch (depth) {
	case 8:
		break;
	case 16:
		value = BIT(6);
		break;
	case 24:
	case 32:
		value = BIT(7) | BIT(6);
		break;
	default:
		break;
	}

	if ((depth == 8) || (depth == 16) ||
		(depth == 24) || (depth == 32)) {
		/* 3X5.67[7:6] - Display Color Depth Select
		 *               00: 8bpp
		 *               01: 16bpp
		 *               10: 30bpp
		 *               11: 32bpp */
		svga_wcrt_mask(VGABASE, 0x67, value, 0xC0);
		DRM_INFO("IGA2 Color Depth: %d bit\n", depth);
	} else {
		DRM_ERROR("Unsupported IGA2 Color Depth: %d bit\n",
				depth);
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void via_hide_cursor(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;
	uint32_t temp;

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_PM800:
	case PCI_DEVICE_ID_VIA_VT3157:
	case PCI_DEVICE_ID_VIA_VT3343:
	case PCI_DEVICE_ID_VIA_P4M900:
	case PCI_DEVICE_ID_VIA_VT1122:
	case PCI_DEVICE_ID_VIA_VX875:
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		if (iga->index) {
			temp = VIA_READ(HI_CONTROL);
			VIA_WRITE(HI_CONTROL, temp & 0xFFFFFFFA);
		} else {
			temp = VIA_READ(PRIM_HI_CTRL);
			VIA_WRITE(PRIM_HI_CTRL, temp & 0xFFFFFFFA);
		}

		break;
	default:
		temp = VIA_READ(HI_CONTROL);
		VIA_WRITE(HI_CONTROL, temp & 0xFFFFFFFA);
		break;
	}
}

static void via_show_cursor(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_PM800:
	case PCI_DEVICE_ID_VIA_VT3157:
	case PCI_DEVICE_ID_VIA_VT3343:
	case PCI_DEVICE_ID_VIA_P4M900:
	case PCI_DEVICE_ID_VIA_VT1122:
	case PCI_DEVICE_ID_VIA_VX875:
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		/* Program Hardware Icon (HI) FIFO, foreground, and
		 * background colors. */
		if (iga->index) {
			VIA_WRITE(HI_TRANSPARENT_COLOR, 0x00000000);
			VIA_WRITE(HI_INVTCOLOR, 0x00FFFFFF);
			VIA_WRITE(ALPHA_V3_PREFIFO_CONTROL, 0x000E0000);
			VIA_WRITE(ALPHA_V3_FIFO_CONTROL, 0x0E0F0000);
		} else {
			VIA_WRITE(PRIM_HI_TRANSCOLOR, 0x00000000);
			VIA_WRITE(PRIM_HI_INVTCOLOR, 0x00FFFFFF);
			VIA_WRITE(V327_HI_INVTCOLOR, 0x00FFFFFF);
			VIA_WRITE(PRIM_HI_FIFO, 0x0D000D0F);
		}

		break;
	default:
		VIA_WRITE(HI_TRANSPARENT_COLOR, 0x00000000);
		VIA_WRITE(HI_INVTCOLOR, 0x00FFFFFF);
		VIA_WRITE(ALPHA_V3_PREFIFO_CONTROL, 0x000E0000);
		VIA_WRITE(ALPHA_V3_FIFO_CONTROL, 0xE0F0000);
		break;
	}

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_PM800:
	case PCI_DEVICE_ID_VIA_VT3157:
	case PCI_DEVICE_ID_VIA_VT3343:
	case PCI_DEVICE_ID_VIA_P4M900:
	case PCI_DEVICE_ID_VIA_VT1122:
	case PCI_DEVICE_ID_VIA_VX875:
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		/* Turn on Hardware icon Cursor */
		if (iga->index) {
			VIA_WRITE(HI_CONTROL, 0xB6000005);
		} else {
			VIA_WRITE(PRIM_HI_CTRL, 0x36000005);
		}

		break;
	default:
		if (iga->index) {
			VIA_WRITE(HI_CONTROL, 0xB6000005);
		} else {
			VIA_WRITE(HI_CONTROL, 0x36000005);
		}

		break;
	}
}

static void via_cursor_address(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;

	if (!iga->cursor_kmap.bo) {
		return;
	}

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_PM800:
	case PCI_DEVICE_ID_VIA_VT3157:
	case PCI_DEVICE_ID_VIA_VT3343:
	case PCI_DEVICE_ID_VIA_P4M900:
	case PCI_DEVICE_ID_VIA_VT1122:
	case PCI_DEVICE_ID_VIA_VX875:
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		/* Program the HI offset. */
		if (iga->index) {
			VIA_WRITE(HI_FBOFFSET, iga->cursor_kmap.bo->offset);
		} else {
			VIA_WRITE(PRIM_HI_FBOFFSET, iga->cursor_kmap.bo->offset);
		}
		break;
	default:
		VIA_WRITE(HI_FBOFFSET, iga->cursor_kmap.bo->offset);
		break;
	}
}

static int via_crtc_cursor_set(struct drm_crtc *crtc,
				struct drm_file *file_priv,
				uint32_t handle,
				uint32_t width, uint32_t height)
{
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	int max_height = 64, max_width = 64, ret = 0, i;
	struct drm_device *dev = crtc->dev;
	struct drm_gem_object *obj = NULL;
	struct ttm_bo_kmap_obj user_kmap;

	if (!iga->cursor_kmap.bo)
		return -ENXIO;

	if (!handle) {
		/* turn off cursor */
		via_hide_cursor(crtc);
		return ret;
	}

	if (dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266 ||
		dev->pdev->device == PCI_DEVICE_ID_VIA_KM400) {
		max_height >>= 1;
		max_width >>= 1;
	}

	if ((height > max_height) || (width > max_width)) {
		DRM_ERROR("bad cursor width or height %d x %d\n",
				width, height);
		return -EINVAL;
	}

	obj = drm_gem_object_lookup(file_priv, handle);
	if (!obj) {
		DRM_ERROR("Cannot find cursor object %x for crtc %d\n",
				handle, crtc->base.id);
		return -ENOENT;
	}

	user_kmap.bo = ttm_gem_mapping(obj);
	ret = ttm_bo_kmap(user_kmap.bo, 0, user_kmap.bo->num_pages, &user_kmap);
	if (!ret) {
		/* Copy data from userland to cursor memory region */
		u32 *dst = iga->cursor_kmap.virtual, *src = user_kmap.virtual;

		memset_io(dst, 0x0, iga->cursor_kmap.bo->mem.size);
		for (i = 0; i < height; i++) {
			__iowrite32_copy(dst, src, width);
			dst += max_width;
			src += width;
		}
		ttm_bo_kunmap(&user_kmap);
	}
	drm_gem_object_unreference_unlocked(obj);
	via_cursor_address(crtc);
	via_show_cursor(crtc);

	return ret;
}

static int via_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	struct drm_device *dev = crtc->dev;
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;
	unsigned char xoff = 0, yoff = 0;
	int xpos = x, ypos = y;

	if (x < 0) {
		xoff = (-x) & 0xFF;
		xpos = 0;
	}

	if (y < 0) {
		yoff = (-y) & 0xFF;
		ypos = 0;
	}

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_PM800:
	case PCI_DEVICE_ID_VIA_VT3157:
	case PCI_DEVICE_ID_VIA_VT3343:
	case PCI_DEVICE_ID_VIA_P4M900:
	case PCI_DEVICE_ID_VIA_VT1122:
	case PCI_DEVICE_ID_VIA_VX875:
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		if (iga->index) {
			VIA_WRITE(HI_POSSTART, ((xpos << 16) | (ypos & 0x07ff)));
			VIA_WRITE(HI_CENTEROFFSET, ((xoff << 16) | (yoff & 0x07ff)));
		} else {
			VIA_WRITE(PRIM_HI_POSSTART, ((xpos << 16) | (ypos & 0x07ff)));
			VIA_WRITE(PRIM_HI_CENTEROFFSET, ((xoff << 16) | (yoff & 0x07ff)));
		}

		break;
	default:
		VIA_WRITE(HI_POSSTART, ((xpos << 16) | (ypos & 0x07ff)));
		VIA_WRITE(HI_CENTEROFFSET, ((xoff << 16) | (yoff & 0x07ff)));
		break;
	}

	via_show_cursor(crtc);

	return 0;
}

static int via_iga1_gamma_set(struct drm_crtc *crtc,
				u16 *r, u16 *g, u16 *b,
				uint32_t size,
				struct drm_modeset_acquire_ctx *ctx)
{
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;
	int end = (size > 256) ? 256 : size, i;
	u8 val, sr1a = vga_rseq(VGABASE, 0x1A);

	if (!crtc->enabled || !crtc->primary->fb)
		return -EINVAL;

	if (crtc->primary->fb->format->cpp[0] * 8 == 8) {
		/* Prepare for initialize IGA1's LUT: */
		vga_wseq(VGABASE, 0x1A, sr1a & 0xFE);
		/* Change to Primary Display's LUT */
		val = vga_rseq(VGABASE, 0x1B);
		vga_wseq(VGABASE, 0x1B, val);
		val = vga_rcrt(VGABASE, 0x67);
		vga_wcrt(VGABASE, 0x67, val);

		/* Fill in IGA1's LUT */
		for (i = 0; i < end; i++) {
			/* Bit mask of palette */
			vga_w(VGABASE, VGA_PEL_MSK, 0xFF);
			vga_w(VGABASE, VGA_PEL_IW, i);
			vga_w(VGABASE, VGA_PEL_D, r[i] >> 8);
			vga_w(VGABASE, VGA_PEL_D, g[i] >> 8);
			vga_w(VGABASE, VGA_PEL_D, b[i] >> 8);
		}
		/* enable LUT */
		svga_wseq_mask(VGABASE, 0x1B, 0x00, BIT(0));
		/* Disable gamma in case it was enabled previously */
		svga_wcrt_mask(VGABASE, 0x33, 0x00, BIT(7));
		/* access Primary Display's LUT */
		vga_wseq(VGABASE, 0x1A, sr1a & 0xFE);
	} else {
		/* Enable Gamma */
		svga_wcrt_mask(VGABASE, 0x33, BIT(7), BIT(7));
		svga_wseq_mask(VGABASE, 0x1A, 0x00, BIT(0));

		/* Fill in IGA1's gamma */
		for (i = 0; i < end; i++) {
			/* bit mask of palette */
			vga_w(VGABASE, VGA_PEL_MSK, 0xFF);
			vga_w(VGABASE, VGA_PEL_IW, i);
			vga_w(VGABASE, VGA_PEL_D, r[i] >> 8);
			vga_w(VGABASE, VGA_PEL_D, g[i] >> 8);
			vga_w(VGABASE, VGA_PEL_D, b[i] >> 8);
		}
		vga_wseq(VGABASE, 0x1A, sr1a);
	}

	return 0;
}

static int via_iga2_gamma_set(struct drm_crtc *crtc,
				u16 *r, u16 *g, u16 *b,
				uint32_t size,
				struct drm_modeset_acquire_ctx *ctx)
{
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;
	int end = (size > 256) ? 256 : size, i;
	u8 sr1a = vga_rseq(VGABASE, 0x1A);

	if (!crtc->enabled || !crtc->primary->fb)
		return -EINVAL;

	if (crtc->primary->fb->format->cpp[0] * 8 == 8) {
		/* Change Shadow to Secondary Display's LUT */
		svga_wseq_mask(VGABASE, 0x1A, BIT(0), BIT(0));
		/* Enable Secondary Display Engine */
		svga_wseq_mask(VGABASE, 0x1B, BIT(7), BIT(7));
		/* Second Display Color Depth, 8bpp */
		svga_wcrt_mask(VGABASE, 0x67, 0x3F, 0x3F);

		/* Enable second display channel just in case. */
		if (!(vga_rcrt(VGABASE, 0x6A) & BIT(7)))
			enable_second_display_channel(VGABASE);

		/* Fill in IGA2's LUT */
		for (i = 0; i < end; i++) {
			/* Bit mask of palette */
			vga_w(VGABASE, VGA_PEL_MSK, 0xFF);
			vga_w(VGABASE, VGA_PEL_IW, i);
			vga_w(VGABASE, VGA_PEL_D, r[i] >> 8);
			vga_w(VGABASE, VGA_PEL_D, g[i] >> 8);
			vga_w(VGABASE, VGA_PEL_D, b[i] >> 8);
		}
		/* Disable gamma in case it was enabled previously */
		svga_wcrt_mask(VGABASE, 0x6A, 0x00, BIT(1));

		/* access Primary Display's LUT */
		vga_wseq(VGABASE, 0x1A, sr1a & 0xFE);
	} else {
		u8 reg_bits = BIT(1);

		svga_wseq_mask(VGABASE, 0x1A, BIT(0), BIT(0));
		/* Bit 1 enables gamma */
		svga_wcrt_mask(VGABASE, 0x6A, BIT(1), BIT(1));

		/* Old platforms LUT are 6 bits in size.
		 * Newer it is 8 bits. */
		switch (crtc->dev->pdev->device) {
		case PCI_DEVICE_ID_VIA_CLE266:
		case PCI_DEVICE_ID_VIA_KM400:
		case PCI_DEVICE_ID_VIA_K8M800:
		case PCI_DEVICE_ID_VIA_PM800:
			break;

		default:
			reg_bits |= BIT(5);
			break;
		}
		svga_wcrt_mask(VGABASE, 0x6A, reg_bits, reg_bits);

		/* Before we fill the second LUT, we have to enable
		 * second display channel. If it's enabled before,
		 * we don't need to do that, or else the secondary
		 * display will be dark for about 1 sec and then be
		 * turned on again.
		 */
		if (!(vga_rcrt(VGABASE, 0x6A) & BIT(7)))
			enable_second_display_channel(VGABASE);

		/* Fill in IGA2's gamma */
		for (i = 0; i < end; i++) {
			/* bit mask of palette */
			vga_w(VGABASE, VGA_PEL_MSK, 0xFF);
			vga_w(VGABASE, VGA_PEL_IW, i);
			vga_w(VGABASE, VGA_PEL_D, r[i] >> 8);
			vga_w(VGABASE, VGA_PEL_D, g[i] >> 8);
			vga_w(VGABASE, VGA_PEL_D, b[i] >> 8);
		}
		/* access Primary Display's LUT */
		vga_wseq(VGABASE, 0x1A, sr1a);
	}

	return 0;
}

static void via_crtc_destroy(struct drm_crtc *crtc)
{
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);

	if (iga->cursor_kmap.bo) {
		via_bo_unpin(iga->cursor_kmap.bo, &iga->cursor_kmap);
		ttm_bo_unref(&iga->cursor_kmap.bo);
	}
	drm_crtc_cleanup(crtc);
}

static const struct drm_crtc_funcs via_iga1_funcs = {
	.cursor_set = via_crtc_cursor_set,
	.cursor_move = via_crtc_cursor_move,
	.gamma_set = via_iga1_gamma_set,
	.set_config = drm_crtc_helper_set_config,
	.destroy = via_crtc_destroy,
};

static const struct drm_crtc_funcs via_iga2_funcs = {
	.cursor_set = via_crtc_cursor_set,
	.cursor_move = via_crtc_cursor_move,
	.gamma_set = via_iga2_gamma_set,
	.set_config = drm_crtc_helper_set_config,
	.destroy = via_crtc_destroy,
};

static void via_load_vpit_regs(
			struct openchrome_drm_private *dev_private)
{
	u8 ar[] = {0x00, 0x01, 0x02, 0x03,
			0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0A, 0x0B,
			0x0C, 0x0D, 0x0E, 0x0F,
			0x01, 0x00, 0x0F, 0x00};
	struct vga_registers vpit_regs;
	unsigned int i = 0;
	u8 reg_value = 0;

	/* Enable changing the palette registers */
	reg_value = vga_r(VGABASE, VGA_IS1_RC);
	vga_w(VGABASE, VGA_ATT_W, 0x00);

	/* Write Misc register */
	vga_w(VGABASE, VGA_MIS_W, 0xCF);

	/* Fill VPIT registers */
	vpit_regs.count = ARRAY_SIZE(vpit_table);
	vpit_regs.regs = vpit_table;
	load_register_tables(VGABASE, &vpit_regs);

	/* Write Attribute Controller */
	for (i = 0; i < 0x14; i++) {
		reg_value = vga_r(VGABASE, VGA_IS1_RC);
		vga_w(VGABASE, VGA_ATT_W, i);
		vga_w(VGABASE, VGA_ATT_W, ar[i]);
	}

	/* Disable changing the palette registers */
	reg_value = vga_r(VGABASE, VGA_IS1_RC);
	vga_w(VGABASE, VGA_ATT_W, BIT(5));
}

static void via_iga1_display_fifo_regs(
			struct drm_device *dev,
			struct openchrome_drm_private *dev_private,
			struct via_crtc *iga,
			struct drm_display_mode *mode,
			struct drm_framebuffer *fb)
{
	u32 reg_value;
	unsigned int fifo_max_depth;
	unsigned int fifo_threshold;
	unsigned int fifo_high_threshold;
	unsigned int display_queue_expire_num;
	bool enable_extended_display_fifo = false;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_CLE266:
		if (dev_private->revision == CLE266_REVISION_AX) {
			if (mode->hdisplay > 1024) {
				/* SR17[6:0] */
				fifo_max_depth = 96;

				/* SR16[5:0] */
				fifo_threshold = 92;

				/* SR18[5:0] */
				fifo_high_threshold = 92;

				enable_extended_display_fifo = true;
			} else {
				/* SR17[6:0] */
				fifo_max_depth = 64;

				/* SR16[5:0] */
				fifo_threshold = 32;

				/* SR18[5:0] */
				fifo_high_threshold = 56;

				enable_extended_display_fifo = false;
			}

			if (dev_private->vram_type <= VIA_MEM_DDR_200) {
				if (fb->format->depth == 32) {
					if (mode->hdisplay > 1024) {
						if (mode->vdisplay > 768) {
							/* SR22[4:0] */
							display_queue_expire_num = 16;
						} else {
							/* SR22[4:0] */
							display_queue_expire_num = 12;
						}
					} else if (mode->hdisplay > 640) {
						/* SR22[4:0] */
						display_queue_expire_num = 40;
					} else {
						/* SR22[4:0] */
						display_queue_expire_num = 124;
					}
				} else if (fb->format->depth == 16){
					if (mode->hdisplay > 1400) {
						/* SR22[4:0] */
						display_queue_expire_num = 16;
					} else {
						/* SR22[4:0] */
						display_queue_expire_num = 12;
					}
				} else {
					/* SR22[4:0] */
					display_queue_expire_num = 124;
				}
			} else {
				if (mode->hdisplay > 1280) {
					/* SR22[4:0] */
					display_queue_expire_num = 16;
				} else if (mode->hdisplay > 1024) {
					/* SR22[4:0] */
					display_queue_expire_num = 12;
				} else {
					/* SR22[4:0] */
					display_queue_expire_num = 124;
				}
			}
		/* dev_private->revision == CLE266_REVISION_CX */
		} else {
			if (mode->hdisplay >= 1024) {
				/* SR17[6:0] */
				fifo_max_depth = 128;

				/* SR16[5:0] */
				fifo_threshold = 112;

				/* SR18[5:0] */
				fifo_high_threshold = 92;

				enable_extended_display_fifo = false;
			} else {
				/* SR17[6:0] */
				fifo_max_depth = 64;

				/* SR16[5:0] */
				fifo_threshold = 32;

				/* SR18[5:0] */
				fifo_high_threshold = 56;

				enable_extended_display_fifo = false;
			}

			if (dev_private->vram_type <= VIA_MEM_DDR_200) {
				if (mode->hdisplay > 1024) {
					if (mode->vdisplay > 768) {
						/* SR22[4:0] */
						display_queue_expire_num = 16;
					} else {
						/* SR22[4:0] */
						display_queue_expire_num = 12;
					}
				} else if (mode->hdisplay > 640) {
					/* SR22[4:0] */
					display_queue_expire_num = 40;
				} else {
					/* SR22[4:0] */
					display_queue_expire_num = 124;
				}
			} else {
				if (mode->hdisplay >= 1280) {
					/* SR22[4:0] */
					display_queue_expire_num = 16;
				} else {
					/* SR22[4:0] */
					display_queue_expire_num = 124;
				}
			}
		}
		break;

	case PCI_DEVICE_ID_VIA_KM400:
		if ((mode->hdisplay >= 1600) &&
			(dev_private->vram_type <= VIA_MEM_DDR_200)) {
			/* SR17[6:0] */
			fifo_max_depth = 58;

			/* SR16[5:0] */
			fifo_threshold = 24;

			/* SR18[5:0] */
			fifo_high_threshold = 92;
		} else {
			/* SR17[6:0] */
			fifo_max_depth = 128;

			/* SR16[5:0] */
			fifo_threshold = 112;

			/* SR18[5:0] */
			fifo_high_threshold = 92;
		}

		enable_extended_display_fifo = false;

		if (dev_private->vram_type <= VIA_MEM_DDR_200) {
			if (mode->hdisplay >= 1600) {
				/* SR22[4:0] */
				display_queue_expire_num = 16;
			} else {
				/* SR22[4:0] */
				display_queue_expire_num = 8;
			}
		} else {
			if (mode->hdisplay >= 1600) {
				/* SR22[4:0] */
				display_queue_expire_num = 40;
			} else {
				/* SR22[4:0] */
				display_queue_expire_num = 36;
			}
		}

		break;
	case PCI_DEVICE_ID_VIA_K8M800:
		/* SR17[7:0] */
		fifo_max_depth = 384;

		/* SR16[7], SR16[5:0] */
		fifo_threshold = 328;

		/* SR18[7], SR18[5:0] */
		fifo_high_threshold = 296;

		if ((fb->format->depth == 32) &&
			(mode->hdisplay >= 1400)) {
			/* SR22[4:0] */
			display_queue_expire_num = 64;
		} else {
			/* SR22[4:0] */
			display_queue_expire_num = 128;
		}

		break;
	case PCI_DEVICE_ID_VIA_PM800:
		/* SR17[7:0] */
		fifo_max_depth = 192;

		/* SR16[7], SR16[5:0] */
		fifo_threshold = 128;

		/* SR18[7], SR18[5:0] */
		fifo_high_threshold = 64;

		if ((fb->format->depth == 32) &&
			(mode->hdisplay >= 1400)) {
			/* SR22[4:0] */
			display_queue_expire_num = 64;
		} else {
			/* SR22[4:0] */
			display_queue_expire_num = 124;
		}

		break;
	case PCI_DEVICE_ID_VIA_CN700:
		/* SR17[7:0] */
		fifo_max_depth = CN700_IGA1_FIFO_MAX_DEPTH;

		/* SR16[7], SR16[5:0] */
		fifo_threshold = CN700_IGA1_FIFO_THRESHOLD;

		/* SR18[7], SR18[5:0] */
		fifo_high_threshold = CN700_IGA1_FIFO_HIGH_THRESHOLD;

		/* SR22[4:0] */
		display_queue_expire_num = CN700_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* CX700 */
	case PCI_DEVICE_ID_VIA_VT3157:
		/* SR17[7:0] */
		fifo_max_depth = CX700_IGA1_FIFO_MAX_DEPTH;

		/* SR16[7], SR16[5:0] */
		fifo_threshold = CX700_IGA1_FIFO_THRESHOLD;

		/* SR18[7], SR18[5:0] */
		fifo_high_threshold = CX700_IGA1_FIFO_HIGH_THRESHOLD;

		/* SR22[4:0] */
		display_queue_expire_num = CX700_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		break;

		/* K8M890 */
	case PCI_DEVICE_ID_VIA_K8M890:
		/* SR17[7:0] */
		fifo_max_depth = K8M890_IGA1_FIFO_MAX_DEPTH;

		/* SR16[7], SR16[5:0] */
		fifo_threshold = K8M890_IGA1_FIFO_THRESHOLD;

		/* SR18[7], SR18[5:0] */
		fifo_high_threshold = K8M890_IGA1_FIFO_HIGH_THRESHOLD;

		/* SR22[4:0] */
		display_queue_expire_num = K8M890_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* P4M890 */
	case PCI_DEVICE_ID_VIA_VT3343:
		/* SR17[7:0] */
		fifo_max_depth = P4M890_IGA1_FIFO_MAX_DEPTH;

		/* SR16[7], SR16[5:0] */
		fifo_threshold = P4M890_IGA1_FIFO_THRESHOLD;

		/* SR18[7], SR18[5:0] */
		fifo_high_threshold = P4M890_IGA1_FIFO_HIGH_THRESHOLD;

		/* SR22[4:0] */
		display_queue_expire_num = P4M890_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* P4M900 */
	case PCI_DEVICE_ID_VIA_P4M900:
		/* SR17[7:0] */
		fifo_max_depth = P4M900_IGA1_FIFO_MAX_DEPTH;

		/* SR16[7], SR16[5:0] */
		fifo_threshold = P4M900_IGA1_FIFO_THRESHOLD;

		/* SR18[7], SR18[5:0] */
		fifo_high_threshold = P4M900_IGA1_FIFO_HIGH_THRESHOLD;

		/* SR22[4:0] */
		display_queue_expire_num = P4M900_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* VX800 */
	case PCI_DEVICE_ID_VIA_VT1122:
		/* SR17[7:0] */
		fifo_max_depth = VX800_IGA1_FIFO_MAX_DEPTH;

		/* SR16[7], SR16[5:0] */
		fifo_threshold = VX800_IGA1_FIFO_THRESHOLD;

		/* SR18[7], SR18[5:0] */
		fifo_high_threshold = VX800_IGA1_FIFO_HIGH_THRESHOLD;

		/* SR22[4:0] */
		display_queue_expire_num = VX800_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* VX855 */
	case PCI_DEVICE_ID_VIA_VX875:
		/* SR17[7:0] */
		fifo_max_depth = VX855_IGA1_FIFO_MAX_DEPTH;

		/* SR16[7], SR16[5:0] */
		fifo_threshold = VX855_IGA1_FIFO_THRESHOLD;

		/* SR18[7], SR18[5:0] */
		fifo_high_threshold = VX855_IGA1_FIFO_HIGH_THRESHOLD;

		/* SR22[4:0] */
		display_queue_expire_num = VX855_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* VX900 */
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		/* SR17[7:0] */
		fifo_max_depth = VX900_IGA1_FIFO_MAX_DEPTH;

		/* SR16[7], SR16[5:0] */
		fifo_threshold = VX900_IGA1_FIFO_THRESHOLD;

		/* SR18[7], SR18[5:0] */
		fifo_high_threshold = VX900_IGA1_FIFO_HIGH_THRESHOLD;

		/* SR22[4:0] */
		display_queue_expire_num = VX900_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
	default:
		break;
	}

	if ((dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_KM400) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_K8M800) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_PM800) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_CN700) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_VT3157)) {
		/* Force PREQ to be always higher than TREQ. */
		svga_wseq_mask(VGABASE, 0x18, BIT(6), BIT(6));
	} else {
		svga_wseq_mask(VGABASE, 0x18, 0x00, BIT(6));
	}

	if ((dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_KM400)) {
		if (enable_extended_display_fifo) {
			reg_value = VIA_READ(0x0298);
			VIA_WRITE(0x0298, reg_value | 0x20000000);

			/* Turn on IGA1 extended display FIFO. */
			reg_value = VIA_READ(0x0230);
			VIA_WRITE(0x0230, reg_value | 0x00200000);

			reg_value = VIA_READ(0x0298);
			VIA_WRITE(0x0298, reg_value & (~0x20000000));
		} else {
			reg_value = VIA_READ(0x0298);
			VIA_WRITE(0x0298, reg_value | 0x20000000);

			/* Turn off IGA1 extended display FIFO. */
			reg_value = VIA_READ(0x0230);
			VIA_WRITE(0x0230, reg_value & (~0x00200000));

			reg_value = VIA_READ(0x0298);
			VIA_WRITE(0x0298, reg_value & (~0x20000000));

		}
	}

	/* Set IGA1 Display FIFO Depth Select */
	reg_value = IGA1_FIFO_DEPTH_SELECT_FORMULA(fifo_max_depth);
	load_value_to_registers(VGABASE, &iga->fifo_depth, reg_value);

	/* Set Display FIFO Threshold Select */
	reg_value = fifo_threshold / 4;
	load_value_to_registers(VGABASE, &iga->threshold, reg_value);

	/* Set FIFO High Threshold Select */
	reg_value = fifo_high_threshold / 4;
	load_value_to_registers(VGABASE, &iga->high_threshold, reg_value);

	/* Set Display Queue Expire Num */
	reg_value = display_queue_expire_num / 4;
	load_value_to_registers(VGABASE, &iga->display_queue, reg_value);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void via_iga2_display_fifo_regs(
			struct drm_device *dev,
			struct openchrome_drm_private *dev_private,
			struct via_crtc *iga,
			struct drm_display_mode *mode,
			struct drm_framebuffer *fb)
{
	u32 reg_value;
	unsigned int fifo_max_depth;
	unsigned int fifo_threshold;
	unsigned int fifo_high_threshold;
	unsigned int display_queue_expire_num;
	bool enable_extended_display_fifo = false;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_CLE266:
		if (dev_private->revision == CLE266_REVISION_AX) {
			if (((dev_private->vram_type <= VIA_MEM_DDR_200) &&
				(fb->format->depth > 16) &&
				(mode->vdisplay > 768)) ||
				((dev_private->vram_type <= VIA_MEM_DDR_266) &&
				(fb->format->depth > 16) &&
				(mode->hdisplay > 1280))) {
				/* CR68[7:4] */
				fifo_max_depth = 88;

				/* CR68[3:0] */
				fifo_threshold = 44;

				enable_extended_display_fifo = true;
			} else {
				/* CR68[7:4] */
				fifo_max_depth = 56;

				/* CR68[3:0] */
				fifo_threshold = 28;

				enable_extended_display_fifo = false;
			}
		/* dev_private->revision == CLE266_REVISION_CX */
		} else {
			if (mode->hdisplay >= 1024) {
				/* CR68[7:4] */
				fifo_max_depth = 88;

				/* CR68[3:0] */
				fifo_threshold = 44;

				enable_extended_display_fifo = false;
			} else {
				/* CR68[7:4] */
				fifo_max_depth = 56;

				/* CR68[3:0] */
				fifo_threshold = 28;

				enable_extended_display_fifo = false;
			}
		}

		fifo_high_threshold = 16;
		display_queue_expire_num = 16;
		break;
	case PCI_DEVICE_ID_VIA_KM400:
		if (mode->hdisplay >= 1600) {
			/* CR68[7:4] */
			fifo_max_depth = 120;

			/* CR68[3:0] */
			fifo_threshold = 44;

			enable_extended_display_fifo = true;
		} else if (((mode->hdisplay > 1024) &&
			(fb->format->depth == 32) &&
			(dev_private->vram_type <= VIA_MEM_DDR_333)) ||
			((mode->hdisplay == 1024) &&
			(fb->format->depth == 32) &&
			(dev_private->vram_type <= VIA_MEM_DDR_200))) {
			/* CR68[7:4] */
			fifo_max_depth = 104;

			/* CR68[3:0] */
			fifo_threshold = 28;

			enable_extended_display_fifo = true;
		} else if (((mode->hdisplay > 1280) &&
			(fb->format->depth == 16) &&
			(dev_private->vram_type <= VIA_MEM_DDR_333)) ||
			((mode->hdisplay == 1280) &&
			(fb->format->depth == 16) &&
			(dev_private->vram_type <= VIA_MEM_DDR_200))) {
			/* CR68[7:4] */
			fifo_max_depth = 88;

			/* CR68[3:0] */
			fifo_threshold = 44;

			enable_extended_display_fifo = true;
		} else {
			/* CR68[7:4] */
			fifo_max_depth = 56;

			/* CR68[3:0] */
			fifo_threshold = 28;

			enable_extended_display_fifo = false;
		}

		break;
	case PCI_DEVICE_ID_VIA_K8M800:
		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_max_depth = 376;

		/* CR95[6:4], CR68[3:0] */
		fifo_threshold = 328;

		/* CR95[2:0], CR92[3:0] */
		fifo_high_threshold = 296;

		if ((fb->format->depth == 32) &&
			(mode->hdisplay >= 1400)) {
			/* CR94[6:0] */
			display_queue_expire_num = 64;
		} else {
			/* CR94[6:0] */
			display_queue_expire_num = 128;
		}

		break;
	case PCI_DEVICE_ID_VIA_PM800:
		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_max_depth = 96;

		/* CR95[6:4], CR68[3:0] */
		fifo_threshold = 64;

		/* CR95[2:0], CR92[3:0] */
		fifo_high_threshold = 32;

		if ((fb->format->depth == 32) &&
				(mode->hdisplay >= 1400)) {
			/* CR94[6:0] */
			display_queue_expire_num = 64;
		} else {
			/* CR94[6:0] */
			display_queue_expire_num = 128;
		}

		break;
	case PCI_DEVICE_ID_VIA_CN700:
		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_max_depth = CN700_IGA2_FIFO_MAX_DEPTH;

		/* CR95[6:4], CR68[3:0] */
		fifo_threshold = CN700_IGA2_FIFO_THRESHOLD;

		/* CR95[2:0], CR92[3:0] */
		fifo_high_threshold = CN700_IGA2_FIFO_HIGH_THRESHOLD;

		/* CR94[6:0] */
		display_queue_expire_num = CN700_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* CX700 */
	case PCI_DEVICE_ID_VIA_VT3157:
		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_max_depth = CX700_IGA2_FIFO_MAX_DEPTH;

		/* CR95[6:4], CR68[3:0] */
		fifo_threshold = CX700_IGA2_FIFO_THRESHOLD;

		/* CR95[2:0], CR92[3:0] */
		fifo_high_threshold = CX700_IGA2_FIFO_HIGH_THRESHOLD;

		/* CR94[6:0] */
		display_queue_expire_num = CX700_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		break;

		/* K8M890 */
	case PCI_DEVICE_ID_VIA_K8M890:
		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_max_depth = K8M890_IGA2_FIFO_MAX_DEPTH;

		/* CR95[6:4], CR68[3:0] */
		fifo_threshold = K8M890_IGA2_FIFO_THRESHOLD;

		/* CR95[2:0], CR92[3:0] */
		fifo_high_threshold = K8M890_IGA2_FIFO_HIGH_THRESHOLD;

		/* CR94[6:0] */
		display_queue_expire_num = K8M890_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* P4M890 */
	case PCI_DEVICE_ID_VIA_VT3343:
		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_max_depth = P4M890_IGA2_FIFO_MAX_DEPTH;

		/* CR95[6:4], CR68[3:0] */
		fifo_threshold = P4M890_IGA2_FIFO_THRESHOLD;

		/* CR95[2:0], CR92[3:0] */
		fifo_high_threshold = P4M890_IGA2_FIFO_HIGH_THRESHOLD;

		/* CR94[6:0] */
		display_queue_expire_num = P4M890_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* P4M900 */
	case PCI_DEVICE_ID_VIA_P4M900:
		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_max_depth = P4M900_IGA2_FIFO_MAX_DEPTH;

		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_threshold = P4M900_IGA2_FIFO_THRESHOLD;

		/* CR95[2:0], CR92[3:0] */
		fifo_high_threshold = P4M900_IGA2_FIFO_HIGH_THRESHOLD;

		/* CR94[6:0] */
		display_queue_expire_num = P4M900_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* VX800 */
	case PCI_DEVICE_ID_VIA_VT1122:
		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_max_depth = VX800_IGA2_FIFO_MAX_DEPTH;

		/* CR95[6:4], CR68[3:0] */
		fifo_threshold = VX800_IGA2_FIFO_THRESHOLD;

		/* CR95[2:0], CR92[3:0] */
		fifo_high_threshold = VX800_IGA2_FIFO_HIGH_THRESHOLD;

		/* CR94[6:0] */
		display_queue_expire_num = VX800_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* VX855 */
	case PCI_DEVICE_ID_VIA_VX875:
		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_max_depth = VX855_IGA2_FIFO_MAX_DEPTH;

		/* CR95[6:4], CR68[3:0] */
		fifo_threshold = VX855_IGA2_FIFO_THRESHOLD;

		/* CR95[2:0], CR92[3:0] */
		fifo_high_threshold = VX855_IGA2_FIFO_HIGH_THRESHOLD;

		/* CR94[6:0] */
		display_queue_expire_num = VX855_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
		/* VX900 */
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		/* CR95[7], CR94[7], CR68[7:4] */
		fifo_max_depth = VX900_IGA2_FIFO_MAX_DEPTH;

		/* CR95[6:4], CR68[3:0] */
		fifo_threshold = VX900_IGA2_FIFO_THRESHOLD;

		/* CR95[2:0], CR92[3:0] */
		fifo_high_threshold = VX900_IGA2_FIFO_HIGH_THRESHOLD;

		/* CR94[6:0] */
		display_queue_expire_num = VX900_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		break;
	default:
		break;
	}

	if ((dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_KM400)) {
		if (enable_extended_display_fifo) {
			/* Enable IGA2 extended display FIFO. */
			svga_wcrt_mask(VGABASE, 0x6a, BIT(5), BIT(5));
		} else {
			/* Disable IGA2 extended display FIFO. */
			svga_wcrt_mask(VGABASE, 0x6a, 0x00, BIT(5));
		}
	}

	if ((dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_KM400)) {
		/* Set IGA2 Display FIFO Depth Select */
		reg_value = IGA2_FIFO_DEPTH_SELECT_FORMULA(fifo_max_depth);
		load_value_to_registers(VGABASE, &iga->fifo_depth, reg_value);

		/* Set Display FIFO Threshold Select */
		reg_value = fifo_threshold / 4;
		load_value_to_registers(VGABASE, &iga->threshold, reg_value);
	} else {
		/* Set IGA2 Display FIFO Depth Select */
		reg_value = IGA2_FIFO_DEPTH_SELECT_FORMULA(fifo_max_depth);
		load_value_to_registers(VGABASE, &iga->fifo_depth, reg_value);

		/* Set Display FIFO Threshold Select */
		reg_value = fifo_threshold / 4;
		load_value_to_registers(VGABASE, &iga->threshold, reg_value);

		/* Set FIFO High Threshold Select */
		reg_value = fifo_high_threshold / 4;
		load_value_to_registers(VGABASE, &iga->high_threshold, reg_value);

		/* Set Display Queue Expire Num */
		reg_value = display_queue_expire_num / 4;
		load_value_to_registers(VGABASE, &iga->display_queue, reg_value);
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

/* Load CRTC Pixel Timing registers */
void
via_load_crtc_pixel_timing(struct drm_crtc *crtc,
				struct drm_display_mode *mode)
{
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;
	u32 reg_value = 0;

	reg_value = IGA1_PIXELTIMING_HOR_TOTAL_FORMULA(mode->crtc_htotal);
	load_value_to_registers(VGABASE, &iga->pixel_timings.htotal,
				reg_value);

	reg_value = IGA1_PIXELTIMING_HOR_ADDR_FORMULA(mode->crtc_hdisplay) << 16;
	load_value_to_registers(VGABASE, &iga->pixel_timings.hdisplay,
				reg_value);

	reg_value = IGA1_PIXELTIMING_HOR_BLANK_START_FORMULA(
					mode->crtc_hblank_start);
	load_value_to_registers(VGABASE, &iga->pixel_timings.hblank_start,
				reg_value);

	reg_value = IGA1_PIXELTIMING_HOR_BLANK_END_FORMULA(mode->crtc_hblank_end) << 16;
	load_value_to_registers(VGABASE, &iga->pixel_timings.hblank_end, reg_value);

	reg_value = IGA1_PIXELTIMING_HOR_SYNC_START_FORMULA(mode->crtc_hsync_start);
	load_value_to_registers(VGABASE, &iga->pixel_timings.hsync_start,
				reg_value);

	reg_value = IGA1_PIXELTIMING_HOR_SYNC_END_FORMULA(mode->crtc_hsync_end) << 16;
	load_value_to_registers(VGABASE, &iga->pixel_timings.hsync_end, reg_value);

	reg_value = IGA1_PIXELTIMING_VER_TOTAL_FORMULA(mode->crtc_vtotal);
	load_value_to_registers(VGABASE, &iga->pixel_timings.vtotal, reg_value);

	reg_value = IGA1_PIXELTIMING_VER_ADDR_FORMULA(mode->crtc_vdisplay) << 16;
	load_value_to_registers(VGABASE, &iga->pixel_timings.vdisplay, reg_value);

	reg_value = IGA1_PIXELTIMING_VER_BLANK_START_FORMULA(
					mode->crtc_vblank_start);
	load_value_to_registers(VGABASE, &iga->pixel_timings.vblank_start, reg_value);

	reg_value = IGA1_PIXELTIMING_VER_BLANK_END_FORMULA(mode->crtc_vblank_end) << 16;
	load_value_to_registers(VGABASE, &iga->pixel_timings.vblank_end, reg_value);

	reg_value = IGA1_PIXELTIMING_VER_SYNC_START_FORMULA(mode->crtc_vsync_start);
	load_value_to_registers(VGABASE, &iga->pixel_timings.vsync_start, reg_value);

	reg_value = IGA1_PIXELTIMING_VER_SYNC_END_FORMULA(mode->crtc_vsync_end) << 12;
	load_value_to_registers(VGABASE, &iga->pixel_timings.vsync_end, reg_value);

	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		reg_value = IGA1_PIXELTIMING_HVSYNC_OFFSET_END_FORMULA(
				mode->crtc_htotal, mode->crtc_hsync_start);
		VIA_WRITE_MASK(IGA1_PIX_HALF_LINE_REG, reg_value,
					IGA1_PIX_HALF_LINE_MASK);

		svga_wcrt_mask(VGABASE, 0x32, BIT(2), BIT(2));
		/**
		 * According to information from HW team,
		 * we need to set 0xC280[1] = 1 (HDMI function enable)
		 * or 0xC640[0] = 1 (DP1 enable)
		 * to let the half line function work.
		 * Otherwise, the clock for interlace mode
		 * will not correct.
		 * This is a special setting for 410.
		 */
		VIA_WRITE_MASK(0xC280, BIT(1), BIT(1));
	} else {
		VIA_WRITE_MASK(IGA1_PIX_HALF_LINE_REG, 0x0, IGA1_PIX_HALF_LINE_MASK);
		svga_wcrt_mask(VGABASE, 0x32, 0x00, BIT(2));

	}
	svga_wcrt_mask(VGABASE, 0xFD, BIT(5), BIT(5));
}

/* Load CRTC timing registers */
void
via_load_crtc_timing(struct via_crtc *iga, struct drm_display_mode *mode)
{
	struct openchrome_drm_private *dev_private =
					iga->base.dev->dev_private;
	struct drm_device *dev = iga->base.dev;
	u32 reg_value = 0;

	if (!iga->index) {
		if (dev->pdev->device == PCI_DEVICE_ID_VIA_VX900_VGA) {
			/* Disable IGA1 shadow timing */
			svga_wcrt_mask(VGABASE, 0x45, 0x00, BIT(0));

			/* Disable IGA1 pixel timing */
			svga_wcrt_mask(VGABASE, 0xFD, 0x00, BIT(5));
		}

		reg_value = IGA1_HOR_TOTAL_FORMULA(mode->crtc_htotal);
		load_value_to_registers(VGABASE, &iga->timings.htotal, reg_value);

		reg_value = IGA1_HOR_ADDR_FORMULA(mode->crtc_hdisplay);
		load_value_to_registers(VGABASE, &iga->timings.hdisplay, reg_value);

		reg_value = IGA1_HOR_BLANK_START_FORMULA(mode->crtc_hblank_start);
		load_value_to_registers(VGABASE, &iga->timings.hblank_start, reg_value);

		reg_value = IGA1_HOR_BLANK_END_FORMULA(mode->crtc_hblank_end);
		load_value_to_registers(VGABASE, &iga->timings.hblank_end, reg_value);

		reg_value = IGA1_HOR_SYNC_START_FORMULA(mode->crtc_hsync_start);
		load_value_to_registers(VGABASE, &iga->timings.hsync_start, reg_value);

		reg_value = IGA1_HOR_SYNC_END_FORMULA(mode->crtc_hsync_end);
		load_value_to_registers(VGABASE, &iga->timings.hsync_end, reg_value);

		reg_value = IGA1_VER_TOTAL_FORMULA(mode->crtc_vtotal);
		load_value_to_registers(VGABASE, &iga->timings.vtotal, reg_value);

		reg_value = IGA1_VER_ADDR_FORMULA(mode->crtc_vdisplay);
		load_value_to_registers(VGABASE, &iga->timings.vdisplay, reg_value);

		reg_value = IGA1_VER_BLANK_START_FORMULA(mode->crtc_vblank_start);
		load_value_to_registers(VGABASE, &iga->timings.vblank_start, reg_value);

		reg_value = IGA1_VER_BLANK_END_FORMULA(mode->crtc_vblank_end);
		load_value_to_registers(VGABASE, &iga->timings.vblank_end, reg_value);

		reg_value = IGA1_VER_SYNC_START_FORMULA(mode->crtc_vsync_start);
		load_value_to_registers(VGABASE, &iga->timings.vsync_start, reg_value);

		reg_value = IGA1_VER_SYNC_END_FORMULA(mode->crtc_vsync_end);
		load_value_to_registers(VGABASE, &iga->timings.vsync_end, reg_value);
	} else {
		reg_value = IGA2_HOR_TOTAL_FORMULA(mode->crtc_htotal);
		load_value_to_registers(VGABASE, &iga->timings.htotal, reg_value);

		reg_value = IGA2_HOR_ADDR_FORMULA(mode->crtc_hdisplay);
		load_value_to_registers(VGABASE, &iga->timings.hdisplay, reg_value);

		reg_value = IGA2_HOR_BLANK_START_FORMULA(mode->crtc_hblank_start);
		load_value_to_registers(VGABASE, &iga->timings.hblank_start, reg_value);

		reg_value = IGA2_HOR_BLANK_END_FORMULA(mode->crtc_hblank_end);
		load_value_to_registers(VGABASE, &iga->timings.hblank_end, reg_value);

		reg_value = IGA2_HOR_SYNC_START_FORMULA(mode->crtc_hsync_start);
		load_value_to_registers(VGABASE, &iga->timings.hsync_start, reg_value);

		reg_value = IGA2_HOR_SYNC_END_FORMULA(mode->crtc_hsync_end);
		load_value_to_registers(VGABASE, &iga->timings.hsync_end, reg_value);

		reg_value = IGA2_VER_TOTAL_FORMULA(mode->crtc_vtotal);
		load_value_to_registers(VGABASE, &iga->timings.vtotal, reg_value);

		reg_value = IGA2_VER_ADDR_FORMULA(mode->crtc_vdisplay);
		load_value_to_registers(VGABASE, &iga->timings.vdisplay, reg_value);

		reg_value = IGA2_VER_BLANK_START_FORMULA(mode->crtc_vblank_start);
		load_value_to_registers(VGABASE, &iga->timings.vblank_start, reg_value);

		reg_value = IGA2_VER_BLANK_END_FORMULA(mode->crtc_vblank_end);
		load_value_to_registers(VGABASE, &iga->timings.vblank_end, reg_value);

		reg_value = IGA2_VER_SYNC_START_FORMULA(mode->crtc_vsync_start);
		load_value_to_registers(VGABASE, &iga->timings.vsync_start, reg_value);

		reg_value = IGA2_VER_SYNC_END_FORMULA(mode->crtc_vsync_end);
		load_value_to_registers(VGABASE, &iga->timings.vsync_end, reg_value);
	}
}

/*
 * This function changes the destination of scaling up/down
 * and CRTC timing registers
 * crtc : which IGA
 * scale_type : upscaling(VIA_EXPAND) or downscaling(VIA_SHRINK)
 */
void
via_set_scale_path(struct drm_crtc *crtc, u32 scale_type)
{
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;
	u8 reg_cr_fd = vga_rcrt(VGABASE, 0xFD);
	struct drm_device *dev = crtc->dev;

	if (!iga->index)
		/* register reuse: select IGA1 path */
		reg_cr_fd |= BIT(7);
	else
		/* register reuse: select IGA2 path */
		reg_cr_fd &= ~BIT(7);

	/* only IGA1 up scaling need to clear this bit CRFD.5. */
	if (dev->pdev->device == PCI_DEVICE_ID_VIA_VX900_VGA) {
		if (!iga->index
			&& ((VIA_HOR_EXPAND & scale_type)
			|| (VIA_VER_EXPAND & scale_type)))
			reg_cr_fd &= ~BIT(5);
	}

	/* CRFD.0 = 0 : common IGA2, = 1 : downscaling IGA */
	switch (scale_type) {
	case VIA_NO_SCALING:
	case VIA_EXPAND:
	case VIA_HOR_EXPAND:
	case VIA_VER_EXPAND:
		/* register reuse: as common IGA2 */
		reg_cr_fd &= ~BIT(0);
		break;

	case VIA_SHRINK:
		/* register reuse: as downscaling IGA */
		reg_cr_fd |= BIT(0);
		break;

	default:
		break;
	}
	vga_wcrt(VGABASE, 0xFD, reg_cr_fd);
}

/* disable IGA scaling */
static void
via_disable_iga_scaling(struct drm_crtc *crtc)
{
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;

	if (iga->index) {
		/* IGA2 scalings disable */
		via_set_scale_path(crtc, VIA_SHRINK);
		/* disable IGA down scaling and buffer sharing. */
		svga_wcrt_mask(VGABASE, 0x89, 0x00, BIT(7) | BIT(0));
		/* Horizontal and Vertical scaling disable */
		svga_wcrt_mask(VGABASE, 0xA2, 0x00, BIT(7) | BIT(3));

		/* Disable scale up as well */
		via_set_scale_path(crtc, VIA_EXPAND);
		/* disable IGA up scaling */
		svga_wcrt_mask(VGABASE, 0x79, 0, BIT(0));
		/* Horizontal and Vertical scaling disable */
		svga_wcrt_mask(VGABASE, 0xA2, 0x00, BIT(7) | BIT(3));
	} else {
		/* IGA1 scalings disable */
		via_set_scale_path(crtc, VIA_SHRINK);
		/* disable IGA down scaling and buffer sharing. */
		svga_wcrt_mask(VGABASE, 0x89, 0x00, BIT(7) | BIT(0));
		/* Horizontal and Vertical scaling disable */
		svga_wcrt_mask(VGABASE, 0xA2, 0x00, BIT(7) | BIT(3));

		/* Disable scale up as well */
		via_set_scale_path(crtc, VIA_EXPAND);
		/* disable IGA up scaling */
		svga_wcrt_mask(VGABASE, 0x79, 0, BIT(0));
		/* Horizontal and Vertical scaling disable */
		svga_wcrt_mask(VGABASE, 0xA2, 0x00, BIT(7) | BIT(3));
	}
}

/*
 * Enable IGA scale functions.
 *
 * input : iga_path =	IGA1 or IGA2 or
 *			IGA1+IGA2
 *
 * scale_type	=	VIA_HOR_EXPAND or VIA_VER_EXPAND or VIA_EXPAND or
 *			VIA_SHRINK or VIA_SHRINK + VIA_EXPAND
 */
bool
via_set_iga_scale_function(struct drm_crtc *crtc, u32 scale_type)
{
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;

	if (!(scale_type & (VIA_SHRINK + VIA_EXPAND)))
		return false;

	if (iga->index) {
		/* IGA2 scalings enable */
		if (VIA_SHRINK & scale_type) {
			via_set_scale_path(crtc, VIA_SHRINK);
			/* Horizontal and Vertical scaling enable */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(7) | BIT(3), BIT(7) | BIT(3));
			/* enable IGA down scaling */
			svga_wcrt_mask(VGABASE, 0x89, BIT(0), BIT(0));
			/* hor and ver scaling : Interpolation */
			svga_wcrt_mask(VGABASE, 0x79, BIT(2) | BIT(1), BIT(2) | BIT(1));
		}

		if (VIA_EXPAND & scale_type) {
			via_set_scale_path(crtc, VIA_EXPAND);
			/* enable IGA up scaling */
			svga_wcrt_mask(VGABASE, 0x79, BIT(0), BIT(0));
		}

		if ((VIA_EXPAND & scale_type) == VIA_EXPAND) {
			/* Horizontal and Vertical scaling enable */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(7) | BIT(3), BIT(7) | BIT(3));
			/* hor and ver scaling : Interpolation */
			svga_wcrt_mask(VGABASE, 0x79, BIT(2) | BIT(1), BIT(2) | BIT(1));
		} else if (VIA_HOR_EXPAND & scale_type) {
			/* Horizontal scaling disable */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(7), BIT(7));
			/* hor scaling : Interpolation */
			svga_wcrt_mask(VGABASE, 0x79, BIT(1), BIT(1));
		} else if (VIA_VER_EXPAND & scale_type) {
			/* Vertical scaling disable */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(3), BIT(3));
			/* ver scaling : Interpolation */
			svga_wcrt_mask(VGABASE, 0x79, BIT(2), BIT(2));
		}
	} else {
		/* IGA1 scalings enable */
		if (VIA_SHRINK & scale_type) {
			via_set_scale_path(crtc, VIA_SHRINK);

			/* Horizontal and Vertical scaling enable */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(7) | BIT(3), BIT(7) | BIT(3));
			/* enable IGA down scaling */
			svga_wcrt_mask(VGABASE, 0x89, BIT(0), BIT(0));
			/* hor and ver scaling : Interpolation */
			svga_wcrt_mask(VGABASE, 0x79, BIT(2) | BIT(1), BIT(2) | BIT(1));
		}

		if (VIA_EXPAND & scale_type) {
			via_set_scale_path(crtc, VIA_EXPAND);
			/* enable IGA up scaling */
			svga_wcrt_mask(VGABASE, 0x79, BIT(0), BIT(0));
		}

		if ((VIA_EXPAND & scale_type) == VIA_EXPAND) {
			/* Horizontal and Vertical scaling enable */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(7) | BIT(3), BIT(7) | BIT(3));
			/* hor and ver scaling : Interpolation */
			svga_wcrt_mask(VGABASE, 0x79, BIT(2) | BIT(1), BIT(2) | BIT(1));
		} else if (VIA_HOR_EXPAND & scale_type) {
			/* Horizontal scaling disable */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(7), BIT(7));
			/* hor scaling : Interpolation */
			svga_wcrt_mask(VGABASE, 0x79, BIT(1), BIT(1));
		} else if (VIA_VER_EXPAND & scale_type) {
			/* Vertical scaling disable */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(3), BIT(3));
			/* ver scaling : Interpolation */
			svga_wcrt_mask(VGABASE, 0x79, BIT(2), BIT(2));
		}
	}
	return true;
}

/*
 * 1. get scale factors from source and dest H & V size
 * 2. load scale factors into registers
 * 3. enable H or V scale ( set CRA2 bit7 or bit3 )
 */
bool via_load_iga_scale_factor_regs(
			struct openchrome_drm_private *dev_private,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode,
			u32 scale_type, u32 is_hor_or_ver)
{
	u32 dst_hor_regs = adjusted_mode->crtc_hdisplay;
	u32 dst_ver_regs = adjusted_mode->crtc_vdisplay;
	u32 src_hor_regs = mode->crtc_hdisplay;
	u32 src_ver_regs = mode->crtc_vdisplay;
	u32 hor_factor = 0, ver_factor = 0;
	struct vga_registers reg;

	if ((0 == src_hor_regs) || (0 == src_ver_regs) || (0 == dst_hor_regs)
			|| (0 == dst_ver_regs))
		return false;

	if (VIA_EXPAND == scale_type) {
		if (HOR_SCALE & is_hor_or_ver) {
			hor_factor = ((src_hor_regs - 1) * 4096) / (dst_hor_regs - 1);
			reg.count = ARRAY_SIZE(lcd_hor_scaling);
			reg.regs = lcd_hor_scaling;
			load_value_to_registers(VGABASE, &reg, hor_factor);
			/* Horizontal scaling enable */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(7), BIT(7));
		}

		if (VER_SCALE & is_hor_or_ver) {
			ver_factor = ((src_ver_regs - 1) * 2048) / (dst_ver_regs - 1);
			reg.count = ARRAY_SIZE(lcd_ver_scaling);
			reg.regs = lcd_ver_scaling;
			load_value_to_registers(VGABASE, &reg, ver_factor);
			/* Vertical scaling enable */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(3), BIT(3));
		}

	} else if (VIA_SHRINK == scale_type) {

		if (src_hor_regs > dst_hor_regs)
			hor_factor = ((src_hor_regs - dst_hor_regs) * 4096) / dst_hor_regs;

		if (src_ver_regs > dst_ver_regs)
			ver_factor = ((src_ver_regs - dst_ver_regs) * 2048) / dst_ver_regs;

		reg.count = ARRAY_SIZE(lcd_hor_scaling);
		reg.regs = lcd_hor_scaling;
		load_value_to_registers(VGABASE, &reg, hor_factor);

		reg.count = ARRAY_SIZE(lcd_ver_scaling);
		reg.regs = lcd_ver_scaling;
		load_value_to_registers(VGABASE, &reg, ver_factor);

		/* set buffer sharing enable bit . */
		if (hor_factor || ver_factor) {
			if (dst_hor_regs > 1024)
				svga_wcrt_mask(VGABASE, 0x89, BIT(7), BIT(7));
			else
				svga_wcrt_mask(VGABASE, 0x89, 0x00, BIT(7));
		}

		if (hor_factor)
			/* CRA2[7]:1 Enable Hor scaling
			   CRA2[6]:1 Linear Mode */
			svga_wcrt_mask(VGABASE, 0xA2, BIT(7) | BIT(6), BIT(7) | BIT(6));
		else
			svga_wcrt_mask(VGABASE, 0xA2, 0, BIT(7));

		if (ver_factor)
			svga_wcrt_mask(VGABASE, 0xA2, BIT(3), BIT(3));
		else
			svga_wcrt_mask(VGABASE, 0xA2, 0, BIT(3));
	}
	return true;
}

void
via_set_iga2_downscale_source_timing(struct drm_crtc *crtc,
					struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	unsigned int viewx = adjusted_mode->hdisplay,
			viewy = adjusted_mode->vdisplay;
	unsigned int srcx = mode->crtc_hdisplay, srcy = mode->crtc_vdisplay;
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct drm_display_mode *src_timing;

	src_timing = drm_mode_duplicate(crtc->dev, adjusted_mode);
	/* derived source timing */
	if (srcx <= viewx) {
		src_timing->crtc_htotal = adjusted_mode->crtc_htotal;
		src_timing->crtc_hdisplay = adjusted_mode->crtc_hdisplay;
	} else {
		unsigned int htotal = adjusted_mode->crtc_htotal -
					adjusted_mode->crtc_hdisplay;

		src_timing->crtc_htotal = htotal + srcx;
		src_timing->crtc_hdisplay = srcx;
	}
	src_timing->crtc_hblank_start = src_timing->crtc_hdisplay;
	src_timing->crtc_hblank_end = src_timing->crtc_htotal;
	src_timing->crtc_hsync_start = src_timing->crtc_hdisplay + 2;
	src_timing->crtc_hsync_end = src_timing->crtc_hsync_start + 1;

	if (srcy <= viewy) {
		src_timing->crtc_vtotal = adjusted_mode->crtc_vtotal;
		src_timing->crtc_vdisplay = adjusted_mode->crtc_vdisplay;
	} else {
		unsigned int vtotal = adjusted_mode->crtc_vtotal -
					adjusted_mode->crtc_vdisplay;

		src_timing->crtc_vtotal = vtotal + srcy;
		src_timing->crtc_vdisplay = srcy;
	}
	src_timing->crtc_vblank_start = src_timing->crtc_vdisplay;
	src_timing->crtc_vblank_end = src_timing->crtc_vtotal;
	src_timing->crtc_vsync_start = src_timing->crtc_vdisplay + 2;
	src_timing->crtc_vsync_end = src_timing->crtc_vsync_start + 1;

	via_set_scale_path(crtc, VIA_NO_SCALING);
	/* load src timing */
	via_load_crtc_timing(iga, src_timing);

	/* Cleanup up source timings */
	drm_mode_destroy(crtc->dev, src_timing);
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
				crtc->gamma_size, NULL);
	}
}

static void
via_iga1_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	switch (mode) {
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_OFF:
		/* turn off CRT screen (IGA1) */
		svga_wseq_mask(VGABASE, 0x01, BIT(5), BIT(5));

		/* clear for TV clock */
		svga_wcrt_mask(VGABASE, 0x6C, 0x00, 0xF0);
		break;

	case DRM_MODE_DPMS_ON:
		/* turn on CRT screen (IGA1) */
		svga_wseq_mask(VGABASE, 0x01, 0x00, BIT(5));

		/* disable simultaneous  */
		svga_wcrt_mask(VGABASE, 0x6B, 0x00, BIT(3));
		drm_mode_crtc_load_lut(crtc);
		break;
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void
via_iga1_crtc_disable(struct drm_crtc *crtc)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Turn off the cursor */
	via_hide_cursor(crtc);

	via_iga1_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void
via_iga1_crtc_prepare(struct drm_crtc *crtc)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Blank the screen */
	if (crtc->enabled)
		via_iga1_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void
via_iga1_crtc_commit(struct drm_crtc *crtc)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Turn on the monitor */
	if (crtc->enabled)
		via_iga1_crtc_dpms(crtc, DRM_MODE_DPMS_ON);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static bool
via_iga1_crtc_mode_fixup(struct drm_crtc *crtc,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return true;
}

static int
via_iga1_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
				struct drm_framebuffer *old_fb)
{
	struct ttm_buffer_object *bo;
	struct via_framebuffer *via_fb = container_of(crtc->primary->fb,
					struct via_framebuffer, fb);
	struct drm_framebuffer *new_fb = &via_fb->fb;
	struct drm_gem_object *gem_obj = via_fb->gem_obj;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* No FB found. */
	if (!new_fb) {
		ret = -ENOMEM;
		DRM_DEBUG_KMS("No FB found.\n");
		goto exit;
	}

	gem_obj = via_fb->gem_obj;
	bo = ttm_gem_mapping(gem_obj);

	ret = via_bo_pin(bo, NULL);
	if (unlikely(ret)) {
		DRM_DEBUG_KMS("Failed to pin FB.\n");
		goto exit;
	}

	ret = crtc->helper_private->mode_set_base_atomic(crtc,
						new_fb, x, y,
						ENTER_ATOMIC_MODE_SET);
	if (unlikely(ret)) {
		DRM_DEBUG_KMS("Failed to set a new FB.\n");
		via_bo_unpin(bo, NULL);
		goto exit;
	}

	/* Free the old framebuffer if it exist */
	if (old_fb) {
		via_fb = container_of(old_fb,
					struct via_framebuffer, fb);
		gem_obj = via_fb->gem_obj;
		bo = ttm_gem_mapping(gem_obj);

		ret = via_bo_unpin(bo, NULL);
		if (unlikely(ret)) {
			DRM_DEBUG_KMS("FB still locked.\n");
			goto exit;
		}
	}

exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int
via_iga1_crtc_mode_set(struct drm_crtc *crtc,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode,
			int x, int y, struct drm_framebuffer *fb)
{
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;
	struct drm_device *dev = crtc->dev;
	u8 reg_value = 0;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Check for IGA2. */
	if (iga->index) {
		DRM_ERROR("Wrong display controller (CRTC) selection.");
		ret = -EINVAL;
		goto exit;
	}

	/* Load standard registers */
	via_load_vpit_regs(dev_private);

	/* Unlock */
	via_unlock_crtc(VGABASE, dev->pdev->device);

	/* IGA1 reset */
	vga_wcrt(VGABASE, 0x09, 0x00); /* initial CR09=0 */
	svga_wcrt_mask(VGABASE, 0x11, 0x00, BIT(6));

	/* disable IGA scales first */
	via_disable_iga_scaling(crtc);

	/* when not down scaling, we only need load one timing. */
	via_load_crtc_timing(iga, adjusted_mode);

	switch (adjusted_mode->crtc_htotal % 8) {
	case 0:
	default:
		break;
	case 2:
		reg_value = BIT(7);
		break;
	case 4:
		reg_value = BIT(6);
		break;
	case 6:
		reg_value = BIT(3);
		break;
	}

	svga_wcrt_mask(VGABASE, 0x47, reg_value, BIT(7) | BIT(6) | BIT(3));

	/* Relock */
	via_lock_crtc(VGABASE);

	/* Set non-interlace / interlace mode. */
	via_iga1_set_interlace_mode(VGABASE,
			adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE);

	/* No HSYNC shift. */
	via_iga1_set_hsync_shift(VGABASE, 0x05);

	/* Load display FIFO. */
	via_iga1_display_fifo_regs(dev, dev_private, iga,
				adjusted_mode, crtc->primary->fb);

	/* Set PLL */
	if (adjusted_mode->clock) {
		u32 clock = adjusted_mode->clock * 1000, pll_regs;

		if (iga->scaling_mode & VIA_SHRINK)
			clock *= 2;
		pll_regs = via_get_clk_value(crtc->dev, clock);
		via_set_vclock(crtc, pll_regs);
	}

	ret = via_iga1_crtc_mode_set_base(crtc, x, y, fb);

	/* Specify the cursor pattern. */
	via_cursor_address(crtc);

	/* Turn off the cursor. */
	via_hide_cursor(crtc);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int
via_iga1_mode_set_base_atomic(struct drm_crtc *crtc,
				struct drm_framebuffer *fb, int x, int y,
				enum mode_set_atomic state)
{
	u32 pitch = y * fb->pitches[0] +
			((x * fb->format->cpp[0] * 8) >> 3), addr;
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;
	struct via_framebuffer *via_fb = container_of(fb,
					struct via_framebuffer, fb);
	struct drm_gem_object *gem_obj = via_fb->gem_obj;
	struct ttm_buffer_object *bo = ttm_gem_mapping(gem_obj);

	if ((fb->format->depth != 8) && (fb->format->depth != 16) &&
		(fb->format->depth != 24) && (fb->format->depth != 32)) {
		DRM_ERROR("Unsupported IGA1 Color Depth: %d bit\n",
						fb->format->depth);
		return -EINVAL;
	}

	via_iga_common_init(VGABASE);

	/* Set palette LUT to 8-bit mode. */
	via_iga1_set_palette_lut_resolution(VGABASE, true);

	via_iga1_set_color_depth(dev_private, fb->format->depth);

	/* Set the framebuffer offset */
	addr = round_up(bo->offset + pitch, 16) >> 1;
	vga_wcrt(VGABASE, 0x0D, addr & 0xFF);
	vga_wcrt(VGABASE, 0x0C, (addr >> 8) & 0xFF);
	/* Yes order of setting these registers matters on some hardware */
	svga_wcrt_mask(VGABASE, 0x48, ((addr >> 24) & 0x1F), 0x1F);
	vga_wcrt(VGABASE, 0x34, (addr >> 16) & 0xFF);

	/* Load fetch count registers */
	pitch = ALIGN(crtc->mode.hdisplay * (fb->format->cpp[0] * 8) >> 3, 16);
	load_value_to_registers(VGABASE, &iga->fetch, pitch >> 4);

	/* Set the primary pitch */
	pitch = ALIGN(fb->pitches[0], 16);
	/* Spec does not say that first adapter skips 3 bits but old
	 * code did it and seems to be reasonable in analogy to
	 * second adapter */
	load_value_to_registers(VGABASE, &iga->offset, pitch >> 3);

	return 0;
}

static void
via_iga2_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	switch (mode) {
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_OFF:
		/* turn off CRT screen (IGA2) */
		svga_wcrt_mask(VGABASE, 0x6B, BIT(2), BIT(2));

		/* clear for TV clock */
		svga_wcrt_mask(VGABASE, 0x6C, 0x00, 0x0F);
		break;

	case DRM_MODE_DPMS_ON:
		/* turn on CRT screen (IGA2) */
		svga_wcrt_mask(VGABASE, 0x6B, 0x00, BIT(2));

		/* disable simultaneous  */
		svga_wcrt_mask(VGABASE, 0x6B, 0x00, BIT(3));
		drm_mode_crtc_load_lut(crtc);
		break;
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void
via_iga2_crtc_disable(struct drm_crtc *crtc)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Turn off the cursor */
	via_hide_cursor(crtc);

	via_iga2_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void
via_iga2_crtc_prepare(struct drm_crtc *crtc)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Blank the screen */
	if (crtc->enabled)
		via_iga2_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static void
via_iga2_crtc_commit(struct drm_crtc *crtc)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Turn on the monitor */
	if (crtc->enabled)
		via_iga2_crtc_dpms(crtc, DRM_MODE_DPMS_ON);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static bool
via_iga2_crtc_mode_fixup(struct drm_crtc *crtc,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return true;
}

static int
via_iga2_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
				struct drm_framebuffer *old_fb)
{
	struct ttm_buffer_object *bo;
	struct via_framebuffer *via_fb = container_of(crtc->primary->fb,
					struct via_framebuffer, fb);
	struct drm_framebuffer *new_fb = &via_fb->fb;
	struct drm_gem_object *gem_obj = via_fb->gem_obj;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* No FB found. */
	if (!new_fb) {
		ret = -ENOMEM;
		DRM_DEBUG_KMS("No FB found.\n");
		goto exit;
	}

	gem_obj = via_fb->gem_obj;
	bo = ttm_gem_mapping(gem_obj);

	ret = via_bo_pin(bo, NULL);
	if (unlikely(ret)) {
		DRM_DEBUG_KMS("Failed to pin FB.\n");
		goto exit;
	}

	ret = crtc->helper_private->mode_set_base_atomic(crtc, new_fb, x, y,
			ENTER_ATOMIC_MODE_SET);
	if (unlikely(ret)) {
		DRM_DEBUG_KMS("Failed to set a new FB.\n");
		via_bo_unpin(bo, NULL);
		goto exit;
	}

	/* Free the old framebuffer if it exist */
	if (old_fb) {
		via_fb = container_of(old_fb,
				struct via_framebuffer, fb);
		gem_obj = via_fb->gem_obj;
		bo = ttm_gem_mapping(gem_obj);

		ret = via_bo_unpin(bo, NULL);
		if (unlikely(ret)) {
			DRM_DEBUG_KMS("FB still locked.\n");
			goto exit;
		}
	}

exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int
via_iga2_crtc_mode_set(struct drm_crtc *crtc,
			struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode,
			int x, int y, struct drm_framebuffer *fb)
{
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;
	struct drm_device *dev = crtc->dev;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Check for IGA1. */
	if (!iga->index) {
		DRM_ERROR("Wrong display controller (CRTC) selection.");
		ret = -EINVAL;
		goto exit;
	}

	/* Load standard registers */
	via_load_vpit_regs(dev_private);

	/* Unlock */
	via_unlock_crtc(VGABASE, dev->pdev->device);

	/* disable IGA scales first */
	via_disable_iga_scaling(crtc);

	/* Load crtc timing and IGA scaling */
	if (iga->scaling_mode & VIA_SHRINK) {
		/* enable IGA2 down scaling and set Interpolation */
		via_set_iga_scale_function(crtc, VIA_SHRINK);

		/* load hor and ver downscaling factor */
		/**
		 * interlace modes scaling support(example 1080I):
		 * we should use mode->crtc_vdisplay here,
		 * because crtc_vdisplay=540, vdisplay=1080,
		 * we need 540 here, not 1080.
		 */
		via_load_iga_scale_factor_regs(dev_private,
						mode, adjusted_mode,
						VIA_SHRINK,
						HOR_VER_SCALE);
		/* load src timing to timing registers */
		/**
		 * interlace modes scaling support(example 1080I):
		 * we should use mode->crtc_vdisplay here,
		 * because crtc_vdisplay=540, vdisplay=1080,
		 * we need 540 here, not 1080.
		 */
		via_set_iga2_downscale_source_timing(crtc, mode, adjusted_mode);

		/* Download dst timing */
		via_set_scale_path(crtc, VIA_SHRINK);
		via_load_crtc_timing(iga, adjusted_mode);
		/* very necessary to set IGA to none scaling status */
		/* need to fix why so need. */
		via_set_scale_path(crtc, VIA_NO_SCALING);
	} else {
		/* when not down scaling, we only need load one timing. */
		via_load_crtc_timing(iga, adjusted_mode);

		/* II. up scaling */
		if (iga->scaling_mode & VIA_EXPAND) {
			/* Horizontal scaling */
			if (iga->scaling_mode & VIA_HOR_EXPAND) {
				via_set_iga_scale_function(crtc,
							VIA_HOR_EXPAND);
				via_load_iga_scale_factor_regs(
						dev_private,
						mode, adjusted_mode,
						VIA_EXPAND,
						HOR_SCALE);
			}

			/* Vertical scaling */
			if (iga->scaling_mode & VIA_VER_EXPAND) {
				via_set_iga_scale_function(crtc,
							VIA_VER_EXPAND);
				via_load_iga_scale_factor_regs(
						dev_private,
						mode, adjusted_mode,
						VIA_EXPAND,
						VER_SCALE);
			}
		}
	}

	/* Relock */
	via_lock_crtc(VGABASE);

	/* Set non-interlace / interlace mode. */
	via_iga2_set_interlace_mode(VGABASE,
			adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE);

	/* Load display FIFO. */
	via_iga2_display_fifo_regs(dev, dev_private, iga,
			adjusted_mode, crtc->primary->fb);

	/* Set PLL */
	if (adjusted_mode->clock) {
		u32 clock = adjusted_mode->clock * 1000, pll_regs;

		if (iga->scaling_mode & VIA_SHRINK)
			clock *= 2;
		pll_regs = via_get_clk_value(crtc->dev, clock);
		via_set_vclock(crtc, pll_regs);
	}

	ret = via_iga2_crtc_mode_set_base(crtc, x, y, fb);

	/* Specify the cursor pattern. */
	via_cursor_address(crtc);

	/* Turn off the cursor. */
	via_hide_cursor(crtc);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static int
via_iga2_mode_set_base_atomic(struct drm_crtc *crtc,
				struct drm_framebuffer *fb,
				int x, int y, enum mode_set_atomic state)
{
	u32 pitch = y * fb->pitches[0] +
			((x * fb->format->cpp[0] * 8) >> 3), addr;
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
						crtc->dev->dev_private;
	struct via_framebuffer *via_fb = container_of(fb,
					struct via_framebuffer, fb);
	struct drm_gem_object *gem_obj = via_fb->gem_obj;
	struct ttm_buffer_object *bo = ttm_gem_mapping(gem_obj);

	if ((fb->format->depth != 8) && (fb->format->depth != 16) &&
		(fb->format->depth != 24) && (fb->format->depth != 32)) {
		DRM_ERROR("Unsupported IGA2 Color Depth: %d bit\n",
						fb->format->depth);
		return -EINVAL;
	}

	via_iga_common_init(VGABASE);

	/* Set palette LUT to 8-bit mode. */
	via_iga2_set_palette_lut_resolution(VGABASE, true);

	via_iga2_set_color_depth(dev_private, fb->format->depth);

	/* Set the framebuffer offset */
	addr = round_up(bo->offset + pitch, 16);
	/* Bits 9 to 3 of the frame buffer go into bits 7 to 1
	 * of the register. Bit 0 is for setting tile mode or
	 * linear mode. A value of zero sets it to linear mode */
	vga_wcrt(VGABASE, 0x62, ((addr >> 3) & 0x7F) << 1);
	vga_wcrt(VGABASE, 0x63, (addr >> 10) & 0xFF);
	vga_wcrt(VGABASE, 0x64, (addr >> 18) & 0xFF);
	svga_wcrt_mask(VGABASE, 0xA3, ((addr >> 26) & 0x07), 0x07);

	/* Load fetch count registers */
	pitch = ALIGN(crtc->mode.hdisplay * (fb->format->cpp[0] * 8) >> 3, 16);
	load_value_to_registers(VGABASE, &iga->fetch, pitch >> 4);

	/* Set secondary pitch */
	pitch = ALIGN(fb->pitches[0], 16);
	load_value_to_registers(VGABASE, &iga->offset, pitch >> 3);

	enable_second_display_channel(VGABASE);

	return 0;
}

static const struct drm_crtc_helper_funcs via_iga1_helper_funcs = {
	.dpms = via_iga1_crtc_dpms,
	.disable = via_iga1_crtc_disable,
	.prepare = via_iga1_crtc_prepare,
	.commit = via_iga1_crtc_commit,
	.mode_fixup = via_iga1_crtc_mode_fixup,
	.mode_set = via_iga1_crtc_mode_set,
	.mode_set_base_atomic = via_iga1_mode_set_base_atomic,
};

static const struct drm_crtc_helper_funcs via_iga2_helper_funcs = {
	.dpms = via_iga2_crtc_dpms,
	.disable = via_iga2_crtc_disable,
	.prepare = via_iga2_crtc_prepare,
	.commit = via_iga2_crtc_commit,
	.mode_fixup = via_iga2_crtc_mode_fixup,
	.mode_set = via_iga2_crtc_mode_set,
	.mode_set_base_atomic = via_iga2_mode_set_base_atomic,
};

void
via_crtc_init(struct drm_device *dev, int index)
{
	struct openchrome_drm_private *dev_private =
						dev->dev_private;
	struct via_crtc *iga = &dev_private->iga[index];
	struct drm_crtc *crtc = &iga->base;
	int cursor_size = 64 * 64 * 4, i;
	u16 *gamma;

	iga->index = index;
	if (index) {
		drm_crtc_init(dev, crtc, &via_iga2_funcs);
		drm_crtc_helper_add(crtc, &via_iga2_helper_funcs);

		iga->timings.htotal.count = ARRAY_SIZE(iga2_hor_total);
		iga->timings.htotal.regs = iga2_hor_total;

		iga->timings.hdisplay.count = ARRAY_SIZE(iga2_hor_addr);
		iga->timings.hdisplay.regs = iga2_hor_addr;
		if (dev->pdev->device != PCI_DEVICE_ID_VIA_VX900_VGA)
			iga->timings.hdisplay.count--;

		iga->timings.hblank_start.count = ARRAY_SIZE(iga2_hor_blank_start);
		iga->timings.hblank_start.regs = iga2_hor_blank_start;
		if (dev->pdev->device != PCI_DEVICE_ID_VIA_VX900_VGA)
			iga->timings.hblank_start.count--;

		iga->timings.hblank_end.count = ARRAY_SIZE(iga2_hor_blank_end);
		iga->timings.hblank_end.regs = iga2_hor_blank_end;

		iga->timings.hsync_start.count = ARRAY_SIZE(iga2_hor_sync_start);
		iga->timings.hsync_start.regs = iga2_hor_sync_start;
		if (dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266
			|| dev->pdev->device == PCI_DEVICE_ID_VIA_KM400)
			iga->timings.hsync_start.count--;

		iga->timings.hsync_end.count = ARRAY_SIZE(iga2_hor_sync_end);
		iga->timings.hsync_end.regs = iga2_hor_sync_end;

		iga->timings.vtotal.count = ARRAY_SIZE(iga2_ver_total);
		iga->timings.vtotal.regs = iga2_ver_total;

		iga->timings.vdisplay.count = ARRAY_SIZE(iga2_ver_addr);
		iga->timings.vdisplay.regs = iga2_ver_addr;

		iga->timings.vblank_start.count = ARRAY_SIZE(iga2_ver_blank_start);
		iga->timings.vblank_start.regs = iga2_ver_blank_start;

		iga->timings.vblank_end.count = ARRAY_SIZE(iga2_ver_blank_end);
		iga->timings.vblank_end.regs = iga2_ver_blank_end;

		iga->timings.vsync_start.count = ARRAY_SIZE(iga2_ver_sync_start);
		iga->timings.vsync_start.regs = iga2_ver_sync_start;

		iga->timings.vsync_end.count = ARRAY_SIZE(iga2_ver_sync_end);
		iga->timings.vsync_end.regs = iga2_ver_sync_end;

		/* Secondary FIFO setup */
		if ((dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266) ||
			(dev->pdev->device == PCI_DEVICE_ID_VIA_KM400)) {
			iga->fifo_depth.count =
				ARRAY_SIZE(iga2_cle266_fifo_depth_select);
			iga->fifo_depth.regs = iga2_cle266_fifo_depth_select;

			iga->threshold.count =
				ARRAY_SIZE(iga2_cle266_fifo_threshold_select);
			iga->threshold.regs = iga2_cle266_fifo_threshold_select;
		} else {
			iga->fifo_depth.count = ARRAY_SIZE(iga2_k8m800_fifo_depth_select);
			iga->fifo_depth.regs = iga2_k8m800_fifo_depth_select;

			iga->threshold.count = ARRAY_SIZE(iga2_k8m800_fifo_threshold_select);
			iga->threshold.regs = iga2_k8m800_fifo_threshold_select;

			iga->high_threshold.count = ARRAY_SIZE(iga2_fifo_high_threshold_select);
			iga->high_threshold.regs = iga2_fifo_high_threshold_select;

			iga->display_queue.count = ARRAY_SIZE(iga2_display_queue_expire_num);
			iga->display_queue.regs = iga2_display_queue_expire_num;
		}

		iga->fetch.count = ARRAY_SIZE(iga2_fetch_count);
		iga->fetch.regs = iga2_fetch_count;

		/* Older hardware only uses 12 bits */
		iga->offset.count = ARRAY_SIZE(iga2_offset) - 1;
		iga->offset.regs = iga2_offset;
	} else {
		drm_crtc_init(dev, crtc, &via_iga1_funcs);
		drm_crtc_helper_add(crtc, &via_iga1_helper_funcs);

		iga->timings.htotal.count = ARRAY_SIZE(iga1_hor_total);
		iga->timings.htotal.regs = iga1_hor_total;

		iga->timings.hdisplay.count = ARRAY_SIZE(iga1_hor_addr);
		iga->timings.hdisplay.regs = iga1_hor_addr;
		if (dev->pdev->device != PCI_DEVICE_ID_VIA_VX900_VGA)
			iga->timings.hdisplay.count--;

		iga->timings.hblank_start.count = ARRAY_SIZE(iga1_hor_blank_start);
		iga->timings.hblank_start.regs = iga1_hor_blank_start;
		if (dev->pdev->device != PCI_DEVICE_ID_VIA_VX900_VGA)
			iga->timings.hblank_start.count--;

		iga->timings.hblank_end.count = ARRAY_SIZE(iga1_hor_blank_end);
		iga->timings.hblank_end.regs = iga1_hor_blank_end;

		iga->timings.hsync_start.count = ARRAY_SIZE(iga1_hor_sync_start);
		iga->timings.hsync_start.regs = iga1_hor_sync_start;

		iga->timings.hsync_end.count = ARRAY_SIZE(iga1_hor_sync_end);
		iga->timings.hsync_end.regs = iga1_hor_sync_end;

		iga->timings.vtotal.count = ARRAY_SIZE(iga1_ver_total);
		iga->timings.vtotal.regs = iga1_ver_total;

		iga->timings.vdisplay.count = ARRAY_SIZE(iga1_ver_addr);
		iga->timings.vdisplay.regs = iga1_ver_addr;

		iga->timings.vblank_start.count = ARRAY_SIZE(iga1_ver_blank_start);
		iga->timings.vblank_start.regs = iga1_ver_blank_start;

		iga->timings.vblank_end.count = ARRAY_SIZE(iga1_ver_blank_end);
		iga->timings.vblank_end.regs = iga1_ver_blank_end;

		iga->timings.vsync_start.count = ARRAY_SIZE(iga1_ver_sync_start);
		iga->timings.vsync_start.regs = iga1_ver_sync_start;

		iga->timings.vsync_end.count = ARRAY_SIZE(iga1_ver_sync_end);
		iga->timings.vsync_end.regs = iga1_ver_sync_end;

		/* Primary FIFO setup */
		if ((dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266) ||
			(dev->pdev->device == PCI_DEVICE_ID_VIA_KM400)) {
			iga->fifo_depth.count = ARRAY_SIZE(iga1_cle266_fifo_depth_select);
			iga->fifo_depth.regs = iga1_cle266_fifo_depth_select;

			iga->threshold.count = ARRAY_SIZE(iga1_cle266_fifo_threshold_select);
			iga->threshold.regs = iga1_cle266_fifo_threshold_select;

			iga->high_threshold.count = ARRAY_SIZE(iga1_cle266_fifo_high_threshold_select);
			iga->high_threshold.regs = iga1_cle266_fifo_high_threshold_select;

			iga->display_queue.count = ARRAY_SIZE(iga1_cle266_display_queue_expire_num);
			iga->display_queue.regs = iga1_cle266_display_queue_expire_num;
		} else {
			iga->fifo_depth.count = ARRAY_SIZE(iga1_k8m800_fifo_depth_select);
			iga->fifo_depth.regs = iga1_k8m800_fifo_depth_select;

			iga->threshold.count = ARRAY_SIZE(iga1_k8m800_fifo_threshold_select);
			iga->threshold.regs = iga1_k8m800_fifo_threshold_select;

			iga->high_threshold.count = ARRAY_SIZE(iga1_k8m800_fifo_high_threshold_select);
			iga->high_threshold.regs = iga1_k8m800_fifo_high_threshold_select;

			iga->display_queue.count = ARRAY_SIZE(iga1_k8m800_display_queue_expire_num);
			iga->display_queue.regs = iga1_k8m800_display_queue_expire_num;
		}

		iga->fetch.count = ARRAY_SIZE(iga1_fetch_count);
		iga->fetch.regs = iga1_fetch_count;

		iga->offset.count = ARRAY_SIZE(iga1_offset);
		iga->offset.regs = iga1_offset;
	}

	drm_mode_crtc_set_gamma_size(crtc, 256);
	gamma = crtc->gamma_store;

	for (i = 0; i < 256; i++) {
		gamma[i] = i << 8 | i;
		gamma[i + 256] = i << 8 | i;
		gamma[i + 512] = i << 8 | i;
	}

	if (dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266
			|| dev->pdev->device == PCI_DEVICE_ID_VIA_KM400)
		cursor_size = 32 * 32 * 4;

	if (via_ttm_allocate_kernel_buffer(&dev_private->ttm.bdev,
				cursor_size, 16,
				TTM_PL_FLAG_VRAM, &iga->cursor_kmap))
		DRM_ERROR("failed to create cursor\n");
}
