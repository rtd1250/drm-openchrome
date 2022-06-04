/*
 * Copyright 2012 James Simmons <jsimmons@infradead.org>
 *
 * Based on code for the viafb driver.
 * UniChrome PLL parameters calculation code borrowed from OpenChrome DDX
 * device driver.
 * Copyright 1998-2008 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2008 S3 Graphics, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation;
 * either version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTIES OR REPRESENTATIONS; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.See the GNU General Public License
 * for more details.
 */

#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>

#include "openchrome_drv.h"


#define CSR_VCO_UP	600000000
#define CSR_VCO_DOWN	300000000

#define PLL_DTZ_DEFAULT		(BIT(0) | BIT(1))

#define VIA_CLK_REFERENCE	14318180

struct pll_mrn_value {
	u32 pll_m;
	u32 pll_r;
	u32 pll_n;
	u32 diff_clk;
	u32 pll_fout;
};

/*
 * This function first gets the best frequency M, R, N value
 * to program the PLL according to the supplied frequence
 * passed in. After we get the MRN values the results are
 * formatted to fit properly into the PLL clock registers.
 *
 * PLL registers M, R, N value
 * [31:16]  DM[7:0]
 * [15:8 ]  DR[2:0]
 * [7 :0 ]  DN[6:0]
 */
u32
via_get_clk_value(struct drm_device *dev, u32 freq)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	u32 best_pll_n = 2, best_pll_r = 0, best_pll_m = 2, best_clk_diff = freq;
	u32 pll_fout, pll_fvco, pll_mrn = 0;
	u32 pll_n, pll_r, pll_m, clk_diff;
	struct pll_mrn_value pll_tmp[5] = {
		{ 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0 } };
	int count;

    if ((pdev->device != PCI_DEVICE_ID_VIA_CLE266)
        && (pdev->device != PCI_DEVICE_ID_VIA_KM400)) {
		/* DN[6:0] */
		for (pll_n = 2; pll_n < 6; pll_n++) {
			/* DR[2:0] */
			for (pll_r = 0; pll_r < 6; pll_r++) {
				/* DM[9:0] */
				for (pll_m = 2; pll_m < 512; pll_m++) {
					/* first divide pll_n then multiply
					 * pll_m. We have to reduce pll_m
					 * to 512 to get rid of the overflow */
					pll_fvco = (VIA_CLK_REFERENCE / pll_n) * pll_m;
					if ((pll_fvco >= CSR_VCO_DOWN) && (pll_fvco <= CSR_VCO_UP)) {
						pll_fout = pll_fvco >> pll_r;
						if (pll_fout < freq)
							clk_diff = freq - pll_fout;
						else
							clk_diff = pll_fout - freq;

						/* if frequency (which is the PLL we want
						 * to set) > 150MHz, the MRN value we
						 * write in register must < frequency, and
						 * get MRN value whose M is the largeset */
						if (freq >= 150000000) {
							if ((clk_diff <= pll_tmp[0].diff_clk) || pll_tmp[0].pll_fout == 0) {
								for (count = ARRAY_SIZE(pll_tmp) - 1; count >= 1; count--)
									pll_tmp[count] = pll_tmp[count - 1];

								pll_tmp[0].pll_m = pll_m;
								pll_tmp[0].pll_r = pll_r;
								pll_tmp[0].pll_n = pll_n;
								pll_tmp[0].diff_clk = clk_diff;
								pll_tmp[0].pll_fout = pll_fout;
							}
						}

						if (clk_diff < best_clk_diff) {
							best_clk_diff = clk_diff;
							best_pll_m = pll_m;
							best_pll_n = pll_n;
							best_pll_r = pll_r;
						}
					} /* if pll_fvco in VCO range */
				} /* for PLL M */
			} /* for PLL R */
		} /* for PLL N */

		/* if frequency(which is the PLL we want to set) > 150MHz,
		 * the MRN value we write in register must < frequency,
		 * and get MRN value whose M is the largeset */
		if (freq > 150000000) {
			best_pll_m = pll_tmp[0].pll_m;
			best_pll_r = pll_tmp[0].pll_r;
			best_pll_n = pll_tmp[0].pll_n;
		}
	/* UniChrome IGP (CLE266, KM400(A), KN400, and P4M800 chipsets)
	 * requires a different formula for calculating the PLL parameters.
	 * The code was borrowed from OpenChrome DDX device driver UMS
	 * (User Mode Setting) section, but was modified to not use float type
	 * variables. */
    } else {
		for (pll_r = 0; pll_r < 4; ++pll_r) {
			for (pll_n = (pll_r == 0) ? 2 : 1; pll_n <= 7; ++pll_n) {
				for (pll_m = 1; pll_m <= 127; ++pll_m) {
					pll_fout = VIA_CLK_REFERENCE * pll_m;
					pll_fout /= (pll_n << pll_r);
					if (pll_fout < freq)
						clk_diff = freq - pll_fout;
					else
						clk_diff = pll_fout - freq;

					if (clk_diff < best_clk_diff) {
						best_clk_diff = clk_diff;
						best_pll_m = pll_m & 0x7F;
						best_pll_n = pll_n & 0x1F;
						best_pll_r = pll_r & 0x03;
					}
				}
			}
		}
	}

	switch (pdev->device) {
	case PCI_DEVICE_ID_VIA_CLE266:
	case PCI_DEVICE_ID_VIA_KM400:
		/* Clock Synthesizer Value 0[7:6]: DR[1:0]
		 * Clock Synthesizer Value 0[5:0]: DN[5:0] */
		pll_mrn = ((best_pll_r & 0x3) << 14 |
			   (best_pll_n & 0x1F) << 8);
		/* Clock Synthesizer Value 1[6:0]: DM[6:0] */
		pll_mrn |= (best_pll_m & 0x7F);
		break;
	case PCI_DEVICE_ID_VIA_VX875:
	case PCI_DEVICE_ID_VIA_VX900_VGA:
		/* Clock Synthesizer Value 0 : DM[7:0] */
		pll_mrn = (best_pll_m & 0xFF) << 16;
		/* Clock Synthesizer Value 1[1:0] : DM[9:8]
		 * Clock Synthesizer Value 1[4:2] : DR[2:0]
		 * Clock Synthesizer Value 1[7] : DTZ[0] */
		pll_mrn |= (((PLL_DTZ_DEFAULT & 0x1) << 7) |
			   ((best_pll_r & 0x7) << 2) |
			   (((best_pll_m) >> 8) & 0x3)) << 8;
		/* Clock Synthesizer Value 2[6:0] : DN[6:0]
		 * Clock Synthesizer Value 2[7] : DTZ[1] */
		pll_mrn |= (((PLL_DTZ_DEFAULT >> 1) & 0x1) << 7) |
			   ((best_pll_n) & 0x7F);
		break;
	default:
		/* Clock Synthesizer Value 0 : DM[7:0] */
		pll_mrn = ((best_pll_m - 2) & 0xFF) << 16;
		/* Clock Synthesizer Value 1[1:0] : DM[9:8]
		 * Clock Synthesizer Value 1[4:2] : DR[2:0]
		 * Clock Synthesizer Value 1[7] : DTZ[0] */
		pll_mrn |= (((PLL_DTZ_DEFAULT & 0x1) << 7) |
			   ((best_pll_r & 0x7) << 2) |
			   (((best_pll_m - 2) >> 8) & 0x3)) << 8;
		/* Clock Synthesizer Value 2[6:0] : DN[6:0]
		 * Clock Synthesizer Value 2[7] : DTZ[1] */
		pll_mrn |= (((PLL_DTZ_DEFAULT >> 1) & 0x1) << 7) |
			   ((best_pll_n - 2) & 0x7F);
		break;
	}
	return pll_mrn;
}

