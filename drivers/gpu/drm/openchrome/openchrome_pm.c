/*
 * Copyright 2012 James Simmons <jsimmons@infradead.org>. All Rights Reserved.
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

#include "drmP.h"
#include "via_drv.h"

static void
via_init_2d(struct via_device *dev_priv, int pci_device)
{
	int i;

	for (i = 0x04; i < 0x5c; i += 4)
		VIA_WRITE(i, 0x0);

	/* For 410 chip*/
	if (pci_device == PCI_DEVICE_ID_VIA_VX900_VGA)
		VIA_WRITE(0x60, 0x0);
}

static void
via_init_3d(struct via_device *dev_priv)
{
	unsigned long texture_stage;
	int i;

	VIA_WRITE(VIA_REG_TRANSET, 0x00010000);
	for (i = 0; i <= 0x9A; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, i << 24);

	/* guardband clipping default setting */
	VIA_WRITE(VIA_REG_TRANSPACE, (0x88 << 24) | 0x00001ed0);
	VIA_WRITE(VIA_REG_TRANSPACE, (0x89 << 24) | 0x00000800);

	/* Initial Texture Stage Setting */
	for (texture_stage = 0; texture_stage <= 0xf; texture_stage++) {
		VIA_WRITE(VIA_REG_TRANSET, (0x00020000 | 0x00000000 |
					(texture_stage & 0xf) << 24));
		for (i = 0 ; i <= 0x30 ; i++)
			VIA_WRITE(VIA_REG_TRANSPACE, i << 24);
	}

	/* Initial Texture Sampler Setting */
	for (texture_stage = 0; texture_stage <= 0xf; texture_stage++) {
		VIA_WRITE(VIA_REG_TRANSET, (0x00020000 | 0x20000000 |
					(texture_stage & 0x10) << 24));
		for (i = 0; i <= 0x36; i++)
			VIA_WRITE(VIA_REG_TRANSPACE, i << 24);
	}

	VIA_WRITE(VIA_REG_TRANSET, (0x00020000 | 0xfe000000));
	for (i = 0 ; i <= 0x13 ; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, i << 24);

	/* degamma table */
	VIA_WRITE(VIA_REG_TRANSET, (0x00030000 | 0x15000000));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (30 << 20) | (15 << 10) | (5)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((119 << 20) | (81 << 10) | (52)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((283 << 20) | (219 << 10) | (165)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((535 << 20) | (441 << 10) | (357)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((119 << 20) | (884 << 20) | (757 << 10) | (640)));

	/* gamma table */
	VIA_WRITE(VIA_REG_TRANSET, (0x00030000 | 0x17000000));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (13 << 20) | (13 << 10) | (13)));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (26 << 20) | (26 << 10) | (26)));
	VIA_WRITE(VIA_REG_TRANSPACE, (0x40000000 | (39 << 20) | (39 << 10) | (39)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((51 << 20) | (51 << 10) | (51)));
	VIA_WRITE(VIA_REG_TRANSPACE, ((71 << 20) | (71 << 10) | (71)));
	VIA_WRITE(VIA_REG_TRANSPACE, (87 << 20) | (87 << 10) | (87));
	VIA_WRITE(VIA_REG_TRANSPACE, (113 << 20) | (113 << 10) | (113));
	VIA_WRITE(VIA_REG_TRANSPACE, (135 << 20) | (135 << 10) | (135));
	VIA_WRITE(VIA_REG_TRANSPACE, (170 << 20) | (170 << 10) | (170));
	VIA_WRITE(VIA_REG_TRANSPACE, (199 << 20) | (199 << 10) | (199));
	VIA_WRITE(VIA_REG_TRANSPACE, (246 << 20) | (246 << 10) | (246));
	VIA_WRITE(VIA_REG_TRANSPACE, (284 << 20) | (284 << 10) | (284));
	VIA_WRITE(VIA_REG_TRANSPACE, (317 << 20) | (317 << 10) | (317));
	VIA_WRITE(VIA_REG_TRANSPACE, (347 << 20) | (347 << 10) | (347));
	VIA_WRITE(VIA_REG_TRANSPACE, (373 << 20) | (373 << 10) | (373));
	VIA_WRITE(VIA_REG_TRANSPACE, (398 << 20) | (398 << 10) | (398));
	VIA_WRITE(VIA_REG_TRANSPACE, (442 << 20) | (442 << 10) | (442));
	VIA_WRITE(VIA_REG_TRANSPACE, (481 << 20) | (481 << 10) | (481));
	VIA_WRITE(VIA_REG_TRANSPACE, (517 << 20) | (517 << 10) | (517));
	VIA_WRITE(VIA_REG_TRANSPACE, (550 << 20) | (550 << 10) | (550));
	VIA_WRITE(VIA_REG_TRANSPACE, (609 << 20) | (609 << 10) | (609));
	VIA_WRITE(VIA_REG_TRANSPACE, (662 << 20) | (662 << 10) | (662));
	VIA_WRITE(VIA_REG_TRANSPACE, (709 << 20) | (709 << 10) | (709));
	VIA_WRITE(VIA_REG_TRANSPACE, (753 << 20) | (753 << 10) | (753));
	VIA_WRITE(VIA_REG_TRANSPACE, (794 << 20) | (794 << 10) | (794));
	VIA_WRITE(VIA_REG_TRANSPACE, (832 << 20) | (832 << 10) | (832));
	VIA_WRITE(VIA_REG_TRANSPACE, (868 << 20) | (868 << 10) | (868));
	VIA_WRITE(VIA_REG_TRANSPACE, (902 << 20) | (902 << 10) | (902));
	VIA_WRITE(VIA_REG_TRANSPACE, (934 << 20) | (934 << 10) | (934));
	VIA_WRITE(VIA_REG_TRANSPACE, (966 << 20) | (966 << 10) | (966));
	VIA_WRITE(VIA_REG_TRANSPACE, (996 << 20) | (996 << 10) | (996));

	/* Initialize INV_ParaSubType_TexPa */
	VIA_WRITE(VIA_REG_TRANSET, (0x00030000 | 0x00000000));
	for (i = 0; i < 16; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);

	/* Initialize INV_ParaSubType_4X4Cof */
	VIA_WRITE(VIA_REG_TRANSET, (0x00030000 | 0x11000000));
	for (i = 0; i < 32; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);

	/* Initialize INV_ParaSubType_StipPal */
	VIA_WRITE(VIA_REG_TRANSET, (0x00030000 | 0x14000000));
	for (i = 0; i < (5 + 3); i++)
		VIA_WRITE(VIA_REG_TRANSPACE, 0x00000000);

	/* primitive setting & vertex format */
	VIA_WRITE(VIA_REG_TRANSET, (0x00040000));
	for (i = 0; i <= 0x62; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, (i << 24));

	/* c2s clamping value for screen coordinate */
	VIA_WRITE(VIA_REG_TRANSPACE, (0x50 << 24) | 0x00000000);
	VIA_WRITE(VIA_REG_TRANSPACE, (0x51 << 24) | 0x00000000);
	VIA_WRITE(VIA_REG_TRANSPACE, (0x52 << 24) | 0x00147fff);

	/* ParaType 0xFE - Configure and Misc Setting */
	VIA_WRITE(VIA_REG_TRANSET, (0x00fe0000));
	for (i = 0; i <= 0x47; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, (i << 24));

	/* ParaType 0x11 - Frame Buffer Auto-Swapping and Command Regulator */
	VIA_WRITE(VIA_REG_TRANSET, (0x00110000));
	for (i = 0; i <= 0x20; i++)
		VIA_WRITE(VIA_REG_TRANSPACE, (i << 24));

	VIA_WRITE(VIA_REG_TRANSET, 0x00fe0000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x4000840f);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x47000404);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x44000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x46000005);

	/* setting Misconfig */
	VIA_WRITE(VIA_REG_TRANSET, 0x00fe0000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x00001004);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x08000249);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x0a0002c9);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x0b0002fb);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x0c000000);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x0d0002cb);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x0e000009);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x10000049);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x110002ff);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x12000008);
	VIA_WRITE(VIA_REG_TRANSPACE, 0x130002db);
}

