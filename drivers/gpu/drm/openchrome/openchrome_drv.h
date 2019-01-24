/*
 * Copyright © 2019 Kevin Brace
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
 */

#ifndef _OPENCHROME_DRV_H
#define _OPENCHROME_DRV_H


#include <linux/module.h>

#include <video/vga.h>

#include <drm/drmP.h>
#include <drm/drm_gem.h>

#include "ttm/ttm_bo_api.h"
#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_placement.h"

#include <drm/via_drm.h>

#include "openchrome_display.h"
#include "openchrome_regs.h"


#define DRIVER_MAJOR		3
#define DRIVER_MINOR		1
#define DRIVER_PATCHLEVEL	3
#define DRIVER_NAME		"openchrome"
#define DRIVER_DESC		"OpenChrome DRM for VIA Technologies Chrome IGP"
#define DRIVER_DATE		"20190110"
#define DRIVER_AUTHOR		"OpenChrome Project"


#define VIA_MM_ALIGN_SIZE	16

#define DRM_FILE_PAGE_OFFSET	(0x100000000ULL >> PAGE_SHIFT)


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


struct via_state {
	struct vga_regset crt_regs[256];
	struct vga_regset seq_regs[256];
};

struct via_ttm {
	struct ttm_bo_device bdev;
};

struct ttm_heap {
	struct ttm_buffer_object bo;
	struct ttm_place busy_placements[TTM_NUM_MEM_TYPES];
	struct ttm_place placements[TTM_NUM_MEM_TYPES];
};

struct ttm_gem_object {
	struct drm_gem_object gem;
	struct ttm_heap *heap;
};

struct via_framebuffer {
	struct drm_framebuffer fb;
	struct drm_gem_object *gem_obj;
};

struct via_framebuffer_device {
	struct drm_fb_helper helper;
	struct ttm_bo_kmap_obj kmap;
	struct via_framebuffer via_fb;
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

	struct via_ttm ttm;

	int revision;

	struct ttm_bo_kmap_obj gart;
	struct ttm_bo_kmap_obj vq;

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

	struct via_crtc iga[2];
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

extern const struct drm_ioctl_desc via_ioctls[];
extern int via_max_ioctl;

extern int via_hdmi_audio;

int openchrome_mmio_init(struct openchrome_drm_private *dev_private);
void openchrome_mmio_fini(struct openchrome_drm_private *dev_private);
void openchrome_graphics_unlock(
			struct openchrome_drm_private *dev_private);
void chip_revision_info(struct openchrome_drm_private *dev_private);
void openchrome_flag_init(struct openchrome_drm_private *dev_private);
int openchrome_device_init(struct openchrome_drm_private *dev_private);

extern void via_engine_init(struct drm_device *dev);

int openchrome_dev_pm_ops_suspend(struct device *dev);
int openchrome_dev_pm_ops_resume(struct device *dev);

extern int via_vram_detect(struct openchrome_drm_private *dev_private);
extern int openchrome_vram_init(
			struct openchrome_drm_private *dev_private);
extern void openchrome_vram_fini(
			struct openchrome_drm_private *dev_private);

extern int via_mm_init(struct openchrome_drm_private *dev_private);
void via_mm_fini(struct drm_device *dev);
extern void ttm_placement_from_domain(struct ttm_buffer_object *bo,
			struct ttm_placement *placement,
			u32 domains, struct ttm_bo_device *bdev);
extern int via_bo_create(struct ttm_bo_device *bdev,
				struct ttm_buffer_object **p_bo,
				unsigned long size,
				enum ttm_bo_type type,
				uint32_t domains,
				uint32_t byte_alignment,
				uint32_t page_alignment,
				bool interruptible,
				struct sg_table *sg,
				struct reservation_object *resv);
extern int via_bo_pin(struct ttm_buffer_object *bo,
				struct ttm_bo_kmap_obj *kmap);
extern int via_bo_unpin(struct ttm_buffer_object *bo,
				struct ttm_bo_kmap_obj *kmap);
extern int via_ttm_allocate_kernel_buffer(struct ttm_bo_device *bdev,
				unsigned long size,
				uint32_t alignment, uint32_t domain,
				struct ttm_bo_kmap_obj *kmap);


extern int ttm_mmap(struct file *filp, struct vm_area_struct *vma);

extern int ttm_gem_open_object(struct drm_gem_object *obj,
				struct drm_file *file_priv);
extern void ttm_gem_free_object(struct drm_gem_object *obj);
extern struct drm_gem_object* ttm_gem_create(struct drm_device *dev,
					struct ttm_bo_device *bdev,
					unsigned long size,
					enum ttm_bo_type type,
					uint32_t domains,
					uint32_t byte_alignment,
					uint32_t page_alignment,
					bool interruptible);
extern struct ttm_buffer_object* ttm_gem_mapping(
					struct drm_gem_object *obj);

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

#endif /* _OPENCHROME_DRV_H */
