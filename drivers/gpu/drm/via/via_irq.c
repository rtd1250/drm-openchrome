/*
 * Copyright 2004 BEAM Ltd.
 * Copyright 2002 Tungsten Graphics, Inc.
 * Copyright 2005 Thomas Hellstrom.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) OR COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Terry Barnaby <terry1@beam.ltd.uk>
 *    Keith Whitwell <keith@tungstengraphics.com>
 *    Thomas Hellstrom <unichrome@shipmail.org>
 *
 * This code provides standard DRM access to the Via Unichrome / Pro Vertical blank
 * interrupt, as well as an infrastructure to handle other interrupts of the chip.
 * The refresh rate is also calculated for video playback sync purposes.
 */

#include "drmP.h"
#include "via_drv.h"

/* HW Interrupt Register Setting */
#define INTERRUPT_CTRL_REG1		0x200

/* mmio 0x200 IRQ enable and status bits. */
#define VIA_IRQ_ALL_ENABLE		BIT(31)

#define VIA_IRQ_IGA1_VBLANK_STATUS	BIT(1)

#define VIA_IRQ_IGA1_VSYNC_ENABLE	BIT(19)
#define VIA_IRQ_IGA2_VSYNC_ENABLE	BIT(17)
#define VIA_IRQ_IGA1_VSYNC_STATUS	BIT(3)
#define VIA_IRQ_IGA2_VSYNC_STATUS	BIT(15)

#define VIA_IRQ_CAPTURE0_ACTIVE_ENABLE	BIT(28)
#define VIA_IRQ_CAPTURE1_ACTIVE_ENABLE	BIT(24)
#define VIA_IRQ_CAPTURE0_ACTIVE_STATUS	BIT(12)
#define VIA_IRQ_CAPTURE1_ACTIVE_STATUS	BIT(8)

#define VIA_IRQ_HQV0_ENABLE		BIT(25)
#define VIA_IRQ_HQV1_ENABLE		BIT(9)
#define VIA_IRQ_HQV0_STATUS		BIT(12)
#define VIA_IRQ_HQV1_STATUS		BIT(10)

#define VIA_IRQ_DMA0_DD_ENABLE		BIT(20)
#define VIA_IRQ_DMA0_TD_ENABLE		BIT(21)
#define VIA_IRQ_DMA1_DD_ENABLE		BIT(22)
#define VIA_IRQ_DMA1_TD_ENABLE		BIT(23)

#define VIA_IRQ_DMA0_DD_STATUS		BIT(4)
#define VIA_IRQ_DMA0_TD_STATUS		BIT(5)
#define VIA_IRQ_DMA1_DD_STATUS		BIT(6)
#define VIA_IRQ_DMA1_TD_STATUS		BIT(7)

#define INTR_ENABLE_MASK (VIA_IRQ_DMA0_TD_ENABLE | VIA_IRQ_DMA1_TD_ENABLE | \
			VIA_IRQ_DMA0_DD_ENABLE | VIA_IRQ_DMA1_DD_ENABLE | \
			VIA_IRQ_IGA1_VSYNC_ENABLE | VIA_IRQ_IGA2_VSYNC_ENABLE)

#define INTERRUPT_ENABLE_MASK (VIA_IRQ_CAPTURE0_ACTIVE_ENABLE | VIA_IRQ_CAPTURE1_ACTIVE_ENABLE | \
				VIA_IRQ_HQV0_ENABLE | VIA_IRQ_HQV1_ENABLE | \
				INTR_ENABLE_MASK)

#define INTR_STATUS_MASK (VIA_IRQ_DMA0_TD_STATUS | VIA_IRQ_DMA1_TD_STATUS | \
			VIA_IRQ_DMA0_DD_STATUS  | VIA_IRQ_DMA1_DD_STATUS  | \
			VIA_IRQ_IGA1_VSYNC_STATUS | VIA_IRQ_IGA2_VSYNC_STATUS)

#define INTERRUPT_STATUS_MASK (VIA_IRQ_CAPTURE0_ACTIVE_STATUS | VIA_IRQ_CAPTURE1_ACTIVE_STATUS | \
				VIA_IRQ_HQV0_STATUS | VIA_IRQ_HQV1_STATUS | \
				INTR_STATUS_MASK)

/*
 * Device-specific IRQs go here. This type might need to be extended with
 * the register if there are multiple IRQ control registers.
 * Currently we activate the HQV interrupts of  Unichrome Pro group A.
 */

static maskarray_t via_unichrome_irqs[] = {
	{ VIA_IRQ_DMA0_TD_ENABLE, VIA_IRQ_DMA0_TD_STATUS, VIA_PCI_DMA_CSR0,
	  VIA_DMA_CSR_TA | VIA_DMA_CSR_TD, 0x00000008},
	{ VIA_IRQ_DMA1_TD_ENABLE, VIA_IRQ_DMA1_TD_STATUS, VIA_PCI_DMA_CSR1,
	  VIA_DMA_CSR_TA | VIA_DMA_CSR_TD, 0x00000008}
};
static int via_num_unichrome = ARRAY_SIZE(via_unichrome_irqs);
static int via_irqmap_unichrome[] = {-1, -1, -1, 0, -1, 1};

