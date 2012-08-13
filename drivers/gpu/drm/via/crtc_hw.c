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
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) OR COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	James Simmons <jsimmons@infradead.org>
 */

#include <video/vga.h>
#include "crtc_hw.h"

void
regs_init(void __iomem *regs)
{
	int i;

	vga_wseq(regs, 0x1, 0x01);
	vga_wseq(regs, 0x2, 0x0f);
	vga_wseq(regs, 0x3, 0x00);
	vga_wseq(regs, 0x4, 0x0e);

	for (i = 0; i < 0x5; i++)
		vga_wgfx(regs, i, 0x00);

	vga_wgfx(regs, 0x05, 0x40);
	vga_wgfx(regs, 0x06, 0x05);
	vga_wgfx(regs, 0x07, 0x0f);
	vga_wgfx(regs, 0x08, 0xff);

	for (i = 0; i < 0x10; i++)
		vga_wattr(regs, i, i);

	vga_r(regs, VGA_IS1_RC);
	vga_wattr(regs, 0x10, 0x41);
	vga_wattr(regs, 0x11, 0xff);
	vga_wattr(regs, 0x12, 0x0f);
	vga_wattr(regs, 0x13, 0x00);
}

void
load_register_tables(void __iomem *regbase, struct vga_registers *regs)
{
	u8 port, cr_index, orig, reg_mask, data;
	int i;

	for (i = 0; i < regs->count; i++) {
		reg_mask = regs->regs[i].start_bit;
		data = regs->regs[i].end_bit;
		cr_index = regs->regs[i].io_addr;
		port = regs->regs[i].ioport;

		vga_w(regbase, port, cr_index);
		orig = (vga_r(regbase, port + 1) & ~reg_mask);
		vga_w(regbase, port + 1, ((data & reg_mask) | orig));
	}
}

void
load_value_to_registers(void __iomem *regbase, struct vga_registers *regs,
			unsigned int value)
{
	unsigned int bit_num = 0, shift_next_reg, reg_mask;
	u8 start_index, end_index, cr_index, orig;
	unsigned int data, i, j;
	u16 get_bit, port;

	for (i = 0; i < regs->count; i++) {
		start_index = regs->regs[i].start_bit;
		end_index = regs->regs[i].end_bit;
		cr_index = regs->regs[i].io_addr;
		port = regs->regs[i].ioport;
		reg_mask = data = 0;

		shift_next_reg = bit_num;
		for (j = start_index; j <= end_index; j++) {
			reg_mask = reg_mask | (1 << j);
			get_bit = (value & (1 << bit_num));
			data |= ((get_bit >> shift_next_reg) << start_index);
			bit_num++;
		}

		vga_w(regbase, port, cr_index);
		orig = (vga_r(regbase, port + 1) & ~reg_mask);
		vga_w(regbase, port + 1, ((data & reg_mask) | orig));
	}
}
