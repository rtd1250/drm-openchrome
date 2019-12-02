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

#ifndef _OPENCHROME_DRV_H
#define _OPENCHROME_DRV_H


#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>

#include <video/vga.h>

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_plane.h>

#include <drm/ttm/ttm_bo_api.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_placement.h>

#include <drm/via_drm.h>

#include "openchrome_crtc_hw.h"
#include "openchrome_regs.h"


#define DRIVER_MAJOR		3
#define DRIVER_MINOR		1
#define DRIVER_PATCHLEVEL	22
#define DRIVER_NAME		"openchrome"
#define DRIVER_DESC		"OpenChrome DRM for VIA Technologies Chrome IGP"
#define DRIVER_DATE		"20191202"
#define DRIVER_AUTHOR		"OpenChrome Project"


#define OPENCHROME_TTM_PL_NUM	2

#define OPENCHROME_MAX_CRTC	2

#define VIA_MM_ALIGN_SIZE	16


#define CLE266_REVISION_AX	0x0A
#define CLE266_REVISION_CX	0x0C

#define CX700_REVISION_700	0x0
#define CX700_REVISION_700M	0x1
#define CX700_REVISION_700M2	0x2

#define VIA_MEM_NONE		0x00
#define VIA_MEM_SDR66		0x01
#define VIA_MEM_SDR100		0x02
#define VIA_MEM_SDR133		0x03
#define VIA_MEM_DDR_200		0x04
#define VIA_MEM_DDR_266		0x05
#define VIA_MEM_DDR_333		0x06
#define VIA_MEM_DDR_400		0x07
#define VIA_MEM_DDR2_400	0x08
#define VIA_MEM_DDR2_533	0x09
#define VIA_MEM_DDR2_667	0x0A
#define VIA_MEM_DDR2_800	0x0B
#define VIA_MEM_DDR2_1066	0x0C
#define VIA_MEM_DDR3_533	0x0D
#define VIA_MEM_DDR3_667	0x0E
#define VIA_MEM_DDR3_800	0x0F
#define VIA_MEM_DDR3_1066	0x10
#define VIA_MEM_DDR3_1333	0x11
#define VIA_MEM_DDR3_1600	0x12

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
	uint32_t index;
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

struct via_state {
	struct vga_regset crt_regs[256];
	struct vga_regset seq_regs[256];
};

struct openchrome_bo {
	struct ttm_buffer_object	ttm_bo;
	struct ttm_bo_kmap_obj		kmap;
	struct ttm_placement		placement;
	struct ttm_place		placements[OPENCHROME_TTM_PL_NUM];
	struct drm_gem_object		gem;
};

struct via_framebuffer {
	struct drm_framebuffer		fb;
	struct drm_gem_object		*gem;
};

struct via_framebuffer_device {
	struct drm_fb_helper		helper;
	struct via_framebuffer		via_fb;
	struct openchrome_bo		*bo;
};

enum via_engine {
	VIA_ENG_H1 = 0,
	VIA_ENG_H2,
	VIA_ENG_H5S1,
	VIA_ENG_H5S2VP1,
	VIA_ENG_H6S2
};

struct openchrome_drm_private {
	struct drm_device *dev;

	struct ttm_bo_device		bdev;
	struct drm_vma_offset_manager	vma_manager;

	/* Set this flag for ttm_bo_device_init. */
	bool need_dma32;

	int revision;

	struct via_framebuffer_device *via_fbdev;
	u8 vram_type;
	unsigned long long vram_start;
	unsigned int vram_size;
	int vram_mtrr;

	unsigned long long	mmio_base;
	unsigned int		mmio_size;
	void __iomem		*mmio;

	struct via_state pm_cache;

	enum via_engine engine_type;

	bool spread_spectrum;

	/*
	 * Frame Buffer Size Control register (SR14) needs to be saved and
	 * restored upon standby resume or can lead to a display corruption
	 * issue. These registers are only available on VX800, VX855, and
	 * VX900 chipsets. This bug was observed on VIA EPIA-M830 mainboard.
	 */
	uint8_t saved_sr14;

	/*
	 * GTI registers (SR66 through SR6F) need to be saved and restored
	 * upon standby resume or can lead to a display corruption issue.
	 * These registers are only available on VX800, VX855, and VX900
	 * chipsets. This bug was observed on VIA EPIA-M830 mainboard.
	 */
	uint8_t saved_sr66;
	uint8_t saved_sr67;
	uint8_t saved_sr68;
	uint8_t saved_sr69;
	uint8_t saved_sr6a;
	uint8_t saved_sr6b;
	uint8_t saved_sr6c;
	uint8_t saved_sr6d;
	uint8_t saved_sr6e;
	uint8_t saved_sr6f;

	/* 3X5.3B through 3X5.3F are scratch pad registers.
	 * They are important for FP detection. */
	uint8_t saved_cr3b;
	uint8_t saved_cr3c;
	uint8_t saved_cr3d;
	uint8_t saved_cr3e;
	uint8_t saved_cr3f;

	/*
	 * Due to the way VIA NanoBook reference design implemented
	 * the pin strapping settings, DRM needs to ignore them for
	 * FP and DVI to be properly detected.
	 */
	bool is_via_nanobook;

