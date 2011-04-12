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

#include "ttm/ttm_module.h"
#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_placement.h"

#include "drmP.h"

#include <linux/module.h>
#include <linux/slab.h>

struct ttm_pci_backend {
	struct ttm_backend backend;
	struct drm_device *dev;
};

static int ttm_pci_populate(struct ttm_backend *backend,
			    unsigned long num_pages, struct page **pages,
			    struct page *dummy_read_page,
			    dma_addr_t *dma_addrs)
{	DRM_INFO("ttm_pci_populate\n");
	return 0;
}

static int ttm_pci_bind(struct ttm_backend *backend, struct ttm_mem_reg *bo_mem)
{
	DRM_INFO("ttm_pci_bind\n");
	return 1;
}

static int ttm_pci_unbind(struct ttm_backend *backend)
{
	DRM_INFO("ttm_pci_unbind\n");
	return 1;
}

static void ttm_pci_clear(struct ttm_backend *backend)
{
	DRM_INFO("ttm_pci_clear\n");
}

static void ttm_pci_destroy(struct ttm_backend *backend)
{
	DRM_INFO("ttm_pci_destroy\n");
}

static struct ttm_backend_func ttm_pci_func = {
	.populate = ttm_pci_populate,
	.clear = ttm_pci_clear,
	.bind = ttm_pci_bind,
	.unbind = ttm_pci_unbind,
	.destroy = ttm_pci_destroy,
};

struct ttm_backend *ttm_pci_backend_init(struct ttm_bo_device *bdev, struct drm_device *dev)
{
	struct ttm_pci_backend *pci_be;

	pci_be = kmalloc(sizeof(*pci_be), GFP_KERNEL);
	if (!pci_be)
		return NULL;

	pci_be->backend.func = &ttm_pci_func;
	pci_be->backend.bdev = bdev;
	pci_be->dev = dev;
	return &pci_be->backend;
}
EXPORT_SYMBOL(ttm_pci_backend_init);
