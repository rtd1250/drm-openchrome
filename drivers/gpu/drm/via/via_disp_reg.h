/*
 * Copyright 1998-2009 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2009 S3 Graphics, Inc. All Rights Reserved.
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

#ifndef __VIA_DISP_REG_H__
#define __VIA_DISP_REG_H__

/***********************************************/
/*************** FIFO register *****************/
/***********************************************/
#define IGA1_FIFO_DEPTH_SELECT_REG_NUM          1
#define IGA1_FIFO_THRESHOLD_REG_NUM             2
#define IGA1_FIFO_HIGH_THRESHOLD_REG_NUM        2
#define IGA1_DISPLAY_QUEUE_EXPIRE_NUM_REG_NUM   2

#define IGA2_FIFO_DEPTH_SELECT_REG_NUM          3
#define IGA2_FIFO_THRESHOLD_REG_NUM             2
#define IGA2_FIFO_HIGH_THRESHOLD_REG_NUM        2
#define IGA2_DISPLAY_QUEUE_EXPIRE_NUM_REG_NUM   1


/* Definition FIFO Design Method */
#define IGA1_FIFO_DEPTH_SELECT_FORMULA(x)                   (x/2)-1
#define IGA1_FIFO_THRESHOLD_FORMULA(x)                      (x/4)
#define IGA1_DISPLAY_QUEUE_EXPIRE_NUM_FORMULA(x)            (x/4)
#define IGA1_FIFO_HIGH_THRESHOLD_FORMULA(x)                 (x/4)

#define IGA2_FIFO_DEPTH_SELECT_FORMULA(x)                   ((x/2)/4)-1
#define IGA2_FIFO_THRESHOLD_FORMULA(x)                      (x/4)
#define IGA2_DISPLAY_QUEUE_EXPIRE_NUM_FORMULA(x)            (x/4)
#define IGA2_FIFO_HIGH_THRESHOLD_FORMULA(x)                 (x/4)


/*Define Display OFFSET*/
/* VT3314 chipset*/
#define CN700_IGA1_FIFO_MAX_DEPTH               96 /* location: {SR17,0,7}*/
#define CN700_IGA1_FIFO_THRESHOLD               80 /* location: {SR16,0,5},{SR16,7,7}*/
#define CN700_IGA1_FIFO_HIGH_THRESHOLD          64  /* location: {SR18,0,5},{SR18,7,7}*/
#define CN700_IGA1_DISPLAY_QUEUE_EXPIRE_NUM     0   /* location: {SR22,0,4}. (128/4) =64, P800 must be set zero, because HW only 5 bits*/

#define CN700_IGA2_FIFO_MAX_DEPTH               96  /* location: {CR68,4,7},{CR94,7,7},{CR95,7,7}*/
#define CN700_IGA2_FIFO_THRESHOLD               80  /* location: {CR68,0,3},{CR95,4,6}*/
#define CN700_IGA2_FIFO_HIGH_THRESHOLD          32   /* location: {CR92,0,3},{CR95,0,2}*/
#define CN700_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     128 /* location: {CR94,0,6}*/

/* For VT3324, these values are suggested by HW */
#define CX700_IGA1_FIFO_MAX_DEPTH               192     /* location: {SR17,0,7}*/
#define CX700_IGA1_FIFO_THRESHOLD               128     /* location: {SR16,0,5},{SR16,7,7}*/
#define CX700_IGA1_FIFO_HIGH_THRESHOLD          128     /* location: {SR18,0,5},{SR18,7,7} */
#define CX700_IGA1_DISPLAY_QUEUE_EXPIRE_NUM     124     /* location: {SR22,0,4} */

#define CX700_IGA2_FIFO_MAX_DEPTH               96      /* location: {CR68,4,7},{CR94,7,7},{CR95,7,7}*/
#define CX700_IGA2_FIFO_THRESHOLD               64      /* location: {CR68,0,3},{CR95,4,6}*/
#define CX700_IGA2_FIFO_HIGH_THRESHOLD          32      /* location: {CR92,0,3},{CR95,0,2} */
#define CX700_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     128     /* location: {CR94,0,6}*/

