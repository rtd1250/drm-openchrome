/*
 * Copyright 2011 James Simmons <jsimmons@infradead.org>
 *
 * Based on code for the viafb driver.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "drmP.h"

#include "via_pll.h"
#include "via_drv.h"

static struct pll_config cle266_pll_config[] = {
	{19, 4, 0},
	{26, 5, 0},
	{28, 5, 0},
	{31, 5, 0},
	{33, 5, 0},
	{55, 5, 0},
	{102, 5, 0},
	{53, 6, 0},
	{92, 6, 0},
	{98, 6, 0},
	{112, 6, 0},
	{41, 7, 0},
	{60, 7, 0},
	{99, 7, 0},
	{100, 7, 0},
	{83, 8, 0},
	{86, 8, 0},
	{108, 8, 0},
	{87, 9, 0},
	{118, 9, 0},
	{95, 12, 0},
	{115, 12, 0},
	{108, 13, 0},
	{83, 17, 0},
	{67, 20, 0},
	{86, 20, 0},
	{98, 20, 0},
	{121, 24, 0},
	{99, 29, 0},
	{33, 3, 1},
	{15, 4, 1},
	{23, 4, 1},
	{37, 5, 1},
	{83, 5, 1},
	{85, 5, 1},
	{94, 5, 1},
	{103, 5, 1},
	{109, 5, 1},
	{113, 5, 1},
	{121, 5, 1},
	{82, 6, 1},
	{31, 7, 1},
	{55, 7, 1},
	{84, 7, 1},
	{83, 8, 1},
	{76, 9, 1},
	{127, 9, 1},
	{33, 4, 2},
	{75, 4, 2},
	{119, 4, 2},
	{121, 4, 2},
	{91, 5, 2},
	{118, 5, 2},
	{83, 6, 2},
	{109, 6, 2},
	{90, 7, 2},
	{93, 2, 3},
	{53, 3, 3},
	{73, 4, 3},
	{89, 4, 3},
	{105, 4, 3},
	{117, 4, 3},
	{101, 5, 3},
	{121, 5, 3},
	{127, 5, 3},
	{99, 7, 3}
};

static struct pll_config k800_pll_config[] = {
	{22, 2, 0},
	{28, 3, 0},
	{81, 3, 1},
	{85, 3, 1},
	{98, 3, 1},
	{112, 3, 1},
	{86, 4, 1},
	{166, 4, 1},
	{109, 5, 1},
	{113, 5, 1},
	{121, 5, 1},
	{131, 5, 1},
	{143, 5, 1},
	{153, 5, 1},
	{66, 3, 2},
	{68, 3, 2},
	{95, 3, 2},
	{106, 3, 2},
	{116, 3, 2},
	{93, 4, 2},
	{119, 4, 2},
	{121, 4, 2},
	{133, 4, 2},
	{137, 4, 2},
	{117, 5, 2},
	{118, 5, 2},
	{120, 5, 2},
	{124, 5, 2},
	{132, 5, 2},
	{137, 5, 2},
	{141, 5, 2},
	{166, 5, 2},
	{170, 5, 2},
	{191, 5, 2},
	{206, 5, 2},
	{208, 5, 2},
	{30, 2, 3},
	{69, 3, 3},
	{82, 3, 3},
	{83, 3, 3},
	{109, 3, 3},
	{114, 3, 3},
	{125, 3, 3},
	{89, 4, 3},
	{103, 4, 3},
	{117, 4, 3},
	{126, 4, 3},
	{150, 4, 3},
	{161, 4, 3},
	{121, 5, 3},
	{127, 5, 3},
	{131, 5, 3},
	{134, 5, 3},
	{148, 5, 3},
	{169, 5, 3},
	{172, 5, 3},
	{182, 5, 3},
	{195, 5, 3},
	{196, 5, 3},
	{208, 5, 3},
	{66, 2, 4},
	{85, 3, 4},
	{141, 4, 4},
	{146, 4, 4},
	{161, 4, 4},
	{177, 5, 4}
};

static struct pll_config cx700_pll_config[] = {
	{98, 3, 1},
	{86, 4, 1},
	{109, 5, 1},
	{110, 5, 1},
	{113, 5, 1},
	{121, 5, 1},
	{131, 5, 1},
	{135, 5, 1},
	{142, 5, 1},
	{143, 5, 1},
	{153, 5, 1},
	{187, 5, 1},
	{208, 5, 1},
	{68, 2, 2},
	{95, 3, 2},
	{116, 3, 2},
	{93, 4, 2},
	{119, 4, 2},
	{133, 4, 2},
	{137, 4, 2},
	{151, 4, 2},
	{166, 4, 2},
	{110, 5, 2},
	{112, 5, 2},
	{117, 5, 2},
	{118, 5, 2},
	{120, 5, 2},
	{132, 5, 2},
	{137, 5, 2},
	{141, 5, 2},
	{151, 5, 2},
	{166, 5, 2},
	{175, 5, 2},
	{191, 5, 2},
	{206, 5, 2},
	{174, 7, 2},
	{82, 3, 3},
	{109, 3, 3},
	{117, 4, 3},
	{150, 4, 3},
	{161, 4, 3},
	{112, 5, 3},
	{115, 5, 3},
	{121, 5, 3},
	{127, 5, 3},
	{129, 5, 3},
	{131, 5, 3},
	{134, 5, 3},
	{138, 5, 3},
	{148, 5, 3},
	{157, 5, 3},
	{169, 5, 3},
	{172, 5, 3},
	{190, 5, 3},
	{195, 5, 3},
	{196, 5, 3},
	{208, 5, 3},
	{141, 5, 4},
	{150, 5, 4},
	{166, 5, 4},
	{176, 5, 4},
	{177, 5, 4},
	{183, 5, 4},
	{202, 5, 4}
};

static struct pll_config vx855_pll_config[] = {
	{86, 4, 1},
	{108, 5, 1},
	{110, 5, 1},
	{113, 5, 1},
	{121, 5, 1},
	{131, 5, 1},
	{135, 5, 1},
	{142, 5, 1},
	{143, 5, 1},
	{153, 5, 1},
	{164, 5, 1},
	{187, 5, 1},
	{208, 5, 1},
	{110, 5, 2},
	{112, 5, 2},
	{117, 5, 2},
	{118, 5, 2},
	{124, 5, 2},
	{132, 5, 2},
	{137, 5, 2},
	{141, 5, 2},
	{149, 5, 2},
	{151, 5, 2},
	{159, 5, 2},
	{166, 5, 2},
	{167, 5, 2},
	{172, 5, 2},
	{189, 5, 2},
	{191, 5, 2},
	{194, 5, 2},
	{206, 5, 2},
	{208, 5, 2},
	{83, 3, 3},
	{88, 3, 3},
	{109, 3, 3},
	{112, 3, 3},
	{103, 4, 3},
	{105, 4, 3},
	{161, 4, 3},
	{112, 5, 3},
	{115, 5, 3},
	{121, 5, 3},
	{127, 5, 3},
	{134, 5, 3},
	{137, 5, 3},
	{148, 5, 3},
	{157, 5, 3},
	{169, 5, 3},
	{172, 5, 3},
	{182, 5, 3},
	{191, 5, 3},
	{195, 5, 3},
	{209, 5, 3},
	{142, 4, 4},
	{146, 4, 4},
	{161, 4, 4},
	{141, 5, 4},
	{150, 5, 4},
	{165, 5, 4},
	{176, 5, 4}
};

static u32 cle266_encode_pll(struct pll_config pll)
{
	return (pll.multiplier << 8)
		| (pll.rshift << 6)
		| pll.divisor;
}

static u32 k800_encode_pll(struct pll_config pll)
{
	return ((pll.divisor - 2) << 16)
		| (pll.rshift << 10)
		| (pll.multiplier - 2);
}

static u32 vx855_encode_pll(struct pll_config pll)
{
	return (pll.divisor << 16)
		| (pll.rshift << 10)
		| pll.multiplier;
}

static inline u32 get_pll_internal_frequency(u32 ref_freq,
	struct pll_config pll)
{
	return ref_freq / pll.divisor * pll.multiplier;
}

static inline u32 get_pll_output_frequency(u32 ref_freq, struct pll_config pll)
{
	return get_pll_internal_frequency(ref_freq, pll)>>pll.rshift;
}

static struct pll_config get_pll_config(struct pll_config *config, int size,
	int clk)
{
	struct pll_config best = config[0];
	const u32 f0 = 14318180; /* X1 frequency */
	int i;

	for (i = 1; i < size; i++) {
		if (abs(get_pll_output_frequency(f0, config[i]) - clk)
			< abs(get_pll_output_frequency(f0, best) - clk))
			best = config[i];
	}
	return best;
}

