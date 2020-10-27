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
#include <drm/drm_fourcc.h>
#include <drm/drm_gem.h>
#include <drm_gem_framebuffer_helper.h>

#include "openchrome_drv.h"


static const struct drm_framebuffer_funcs
openchrome_drm_framebuffer_funcs = {
	.create_handle	= drm_gem_fb_create_handle,
	.destroy	= drm_gem_fb_destroy,
};

static int openchrome_framebuffer_init(
			struct drm_device *dev,
			struct drm_gem_object *gem,
			const struct drm_mode_fb_cmd2 *mode_cmd,
			struct drm_framebuffer **pfb)
{
	struct drm_framebuffer *fb;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	fb = kzalloc(sizeof(struct drm_framebuffer), GFP_KERNEL);
	if (!fb) {
		ret = -ENOMEM;
		goto exit;
	}

	drm_helper_mode_fill_fb_struct(dev, fb, mode_cmd);
	fb->obj[0] = gem;
	ret = drm_framebuffer_init(dev, fb,
				&openchrome_drm_framebuffer_funcs);
	if (ret) {
		kfree(fb);
	}
exit:
	*pfb = fb;

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static struct drm_framebuffer *
openchrome_fb_create(struct drm_device *dev,
			struct drm_file *file_priv,
			const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_framebuffer *fb;
	struct drm_gem_object *gem;
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	gem = drm_gem_object_lookup(file_priv, mode_cmd->handles[0]);
	if (!gem) {
		DRM_ERROR("No GEM object found for handle 0x%08X\n",
				mode_cmd->handles[0]);
		return ERR_PTR(-ENOENT);
	}

	ret = openchrome_framebuffer_init(dev, gem, mode_cmd, &fb);
	if (ret) {
		drm_gem_object_put(gem);
		DRM_DEBUG_KMS("Exiting %s.\n", __func__);
		return ERR_PTR(ret);
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return fb;
}

static const struct drm_mode_config_funcs openchrome_drm_mode_config_funcs = {
	.fb_create		= openchrome_fb_create,
	.output_poll_changed	= drm_fb_helper_output_poll_changed
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

	dev->mode_config.preferred_depth = 24;

	if ((dev->pdev->device == PCI_DEVICE_ID_VIA_CLE266) ||
		(dev->pdev->device == PCI_DEVICE_ID_VIA_KM400)) {
		dev->mode_config.cursor_width =
		dev->mode_config.cursor_height =
				OPENCHROME_UNICHROME_CURSOR_SIZE;
	} else {
		dev->mode_config.cursor_width =
		dev->mode_config.cursor_height =
				OPENCHROME_UNICHROME_PRO_CURSOR_SIZE;
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static struct fb_ops via_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= drm_fb_helper_check_var,
	.fb_set_par	= drm_fb_helper_set_par,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_blank	= drm_fb_helper_blank,
	.fb_setcmap	= drm_fb_helper_setcmap,
	.fb_debug_enter	= drm_fb_helper_debug_enter,
	.fb_debug_leave	= drm_fb_helper_debug_leave,
};

static int
openchrome_fb_probe(struct drm_fb_helper *helper,
		struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *dev = helper->dev;
	struct openchrome_drm_private *dev_private =
					helper->dev->dev_private;
	struct openchrome_framebuffer *openchrome_fb = container_of(helper,
				struct openchrome_framebuffer, helper);
	struct drm_framebuffer *fb;
	struct drm_gem_object *gem;
	struct fb_info *info = helper->fbdev;
	const struct drm_format_info *format_info;
	struct drm_mode_fb_cmd2 mode_cmd;
	bool is_iomem;
	int size, cpp;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(
						sizes->surface_bpp,
						sizes->surface_depth);
	format_info = drm_get_format_info(dev, &mode_cmd);
	cpp = format_info->cpp[0];
	mode_cmd.pitches[0] = (mode_cmd.width * cpp);
	mode_cmd.pitches[0] = round_up(mode_cmd.pitches[0], 16);
	size = mode_cmd.pitches[0] * mode_cmd.height;
	size = ALIGN(size, PAGE_SIZE);

	ret = openchrome_bo_create(dev,
					&dev_private->bdev,
					size,
					ttm_bo_type_kernel,
					TTM_PL_FLAG_VRAM,
					true,
					&openchrome_fb->bo);
	if (ret) {
		goto exit;
	}

	gem = &openchrome_fb->bo->gem;
	ret = openchrome_framebuffer_init(dev, gem, &mode_cmd, &fb);
	if (ret) {
		goto free_bo;
	}

	info = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(info)) {
		ret = PTR_ERR(info);
		goto cleanup_fb;
	}

	openchrome_fb->helper.fb = fb;
	openchrome_fb->helper.fbdev = info;

	info->fbops = &via_fb_ops;

	info->fix.smem_start = openchrome_fb->bo->ttm_bo.mem.bus.base +
				openchrome_fb->bo->ttm_bo.mem.bus.offset;
	info->fix.smem_len = openchrome_fb->bo->ttm_bo.mem.num_pages <<
							PAGE_SHIFT;
	info->screen_base = ttm_kmap_obj_virtual(
				&openchrome_fb->bo->kmap, &is_iomem);
	info->screen_size = openchrome_fb->bo->ttm_bo.mem.num_pages <<
							PAGE_SHIFT;

	info->apertures->ranges[0].base = dev_private->vram_start;
	info->apertures->ranges[0].size = dev_private->vram_size;

	drm_fb_helper_fill_info(info, helper, sizes);
	goto exit;
cleanup_fb:
	drm_framebuffer_cleanup(fb);

	/*
	 * openchrome_framebuffer_init() allocates a drm_framebuffer
	 * struct (in this case, fb pointer), so it needs to be
	 * released if there was an error.
	 */
	kfree(fb);
free_bo:
	openchrome_bo_destroy(openchrome_fb->bo, true);
	openchrome_fb->bo = NULL;
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static struct drm_fb_helper_funcs openchrome_drm_fb_helper_funcs = {
	.fb_probe = openchrome_fb_probe,
};

int openchrome_fb_init(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct openchrome_framebuffer *openchrome_fb;
	int bpp_sel = 32;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	openchrome_fb = kzalloc(sizeof(struct openchrome_framebuffer),
				GFP_KERNEL);
	if (!openchrome_fb) {
		ret = -ENOMEM;
		goto exit;
	}

	dev_private->openchrome_fb = openchrome_fb;

	drm_fb_helper_prepare(dev, &openchrome_fb->helper,
				&openchrome_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, &openchrome_fb->helper);
	if (ret) {
		goto free_fbdev;
	}

	drm_helper_disable_unused_functions(dev);
	ret = drm_fb_helper_initial_config(&openchrome_fb->helper, bpp_sel);
	if (ret) {
		goto free_fb_helper;
	}

	goto exit;
free_fb_helper:
	drm_fb_helper_fini(&openchrome_fb->helper);
free_fbdev:
	kfree(openchrome_fb);
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void openchrome_fb_fini(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct drm_fb_helper *fb_helper = &dev_private->
						openchrome_fb->helper;
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

	drm_fb_helper_fini(&dev_private->openchrome_fb->helper);
	if (dev_private->openchrome_fb) {
		kfree(dev_private->openchrome_fb);
		dev_private->openchrome_fb = NULL;
	}
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}