/* VT3336 chipset*/
#define K8M890_IGA1_FIFO_MAX_DEPTH               360 /* location: {SR17,0,7}*/
#define K8M890_IGA1_FIFO_THRESHOLD               328 /* location: {SR16,0,5},{SR16,7,7}*/
#define K8M890_IGA1_FIFO_HIGH_THRESHOLD          296 /* location: {SR18,0,5},{SR18,7,7}*/
#define K8M890_IGA1_DISPLAY_QUEUE_EXPIRE_NUM     124 /* location: {SR22,0,4}.*/

#define K8M890_IGA2_FIFO_MAX_DEPTH               360 /* location: {CR68,4,7},{CR94,7,7},{CR95,7,7}*/
#define K8M890_IGA2_FIFO_THRESHOLD               328 /* location: {CR68,0,3},{CR95,4,6}*/
#define K8M890_IGA2_FIFO_HIGH_THRESHOLD          296 /* location: {CR92,0,3},{CR95,0,2}*/
#define K8M890_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     124 /* location: {CR94,0,6}*/

/* VT3327 chipset*/
#define P4M890_IGA1_FIFO_MAX_DEPTH               96  /* location: {SR17,0,7}*/
#define P4M890_IGA1_FIFO_THRESHOLD               76  /* location: {SR16,0,5},{SR16,7,7}*/
#define P4M890_IGA1_FIFO_HIGH_THRESHOLD          64  /* location: {SR18,0,5},{SR18,7,7}*/
#define P4M890_IGA1_DISPLAY_QUEUE_EXPIRE_NUM     32  /* location: {SR22,0,4}. (32/4) =8*/

#define P4M890_IGA2_FIFO_MAX_DEPTH               96  /* location: {CR68,4,7},{CR94,7,7},{CR95,7,7}*/
#define P4M890_IGA2_FIFO_THRESHOLD               76  /* location: {CR68,0,3},{CR95,4,6}*/
#define P4M890_IGA2_FIFO_HIGH_THRESHOLD          64  /* location: {CR92,0,3},{CR95,0,2}*/
#define P4M890_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     32  /* location: {CR94,0,6}*/

/* VT3364 chipset*/
#define P4M900_IGA1_FIFO_MAX_DEPTH               96     /* location: {SR17,0,7}*/
#define P4M900_IGA1_FIFO_THRESHOLD               76     /* location: {SR16,0,5},{SR16,7,7}*/
#define P4M900_IGA1_FIFO_HIGH_THRESHOLD          76     /* location: {SR18,0,5},{SR18,7,7}*/
#define P4M900_IGA1_DISPLAY_QUEUE_EXPIRE_NUM     32     /* location: {SR22,0,4}.*/

#define P4M900_IGA2_FIFO_MAX_DEPTH               96     /* location: {CR68,4,7},{CR94,7,7},{CR95,7,7}*/
#define P4M900_IGA2_FIFO_THRESHOLD               76     /* location: {CR68,0,3},{CR95,4,6}*/
#define P4M900_IGA2_FIFO_HIGH_THRESHOLD          76     /* location: {CR92,0,3},{CR95,0,2}*/
#define P4M900_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     32     /* location: {CR94,0,6}*/

/* For VT3353, these values are suggested by HW */
#define VX800_IGA1_FIFO_MAX_DEPTH               192     /* location: {SR17,0,7}*/
#define VX800_IGA1_FIFO_THRESHOLD               152     /* location: {SR16,0,5},{SR16,7,7}*/
#define VX800_IGA1_FIFO_HIGH_THRESHOLD          152     /* location: {SR18,0,5},{SR18,7,7} */
#define VX800_IGA1_DISPLAY_QUEUE_EXPIRE_NUM      64     /* location: {SR22,0,4} */