static maskarray_t via_pro_group_a_irqs[] = {
	{ VIA_IRQ_HQV0_ENABLE, VIA_IRQ_HQV0_STATUS, 0x000003D0, 0x00008010,
	  0x00000000 },
	{ VIA_IRQ_HQV1_ENABLE, VIA_IRQ_HQV1_STATUS, 0x000013D0, 0x00008010,
	  0x00000000 },
	{ VIA_IRQ_DMA0_TD_ENABLE, VIA_IRQ_DMA0_TD_STATUS, VIA_PCI_DMA_CSR0,
	  VIA_DMA_CSR_TA | VIA_DMA_CSR_TD, 0x00000008 },
	{ VIA_IRQ_DMA1_TD_ENABLE, VIA_IRQ_DMA1_TD_STATUS, VIA_PCI_DMA_CSR1,
	  VIA_DMA_CSR_TA | VIA_DMA_CSR_TD, 0x00000008 },
};
static int via_num_pro_group_a = ARRAY_SIZE(via_pro_group_a_irqs);
static int via_irqmap_pro_group_a[] = {0, 1, -1, 2, -1, 3};

irqreturn_t via_driver_irq_handler(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *) arg;
	struct drm_via_private *dev_priv = dev->dev_private;
	drm_via_irq_t *cur_irq = dev_priv->via_irqs;
	u32 status = VIA_READ(INTERRUPT_CTRL_REG1);
	irqreturn_t ret = IRQ_NONE;
	int i;

	if (status & VIA_IRQ_IGA1_VSYNC_STATUS) {
		drm_handle_vblank(dev, 0);
		ret = IRQ_HANDLED;
	}

	for (i = 0; i < dev_priv->num_irqs; ++i) {
		if (status & cur_irq->pending_mask) {
			struct via_fence_engine *eng = NULL;

			atomic_inc(&cur_irq->irq_received);
			DRM_WAKEUP(&cur_irq->irq_queue);
			ret = IRQ_HANDLED;

			if (dev_priv->irq_map[drm_via_irq_dma0_td] == i)
				eng = dev_priv->dma_fences.engines[0];
			else if (dev_priv->irq_map[drm_via_irq_dma1_td] == i)
				eng = dev_priv->dma_fences.engines[1];

			if (eng)
				queue_work(eng->pool->fence_wq, &eng->fence_work);
		}
		cur_irq++;
	}

	/* Acknowledge interrupts */
	VIA_WRITE(INTERRUPT_CTRL_REG1, status);
	return ret;
}

int
via_enable_vblank(struct drm_device *dev, int crtc)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	u32 status;

	if (crtc < 0 || crtc >= dev->num_crtcs) {
		DRM_ERROR("%s: Invalid crtc %d\n", __func__, crtc);
		return -EINVAL;
	}

	status = VIA_READ(INTERRUPT_CTRL_REG1);
	if (crtc == 1)
		status |= VIA_IRQ_IGA2_VSYNC_ENABLE | VIA_IRQ_IGA2_VSYNC_STATUS;
	else if (!crtc)
		status |= VIA_IRQ_IGA1_VSYNC_ENABLE | VIA_IRQ_IGA1_VSYNC_STATUS;

	svga_wcrt_mask(VGABASE, 0xF3, 0, BIT(1));
	svga_wcrt_mask(VGABASE, 0x11, BIT(4), BIT(4));

	VIA_WRITE(INTERRUPT_CTRL_REG1, status);
	return 0;
}

void
via_disable_vblank(struct drm_device *dev, int crtc)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	u32 status;

	if (crtc < 0 || crtc >= dev->num_crtcs) {
		DRM_ERROR("%s: Invalid crtc %d\n", __func__, crtc);
		return;
	}

	status = VIA_READ(INTERRUPT_CTRL_REG1);
	if (crtc == 1)
		status &= ~VIA_IRQ_IGA2_VSYNC_ENABLE;
	else if (!crtc)
		status &= ~VIA_IRQ_IGA1_VSYNC_ENABLE;

	VIA_WRITE(INTERRUPT_CTRL_REG1, status);
}

