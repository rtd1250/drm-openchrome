/*
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
