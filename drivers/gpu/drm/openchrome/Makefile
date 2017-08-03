#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

ccflags-y := -Iinclude/drm
openchrome-y    := via_drv.o via_pm.o via_i2c.o via_irq.o via_verifier.o via_ioc32.o \
		ttm_gem.o via_ttm.o via_fence.o via_sgdma.o \
		via_h1_dma.o via_h1_cmdbuf.o \
		via_display.o via_crtc.o crtc_hw.o via_clocks.o \
		via_analog.o via_fp.o via_tmds.o via_hdmi.o via_fb.o

obj-$(CONFIG_DRM_OPENCHROME)	+= openchrome.o