	/*
	 * Quanta IL1 netbook has its FP connected to DVP1
	 * rather than LVDS, hence, a special flag register
	 * is needed for properly controlling its FP.
	 */
	bool is_quanta_il1;

	/*
	 * Samsung NC20 netbook has its FP connected to LVDS2
	 * rather than the more logical LVDS1, hence, a special
	 * flag register is needed for properly controlling its
	 * FP.
	 */
	bool is_samsung_nc20;

	bool analog_presence;
	u32 analog_i2c_bus;

	bool int_tmds_presence;
	u32 int_tmds_di_port;
	u32 int_tmds_i2c_bus;

	bool ext_tmds_presence;
	u32 ext_tmds_di_port;
	u32 ext_tmds_i2c_bus;
	u32 ext_tmds_transmitter;

	bool int_fp1_presence;
	u32 int_fp1_di_port;
	u32 int_fp1_i2c_bus;

	bool int_fp2_presence;
	u32 int_fp2_di_port;
	u32 int_fp2_i2c_bus;

	/* Keeping track of the number of DVI connectors. */
	u32 number_dvi;

	/* Keeping track of the number of FP (Flat Panel) connectors. */
	u32 number_fp;

	u32 mapped_i2c_bus;
};


/* VIA MMIO register access */
#define VIA_BASE ((dev_private->mmio))

#define VIA_READ(reg)		ioread32(VIA_BASE + reg)
#define VIA_WRITE(reg, val)	iowrite32(val, VIA_BASE + reg)
#define VIA_READ8(reg)		ioread8(VIA_BASE + reg)
#define VIA_WRITE8(reg, val)	iowrite8(val, VIA_BASE + reg)

#define VIA_WRITE_MASK(reg, data, mask) \
	VIA_WRITE(reg, (data & mask) | (VIA_READ(reg) & ~mask)) \

#define VGABASE (VIA_BASE+VIA_MMIO_VGABASE)


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


extern const struct drm_ioctl_desc via_ioctls[];
extern int via_max_ioctl;

extern int via_hdmi_audio;

extern struct ttm_bo_driver openchrome_bo_driver;

int openchrome_vram_detect(struct openchrome_drm_private *dev_private);
extern int openchrome_vram_init(
			struct openchrome_drm_private *dev_private);
extern void openchrome_vram_fini(
			struct openchrome_drm_private *dev_private);
int openchrome_mmio_init(struct openchrome_drm_private *dev_private);
void openchrome_mmio_fini(struct openchrome_drm_private *dev_private);
void openchrome_graphics_unlock(
			struct openchrome_drm_private *dev_private);
void chip_revision_info(struct openchrome_drm_private *dev_private);
void openchrome_flag_init(struct openchrome_drm_private *dev_private);
int openchrome_device_init(struct openchrome_drm_private *dev_private);

int openchrome_dev_pm_ops_suspend(struct device *dev);
int openchrome_dev_pm_ops_resume(struct device *dev);

void openchrome_mode_config_init(
			struct openchrome_drm_private *dev_private);

void openchrome_ttm_domain_to_placement(struct openchrome_bo *bo,
					uint32_t ttm_domain);
void openchrome_ttm_bo_destroy(struct ttm_buffer_object *tbo);
int openchrome_bo_pin(struct openchrome_bo *bo, uint32_t ttm_domain);
int openchrome_bo_unpin(struct openchrome_bo *bo);
int openchrome_bo_create(struct drm_device *dev,
				struct ttm_bo_device *bdev,
				uint64_t size,
				enum ttm_bo_type type,
				uint32_t ttm_domain,
				bool kmap,
				struct openchrome_bo **bo_ptr);
void openchrome_bo_destroy(struct openchrome_bo *bo, bool kmap);
int openchrome_mm_init(struct openchrome_drm_private *dev_private);
void openchrome_mm_fini(struct openchrome_drm_private *dev_private);

void openchrome_transmitter_io_pad_state(
			struct openchrome_drm_private *dev_private,
			uint32_t di_port, bool io_pad_on);
void openchrome_transmitter_clock_drive_strength(
			struct openchrome_drm_private *dev_private,
			u32 di_port, u8 drive_strength);
void openchrome_transmitter_data_drive_strength(
			struct openchrome_drm_private *dev_private,
			u32 di_port, u8 drive_strength);
void openchrome_transmitter_display_source(
			struct openchrome_drm_private *dev_private,
			u32 di_port, int index);

extern const struct drm_plane_funcs openchrome_cursor_drm_plane_funcs;
extern const uint32_t openchrome_cursor_formats[];
extern const unsigned int openchrome_cursor_formats_size;

extern struct drm_fb_helper_funcs via_drm_fb_helper_funcs;

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
int openchrome_crtc_init(struct openchrome_drm_private *dev_private,
				uint32_t index);
void openchrome_crtc_param_init(
			struct openchrome_drm_private *dev_private,
			struct drm_crtc *crtc,
			uint32_t index);

/* encoders */
extern void via_set_sync_polarity(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode);
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

#endif /* _OPENCHROME_DRV_H */
