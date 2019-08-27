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

#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_crtc_helper.h>

#include "openchrome_drv.h"

int openchrome_vram_init(struct openchrome_drm_private *dev_private)
{
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	/* Add an MTRR for the video RAM. */
	dev_private->vram_mtrr = arch_phys_wc_add(
					dev_private->vram_start,
					dev_private->vram_size);

	DRM_INFO("VIA Technologies Chrome IGP VRAM "
			"Physical Address: 0x%08llx\n",
			dev_private->vram_start);
	DRM_INFO("VIA Technologies Chrome IGP VRAM "
			"Size: %llu\n",
			(unsigned long long) dev_private->vram_size >> 20);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

void openchrome_vram_fini(struct openchrome_drm_private *dev_private)
{
	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (dev_private->vram_mtrr) {
		arch_phys_wc_del(dev_private->vram_mtrr);
		arch_io_free_memtype_wc(dev_private->vram_start,
					dev_private->vram_size);
		dev_private->vram_mtrr = 0;
	}

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static int
via_user_framebuffer_create_handle(struct drm_framebuffer *fb,
					struct drm_file *file_priv,
					unsigned int *handle)
{
	struct via_framebuffer *via_fb = container_of(fb,
					struct via_framebuffer, fb);
	int ret;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	ret = drm_gem_handle_create(file_priv, via_fb->gem, handle);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static void
via_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct via_framebuffer *via_fb = container_of(fb,
					struct via_framebuffer, fb);

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	if (via_fb->gem) {
		drm_gem_object_put_unlocked(via_fb->gem);
		via_fb->gem = NULL;
	}

	drm_framebuffer_cleanup(fb);
	kfree(via_fb);

	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
}

static const struct drm_framebuffer_funcs via_fb_funcs = {
	.create_handle	= via_user_framebuffer_create_handle,
	.destroy	= via_user_framebuffer_destroy,
};

static void
via_output_poll_changed(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;

	drm_fb_helper_hotplug_event(&dev_private->via_fbdev->helper);
}

static struct drm_framebuffer *
via_user_framebuffer_create(struct drm_device *dev,
				struct drm_file *file_priv,
				const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct via_framebuffer *via_fb;
	struct drm_gem_object *gem;
	int ret;

	gem = drm_gem_object_lookup(file_priv, mode_cmd->handles[0]);
	if (!gem) {
		DRM_ERROR("No GEM object found for handle 0x%08X\n",
				mode_cmd->handles[0]);
		return ERR_PTR(-ENOENT);
	}

	via_fb = kzalloc(sizeof(struct via_framebuffer), GFP_KERNEL);
	if (!via_fb) {
		return ERR_PTR(-ENOMEM);
	}

	via_fb->gem = gem;

	drm_helper_mode_fill_fb_struct(dev, &via_fb->fb, mode_cmd);
	ret = drm_framebuffer_init(dev, &via_fb->fb, &via_fb_funcs);
	if (ret) {
		drm_gem_object_put_unlocked(via_fb->gem);
		via_fb->gem = NULL;
		kfree(via_fb);
		return ERR_PTR(ret);
	}

	return &via_fb->fb;
}

static const struct drm_mode_config_funcs via_mode_funcs = {
	.fb_create		= via_user_framebuffer_create,
	.output_poll_changed	= via_output_poll_changed
};

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
via_fb_probe(struct drm_fb_helper *helper,
		struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *dev = helper->dev;
	struct openchrome_drm_private *dev_private =
					helper->dev->dev_private;
	struct via_framebuffer_device *via_fbdev = container_of(helper,
				struct via_framebuffer_device, helper);
	struct via_framebuffer *via_fb = &via_fbdev->via_fb;
	struct drm_framebuffer *fb = &via_fbdev->via_fb.fb;
	struct fb_info *info = helper->fbdev;
	const struct drm_format_info *format_info;
	struct drm_mode_fb_cmd2 mode_cmd;
	struct apertures_struct *ap;
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
					&via_fbdev->bo);
	if (ret) {
		goto exit;
	}

	info = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(info)) {
		ret = PTR_ERR(info);
		goto out_err;
	}

	info->skip_vt_switch = true;

	drm_helper_mode_fill_fb_struct(dev, fb, &mode_cmd);
	ret = drm_framebuffer_init(helper->dev, fb, &via_fb_funcs);
	if (unlikely(ret)) {
		goto out_err;
	}

	via_fb->gem = &via_fbdev->bo->gem;
	via_fbdev->helper.fb = fb;
	via_fbdev->helper.fbdev = info;

	info->fbops = &via_fb_ops;

	info->fix.smem_start = via_fbdev->bo->kmap.bo->mem.bus.base +
				via_fbdev->bo->kmap.bo->mem.bus.offset;
	info->fix.smem_len = size;
	info->screen_base = via_fbdev->bo->kmap.virtual;
	info->screen_size = size;

	/* Setup aperture base / size for takeover (i.e., vesafb). */
	ap = alloc_apertures(1);
	if (!ap) {
		drm_framebuffer_cleanup(fb);
		goto out_err;
	}

	ap->ranges[0].size = via_fbdev->bo->kmap.bo->bdev->
			man[via_fbdev->bo->kmap.bo->mem.mem_type].size;
	ap->ranges[0].base = via_fbdev->bo->kmap.bo->mem.bus.base;
	info->apertures = ap;

	drm_fb_helper_fill_info(info, helper, sizes);
	goto exit;
out_err:
	if (via_fbdev->bo) {
		openchrome_bo_destroy(via_fbdev->bo, true);
		via_fbdev->bo = NULL;
	}

	if (via_fb->gem) {
		drm_gem_object_put_unlocked(via_fb->gem);
		via_fb->gem = NULL;
	}
exit:
	DRM_DEBUG_KMS("Exiting %s.\n", __func__);
	return ret;
}

static struct drm_fb_helper_funcs via_drm_fb_helper_funcs = {
	.fb_probe = via_fb_probe,
};

int via_fbdev_init(struct drm_device *dev)
{
	struct openchrome_drm_private *dev_private = dev->dev_private;
	struct via_framebuffer_device *via_fbdev;
	int bpp_sel = 32;
	int ret = 0;

	DRM_DEBUG_KMS("Entered %s.\n", __func__);

	dev->mode_config.funcs = &via_mode_funcs;

	via_fbdev = kzalloc(sizeof(struct via_framebuffer_device),
				GFP_KERNEL);
	if (!via_fbdev) {
		ret = -ENOMEM;
		goto exit;
	}

	dev_private->via_fbdev = via_fbdev;

	drm_fb_helper_prepare(dev, &via_fbdev->helper,
				&via_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, &via_fbdev->helper,
				dev->mode_config.num_connector);
	if (ret) {
		goto free_fbdev;
	}

	ret = drm_fb_helper_single_add_all_connectors(&via_fbdev->helper);
	if (ret) {
		goto free_fb_helper;
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
	struct drm_fb_helper *fb_helper = &dev_private->via_fbdev->helper;
	struct via_framebuffer *via_fb = &dev_private->via_fbdev->via_fb;
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
