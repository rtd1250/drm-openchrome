#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

ccflags-y := -Iinclude/drm
via-y    := via_irq.o via_drv.o via_ioc32.o via_verifier.o via_video.o \
		via_dma.o via_dmablit.o via_fb.o init_ttm.o via_ttm.o via_gem.o

obj-$(CONFIG_DRM_VIA)	+=via.o
