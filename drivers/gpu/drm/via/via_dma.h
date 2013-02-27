/* via_dma.h -- PCI DMA BitBlt support for the VIA Unichrome/Pro
 *
 * Copyright 2005 Thomas Hellstrom.
 * All Rights Reserved.
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
 *    Register info from Digeo Inc.
 */

#ifndef _VIA_DMA_H
#define _VIA_DMA_H

#include <linux/dma-mapping.h>

#define VIA_NUM_BLIT_ENGINES 2
#define VIA_NUM_BLIT_SLOTS 8

#define VIA_OUT_RING_H1(nReg, nData) {				\
	*((uint32_t *)(vb)) = ((nReg) >> 2) | HALCYON_HEADER1;	\
	*((uint32_t *)(vb) + 1) = (nData);			\
        vb = ((uint32_t *)vb) + 2;				\
        dev_priv->dma_low += 8;					\
}

/* For H5/6 2D cmd load(Two value :cmd with address)***/
#define VIA_OUT_RING_QW(w1, w2) do {		\
        *vb++ = (w1);				\
        *vb++ = (w2);				\
	dev_priv->dma_low += 8;			\
} while (0)

struct via_h1_header {
	uint32_t mem_addr;
	uint32_t dev_addr;
	uint32_t size;
	uint32_t next;
};

struct drm_via_sg_info {
	struct ttm_tt *ttm;
	unsigned long **desc_pages;
	int num_desc_pages;
	int num_desc;
	enum dma_data_direction direction;
	unsigned long dev_start;
	dma_addr_t chain_start;
	unsigned int descriptors_per_page;
	int aborted;
	enum {
		dr_via_device_mapped,
		dr_via_desc_pages_alloc,
		dr_via_pages_locked,
		dr_via_pages_alloc,
		dr_via_sg_init
	} state;
};

/*
 *  PCI DMA Registers
 *  Channels 2 & 3 don't seem to be implemented in hardware.
 */

#define VIA_PCI_DMA_MAR0            0xE40   /* Memory Address Register of Channel 0 */
#define VIA_PCI_DMA_DAR0            0xE44   /* Device Address Register of Channel 0 */
#define VIA_PCI_DMA_BCR0            0xE48   /* Byte Count Register of Channel 0 */
#define VIA_PCI_DMA_DPR0            0xE4C   /* Descriptor Pointer Register of Channel 0 */

#define VIA_PCI_DMA_MAR1            0xE50   /* Memory Address Register of Channel 1 */
#define VIA_PCI_DMA_DAR1            0xE54   /* Device Address Register of Channel 1 */
#define VIA_PCI_DMA_BCR1            0xE58   /* Byte Count Register of Channel 1 */
#define VIA_PCI_DMA_DPR1            0xE5C   /* Descriptor Pointer Register of Channel 1 */

#define VIA_PCI_DMA_MAR2            0xE60   /* Memory Address Register of Channel 2 */
#define VIA_PCI_DMA_DAR2            0xE64   /* Device Address Register of Channel 2 */
#define VIA_PCI_DMA_BCR2            0xE68   /* Byte Count Register of Channel 2 */
#define VIA_PCI_DMA_DPR2            0xE6C   /* Descriptor Pointer Register of Channel 2 */

#define VIA_PCI_DMA_MAR3            0xE70   /* Memory Address Register of Channel 3 */
#define VIA_PCI_DMA_DAR3            0xE74   /* Device Address Register of Channel 3 */
#define VIA_PCI_DMA_BCR3            0xE78   /* Byte Count Register of Channel 3 */
#define VIA_PCI_DMA_DPR3            0xE7C   /* Descriptor Pointer Register of Channel 3 */

#define VIA_PCI_DMA_MR0             0xE80   /* Mode Register of Channel 0 */
#define VIA_PCI_DMA_MR1             0xE84   /* Mode Register of Channel 1 */
#define VIA_PCI_DMA_MR2             0xE88   /* Mode Register of Channel 2 */
#define VIA_PCI_DMA_MR3             0xE8C   /* Mode Register of Channel 3 */

#define VIA_PCI_DMA_CSR0            0xE90   /* Command/Status Register of Channel 0 */
#define VIA_PCI_DMA_CSR1            0xE94   /* Command/Status Register of Channel 1 */
#define VIA_PCI_DMA_CSR2            0xE98   /* Command/Status Register of Channel 2 */
#define VIA_PCI_DMA_CSR3            0xE9C   /* Command/Status Register of Channel 3 */

#define VIA_PCI_DMA_PTR             0xEA0   /* Priority Type Register */

/* Define for DMA engine */
/* DPR */
#define VIA_DMA_DPR_EC		(1<<1)	/* end of chain */
#define VIA_DMA_DPR_DDIE	(1<<2)	/* descriptor done interrupt enable */
#define VIA_DMA_DPR_DT		(1<<3)	/* direction of transfer (RO) */

/* MR */
#define VIA_DMA_MR_CM		(1<<0)	/* chaining mode */
#define VIA_DMA_MR_TDIE		(1<<1)	/* transfer done interrupt enable */
#define VIA_DMA_MR_HENDMACMD		(1<<7) /* ? */

/* CSR */
#define VIA_DMA_CSR_DE		(1<<0)	/* DMA enable */
#define VIA_DMA_CSR_TS		(1<<1)	/* transfer start */
#define VIA_DMA_CSR_TA		(1<<2)	/* transfer abort */
#define VIA_DMA_CSR_TD		(1<<3)	/* transfer done */
#define VIA_DMA_CSR_DD		(1<<4)	/* descriptor done */
#define VIA_DMA_DPR_EC          (1<<1)  /* end of chain */

#endif