void
via_driver_irq_preinstall(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	drm_via_irq_t *cur_irq;
	u32 status;
	int i;

	cur_irq = dev_priv->via_irqs;

	if (dev_priv->engine_type != VIA_ENG_H1) {
		dev_priv->irq_masks = via_pro_group_a_irqs;
		dev_priv->num_irqs = via_num_pro_group_a;
		dev_priv->irq_map = via_irqmap_pro_group_a;

		dev_priv->irq_pending_mask = INTR_STATUS_MASK;
		dev_priv->irq_enable_mask = INTR_ENABLE_MASK;
	} else {
		dev_priv->irq_masks = via_unichrome_irqs;
		dev_priv->num_irqs = via_num_unichrome;
		dev_priv->irq_map = via_irqmap_unichrome;

		dev_priv->irq_pending_mask = INTERRUPT_STATUS_MASK;
		dev_priv->irq_enable_mask = INTERRUPT_ENABLE_MASK;
	}

	for (i = 0; i < dev_priv->num_irqs; ++i) {
		atomic_set(&cur_irq->irq_received, 0);
		cur_irq->enable_mask = dev_priv->irq_masks[i][0];
		cur_irq->pending_mask = dev_priv->irq_masks[i][1];
		DRM_INIT_WAITQUEUE(&cur_irq->irq_queue);
		cur_irq++;

		DRM_DEBUG("Initializing IRQ %d\n", i);
	}

	/* Clear VSync interrupt regs */
	status = VIA_READ(INTERRUPT_CTRL_REG1);
	VIA_WRITE(INTERRUPT_CTRL_REG1, status & ~(dev_priv->irq_enable_mask));

	/* Acknowledge interrupts */
	status = VIA_READ(INTERRUPT_CTRL_REG1);
	VIA_WRITE(INTERRUPT_CTRL_REG1, status | dev_priv->irq_pending_mask);
}

int
via_driver_irq_postinstall(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	u32 status = VIA_READ(INTERRUPT_CTRL_REG1);

	VIA_WRITE(INTERRUPT_CTRL_REG1, status | VIA_IRQ_ALL_ENABLE |
			dev_priv->irq_enable_mask);
	return 0;
}

void
via_driver_irq_uninstall(struct drm_device *dev)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	u32 status;

	/* Some more magic, oh for some data sheets ! */
	VIA_WRITE8(0x83d4, 0x11);
	VIA_WRITE8(0x83d5, VIA_READ8(0x83d5) & ~0x30);

	status = VIA_READ(INTERRUPT_CTRL_REG1);
	VIA_WRITE(INTERRUPT_CTRL_REG1, status &
		  ~(VIA_IRQ_IGA1_VSYNC_ENABLE | dev_priv->irq_enable_mask));
}

static int
via_driver_irq_wait(struct drm_device *dev, unsigned int irq, int force_sequence,
		    unsigned int *sequence)
{
	struct drm_via_private *dev_priv = dev->dev_private;
	unsigned int cur_irq_sequence;
	drm_via_irq_t *cur_irq;
	int ret = 0;
	maskarray_t *masks;
	int real_irq;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	if (irq >= drm_via_irq_num) {
		DRM_ERROR("Trying to wait on unknown irq %d\n", irq);
		return -EINVAL;
	}

	real_irq = dev_priv->irq_map[irq];

	if (real_irq < 0) {
		DRM_ERROR("Video IRQ %d not available on this hardware.\n",
			  irq);
		return -EINVAL;
	}

	masks = dev_priv->irq_masks;
	cur_irq = dev_priv->via_irqs + real_irq;

	if (masks[real_irq][2] && !force_sequence) {
		DRM_WAIT_ON(ret, cur_irq->irq_queue, 3 * DRM_HZ,
			    ((VIA_READ(masks[irq][2]) & masks[irq][3]) ==
			     masks[irq][4]));
		cur_irq_sequence = atomic_read(&cur_irq->irq_received);
	} else {
		DRM_WAIT_ON(ret, cur_irq->irq_queue, 3 * DRM_HZ,
			    (((cur_irq_sequence =
			       atomic_read(&cur_irq->irq_received)) -
			      *sequence) <= (1 << 23)));
	}
	*sequence = cur_irq_sequence;
	return ret;
}

int
via_wait_irq(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_via_irqwait_t *irqwait = data;
	struct timeval now;
	int ret = 0;
	struct drm_via_private *dev_priv = dev->dev_private;
	drm_via_irq_t *cur_irq = dev_priv->via_irqs;
	int force_sequence;

	if (irqwait->request.irq >= dev_priv->num_irqs) {
		DRM_ERROR("Trying to wait on unknown irq %d\n",
			  irqwait->request.irq);
		return -EINVAL;
	}

	cur_irq += irqwait->request.irq;

	switch (irqwait->request.type & ~VIA_IRQ_FLAGS_MASK) {
	case VIA_IRQ_RELATIVE:
		irqwait->request.sequence +=
			atomic_read(&cur_irq->irq_received);
		irqwait->request.type &= ~_DRM_VBLANK_RELATIVE;
	case VIA_IRQ_ABSOLUTE:
		break;
	default:
		return -EINVAL;
	}

	if (irqwait->request.type & VIA_IRQ_SIGNAL) {
		DRM_ERROR("Signals on Via IRQs not implemented yet.\n");
		return -EINVAL;
	}

	force_sequence = (irqwait->request.type & VIA_IRQ_FORCE_SEQUENCE);

	ret = via_driver_irq_wait(dev, irqwait->request.irq, force_sequence,
				  &irqwait->request.sequence);
	do_gettimeofday(&now);
	irqwait->reply.tval_sec = now.tv_sec;
	irqwait->reply.tval_usec = now.tv_usec;

	return ret;
}
