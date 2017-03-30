/*
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	James Simmons <jsimmons@infradead.org>
 */

#ifndef __CRTC_HW_H__
#define __CRTC_HW_H__

#include <video/vga.h>


struct vga_regset {
	u16	ioport;
	u8	io_addr;
	u8	start_bit;
	u8	end_bit;
};

struct vga_registers {
	unsigned int count;
	struct vga_regset *regs;
};

/************************************************
 *****     Display Timing       *****
 ************************************************/

struct crtc_timings {
	struct vga_registers htotal;
	struct vga_registers hdisplay;
	struct vga_registers hblank_start;
	struct vga_registers hblank_end;
	struct vga_registers hsync_start;
	struct vga_registers hsync_end;
	struct vga_registers vtotal;
	struct vga_registers vdisplay;
	struct vga_registers vblank_start;
	struct vga_registers vblank_end;
	struct vga_registers vsync_start;
	struct vga_registers vsync_end;
};

/* Write a value to misc register with a mask */
static inline void svga_wmisc_mask(void __iomem *regbase, u8 data, u8 mask)
{
	vga_w(regbase, VGA_MIS_W, (data & mask) | (vga_r(regbase, VGA_MIS_R) & ~mask));
}

/* Write a value to a sequence register with a mask */
static inline void svga_wseq_mask(void __iomem *regbase, u8 index, u8 data, u8 mask)
{
	vga_wseq(regbase, index, (data & mask) | (vga_rseq(regbase, index) & ~mask));
}

/* Write a value to a CRT register with a mask */
static inline void svga_wcrt_mask(void __iomem *regbase, u8 index, u8 data, u8 mask)
{
	vga_wcrt(regbase, index, (data & mask) | (vga_rcrt(regbase, index) & ~mask));
}

extern void load_register_tables(void __iomem *regbase, struct vga_registers *regs);
extern void load_value_to_registers(void __iomem *regbase, struct vga_registers *regs,
					unsigned int value);

#endif /* __CRTC_HW_H__ */
