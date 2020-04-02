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

#include <linux/fb.h>

#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem.h>

#include "openchrome_drv.h"


int via_fbdev_init(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct via_framebuffer_device *via_fbdev;
	int bpp_sel = 32;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	via_fbdev = kzalloc(sizeof(struct via_framebuffer_device),
				GFP_KERNEL);
	if (!via_fbdev) {
		ret = -ENOMEM;
		goto exit;
	}

	dev_private->via_fbdev = via_fbdev;

	drm_fb_helper_prepare(dev, &via_fbdev->helper,
				&via_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, &via_fbdev->helper);
	if (ret) {
		goto free_fbdev;
	}

	drm_helper_disable_unused_functions(dev);
	ret = drm_fb_helper_initial_config(&via_fbdev->helper, bpp_sel);
	if (ret) {
		goto free_fb_helper;
	}

	goto exit;
free_fb_helper:
	drm_fb_helper_fini(&via_fbdev->helper);
free_fbdev:
	kfree(via_fbdev);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void via_fbdev_fini(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct drm_fb_helper *fb_helper = &dev_private->
						via_fbdev->helper;
	struct via_framebuffer *via_fb = &dev_private->
						via_fbdev->via_fb;
	struct fb_info *info;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (!fb_helper) {
		goto exit;
	}

	info = fb_helper->fbdev;
	if (info) {
		unregister_framebuffer(info);
		kfree(info->apertures);
		framebuffer_release(info);
		fb_helper->fbdev = NULL;
	}

	if (via_fb->gem) {
		drm_gem_object_put_unlocked(via_fb->gem);
		via_fb->gem = NULL;
	}

	drm_fb_helper_fini(&dev_private->via_fbdev->helper);
	drm_framebuffer_cleanup(&dev_private->via_fbdev->via_fb.fb);
	if (dev_private->via_fbdev) {
		kfree(dev_private->via_fbdev);
		dev_private->via_fbdev = NULL;
	}
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}
