/* via_h1_dma.c -- PCI DMA BitBlt support for the VIA Unichrome/Pro
 *
 * Copyright (C) 2005 Thomas Hellstrom, All Rights Reserved.
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
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Thomas Hellstrom.
 *    Partially based on code obtained from Digeo Inc.
 */
#include <drm/drmP.h>
#include "via_drv.h"
#include "via_dma.h"

/*
 * Fire a blit engine.
 */
static void
via_h1_fire_dmablit(struct drm_device *dev, struct drm_via_sg_info *vsg, int engine)
{
	struct drm_via_private *dev_priv = dev->dev_private;

	VIA_WRITE(VIA_PCI_DMA_MAR0 + engine * 0x10, 0);
	VIA_WRITE(VIA_PCI_DMA_DAR0 + engine * 0x10, 0);
	VIA_WRITE(VIA_PCI_DMA_CSR0 + engine * 0x04, VIA_DMA_CSR_DD | VIA_DMA_CSR_TD |
		  VIA_DMA_CSR_DE);
	VIA_WRITE(VIA_PCI_DMA_MR0  + engine * 0x04, VIA_DMA_MR_CM | VIA_DMA_MR_TDIE);
	VIA_WRITE(VIA_PCI_DMA_BCR0 + engine * 0x10, 0);
	VIA_WRITE(VIA_PCI_DMA_DPR0 + engine * 0x10, vsg->chain_start);
	wmb();
	VIA_WRITE(VIA_PCI_DMA_CSR0 + engine * 0x04, VIA_DMA_CSR_DE | VIA_DMA_CSR_TS);
	VIA_READ(VIA_PCI_DMA_CSR0  + engine * 0x04);
}

static void
via_abort_dmablit(struct drm_device *dev, int engine)
{
	struct drm_via_private *dev_priv = dev->dev_private;

	VIA_WRITE(VIA_PCI_DMA_CSR0 + engine * 0x04, VIA_DMA_CSR_TA);
}

static void
via_dmablit_engine_off(struct drm_device *dev, int engine)
{
	struct drm_via_private *dev_priv = dev->dev_private;

	VIA_WRITE(VIA_PCI_DMA_CSR0 + engine * 0x04, VIA_DMA_CSR_TD | VIA_DMA_CSR_DD);
}

static void
via_dmablit_done(struct drm_device *dev, int engine)
{
	struct drm_via_private *dev_priv = dev->dev_private;

	/* Clear transfer done flag. */
	VIA_WRITE(VIA_PCI_DMA_CSR0 + engine * 0x04,  VIA_DMA_CSR_TD);
}

/*
 * Unmap a DMA mapping.
 */
static void
via_unmap_from_device(struct drm_device *dev, struct drm_via_sg_info *vsg)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	int num_desc = vsg->num_desc;
	unsigned cur_descriptor_page = num_desc / vsg->descriptors_per_page;
	unsigned descriptor_this_page = num_desc % vsg->descriptors_per_page;
	dma_addr_t next = vsg->chain_start;
	struct via_h1_header *desc_ptr;

	desc_ptr = (struct via_h1_header *) vsg->desc_pages[cur_descriptor_page] +
						descriptor_this_page;
	while (num_desc--) {
		if (descriptor_this_page-- == 0) {
			cur_descriptor_page--;
			descriptor_this_page = vsg->descriptors_per_page - 1;
			desc_ptr = (struct via_h1_header *) vsg->desc_pages[cur_descriptor_page] +
							descriptor_this_page;
		}

		dma_unmap_single(dev->dev, next, dev_priv->desc_size, DMA_TO_DEVICE);
		dma_unmap_page(dev->dev, desc_ptr->mem_addr, desc_ptr->size, vsg->direction);
		next = (dma_addr_t) desc_ptr->next;
		desc_ptr--;
	}
}

/*
 * Map the DMA pages for the device, put together and map also the descriptors. Descriptors
 * are run in reverse order by the hardware because we are not allowed to update the
 * 'next' field without syncing calls when the descriptor is already mapped.
 */