#define VX800_IGA2_FIFO_MAX_DEPTH               96      /* location: {CR68,4,7},{CR94,7,7},{CR95,7,7}*/
#define VX800_IGA2_FIFO_THRESHOLD               64      /* location: {CR68,0,3},{CR95,4,6}*/
#define VX800_IGA2_FIFO_HIGH_THRESHOLD          32      /* location: {CR92,0,3},{CR95,0,2} */
#define VX800_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     128     /* location: {CR94,0,6}*/

/* For VT3409 */
#define VX855_IGA1_FIFO_MAX_DEPTH               400
#define VX855_IGA1_FIFO_THRESHOLD               320
#define VX855_IGA1_FIFO_HIGH_THRESHOLD          320
#define VX855_IGA1_DISPLAY_QUEUE_EXPIRE_NUM     160

#define VX855_IGA2_FIFO_MAX_DEPTH               200
#define VX855_IGA2_FIFO_THRESHOLD               160
#define VX855_IGA2_FIFO_HIGH_THRESHOLD          160
#define VX855_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     320

/* For VT3410 */
#define VX900_IGA1_FIFO_MAX_DEPTH               400
#define VX900_IGA1_FIFO_THRESHOLD               320
#define VX900_IGA1_FIFO_HIGH_THRESHOLD          320
#define VX900_IGA1_DISPLAY_QUEUE_EXPIRE_NUM     160

#define VX900_IGA2_FIFO_MAX_DEPTH               192
#define VX900_IGA2_FIFO_THRESHOLD               160
#define VX900_IGA2_FIFO_HIGH_THRESHOLD          160
#define VX900_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     320

#ifdef VIA_VT3293_SUPPORT
/* For VT3293  */
#define CN750_IGA1_FIFO_MAX_DEPTH               96
#define CN750_IGA1_FIFO_THRESHOLD               76
#define CN750_IGA1_FIFO_HIGH_THRESHOLD          76
#define CN750_IGA1_DISPLAY_QUEUE_EXPIRE_NUM     32

#define CN750_IGA2_FIFO_MAX_DEPTH               96
#define CN750_IGA2_FIFO_THRESHOLD               76
#define CN750_IGA2_FIFO_HIGH_THRESHOLD          76
#define CN750_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     32
#endif


/* Display FIFO Relation Registers */

/* IGA1 FIFO Depth_Select */
struct io_register iga1_fifo_depth_select[] = {
	{ VGA_SEQ_I, 0x17, 0, 7}
};

/* IGA2 FIFO Depth_Select */
struct io_register iga2_fifo_depth_select[] = {
	{ VGA_CRT_IC, 0x68, 4, 7},
	{ VGA_CRT_IC, 0x94, 7, 7},
	{ VGA_CRT_IC, 0x95, 7, 7}
};

/* IGA1 FIFO Threshold Select */
struct io_register iga1_fifo_high_threshold_select[] = {
	{ VGA_SEQ_I, 0x16, 0, 5},
	{ VGA_SEQ_I, 0x16, 7, 7}
};

/* IGA2 FIFO Threshold Select */
struct io_register iga2_fifo_high_threshold_select[] = {
	{ VGA_CRT_IC, 0x68, 0, 3},
	{ VGA_CRT_IC, 0x95, 4, 6}
};

/* IGA1 FIFO High Threshold Select */
struct io_register iga1_fifo_threshold_select[] = {
	{ VGA_SEQ_I, 0x18, 0, 5},
	{ VGA_SEQ_I, 0x18, 7, 7}
};

/* IGA2 FIFO High Threshold Select */
struct io_register iga2_fifo_threshold_select[] = {
	{ VGA_CRT_IC, 0x92, 0, 3},
	{ VGA_CRT_IC, 0x95, 0, 2}
};

