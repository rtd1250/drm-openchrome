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

#include <linux/pci.h>
#include <linux/pci_ids.h>

#include <video/vga.h>

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_plane_helper.h>

#include "openchrome_crtc_hw.h"


/* IGA Scaling disable */
#define VIA_NO_SCALING	0

/* IGA Scaling down */
#define VIA_HOR_SHRINK	BIT(0)
#define VIA_VER_SHRINK	BIT(1)
#define VIA_SHRINK	(BIT(0) | BIT(1))

/* IGA Scaling up */
#define VIA_HOR_EXPAND	BIT(2)
#define VIA_VER_EXPAND	BIT(3)
#define VIA_EXPAND	(BIT(2) | BIT(3))

/* Define IGA Scaling up/down status :  Horizontal or Vertical  */
/* Is IGA Hor scaling up/down status */
#define	HOR_SCALE	BIT(0)
/* Is IGA Ver scaling up/down status */
#define	VER_SCALE	BIT(1)
/* Is IGA Hor and Ver scaling up/down status */
#define	HOR_VER_SCALE	(BIT(0) | BIT(1))

#define	VIA_I2C_NONE	0x0
#define	VIA_I2C_BUS1	BIT(0)
#define	VIA_I2C_BUS2	BIT(1)
#define	VIA_I2C_BUS3	BIT(2)
#define	VIA_I2C_BUS4	BIT(3)
#define	VIA_I2C_BUS5	BIT(4)

#define VIA_DI_PORT_NONE	0x0
#define VIA_DI_PORT_DIP0	BIT(0)
#define VIA_DI_PORT_DIP1	BIT(1)
#define VIA_DI_PORT_DVP0	BIT(2)
#define VIA_DI_PORT_DVP1	BIT(3)
#define VIA_DI_PORT_DFPL	BIT(4)
#define VIA_DI_PORT_FPDPLOW	BIT(4)
#define VIA_DI_PORT_DFPH	BIT(5)
#define VIA_DI_PORT_FPDPHIGH	BIT(5)
#define VIA_DI_PORT_DFP		BIT(6)
#define VIA_DI_PORT_LVDS1	BIT(7)
#define VIA_DI_PORT_TMDS	BIT(7)
#define VIA_DI_PORT_LVDS2	BIT(8)

/* External TMDS (DVI) Transmitter Type */
#define	VIA_TMDS_NONE	0x0
#define	VIA_TMDS_VT1632	BIT(0)
#define	VIA_TMDS_SII164	BIT(1)


typedef struct _via_fp_info {
	u32 x;
	u32 y;
} via_fp_info;

struct via_crtc {
	struct drm_crtc base;
	struct openchrome_bo *cursor_bo;
	struct crtc_timings pixel_timings;
	struct crtc_timings timings;
	struct vga_registers display_queue;
	struct vga_registers high_threshold;
	struct vga_registers threshold;
	struct vga_registers fifo_depth;
	struct vga_registers offset;
	struct vga_registers fetch;
	int scaling_mode;
	uint8_t index;
};

struct via_connector {
	struct drm_connector base;
	u32 i2c_bus;
	struct list_head props;
	uint32_t flags;
};

struct via_encoder {
	struct drm_encoder base;
	u32 i2c_bus;
	u32 di_port;
	struct via_connector cons[];
};

static inline void
via_lock_crtc(void __iomem *regs)
{
	svga_wcrt_mask(regs, 0x11, BIT(7), BIT(7));
}

static inline void
via_unlock_crtc(void __iomem *regs, int pci_id)
{
	u8 mask = BIT(0);

	svga_wcrt_mask(regs, 0x11, 0, BIT(7));
	if ((pci_id == PCI_DEVICE_ID_VIA_VX875) ||
	    (pci_id == PCI_DEVICE_ID_VIA_VX900_VGA))
		mask = BIT(4);
	svga_wcrt_mask(regs, 0x47, 0, mask);
}

static inline void
enable_second_display_channel(void __iomem *regs)
{
	svga_wcrt_mask(regs, 0x6A, BIT(7), BIT(7));
}

static inline void
disable_second_display_channel(void __iomem *regs)
{
	svga_wcrt_mask(regs, 0x6A, 0x00, BIT(7));
}

/* display */
extern int via_modeset_init(struct drm_device *dev);
extern void via_modeset_fini(struct drm_device *dev);

/* i2c */
extern struct i2c_adapter *via_find_ddc_bus(int port);
extern void via_i2c_readbytes(struct i2c_adapter *adapter,
				u8 slave_addr, char offset,
				u8 *buffer, unsigned int size);
extern void via_i2c_writebytes(struct i2c_adapter *adapter,
				u8 slave_addr, char offset,
				u8 *data, unsigned int size);
extern int via_i2c_init(struct drm_device *dev);
extern void via_i2c_exit(void);

/* clock */
extern u32 via_get_clk_value(struct drm_device *dev, u32 clk);
extern void via_set_vclock(struct drm_crtc *crtc, u32 clk);

/* framebuffers */
extern int via_fbdev_init(struct drm_device *dev);
extern void via_fbdev_fini(struct drm_device *dev);

/* crtc */
extern void via_load_crtc_pixel_timing(struct drm_crtc *crtc,
					struct drm_display_mode *mode);
int openchrome_plane_init(struct drm_device *dev, int index);
extern int via_crtc_init(struct drm_device *dev, int index);

/* encoders */
extern void via_set_sync_polarity(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode);
extern struct drm_encoder *via_best_encoder(struct drm_connector *connector);
extern void via_encoder_cleanup(struct drm_encoder *encoder);
extern void via_encoder_prepare(struct drm_encoder *encoder);
extern void via_encoder_disable(struct drm_encoder *encoder);
extern void via_encoder_commit(struct drm_encoder *encoder);

/* connectors */
extern int via_connector_set_property(struct drm_connector *connector,
					struct drm_property *property,
					uint64_t value);
extern int via_connector_mode_valid(struct drm_connector *connector,
					struct drm_display_mode *mode);
extern void via_connector_destroy(struct drm_connector *connector);

extern void via_analog_probe(struct drm_device *dev);
extern bool openchrome_vt1632_probe(struct i2c_adapter *i2c_bus);
extern bool openchrome_sii164_probe(struct i2c_adapter *i2c_bus);
extern void openchrome_ext_dvi_probe(struct drm_device *dev);
extern void via_tmds_probe(struct drm_device *dev);
extern void via_fp_probe(struct drm_device *dev);

extern void via_hdmi_init(struct drm_device *dev, u32 di_port);
extern void via_analog_init(struct drm_device *dev);
extern void openchrome_vt1632_init(struct drm_device *dev);
extern void openchrome_sii164_init(struct drm_device *dev);
extern void openchrome_ext_dvi_init(struct drm_device *dev);
extern void via_tmds_init(struct drm_device *dev);
extern void via_fp_init(struct drm_device *dev);

#endif