static void
via_init_vq(struct via_device *dev_priv)
{
	unsigned long vq_start_addr, vq_end_addr, vqlen;
	unsigned long vqstartl, vqendl, vqstart_endh;
	struct ttm_buffer_object *bo = dev_priv->vq.bo;

	if (!bo)
		return;

	vq_start_addr = bo->offset;
	vq_end_addr = vq_start_addr + bo->mem.size - 1;
	vqstartl = 0x70000000 | (vq_start_addr & 0xFFFFFF);
	vqendl = 0x71000000 | (vq_end_addr & 0xFFFFFF);
	vqstart_endh = 0x72000000 | ((vq_start_addr & 0xFF000000) >> 24) |
			((vq_end_addr & 0xFF000000) >> 16);
	vqlen = 0x73000000 | (bo->mem.size >> 3);

	VIA_WRITE(0x41c, 0x00100000);
	VIA_WRITE(0x420, vqstart_endh);
	VIA_WRITE(0x420, vqstartl);
	VIA_WRITE(0x420, vqendl);
	VIA_WRITE(0x420, vqlen);
	VIA_WRITE(0x420, 0x74301001);
	VIA_WRITE(0x420, 0x00000000);
}

static void
via_init_pcie_gart_table(struct via_device *dev_priv,
				struct pci_dev *pdev)
{
	struct ttm_buffer_object *bo = dev_priv->gart.bo;
	u8 value;

	if (!pci_is_pcie(pdev) || !bo)
		return;

	/* enable gtt write */
	svga_wseq_mask(VGABASE, 0x6C, 0x00, BIT(7));

	/* set the base address of gart table */
	value = (bo->offset & 0xff000) >> 12;
	vga_wseq(VGABASE, 0x6A, value);

	value = (bo->offset & 0xff000) >> 20;
	vga_wseq(VGABASE, 0x6B, value);

	value = vga_rseq(VGABASE, 0x6C);
	value |= ((bo->offset >> 28) & 0x01);
	vga_wseq(VGABASE, 0x6C, value);

	/* flush the gtt cache */
	svga_wseq_mask(VGABASE, 0x6F, BIT(7), BIT(7));

	/* disable the gtt write */
	svga_wseq_mask(VGABASE, 0x6C, BIT(7), BIT(7));
}

/* This function does:
 * 1. Command buffer allocation
 * 2. hw engine intialization:2D;3D;VQ
 * 3. Ring Buffer mechanism setup
 */
void
via_engine_init(struct drm_device *dev)
{
	struct via_device *dev_priv = dev->dev_private;

	/* initial engines */
	via_init_2d(dev_priv, dev->pdev->device);
	via_init_3d(dev_priv);
	via_init_vq(dev_priv);

	/* pcie gart table setup */
	via_init_pcie_gart_table(dev_priv, dev->pdev);
}