/* IGA1 FIFO display queue expire */
struct io_register iga1_display_queue_expire_num[] = {
	{ VGA_SEQ_I, 0x22, 0, 4 }
};

/* IGA2 FIFO display queue expire */
struct io_register iga2_display_queue_expire_num[] = {
	{ VGA_CRT_IC, 0x94, 0, 6 }
};

/***********************************************/
/************* Offset register *****************/
/***********************************************/
/* Define Offset and Fetch Count Register*/
#define IGA1_OFFSET_REG_NUM             2                                 /* location: {CR13,0,7},{CR35,5,7} */
#define IGA1_OFFSER_ALIGN_BYTE          8                                 /* 8 bytes alignment. */
#define IGA1_OFFSET_FORMULA(x,y)        (x*y)/IGA1_OFFSER_ALIGN_BYTE      /* x: H resolution, y: color depth */

#define IGA2_OFFSET_REG_NUM             3           /* location: {CR66,0,07},{CR67,0,1}, {CR71,7,7} */
#define IGA2_OFFSET_ALIGN_BYTE		    8
#define IGA2_OFFSET_FORMULA(x,y)	    (x*y)/IGA2_OFFSET_ALIGN_BYTE	 /* x: H resolution, y: color depth */

/* IGA1 Offset Register */
struct io_register iga1_offset[] = {
	{ VGA_CRT_IC, 0x13, 0, 7 },
	{ VGA_CRT_IC, 0x35, 5, 7 }
};

/* IGA2 Offset Register */
struct io_register iga2_offset[] = {
	{ VGA_CRT_IC, 0x66, 0, 7 },
	{ VGA_CRT_IC, 0x67, 0, 1 },
	{ VGA_CRT_IC, 0x71, 7, 7 }
};

/***********************************************/
/*********** Fetch count register **************/
/***********************************************/
#define IGA1_FETCH_COUNT_REG_NUM        2                                 /* location: {SR1C,0,7},{SR1D,0,1} */
#define IGA1_FETCH_COUNT_ALIGN_BYTE     16                                /* 16 bytes alignment. */
#define IGA1_FETCH_COUNT_PATCH_VALUE    4
#define IGA1_FETCH_COUNT_FORMULA(x,y)   ((x*y)/IGA1_FETCH_COUNT_ALIGN_BYTE)+ IGA1_FETCH_COUNT_PATCH_VALUE

#define IGA2_FETCH_COUNT_REG_NUM        2           /* location: {CR65,0,07},{CR67,2,3} */
#define IGA2_FETCH_COUNT_ALIGN_BYTE	    16
#define IGA2_FETCH_COUNT_PATCH_VALUE	0
#define IGA2_FETCH_COUNT_FORMULA(x,y)	((x*y)/IGA1_FETCH_COUNT_ALIGN_BYTE)+ IGA2_FETCH_COUNT_PATCH_VALUE

static struct io_register iga1_fetch_count[] = {
	{ VGA_SEQ_I, 0x1C, 0, 7},
	{ VGA_SEQ_I, 0x1D, 0, 1}
};

static struct io_register iga2_fetch_count[] = {
	{ VGA_CRT_IC, 0x65, 0, 7},
	{ VGA_CRT_IC, 0x67, 2, 3}
};

