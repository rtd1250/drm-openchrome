#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

ccflags-y := -Iinclude/drm
openchrome-y := openchrome_analog.o \
		openchrome_clocks.o \
		openchrome_crtc.o \
		openchrome_crtc_hw.o \
		openchrome_display.o \
		openchrome_drv.o \
		openchrome_fb.o \
		openchrome_fence.o \
		openchrome_fp.o \
		openchrome_gem.o \
		openchrome_h1_cmdbuf.o \
		openchrome_h1_dma.o \
		openchrome_hdmi.o \
		openchrome_i2c.o \
		openchrome_ioc32.o \
		openchrome_irq.o \
		openchrome_pm.o \
		openchrome_sgdma.o \
		openchrome_tmds.o \
		openchrome_ttm.o \
		openchrome_verifier.o

obj-$(CONFIG_DRM_OPENCHROME)	+= openchrome.o
