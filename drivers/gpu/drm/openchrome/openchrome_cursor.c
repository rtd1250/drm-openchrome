/*
 * Copyright © 2019 Kevin Brace
 * Copyright 2012 James Simmons <jsimmons@infradead.org>. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including
 * the next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/*
 * Author(s):
 *
 * Kevin Brace <kevinbrace@gmx.com>
 * James Simmons <jsimmons@infradead.org>
 */

#include <linux/pci_ids.h>

#include <drm/drm_crtc.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_mode.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_plane.h>
#include <drm/drm_plane_helper.h>

#include <drm/ttm/ttm_bo_api.h>
#include <drm/ttm/ttm_bo_driver.h>

#include "openchrome_drv.h"


static void openchrome_hide_cursor(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct via_crtc *iga = container_of(crtc,
					struct via_crtc, base);
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

static void openchrome_show_cursor(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct via_crtc *iga = container_of(crtc,
					struct via_crtc, base);
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
			VIA_WRITE(ALPHA_V3_PREFIFO_CONTROL,
							0x000E0000);
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

static void openchrome_cursor_address(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct via_crtc *iga = container_of(crtc,
					struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
					crtc->dev->dev_private;

	if (!iga->cursor_bo->kmap.bo) {
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
			VIA_WRITE(HI_FBOFFSET,
				iga->cursor_bo->kmap.bo->offset);
		} else {
			VIA_WRITE(PRIM_HI_FBOFFSET,
				iga->cursor_bo->kmap.bo->offset);
		}
		break;
	default:
		VIA_WRITE(HI_FBOFFSET,
				iga->cursor_bo->kmap.bo->offset);
		break;
	}
}

static void openchrome_set_hi_location(struct drm_crtc *crtc,
					int crtc_x,
					int crtc_y)
{
	struct drm_device *dev = crtc->dev;
	struct via_crtc *iga = container_of(crtc,
					struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
					crtc->dev->dev_private;
	uint32_t location_x = 0, location_y = 0;
	uint32_t offset_x = 0, offset_y = 0;

	if (crtc_x < 0) {
		offset_x = -crtc_x;
	} else {
		location_x = crtc_x;
	}

	if (crtc_y < 0) {
		offset_y = -crtc_y;
	} else {
		location_y = crtc_y;
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
			VIA_WRITE(HI_POSSTART,
				(((location_x & 0x07ff) << 16) |
				(location_y & 0x07ff)));
			VIA_WRITE(HI_CENTEROFFSET,
				(((offset_x & 0x07ff) << 16) |
				(offset_y & 0x07ff)));
		} else {
			VIA_WRITE(PRIM_HI_POSSTART,
				(((location_x & 0x07ff) << 16) |
				(location_y & 0x07ff)));
			VIA_WRITE(PRIM_HI_CENTEROFFSET,
				(((offset_x & 0x07ff) << 16) |
				(offset_y & 0x07ff)));
		}

		break;
	default:
		VIA_WRITE(HI_POSSTART,
				(((location_x & 0x07ff) << 16) |
				(location_y & 0x07ff)));
		VIA_WRITE(HI_CENTEROFFSET,
				(((offset_x & 0x07ff) << 16) |
				(offset_y & 0x07ff)));
		break;
	}
}

static int openchrome_cursor_update_plane(struct drm_plane *plane,
				struct drm_crtc *crtc,
				struct drm_framebuffer *fb,
				int crtc_x, int crtc_y,
				unsigned int crtc_w,
				unsigned int crtc_h,
				uint32_t src_x, uint32_t src_y,
				uint32_t src_w, uint32_t src_h,
				struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_device *dev = plane->dev;
	struct via_crtc *iga = container_of(crtc,
					struct via_crtc, base);
	struct via_framebuffer *via_fb;
	struct openchrome_bo *user_bo;
	struct drm_gem_object *gem;
	uint32_t *user_bo_src, *cursor_dst;
	bool is_iomem;
	uint32_t i;
	uint32_t width, height, cursor_width;
	int ret = 0;

	if (!crtc) {
		DRM_ERROR("Invalid CRTC!\n");
		ret = -EINVAL;
		goto exit;
	}

	if (!crtc->enabled) {
		DRM_ERROR("CRTC is currently disabled!\n");
		ret = -EINVAL;
		goto exit;
	}

	if (!fb) {
		DRM_ERROR("Invalid frame buffer!\n");
		ret = -EINVAL;
		goto exit;
	}

	if ((dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_KM400)) {
		if ((fb->width != OPENCHROME_UNICHROME_CURSOR_SIZE) ||
		(fb->height != OPENCHROME_UNICHROME_CURSOR_SIZE)) {
			DRM_ERROR("Invalid cursor dimensions.\n");
			ret = -EINVAL;
			goto exit;
		}

		cursor_width = OPENCHROME_UNICHROME_CURSOR_SIZE;
	} else {
		if ((fb->width != OPENCHROME_UNICHROME_PRO_CURSOR_SIZE) ||
		(fb->height != OPENCHROME_UNICHROME_PRO_CURSOR_SIZE)) {
			DRM_ERROR("Invalid cursor dimensions.\n");
			ret = -EINVAL;
			goto exit;
		}

		cursor_width = OPENCHROME_UNICHROME_PRO_CURSOR_SIZE;
	}

	if (fb->width != fb->height) {
		DRM_ERROR("Hardware cursor is expected to have "
				"square dimensions.\n");
		ret = -EINVAL;
		goto exit;
	}

	if (fb != crtc->cursor->fb) {
		width = fb->width;
		height = fb->height;

		via_fb = container_of(fb, struct via_framebuffer, fb);
		gem = via_fb->gem;
		user_bo = container_of(gem, struct openchrome_bo, gem);
		ret = ttm_bo_kmap(&user_bo->ttm_bo, 0,
					user_bo->ttm_bo.num_pages,
					&user_bo->kmap);
		if (ret) {
			goto exit;
		}

		user_bo_src = ttm_kmap_obj_virtual(&user_bo->kmap,
							&is_iomem);
		cursor_dst =
			ttm_kmap_obj_virtual(&iga->cursor_bo->kmap,
						&is_iomem);
		memset_io(cursor_dst, 0x0,
				iga->cursor_bo->kmap.bo->mem.size);
		for (i = 0; i < height; i++) {
			__iowrite32_copy(cursor_dst, user_bo_src,
						width);
			user_bo_src += width;
			cursor_dst += cursor_width;
		}

		ttm_bo_kunmap(&user_bo->kmap);

		openchrome_cursor_address(crtc);
	} else {
		crtc->cursor_x = crtc_x;
		crtc->cursor_y = crtc_y;
		openchrome_set_hi_location(crtc, crtc_x, crtc_y);
	}

	openchrome_show_cursor(crtc);
exit:
	return ret;
}

static int openchrome_cursor_disable_plane(struct drm_plane *plane,
				struct drm_modeset_acquire_ctx *ctx)
{
	if (plane->crtc) {
		openchrome_hide_cursor(plane->crtc);
	}

	if (plane->fb) {
		drm_framebuffer_put(plane->fb);
		plane->fb = NULL;
	}

	return 0;
}

static void openchrome_cursor_destroy(struct drm_plane *plane)
{
	if (plane->crtc) {
		openchrome_hide_cursor(plane->crtc);
	}

	drm_plane_cleanup(plane);
}

const struct drm_plane_funcs openchrome_cursor_drm_plane_funcs = {
	.update_plane	= openchrome_cursor_update_plane,
	.disable_plane	= openchrome_cursor_disable_plane,
	.destroy	= openchrome_cursor_destroy,
};

const uint32_t openchrome_cursor_formats[] = {
	DRM_FORMAT_ARGB8888,
};

const unsigned int openchrome_cursor_formats_size =
				ARRAY_SIZE(openchrome_cursor_formats);