static u32
via_get_clk_value(struct drm_device *dev, int clk)
{
	u32 value = 0;

	switch (dev->pdev->device) {
	case PCI_DEVICE_ID_VIA_CLE266:
	case PCI_DEVICE_ID_VIA_KM400:
		value = cle266_encode_pll(get_pll_config(cle266_pll_config,
			ARRAY_SIZE(cle266_pll_config), clk));
		break;
	case PCI_DEVICE_ID_VIA_K8M800:
	case PCI_DEVICE_ID_VIA_PM800:
	case PCI_DEVICE_ID_VIA_CN700:
		value = k800_encode_pll(get_pll_config(k800_pll_config,
			ARRAY_SIZE(k800_pll_config), clk));
		break;
	case PCI_DEVICE_ID_VIA_VT3157:
	case PCI_DEVICE_ID_VIA_CN750:
	case PCI_DEVICE_ID_VIA_K8M890:
	case PCI_DEVICE_ID_VIA_VT3343:
	case PCI_DEVICE_ID_VIA_P4M900:
	case PCI_DEVICE_ID_VIA_VT1122:
		value = k800_encode_pll(get_pll_config(cx700_pll_config,
			ARRAY_SIZE(cx700_pll_config), clk));
		break;
	case PCI_DEVICE_ID_VIA_VX875:
	case PCI_DEVICE_ID_VIA_VX900:
		value = vx855_encode_pll(get_pll_config(vx855_pll_config,
			ARRAY_SIZE(vx855_pll_config), clk));
		break;
	}

	return value;
}

