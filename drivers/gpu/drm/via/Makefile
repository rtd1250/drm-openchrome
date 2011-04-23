#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

ccflags-y := -Iinclude/drm
via-y    := via_drv.o via_ioc32.o via_i2c.o via_irq.o via_verifier.o \
		init_ttm.o ttm_pci_backend.o via_ttm.o via_gem.o via_fb.o \
		via_dmablit.o via_dma.o via_video.o \
		via_display.o crtc_hw.o via_analog.o

obj-$(CONFIG_DRM_VIA)	+= via.o
