/**************************************************************************
 *
 * Copyright (c) 2011 James Simmons <jsimmons@infradead.org>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
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
 */

#include "drmP.h"
#include "via_drv.h"

struct ttm_sgdma_backend {
	struct ttm_dma_tt sgdma;
	unsigned long offset;
};

static int
via_pcie_sgdma_bind(struct ttm_tt *ttm, struct ttm_mem_reg *mem)
{
	struct ttm_sgdma_backend *dma_be = (struct ttm_sgdma_backend *)ttm;
	struct ttm_bo_device *bdev = dma_be->sgdma.ttm.bdev;
	struct drm_via_private *dev_priv =
                container_of(bdev, struct drm_via_private, bdev);
	u8 orig;
	int i;

	/* Disable gart table HW protect */
	orig = (vga_rseq(VGABASE, 0x6C) & 0x7F);
	vga_wseq(VGABASE, 0x6C, orig);

	/* Update the relevant entries */
	dma_be->offset = mem->start << PAGE_SHIFT;
        for (i = 0; i < ttm->num_pages; i++) {
		writel(page_to_pfn(ttm->pages[i]) & 0x3FFFFFFF,
			dev_priv->gart.virtual + dma_be->offset + i);
	}

	/* Invalided GTI cache */
	orig = (vga_rseq(VGABASE, 0x6F) | 0x80);
	vga_wseq(VGABASE, 0x6F, orig);

	/* Enable gart table HW protect */
	orig = (vga_rseq(VGABASE, 0x6C) | 0x80);
	vga_wseq(VGABASE, 0x6C, orig);

	return 1;
}

static int
via_pcie_sgdma_unbind(struct ttm_tt *ttm)
{
	struct ttm_sgdma_backend *dma_be = (struct ttm_sgdma_backend *)ttm;
	struct ttm_bo_device *bdev = dma_be->sgdma.ttm.bdev;
	struct drm_via_private *dev_priv =
                container_of(bdev, struct drm_via_private, bdev);
	u8 orig;
	int i;

	if (ttm->state != tt_bound)
		return 0;

	/* Disable gart table HW protect */
	orig = (vga_rseq(VGABASE, 0x6C) & 0x7F);
	vga_wseq(VGABASE, 0x6C, orig);

	/* Update the relevant entries */
	for (i = 0; i < ttm->num_pages; i++)
		writel(0x80000000, dev_priv->gart.virtual + dma_be->offset + i);
	dma_be->offset = 0;

	/* Invalided GTI cache */
	orig = (vga_rseq(VGABASE, 0x6F) | 0x80);
	vga_wseq(VGABASE, 0x6F, orig);

	/* Enable gart table HW protect */
	orig = (vga_rseq(VGABASE, 0x6C) | 0x80);
	vga_wseq(VGABASE, 0x6C, orig);
	return 0;
}

static void
via_sgdma_destroy(struct ttm_tt *ttm)
{
	struct ttm_sgdma_backend *dma_be = (struct ttm_sgdma_backend *)ttm;

        if (ttm) {
                ttm_dma_tt_fini(&dma_be->sgdma);
                kfree(dma_be);
        }
}

static struct ttm_backend_func ttm_sgdma_func = {
	.bind = via_pcie_sgdma_bind,
	.unbind = via_pcie_sgdma_unbind,
	.destroy = via_sgdma_destroy,
};

struct ttm_tt *
via_sgdma_backend_init(struct ttm_bo_device *bdev, unsigned long size,
			uint32_t page_flags, struct page *dummy_read_page)
{
	struct ttm_sgdma_backend *dma_be;

	dma_be = kzalloc(sizeof(*dma_be), GFP_KERNEL);
	if (!dma_be)
		return NULL;

	dma_be->sgdma.ttm.func = &ttm_sgdma_func;

	if (ttm_dma_tt_init(&dma_be->sgdma, bdev, size, page_flags, dummy_read_page)) {
		kfree(dma_be);
		return NULL;
	}
	return &dma_be->sgdma.ttm;
}
EXPORT_SYMBOL(via_sgdma_backend_init);
