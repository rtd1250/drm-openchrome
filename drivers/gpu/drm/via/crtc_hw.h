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
 *      James Simmons <jsimmons@infradead.org>
 */

#ifndef __CRTC_HW_H__
#define __CRTC_HW_H__

struct io_register {
	u16 ioport;
	u8 io_addr;
	u8 start_bit;
	u8 end_bit;
};

struct registers {
	unsigned int count;
	struct io_register *regs;
};

/************************************************
 *****     Display Timing       *****
 ************************************************/

struct crtc_timings {
	struct registers htotal;
	struct registers hdisplay;
	struct registers hblank_start;
	struct registers hblank_end;
	struct registers hsync_start;
	struct registers hsync_end;
	struct registers vtotal;
	struct registers vdisplay;
	struct registers vblank_start;
	struct registers vblank_end;
	struct registers vsync_start;
	struct registers vsync_end;
};

extern void regs_init(void __iomem *regs);
extern void load_value_to_registers(void __iomem *regbase,
					struct registers *regs, int value);

#endif /* __CRTC_HW_H__ */