/***********************************************/
/*********** CRTC timing register **************/
/***********************************************/
/* Define Register Number for IGA1 CRTC Timing */
#define IGA1_HOR_TOTAL_REG_NUM          2           /* location: {CR00,0,7},{CR36,3,3} */
#define IGA1_HOR_ADDR_REG_NUM           2           /* location: {CR01,0,7},{CR45,1,1} */
#define IGA1_HOR_BLANK_START_REG_NUM    2           /* location: {CR02,0,7},{CR45,2,2} */
#define IGA1_HOR_BLANK_END_REG_NUM      3           /* location: {CR03,0,4},{CR05,7,7},{CR33,5,5} */
#define IGA1_HOR_SYNC_START_REG_NUM     2           /* location: {CR04,0,7},{CR33,4,4} */
#define IGA1_HOR_SYNC_END_REG_NUM       1           /* location: {CR05,0,4} */
#define IGA1_VER_TOTAL_REG_NUM          4           /* location: {CR06,0,7},{CR07,0,0},{CR07,5,5},{CR35,0,0} */
#define IGA1_VER_ADDR_REG_NUM           4           /* location: {CR12,0,7},{CR07,1,1},{CR07,6,6},{CR35,2,2} */
#define IGA1_VER_BLANK_START_REG_NUM    4           /* location: {CR15,0,7},{CR07,3,3},{CR09,5,5},{CR35,3,3} */
#define IGA1_VER_BLANK_END_REG_NUM      1           /* location: {CR16,0,7} */
#define IGA1_VER_SYNC_START_REG_NUM     4           /* location: {CR10,0,7},{CR07,2,2},{CR07,7,7},{CR35,1,1} */
#define IGA1_VER_SYNC_END_REG_NUM       1           /* location: {CR11,0,3} */

/* Define Register Number for IGA2 CRTC Timing */
#define IGA2_HOR_TOTAL_REG_NUM          2           /* location: {CR50,0,7},{CR55,0,3} */
#define IGA2_HOR_ADDR_REG_NUM           3           /* location: {CR51,0,7},{CR55,4,6},{CR55,7,7} */
#define IGA2_HOR_BLANK_START_REG_NUM    3           /* location: {CR52,0,7},{CR54,0,2},{CR6B,0,0} */
#define IGA2_HOR_BLANK_END_REG_NUM      3           /* location: CLE266: {CR53,0,7},{CR54,3,5} => CLE266's CR5D[6] is reserved,
                                                       so it may have problem to set 1600x1200 on IGA2. */                                                   /*           Others: {CR53,0,7},{CR54,3,5},{CR5D,6,6} */
#define IGA2_HOR_SYNC_START_REG_NUM     4           /* location: {CR56,0,7},{CR54,6,7},{CR5C,7,7} */
                                                    /*           VT3314 and Later: {CR56,0,7},{CR54,6,7},{CR5C,7,7}, {CR5D,7,7} */
#define IGA2_HOR_SYNC_END_REG_NUM       2           /* location: {CR57,0,7},{CR5C,6,6} */
#define IGA2_VER_TOTAL_REG_NUM          2           /* location: {CR58,0,7},{CR5D,0,2} */
#define IGA2_VER_ADDR_REG_NUM           2           /* location: {CR59,0,7},{CR5D,3,5} */
#define IGA2_VER_BLANK_START_REG_NUM    2           /* location: {CR5A,0,7},{CR5C,0,2} */
#define IGA2_VER_BLANK_END_REG_NUM      2           /* location: {CR5E,0,7},{CR5C,3,5} */
#define IGA2_VER_SYNC_START_REG_NUM     2           /* location: {CR5E,0,7},{CR5F,5,7} */
#define IGA2_VER_SYNC_END_REG_NUM       1           /* location: {CR5F,0,4} */


/* Definition IGA1 Design Method of CRTC Registers */
#define IGA1_HOR_TOTAL_FORMULA(x)	(x/8) - 5
#define IGA1_HOR_ADDR_FORMULA(x)	(x/8) - 1
#define IGA1_HOR_BLANK_START_FORMULA(x)	(x/8) - 1
#define IGA1_HOR_BLANK_END_FORMULA(x)	(x/8) - 1
#define IGA1_HOR_SYNC_START_FORMULA(x)	(x/8) - 1
#define IGA1_HOR_SYNC_END_FORMULA(x)	(x/8) - 1

