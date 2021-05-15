/*
 * Copyright © 2019-2020 Kevin Brace
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

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_state_helper.h>
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


static void openchrome_hide_cursor(struct drm_device *dev,
					struct drm_crtc *crtc)
{
	struct via_crtc *iga = container_of(crtc,
					struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
					dev->dev_private;
	uint32_t temp;

	switch (dev->pdev->device) {
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
	case PCI_DEVICE_ID_VIA_VT3157:
	case PCI_DEVICE_ID_VIA_VT3343:
	case PCI_DEVICE_ID_VIA_P4M900:
	case PCI_DEVICE_ID_VIA_VT1122:
	case PCI_DEVICE_ID_VIA_VX875:
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		/*
		 * Program Hardware Icon (HI) FIFO, foreground color,
		 * and background color.
		 */
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
		/*
		 * Program Hardware Icon (HI) FIFO, foreground color,
		 * and background color.
		 */
		VIA_WRITE(HI_TRANSPARENT_COLOR, 0x00000000);
		VIA_WRITE(HI_INVTCOLOR, 0x00FFFFFF);
		VIA_WRITE(ALPHA_V3_PREFIFO_CONTROL, 0x000E0000);
		VIA_WRITE(ALPHA_V3_FIFO_CONTROL, 0xE0F0000);
		break;
	}

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_VT3157:
	case PCI_DEVICE_ID_VIA_VT3343:
	case PCI_DEVICE_ID_VIA_P4M900:
	case PCI_DEVICE_ID_VIA_VT1122:
	case PCI_DEVICE_ID_VIA_VX875:
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		/*
		 * Turn on Hardware Icon (HI).
		 */
		if (iga->index) {
			VIA_WRITE(HI_CONTROL, 0xB6000005);
		} else {
			VIA_WRITE(PRIM_HI_CTRL, 0x36000005);
		}

		break;
	default:
		/*
		 * Turn on Hardware Icon (HI).
		 */
		if (iga->index) {
			VIA_WRITE(HI_CONTROL, 0xB6000005);
		} else {
			VIA_WRITE(HI_CONTROL, 0x36000005);
		}

		break;
	}
}

static void openchrome_cursor_address(struct drm_crtc *crtc,
					struct openchrome_bo *ttm_bo)
{
	struct drm_device *dev = crtc->dev;
	struct via_crtc *iga = container_of(crtc,
					struct via_crtc, base);
	struct openchrome_drm_private *dev_private =
					crtc->dev->dev_private;

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_VT3157:
	case PCI_DEVICE_ID_VIA_VT3343:
	case PCI_DEVICE_ID_VIA_P4M900:
	case PCI_DEVICE_ID_VIA_VT1122:
	case PCI_DEVICE_ID_VIA_VX875:
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		/*
		 * Program Hardware Icon (HI) offset.
		 */
		if (iga->index) {
			VIA_WRITE(HI_FBOFFSET,
			ttm_bo->kmap.bo->mem.start << PAGE_SHIFT);
		} else {
			VIA_WRITE(PRIM_HI_FBOFFSET,
			ttm_bo->kmap.bo->mem.start << PAGE_SHIFT);
		}
		break;
	default:
		/*
		 * Program Hardware Icon (HI) offset.
		 */
		VIA_WRITE(HI_FBOFFSET,
			ttm_bo->kmap.bo->mem.start << PAGE_SHIFT);
		break;
	}

	return;
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

static int openchrome_cursor_prepare_fb(struct drm_plane *plane,
				struct drm_plane_state *new_state)
{
	struct drm_gem_object *gem;
	struct openchrome_bo *bo;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (!new_state->fb) {
		goto exit;
	}

	gem = new_state->fb->obj[0];
	bo = container_of(gem, struct openchrome_bo, gem);

	ret = ttm_bo_reserve(&bo->ttm_bo, true, false, NULL);
	if (ret) {
		goto exit;
	}

	ret = openchrome_bo_pin(bo, TTM_PL_VRAM);
	ttm_bo_unreserve(&bo->ttm_bo);
	ret = ttm_bo_kmap(&bo->ttm_bo, 0,
				bo->ttm_bo.mem.num_pages,
				&bo->kmap);
	if (ret) {
		goto exit;
	}

exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void openchrome_cursor_cleanup_fb(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct drm_gem_object *gem;
	struct openchrome_bo *bo;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (!old_state->fb) {
		goto exit;
	}

	gem = old_state->fb->obj[0];
	bo = container_of(gem, struct openchrome_bo, gem);

	ttm_bo_kunmap(&bo->kmap);
	ret = ttm_bo_reserve(&bo->ttm_bo, true, false, NULL);
	if (ret) {
		goto exit;
	}

	openchrome_bo_unpin(bo);
	ttm_bo_unreserve(&bo->ttm_bo);

exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static int openchrome_cursor_atomic_check(struct drm_plane *plane,
				 struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state =
			drm_atomic_get_new_plane_state(state, plane);
	struct drm_crtc_state *new_crtc_state;
	struct drm_framebuffer *fb = new_plane_state->fb;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if ((!new_plane_state->crtc) || (!new_plane_state->visible)) {
		goto exit;
	}

	if (fb->width != fb->height) {
		DRM_ERROR("Hardware cursor is expected to have "
				"square dimensions.\n");
		ret = -EINVAL;
		goto exit;
	}

	new_crtc_state = drm_atomic_get_new_crtc_state(state,
						new_plane_state->crtc);
	ret = drm_atomic_helper_check_plane_state(
					new_plane_state,
					new_crtc_state,
					DRM_PLANE_HELPER_NO_SCALING,
					DRM_PLANE_HELPER_NO_SCALING,
					true, true);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void openchrome_cursor_atomic_update(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct drm_crtc *crtc = plane->state->crtc;
	struct openchrome_bo *bo;
	struct drm_gem_object *gem;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (state->fb != old_state->fb) {
		gem = state->fb->obj[0];
		bo = container_of(gem, struct openchrome_bo, gem);
		openchrome_cursor_address(crtc, bo);
	}

	openchrome_set_hi_location(crtc,
				state->crtc_x,
				state->crtc_y);
	openchrome_show_cursor(crtc);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

void openchrome_cursor_atomic_disable(struct drm_plane *plane,
			struct drm_plane_state *old_state)
{
	struct drm_device *dev = plane->dev;
	struct drm_crtc *crtc = plane->state->crtc;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	openchrome_hide_cursor(dev, crtc);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

const struct drm_plane_helper_funcs
openchrome_cursor_drm_plane_helper_funcs = {
	.prepare_fb	= openchrome_cursor_prepare_fb,
	.cleanup_fb	= openchrome_cursor_cleanup_fb,
	.atomic_check	= openchrome_cursor_atomic_check,
	.atomic_update	= openchrome_cursor_atomic_update,
	.atomic_disable	= openchrome_cursor_atomic_disable,
};

const struct drm_plane_funcs openchrome_cursor_drm_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

const uint32_t openchrome_cursor_formats[] = {
	DRM_FORMAT_ARGB8888,
};

const unsigned int openchrome_cursor_formats_size =
				ARRAY_SIZE(openchrome_cursor_formats);
