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
	enum dma_data_direction direction;
	struct ttm_backend backend;
	struct sg_table *table;
	struct drm_device *dev;
	unsigned long offset;
};

static int ttm_sgdma_populate(struct ttm_backend *be, unsigned long num_pages,
				struct page **pages,
				struct page *dummy_read_page,
				dma_addr_t *dma_addrs)
{
	struct ttm_sgdma_backend *dma_be =
		container_of(be, struct ttm_sgdma_backend, backend);
	struct sg_table *sgtab = dma_be->table;
	struct scatterlist *sg;
	int ret = 0, i;

	ret = sg_alloc_table(sgtab, num_pages, GFP_KERNEL);
	if (!ret) {
		struct drm_sg_mem *entry = kzalloc(sizeof(*entry), GFP_KERNEL);

		entry->busaddr = dma_addrs;
		entry->pagelist = pages;

                for_each_sg(sgtab->sgl, sg, sgtab->nents, i)
			sg_set_page(sg, pages[i], PAGE_SIZE, 0);

		entry->pages = dma_map_sg(dma_be->dev->dev, sgtab->sgl,
					sgtab->nents, dma_be->direction);
		dma_be->dev->sg = entry;
	}
	return ret;
}

static int via_pcie_sgdma_bind(struct ttm_backend *be, struct ttm_mem_reg *mem)
{
	struct ttm_sgdma_backend *dma_be =
		container_of(be, struct ttm_sgdma_backend, backend);
	struct drm_via_private *dev_priv = dma_be->dev->dev_private;
	struct drm_sg_mem *entry = dma_be->dev->sg;
	u8 orig;
	int i;

	/* Disable gart table HW protect */
	orig = (vga_rseq(VGABASE, 0x6C) & 0x7F);
	vga_wseq(VGABASE, 0x6C, orig);

	/* Update the relevant entries */
	dma_be->offset = mem->start << PAGE_SHIFT;
	for (i = 0; i < entry->pages; i++) {
		writel(page_to_pfn(entry->pagelist[i]) & 0x3FFFFFFF,
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

static int via_pcie_sgdma_unbind(struct ttm_backend *be)
{
	struct ttm_sgdma_backend *dma_be =
		container_of(be, struct ttm_sgdma_backend, backend);
	struct drm_via_private *dev_priv = dma_be->dev->dev_private;
	struct drm_sg_mem *entry = dma_be->dev->sg;
	u8 orig;
	int i;

	/* Disable gart table HW protect */
	orig = (vga_rseq(VGABASE, 0x6C) & 0x7F);
	vga_wseq(VGABASE, 0x6C, orig);

	/* Update the relevant entries */
	for (i = 0; i < entry->pages; i++)
		writel(0x80000000, dev_priv->gart.virtual + dma_be->offset + i);
	dma_be->offset = 0;

	/* Invalided GTI cache */
	orig = (vga_rseq(VGABASE, 0x6F) | 0x80);
	vga_wseq(VGABASE, 0x6F, orig);

	/* Enable gart table HW protect */
	orig = (vga_rseq(VGABASE, 0x6C) | 0x80);
	vga_wseq(VGABASE, 0x6C, orig);

	return 1;
}

static void ttm_sgdma_destroy(struct ttm_backend *be)
{
	struct ttm_sgdma_backend *dma_be =
		container_of(be, struct ttm_sgdma_backend, backend);
	struct sg_table *sgtab = dma_be->table;

	if (dma_be->offset)
		be->func->unbind(be);

	dma_unmap_sg(dma_be->dev->dev, sgtab->sgl, sgtab->nents,
			dma_be->direction);
	kfree(dma_be->dev->sg);
	dma_be->dev->sg = NULL;
}

/* Called after destroy */
static void ttm_sgdma_clear(struct ttm_backend *be)
{
	struct ttm_sgdma_backend *dma_be =
		container_of(be, struct ttm_sgdma_backend, backend);
	struct sg_table *sgtab = dma_be->table;

	if (dma_be->dev->sg)
		be->func->destroy(be);

	sg_free_table(sgtab);
	kfree(dma_be);
}

static struct ttm_backend_func ttm_sgdma_func = {
	.populate = ttm_sgdma_populate,
	.clear = ttm_sgdma_clear,
	.bind = via_pcie_sgdma_bind,
	.unbind = via_pcie_sgdma_unbind,
	.destroy = ttm_sgdma_destroy,
};

struct ttm_backend *via_sgdma_backend_init(struct ttm_bo_device *bdev, struct drm_device *dev)
{
	struct ttm_sgdma_backend *dma_be;

	dma_be = kzalloc(sizeof(*dma_be), GFP_KERNEL);
	if (!dma_be)
		return NULL;

	dma_be->backend.func = &ttm_sgdma_func;
	dma_be->backend.bdev = bdev;
	dma_be->direction = DMA_BIDIRECTIONAL;
	dma_be->dev = dev;
	return &dma_be->backend;
}
EXPORT_SYMBOL(via_sgdma_backend_init);
