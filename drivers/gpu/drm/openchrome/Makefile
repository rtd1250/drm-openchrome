#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

ccflags-y := -Iinclude/drm
openchrome-y := openchrome_clocks.o \
		openchrome_crtc.o \
		openchrome_crtc_hw.o \
		openchrome_cursor.o \
		openchrome_dac.o \
		openchrome_display.o \
		openchrome_drv.o \
		openchrome_encoder.o \
		openchrome_fp.o \
		openchrome_hdmi.o \
		openchrome_i2c.o \
		openchrome_init.o \
		openchrome_ioctl.o \
		openchrome_object.o \
		openchrome_pm.o \
		openchrome_sii164.o \
		openchrome_tmds.o \
		openchrome_ttm.o \
		openchrome_vt1632.o

obj-$(CONFIG_DRM_OPENCHROME)	+= openchrome.o
