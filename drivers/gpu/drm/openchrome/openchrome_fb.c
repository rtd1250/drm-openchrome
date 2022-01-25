/*
 * Copyright 2012 James Simmons <jsimmons@infradead.org>. All Rights Reserved.
 *
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
 */

#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_mode_config.h>

#include "openchrome_drv.h"


static const struct drm_mode_config_funcs openchrome_drm_mode_config_funcs = {
	.fb_create		= drm_gem_fb_create,
	.atomic_check		= drm_atomic_helper_check,
	.atomic_commit		= drm_atomic_helper_commit,
};

void openchrome_mode_config_init(
			struct openchrome_drm_private *dev_private)
{
	struct drm_device *dev = dev_private->dev;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	drm_mode_config_init(dev);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;
	dev->mode_config.max_width = 2044;
	dev->mode_config.max_height = 4096;

	dev->mode_config.funcs = &openchrome_drm_mode_config_funcs;

	dev->mode_config.fb_base = dev_private->vram_start;

	dev->mode_config.preferred_depth = 24;
	dev->mode_config.prefer_shadow_fbdev = true;

	dev->mode_config.cursor_width =
	dev->mode_config.cursor_height = OPENCHROME_CURSOR_SIZE;

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}