/* Set VCLK*/
static void
via_set_vclock(struct drm_crtc *crtc, u32 clk)
{
	struct via_crtc *iga = container_of(crtc, struct via_crtc, base);
	struct drm_via_private *dev_priv = crtc->dev->dev_private;
	struct drm_device *dev = crtc->dev;
	u8 orig;

	/* H.W. Reset : ON */
	orig = (vga_rcrt(VGABASE, 0x17) & ~0x80);
	vga_wcrt(VGABASE, 0x17, orig);

	/* Enable reset */
	if (!iga->index) {
		orig = (vga_rseq(VGABASE, 0x40) | 0x02);
		vga_wseq(VGABASE, 0x40, orig);
	} else {
		orig = (vga_rseq(VGABASE, 0x40) | 0x04);
		vga_wseq(VGABASE, 0x40, orig);
	}

	if (!iga->index) {
		/* Change D,N FOR VCLK */
		switch (dev->pdev->device) {
		case PCI_DEVICE_ID_VIA_CLE266:
		case PCI_DEVICE_ID_VIA_KM400:
			vga_wseq(VGABASE, 0x46, (clk & 0x00FF));
			vga_wseq(VGABASE, 0x47, (clk & 0xFF00) >> 8);
			break;

		case PCI_DEVICE_ID_VIA_K8M800:
		case PCI_DEVICE_ID_VIA_PM800:
		case PCI_DEVICE_ID_VIA_CN700:
		case PCI_DEVICE_ID_VIA_VT3157:
		case PCI_DEVICE_ID_VIA_CN750:
		case PCI_DEVICE_ID_VIA_K8M890:
		case PCI_DEVICE_ID_VIA_VT3343:
		case PCI_DEVICE_ID_VIA_P4M900:
		case PCI_DEVICE_ID_VIA_VT1122:
		case PCI_DEVICE_ID_VIA_VX875:
		case PCI_DEVICE_ID_VIA_VX900:
			vga_wseq(VGABASE, 0x44, (clk & 0x0000FF));
			vga_wseq(VGABASE, 0x45, (clk & 0x00FF00) >> 8);
			vga_wseq(VGABASE, 0x46, (clk & 0xFF0000) >> 16);
			break;
		}
	} else {
		/* Change D,N FOR LCK */
		switch (dev->pdev->device) {
		case PCI_DEVICE_ID_VIA_CLE266:
		case PCI_DEVICE_ID_VIA_KM400:
			vga_wseq(VGABASE, 0x44, (clk & 0x00FF));
			vga_wseq(VGABASE, 0x45, (clk & 0xFF00) >> 8);
			break;

		case PCI_DEVICE_ID_VIA_K8M800:
		case PCI_DEVICE_ID_VIA_PM800:
		case PCI_DEVICE_ID_VIA_CN700:
		case PCI_DEVICE_ID_VIA_VT3157:
		case PCI_DEVICE_ID_VIA_CN750:
		case PCI_DEVICE_ID_VIA_K8M890:
		case PCI_DEVICE_ID_VIA_VT3343:
		case PCI_DEVICE_ID_VIA_P4M900:
		case PCI_DEVICE_ID_VIA_VT1122:
		case PCI_DEVICE_ID_VIA_VX875:
		case PCI_DEVICE_ID_VIA_VX900:
			vga_wseq(VGABASE, 0x4A, (clk & 0x0000FF));
			vga_wseq(VGABASE, 0x4B, (clk & 0x00FF00) >> 8);
			vga_wseq(VGABASE, 0x4C, (clk & 0xFF0000) >> 16);
			break;
		}
	}

	/* disable reset */
	if (!iga->index) {
		orig = (vga_rseq(VGABASE, 0x40) & ~0x02);
		vga_wseq(VGABASE, 0x40, orig);
	} else {
		orig = (vga_rseq(VGABASE, 0x40) & ~0x04);
		vga_wseq(VGABASE, 0x40, orig);
	}

	/* H.W. Reset : OFF */
	orig = (vga_rcrt(VGABASE, 0x17) & ~0x80);
	vga_wcrt(VGABASE, 0x17, (orig | BIT(7)));

	/* The clock is finally ready !! */
	orig = (vga_r(VGABASE, VGA_MIS_R) & ~0x0C);
	vga_w(VGABASE, VGA_MIS_W, orig | 0x0C);
}

void
via_set_pll(struct drm_crtc *crtc, struct drm_display_mode *mode)
{
	u32 clock = mode->crtc_htotal * mode->crtc_vtotal;
	u32 refresh = drm_mode_vrefresh(mode), pll_D_N;

	pll_D_N = via_get_clk_value(crtc->dev, clock * refresh);
	via_set_vclock(crtc, pll_D_N);
}