#define IGA1_VER_TOTAL_FORMULA(x)	(x - 2)
#define IGA1_VER_ADDR_FORMULA(x)	(x - 1)
#define IGA1_VER_BLANK_START_FORMULA(x)	(x - 1)
#define IGA1_VER_BLANK_END_FORMULA(x)	(x - 1)
#define IGA1_VER_SYNC_START_FORMULA(x)	(x - 1)
#define IGA1_VER_SYNC_END_FORMULA(x)	(x - 1)


/* Definition IGA2 Design Method of CRTC Registers */
#define IGA2_HOR_TOTAL_FORMULA(x)	(x - 1)
#define IGA2_HOR_ADDR_FORMULA(x)	(x - 1)
#define IGA2_HOR_BLANK_START_FORMULA(x)	(x - 1)
#define IGA2_HOR_BLANK_END_FORMULA(x)	(x - 1)
#define IGA2_HOR_SYNC_START_FORMULA(x)	(x - 1)
#define IGA2_HOR_SYNC_END_FORMULA(x)	(x - 1)

#define IGA2_VER_TOTAL_FORMULA(x)	(x - 1)
#define IGA2_VER_ADDR_FORMULA(x)	(x - 1)
#define IGA2_VER_BLANK_START_FORMULA(x)	(x - 1)
#define IGA2_VER_BLANK_END_FORMULA(x)	(x - 1)
#define IGA2_VER_SYNC_START_FORMULA(x)	(x - 1)
#define IGA2_VER_SYNC_END_FORMULA(x)	(x - 1)

/* Definition IGA1 Design Method of CRTC Pixel timing Registers */
#define IGA1_PIXELTIMING_HOR_TOTAL_FORMULA(x)		(x)-1
#define IGA1_PIXELTIMING_HOR_ADDR_FORMULA(x)		(x)-1
#define IGA1_PIXELTIMING_HOR_BLANK_START_FORMULA(x)	(x)-1
#define IGA1_PIXELTIMING_HOR_BLANK_END_FORMULA(x,y)	(x+y)-1
#define IGA1_PIXELTIMING_HOR_SYNC_START_FORMULA(x)	(x)-1
#define IGA1_PIXELTIMING_HOR_SYNC_END_FORMULA(x,y)	(x+y)-1

#define IGA1_PIXELTIMING_VER_TOTAL_FORMULA(x)		(x)-2
#define IGA1_PIXELTIMING_VER_ADDR_FORMULA(x)		(x)-1
#define IGA1_PIXELTIMING_VER_BLANK_START_FORMULA(x)	(x)-1
#define IGA1_PIXELTIMING_VER_BLANK_END_FORMULA(x,y)	(x+y)-1
#define IGA1_PIXELTIMING_VER_SYNC_START_FORMULA(x)	(x)-1
#define IGA1_PIXELTIMING_VER_SYNC_END_FORMULA(x,y)	(x+y)-1

#define IGA1_PIXELTIMING_HVSYNC_OFFSET_END_FORMULA(x,y)	((x)/2)-1-(x-y)

/************************************************/
/*      Define IGA1 Display Timing              */
/************************************************/

/* IGA1 Horizontal Total */
struct io_register iga1_hor_total[] = {
	{ VGA_CRT_IC, 0x00, 0, 7 },
	{ VGA_CRT_IC, 0x36, 3, 3 }
};

/* IGA1 Horizontal Addressable Video */
struct io_register iga1_hor_addr[] = {
	{ VGA_CRT_IC, 0x01, 0, 7 }
};

/* IGA1 Horizontal Blank Start */
struct io_register iga1_hor_blank_start[] = {
	{ VGA_CRT_IC, 0x02, 0, 7 }
};

/* IGA1 Horizontal Blank End */
struct io_register iga1_hor_blank_end[] = {
	{ VGA_CRT_IC, 0x03, 0, 4 },
	{ VGA_CRT_IC, 0x05, 7, 7 },
	{ VGA_CRT_IC, 0x33, 5, 5 }
};

