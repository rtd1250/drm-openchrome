/*
 * Copyright 2012 James Simmons. All Rights Reserved.
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
 *
 * Authors:
 *	James Simmons <jsimmons@infradead.org>
 */

#include <video/vga.h>

#include "via_crtc_hw.h"

/*
 * load_register_table enables the ability to set entire
 * tables of registers. For each register defined by the
 * port and the index for that register is programmed
 * with a masked value.
 */
void
load_register_tables(void __iomem *regbase, struct vga_registers *regs)
{
	u8 cr_index, orig, reg_mask, data;
	unsigned int i;
	u16 port;

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

/*
 * Due to the limitation of how much data you can write to a single
 * register we run into data that can't be written in only one register.
 * So load_value_to_register was developed to be able to define register
 * tables that can load different bit ranges of the data to different
 * registers.
 */
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