static int
via_map_for_device(struct via_fence_engine *eng, struct drm_via_sg_info *vsg,
			unsigned long offset)
{
	unsigned int num_descriptors_this_page = 0, cur_descriptor_page = 0;
	unsigned long dev_start = eng->pool->fence_sync.bo->offset;
	struct device *dev = eng->pool->dev->dev;
	dma_addr_t next = VIA_DMA_DPR_EC;
	struct via_h1_header *desc_ptr;
	struct ttm_tt *ttm = vsg->ttm;
	int num_desc = 0, ret = 0;

	desc_ptr = (struct via_h1_header *) vsg->desc_pages[cur_descriptor_page];
	dev_start = vsg->dev_start;

	for (num_desc = 0; num_desc < ttm->num_pages; num_desc++) {
		/* Map system pages */
		if (!ttm->pages[num_desc]) {
			ret = -ENOMEM;
			goto out;
		}
		desc_ptr->mem_addr = dma_map_page(dev, ttm->pages[num_desc], 0,
						PAGE_SIZE, vsg->direction);
		desc_ptr->dev_addr = dev_start;
		/* size count in 16 bytes */
		desc_ptr->size = PAGE_SIZE / 16;
		desc_ptr->next = (uint32_t) next;

		/* Map decriptors for Chaining mode */
		next = dma_map_single(dev, desc_ptr, sizeof(*desc_ptr), DMA_TO_DEVICE);
		desc_ptr++;
		if (++num_descriptors_this_page >= vsg->descriptors_per_page) {
			num_descriptors_this_page = 0;
			desc_ptr = (struct via_h1_header *) vsg->desc_pages[++cur_descriptor_page];
		}
		dev_start += PAGE_SIZE;
	}

	vsg->chain_start = next;
	vsg->state = dr_via_device_mapped;
out:
	return ret;
}

/*
 * Function that frees up all resources for a blit. It is usable even if the
 * blit info has only been partially built as long as the status enum is consistent
 * with the actual status of the used resources.
 */
static void
via_free_sg_info(struct via_fence *fence)
{
	struct drm_device *dev = fence->pool->dev;
	struct drm_via_sg_info *vsg = fence->priv;
	int i;

	switch (vsg->state) {
	case dr_via_device_mapped:
		via_unmap_from_device(dev, vsg);
	case dr_via_desc_pages_alloc:
		for (i = 0; i < vsg->num_desc_pages; ++i) {
			if (vsg->desc_pages[i])
				free_page((unsigned long)vsg->desc_pages[i]);
		}
		kfree(vsg->desc_pages);
	default:
		vsg->state = dr_via_sg_init;
	}
}

static void
via_h1_dma_fence_signaled(struct via_fence_engine *eng)
{
	via_dmablit_done(eng->pool->dev, eng->index);
}

/*
 * Build all info and do all mappings required for a blit.
 */
static int
via_h1_dma_emit(struct via_fence *fence)
{
	struct via_fence_engine *eng = &fence->pool->engines[fence->engine];
	unsigned long offset = VIA_FENCE_SIZE * eng->index;
	struct drm_via_sg_info *vsg = fence->priv;
	int ret = 0;

	ret = via_map_for_device(eng, vsg, offset);
	if (!ret) {
		writel(fence->seq.key, eng->read_seq);
		via_h1_fire_dmablit(fence->pool->dev, vsg, fence->engine);
	}
	return ret;
}

/*
 * Init all blit engines. Currently we use two, but some hardware have 4.
 */
int
via_dmablit_init(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	struct via_fence_pool *pool;

	pci_set_master(dev->pdev);

	pool = via_fence_pool_init(dev, "viadrm_dma", TTM_PL_FLAG_VRAM, 4);
	if (IS_ERR(pool))
		return PTR_ERR(pool);

	pool->fence_signaled = via_h1_dma_fence_signaled;
	pool->fence_cleanup = via_free_sg_info;
	pool->fence_emit = via_h1_dma_emit;

	dev_priv->dma_fences = pool;
	dev_priv->desc_size = sizeof(struct via_h1_header);
	return 0;
}