/* IGA1 Horizontal Sync Start */
struct io_register iga1_hor_sync_start[] = {
	{ VGA_CRT_IC, 0x04, 0, 7 },
	{ VGA_CRT_IC, 0x33, 4, 4 }
};

/* IGA1 Horizontal Sync End */
struct io_register iga1_hor_sync_end[] = {
	{ VGA_CRT_IC, 0x05, 0, 4 }
};

/* IGA1 Vertical Total */
struct io_register iga1_ver_total[] = {
	{ VGA_CRT_IC, 0x06, 0, 7 },
	{ VGA_CRT_IC, 0x07, 0, 0 },
	{ VGA_CRT_IC, 0x07, 5, 5 },
	{ VGA_CRT_IC, 0x35, 0, 0 }
};

/* IGA1 Vertical Addressable Video */
struct io_register iga1_ver_addr[] = {
	{ VGA_CRT_IC, 0x12, 0, 7 },
	{ VGA_CRT_IC, 0x07, 1, 1 },
	{ VGA_CRT_IC, 0x07, 6, 6 },
	{ VGA_CRT_IC, 0x35, 2, 2 }
};

/* IGA1 Vertical Blank Start */
struct io_register iga1_ver_blank_start[] = {
	{ VGA_CRT_IC, 0x15, 0, 7 },
	{ VGA_CRT_IC, 0x07, 3, 3 },
	{ VGA_CRT_IC, 0x09, 5, 5 },
	{ VGA_CRT_IC, 0x35, 3, 3 }
};

/* IGA1 Vertical Blank End */
struct io_register iga1_ver_blank_end[] = {
	{ VGA_CRT_IC, 0x16, 0, 7 }
};

/* IGA1 Vertical Sync Start */
struct io_register iga1_ver_sync_start[] = {
	{ VGA_CRT_IC, 0x10, 0, 7 },
	{ VGA_CRT_IC, 0x07, 2, 2 },
	{ VGA_CRT_IC, 0x07, 7, 7 },
	{ VGA_CRT_IC, 0x35, 1, 1 }
};

/* IGA1 Vertical Sync End */
struct io_register iga1_ver_sync_end[] = {
	{ VGA_CRT_IC, 0x11, 0, 3 }
};

/************************************************/
/*      Define IGA2 Display Timing              */
/************************************************/

/* IGA2 Horizontal Total */
struct io_register iga2_hor_total[] = {
	{ VGA_CRT_IC, 0x50, 0, 7 },
	{ VGA_CRT_IC, 0x55, 0, 3 }
};

/* IGA2 Horizontal Addressable Video */
struct io_register iga2_hor_addr[] = {
	{ VGA_CRT_IC, 0x51, 0, 7 },
	{ VGA_CRT_IC, 0x55, 4, 6 }
};

/* IGA2 Horizontal Blank Start */
struct io_register iga2_hor_blank_start[] = {
	{ VGA_CRT_IC, 0x52, 0, 7 },
	{ VGA_CRT_IC, 0x54, 0, 2 }
};

/* IGA2 Horizontal Blank End */
struct io_register iga2_hor_blank_end[] = {
	{ VGA_CRT_IC, 0x53, 0, 7 },
	{ VGA_CRT_IC, 0x54, 3, 5 },
	{ VGA_CRT_IC, 0x5D, 6, 6 }
};

/* IGA2 Horizontal Sync Start */
struct io_register iga2_hor_sync_start[] = {
	{ VGA_CRT_IC, 0x56, 0, 7 },
	{ VGA_CRT_IC, 0x54, 6, 7 },
	{ VGA_CRT_IC, 0x5C, 7, 7 },
	{ VGA_CRT_IC, 0x5D, 7, 7 }
};

/* IGA2 Horizontal Sync End */
struct io_register iga2_hor_sync_end[] = {
	{ VGA_CRT_IC, 0x57, 0, 7 },
	{ VGA_CRT_IC, 0x5C, 6, 6 }
};

