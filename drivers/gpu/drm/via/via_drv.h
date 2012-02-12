/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
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
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef _VIA_DRV_H_
#define _VIA_DRV_H_

#define DRIVER_AUTHOR		"Various"

#define DRIVER_NAME		"via"
#define DRIVER_DESC		"VIA Unichrome / Pro"
#define DRIVER_DATE		"20110221"

#define DRIVER_MAJOR		3
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	0

#include "ttm/ttm_bo_api.h"
#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_placement.h"
#include "ttm/ttm_memory.h"
#include "ttm/ttm_module.h"
#include "ttm/ttm_page_alloc.h"

#include <video/vga.h>
#include "crtc_hw.h"
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/via-core.h>

#include "via_regs.h"
#include "via_drm.h"
#include "via_verifier.h"
#include "via_dmabuffer.h"

#define VIA_MM_ALIGN_SIZE 16

#define VIA_PCI_BUF_SIZE 60000
#define VIA_FIRE_BUF_SIZE  1024
#define VIA_NUM_IRQS 4

#define DRM_FILE_PAGE_OFFSET (0x100000000ULL >> PAGE_SHIFT)

#define CLE266_REVISION_AX	0x0A
#define CLE266_REVISION_CX	0x0C
#define CX700_REVISION_700	0x0
#define CX700_REVISION_700M	0x1
#define CX700_REVISION_700M2	0x2

typedef uint32_t maskarray_t[5];

typedef struct drm_via_irq {
	atomic_t irq_received;
	uint32_t pending_mask;
	uint32_t enable_mask;
	wait_queue_head_t irq_queue;
} drm_via_irq_t;

struct sgdma_tt {
        struct ttm_dma_tt sgdma;
        unsigned long offset;
};

enum via_engine {
	VIA_ENG_H1 = 0,
	VIA_ENG_H2,
	VIA_ENG_H5S1,
	VIA_ENG_H5S2VP1,
	VIA_ENG_H6S2
};

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
	struct registers fetch;
	unsigned int display_queue_expire_num;
	unsigned int fifo_high_threshold;
	unsigned int fifo_threshold;
	unsigned int fifo_max_depth;
	struct registers display_queue;
	struct registers high_threshold;
	struct registers threshold;
	struct registers fifo_depth;
	uint32_t index;
};

struct drm_via_private {
	struct drm_global_reference mem_global_ref;
	struct ttm_bo_global_ref bo_global_ref;
	struct ttm_bo_device bdev;
	struct drm_device *dev;
	int revision;
	drm_via_sarea_t *sarea_priv;
	drm_local_map_t *sarea;
	struct drm_fb_helper *helper;
	u8 vram_type;
	wait_queue_head_t decoder_queue[VIA_NR_XVMC_LOCKS];
	struct ttm_bo_kmap_obj dmabuf;
	struct ttm_bo_kmap_obj mmio;
	struct ttm_bo_kmap_obj gart;
	struct ttm_bo_kmap_obj vq;
	enum via_engine engine_type;
	unsigned int dma_low;
	unsigned int dma_high;
	unsigned int dma_offset;
	uint32_t dma_wrap;
	void __iomem *last_pause_ptr;
	void __iomem *hw_addr_ptr;
	struct timeval last_vblank;
	int last_vblank_valid;
	unsigned usec_per_vblank;
	struct drm_via_state hc_state;
	char pci_buf[VIA_PCI_BUF_SIZE];
	const uint32_t *fire_offsets[VIA_FIRE_BUF_SIZE];
	uint32_t num_fire_offsets;
	drm_via_irq_t via_irqs[VIA_NUM_IRQS];
	unsigned num_irqs;
	maskarray_t *irq_masks;
	uint32_t irq_enable_mask;
	uint32_t irq_pending_mask;
	int *irq_map;
	unsigned int idle_fault;
	unsigned long vram_offset;
	unsigned long agp_offset;
	drm_via_blitq_t blit_queues[VIA_NUM_BLIT_ENGINES];
	uint32_t dma_diff;
	struct via_crtc iga[2];
	struct via_i2c i2c_par[5];
};

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

/* VIA MMIO register access */
#define VIA_BASE ((dev_priv->mmio.virtual))

#define VIA_READ(reg)		ioread32(VIA_BASE + reg)
#define VIA_WRITE(reg, val)	iowrite32(val, VIA_BASE + reg)
#define VIA_READ8(reg)		ioread8(VIA_BASE + reg)
#define VIA_WRITE8(reg, val)	iowrite8(val, VIA_BASE + reg)

