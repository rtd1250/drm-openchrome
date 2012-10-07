/*
 * Copyright 2012 James Simmons <jsimmons@infradead.org> All Rights Reserved.
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
#ifndef _VIA_DISPLAY_H_
#define _VIA_DISPLAY_H_

#include <video/vga.h>
#include "crtc_hw.h"

#include "drm_fb_helper.h"

struct via_i2c {
	struct drm_via_private *dev_priv;
	struct via_port_cfg *adap_cfg;
	struct i2c_algo_bit_data algo;
	struct i2c_adapter adapter;
	u16 i2c_port;
};

struct via_crtc {
	struct drm_crtc base;
	struct ttm_bo_kmap_obj cursor_kmap;
	struct crtc_timings timings;
	unsigned int display_queue_expire_num;
	unsigned int fifo_high_threshold;
	unsigned int fifo_threshold;
	unsigned int fifo_max_depth;
	struct vga_registers display_queue;
	struct vga_registers high_threshold;
	struct vga_registers threshold;
	struct vga_registers fifo_depth;
	struct vga_registers offset;
	struct vga_registers fetch;
	uint8_t index;
};

#define DISP_DI_NONE		0x00
#define DISP_DI_DVP0		BIT(0)
#define DISP_DI_DVP1		BIT(1)
#define DISP_DI_DFPL		BIT(2)
#define DISP_DI_DFPH		BIT(3)
#define DISP_DI_DFP		BIT(4)
#define DISP_DI_DAC		BIT(5)

struct via_encoder {
	struct drm_encoder base;
	int diPort;
};

static inline void
via_lock_crt(void __iomem *regs)
{
	svga_wcrt_mask(regs, 0x11, BIT(7), BIT(7));
}

static inline void
via_unlock_crt(void __iomem *regs, int pci_id)
{
	u8 mask = BIT(0);

	svga_wcrt_mask(regs, 0x11, 0, BIT(7));
	if ((pci_id == PCI_DEVICE_ID_VIA_VX875) ||
	    (pci_id == PCI_DEVICE_ID_VIA_VX900))
		mask = BIT(4);
	svga_wcrt_mask(regs, 0x47, 0, mask);
}

/* display */
extern int via_modeset_init(struct drm_device *dev);
extern void via_modeset_fini(struct drm_device *dev);

/* i2c */
extern void via_i2c_exit(struct drm_device *dev);
extern int via_i2c_init(struct drm_device *dev);

/* clock */
extern void via_set_pll(struct drm_crtc *crtc, struct drm_display_mode *mode);

/* framebuffers */
extern int via_framebuffer_init(struct drm_device *dev, struct drm_fb_helper **ptr);
extern void via_framebuffer_fini(struct drm_fb_helper *helper);

/* crtc */
extern void via_crtc_init(struct drm_device *dev, int index);

/* encoders */
extern struct drm_encoder* via_best_encoder(struct drm_connector *connector);
extern void via_diport_set_source(struct drm_encoder *encoder);

/* outputs */
extern int via_get_edid_modes(struct drm_connector *connector);

extern void via_analog_init(struct drm_device *dev);
extern void via_lvds_init(struct drm_device *dev);

#endif