/* IGA2 Vertical Total */
struct io_register iga2_ver_total[] = {
	{ VGA_CRT_IC, 0x58, 0, 7 },
	{ VGA_CRT_IC, 0x5D, 0, 2 }
};

/* IGA2 Vertical Addressable Video */
struct io_register iga2_ver_addr[] = {
	{ VGA_CRT_IC, 0x59, 0, 7 },
	{ VGA_CRT_IC, 0x5D, 3, 5 }
};

/* IGA2 Vertical Blank Start */
struct io_register iga2_ver_blank_start[] = {
	{ VGA_CRT_IC, 0x5A, 0, 7 },
	{ VGA_CRT_IC, 0x5C, 0, 2 }
};

/* IGA2 Vertical Blank End */
struct io_register iga2_ver_blank_end[] = {
	{ VGA_CRT_IC, 0x5B, 0, 7 },
	{ VGA_CRT_IC, 0x5C, 3, 5 }
};

/* IGA2 Vertical Sync Start */
struct io_register iga2_ver_sync_start[] = {
	{ VGA_CRT_IC, 0x5E, 0, 7 },
	{ VGA_CRT_IC, 0x5F, 5, 7 }
};

/* IGA2 Vertical Sync End */
static struct io_register iga2_ver_sync_end[] = {
	{ VGA_CRT_IC, 0x5F, 0, 4 }
};

/* IGA1 pixel timing Registers */
#define	IGA1_PIX_H_TOTAL_REG		0x8400		//[15:0]
#define	IGA1_PIX_H_ADDR_REG		0x8400		//[31:16]
#define	IGA1_PIX_H_BNK_ST_REG		0x8404		//[15:0]
#define	IGA1_PIX_H_BNK_END_REG		0x8404		//[31:16]
#define	IGA1_PIX_H_SYNC_ST_REG		0x8408		//[15:0]
#define	IGA1_PIX_H_SYNC_END_REG		0x8408		//[31:16]
#define	IGA1_PIX_V_TOTAL_REG		0x8424		//[10:0]
#define	IGA1_PIX_V_ADDR_REG		0x8424		//[26:16]
#define	IGA1_PIX_V_BNK_ST_REG		0x8428		//[10:0]
#define IGA1_PIX_V_BNK_END_REG		0x8428		//[26:16]
#define	IGA1_PIX_V_SYNC_ST_REG		0x842C		//[10:0]
#define	IGA1_PIX_V_SYNC_END_REG		0x842C		//[15:12]
#define	IGA1_PIX_HALF_LINE_REG		0x8434		//[15:0]

#define	IGA1_PIX_H_TOTAL_MASK		0x0000FFFF	//[15:0]
#define	IGA1_PIX_H_ADDR_MASK		0xFFFF0000	//[31:16]
#define	IGA1_PIX_H_BNK_ST_MASK		0x0000FFFF	//[15:0]
#define	IGA1_PIX_H_BNK_END_MASK		0xFFFF0000	//[31:16]
#define	IGA1_PIX_H_SYNC_ST_MASK		0x0000FFFF	//[15:0]
#define	IGA1_PIX_H_SYNC_END_MASK	0xFFFF0000	//[31:16]
#define	IGA1_PIX_V_TOTAL_MASK		0x000007FF	//[10:0]
#define	IGA1_PIX_V_ADDR_MASK		0x07FF0000	//[26:16]
#define	IGA1_PIX_V_BNK_ST_MASK		0x000007FF	//[10:0]
#define	IGA1_PIX_V_BNK_END_MASK		0x07FF0000	//[26:16]
#define	IGA1_PIX_V_SYNC_ST_MASK		0x000007FF	//[10:0]
#define	IGA1_PIX_V_SYNC_END_MASK	0x0000F000	//[15:12]
#define	IGA1_PIX_HALF_LINE_MASK		0x0000FFFF	//[15:0]

#endif