#define VGABASE (VIA_BASE+VIA_MMIO_VGABASE)

extern struct drm_ioctl_desc via_ioctls[];
extern int via_max_ioctl;

extern int via_dma_init(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int via_flush_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int via_dispatch_cmdbuffer(struct drm_device *dev, drm_via_cmdbuffer_t *cmd);
extern int via_cmdbuffer(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int via_cmdbuf_size(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int via_pci_cmdbuffer(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int via_decoder_futex(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int via_wait_irq(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int via_dma_blit_sync(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int via_dma_blit(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int via_wait_idle(struct drm_via_private *dev_priv);

extern int via_detect_vram(struct drm_device *dev);
extern int via_framebuffer_init(struct drm_device *dev, struct drm_fb_helper **ptr);
extern void via_framebuffer_fini(struct drm_fb_helper *helper);

extern int via_ttm_init(struct drm_via_private *dev_priv);
extern void via_ttm_bo_destroy(struct ttm_buffer_object *bo);
extern struct ttm_tt *via_sgdma_backend_init(struct ttm_bo_device *bdev, unsigned long size,
					uint32_t page_flags, struct page *dummy_read_page);

extern int ttm_global_init(struct drm_global_reference *global_ref,
				struct ttm_bo_global_ref *global_bo,
				struct ttm_bo_driver *driver,
				struct ttm_bo_device *bdev,
				bool dma32);
extern void ttm_global_fini(struct drm_global_reference *global_ref,
				struct ttm_bo_global_ref *global_bo,
				struct ttm_bo_device *bdev);

extern int ttm_bo_allocate(struct ttm_bo_device *bdev, unsigned long size,
				enum ttm_bo_type origin, int types,
				uint32_t byte_align, uint32_t page_align,
				unsigned long buffer_start, bool interruptible,
				void (*destroy) (struct ttm_buffer_object *),
				struct file *persistant_swap_storage,
				struct ttm_buffer_object **p_bo);
extern int ttm_bo_unpin(struct ttm_buffer_object *bo, struct ttm_bo_kmap_obj *kmap);
extern int ttm_bo_pin(struct ttm_buffer_object *bo, struct ttm_bo_kmap_obj *kmap);
extern int ttm_mmap(struct file *filp, struct vm_area_struct *vma);

extern int ttm_gem_init_object(struct drm_gem_object *obj);
extern void ttm_gem_free_object(struct drm_gem_object *obj);
extern struct drm_gem_object *ttm_gem_create(struct drm_device *dev,
					struct ttm_bo_device *bdev, int type,
					bool interruptible,
					int byte_align, int page_align,
					unsigned long start, unsigned long size,
					void (*destroy) (struct ttm_buffer_object *));

extern int via_enable_vblank(struct drm_device *dev, int crtc);
extern void via_disable_vblank(struct drm_device *dev, int crtc);

extern irqreturn_t via_driver_irq_handler(DRM_IRQ_ARGS);
extern void via_driver_irq_preinstall(struct drm_device *dev);
extern int via_driver_irq_postinstall(struct drm_device *dev);
extern void via_driver_irq_uninstall(struct drm_device *dev);

extern void via_init_command_verifier(void);
extern int via_driver_dma_quiescent(struct drm_device *dev);
extern int via_dma_cleanup(struct drm_device *dev);
extern void via_init_futex(struct drm_via_private *dev_priv);
extern void via_cleanup_futex(struct drm_via_private *dev_priv);
extern void via_release_futex(struct drm_via_private *dev_priv, int context);

extern void via_dmablit_handler(struct drm_device *dev, int engine, int from_irq);
extern void via_init_dmablit(struct drm_device *dev);

extern void via_modeset_fini(struct drm_device *dev);
extern int via_modeset_init(struct drm_device *dev);

extern void via_i2c_exit(struct drm_device *dev);
extern int via_i2c_init(struct drm_device *dev);
extern int via_get_edid_modes(struct drm_connector *connector);

extern void via_lock_crt(void __iomem *regs);
extern void via_unlock_crt(void __iomem *regs);
extern void via_set_pll(struct drm_crtc *crtc, struct drm_display_mode *mode);

extern struct drm_encoder* via_best_encoder(struct drm_connector *connector);
extern void via_analog_init(struct drm_device *dev);

#endif