/* Set VCLK */
void
via_set_vclock(struct drm_crtc *crtc, u32 clk)
{
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct drm_device *dev = crtc->dev;
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	struct via_drm_priv *dev_private = to_via_drm_priv(dev);
	unsigned long max_loop = 50, i = 0;

	if (!iga->index) {
		/* IGA1 HW Reset Enable */
		svga_wcrt_mask(VGABASE, 0x17, 0x00, BIT(7));

		/* set clk */
		if ((pdev->device == PCI_DEVICE_ID_VIA_CLE266) ||
		    (pdev->device == PCI_DEVICE_ID_VIA_KM400)) {
			vga_wseq(VGABASE, 0x46, (clk & 0xFF00) >> 8);	/* rshift + divisor */
			vga_wseq(VGABASE, 0x47, (clk & 0x00FF));	/* multiplier */
		} else {
			vga_wseq(VGABASE, 0x44, (clk & 0xFF0000) >> 16);
			vga_wseq(VGABASE, 0x45, (clk & 0x00FF00) >> 8);
			vga_wseq(VGABASE, 0x46, (clk & 0x0000FF));
		}
		/* Fire */
		svga_wmisc_mask(VGABASE, BIT(3) | BIT(2), BIT(3) | BIT(2));

		/* reset pll */
		svga_wseq_mask(VGABASE, 0x40, 0x02, 0x02);
		svga_wseq_mask(VGABASE, 0x40, 0x00, 0x02);

		/* exit hw reset */
		while ((vga_rseq(VGABASE, 0x3C) & BIT(3)) == 0 && i++ < max_loop)
			udelay(20);

		/* IGA1 HW Reset Disable */
		svga_wcrt_mask(VGABASE, 0x17, BIT(7), BIT(7));
	} else {
		/* IGA2 HW Reset Enable */
		svga_wcrt_mask(VGABASE, 0x6A, 0x00, BIT(6));

		/* set clk */
		if ((pdev->device == PCI_DEVICE_ID_VIA_CLE266) ||
		    (pdev->device == PCI_DEVICE_ID_VIA_KM400)) {
			vga_wseq(VGABASE, 0x44, (clk & 0xFF00) >> 8);
			vga_wseq(VGABASE, 0x45, (clk & 0x00FF));
		} else {
			vga_wseq(VGABASE, 0x4A, (clk & 0xFF0000) >> 16);
			vga_wseq(VGABASE, 0x4B, (clk & 0x00FF00) >> 8);
			vga_wseq(VGABASE, 0x4C, (clk & 0x0000FF));
		}

		/* reset pll */
		svga_wseq_mask(VGABASE, 0x40, 0x04, 0x04);
		svga_wseq_mask(VGABASE, 0x40, 0x00, 0x04);

		/* exit hw reset */
		while ((vga_rseq(VGABASE, 0x3C) & BIT(2)) == 0 && i++ < max_loop)
			udelay(20);

		/* IGA2 HW Reset Disble, CR6A[6] = 1 */
		svga_wcrt_mask(VGABASE, 0x6A, BIT(6), BIT(6));
	}
}
