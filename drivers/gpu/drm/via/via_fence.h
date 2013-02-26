/*
 * Copyright 2013 James Simmons <jsimmons@infradead.org>
 *
 * Influenced by sample code from VIA Technologies and the radeon driver.
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
#ifndef _VIA_FENCE_H_
#define _VIA_FENCE_H_

#define VIA_FENCE_SIZE 32

struct via_fence;

struct via_fence_engine {
	struct work_struct fence_work;
	struct via_fence_pool *pool;

	/* virtual address for getting seq value */
	void *read_seq;

	int index;
};

struct via_fence_pool {
	struct ttm_bo_kmap_obj fence_sync;
	/* BUS address used for fencing */
	dma_addr_t bus_addr;

	/* for access synchronization */
	spinlock_t lock;

	/* Fence command bounce buffer */
	uint32_t *cmd_buffer;

	struct workqueue_struct *fence_wq;
	struct drm_open_hash pending;
	struct drm_device *dev;

	struct via_fence_engine **engines;
	unsigned int num_engines;

	void (*fence_signaled)(struct via_fence_engine *eng);
	void (*fence_cleanup)(struct via_fence *fence);
	int (*fence_emit)(struct via_fence *fence);
};

struct via_fence {
	/* Which fence pool (DMA or other), this fence is associated with */
	struct via_fence_pool *pool;
	/* the sequence number that the fence object emit,
	 * stored in a hash key */
	struct drm_hash_item seq;
	/* the time to wait for the fence object signal */
        unsigned long timeout;
	/* Which engine this belongs too */
	int engine;
	/* the reference information of this fence object */
	struct kref kref;
	/* place holder for special data specific to fence type */
	void *priv;
};

extern bool via_fence_signaled(void *sync_obj);
extern int via_fence_wait(void *sync_obj, bool lazy, bool interruptible);
extern int via_fence_flush(void *sync_obj);
extern void via_fence_unref(void **sync_obj);
extern void *via_fence_ref(void *sync_obj);

extern struct via_fence *
via_fence_create_and_emit(struct via_fence_pool *pool, void *data,
				unsigned int engine);
extern int
via_fence_pool_init(struct via_fence_pool *pool, char *name, int num_engines,
			int domain, struct drm_device *dev);
extern void via_fence_pool_fini(struct via_fence_pool *pool);

#endif
