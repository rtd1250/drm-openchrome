/*
 * Copyright 2013 James Simmons <jsimmons@infradead.org>
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
#include <linux/random.h>
#include "drmP.h"

#include "via_drv.h"

static void
via_fence_destroy(struct kref *kref)
{
	struct via_fence *fence = container_of(kref, struct via_fence, kref);

	if (fence->pool->fence_cleanup)
		fence->pool->fence_cleanup(fence);
	kfree(fence);
}

struct via_fence *
via_fence_create_and_emit(struct via_fence_pool *pool, void *data,
				unsigned int engine)
{
	struct via_fence *fence = NULL;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (fence) {
		struct drm_hash_item *hash;
		int ret = -EINVAL;

		fence->timeout = jiffies + 3 * HZ;
		fence->engine = engine;
		fence->pool = pool;
		fence->priv = data;
		kref_init(&fence->kref);

		if (engine >= pool->num_engines) {
			via_fence_unref((void **) &fence);
			return ERR_PTR(-ENXIO);
		}
		spin_lock(&pool->lock);
try_again:
		/* I like to use get_random_init but it is not exported :-( */
		get_random_bytes(&fence->seq.key, 3);
		/* For the small change you get a zero */
		if (!fence->seq.key)
			goto try_again;

		if (!drm_ht_find_item(&pool->pending, fence->seq.key, &hash))
			goto try_again;

		if (!drm_ht_insert_item(&pool->pending, &fence->seq))
			ret = pool->fence_emit(fence);
		spin_unlock(&pool->lock);

		if (ret) {
			drm_ht_remove_item(&pool->pending, &fence->seq);
			via_fence_unref((void **) &fence);
			fence = ERR_PTR(ret);
		}
	}
	return fence;
}

static void
via_fence_work(struct work_struct *work)
{
	struct via_fence_engine *eng = container_of(work, struct via_fence_engine,
							fence_work);
	uint32_t seq = readl(eng->read_seq);
	struct drm_hash_item *hash;

	spin_lock(&eng->pool->lock);
	if (!drm_ht_find_item(&eng->pool->pending, seq, &hash)) {
		drm_ht_remove_item(&eng->pool->pending, hash);
		if (eng->pool->fence_signaled) {
			struct via_fence *fence;

			fence = drm_hash_entry(hash, struct via_fence, seq);
			if (eng->pool->fence_signaled)
				eng->pool->fence_signaled(fence);
		}
	}
	spin_unlock(&eng->pool->lock);
}

static bool
via_fence_seq_signaled(struct via_fence *fence, u64 seq)
{
	struct drm_hash_item *key;
	bool ret = false;

	/* Still waiting to be processed */
	spin_lock(&fence->pool->lock);
	if (!drm_ht_find_item(&fence->pool->pending, seq, &key))
		ret = true;
	spin_unlock(&fence->pool->lock);
	return ret;
}

/* TTM fence methods */
bool
via_fence_signaled(void *sync_obj)
{
	struct via_fence *fence = sync_obj;

	if (!fence || !fence->seq.key)
		return true;

	if (via_fence_seq_signaled(fence, fence->seq.key)) {
		fence->seq.key = 0;
		return true;
	}
	return false;
}

int
via_fence_wait(void *sync_obj, bool lazy, bool interruptible)
{
	struct via_fence *fence = sync_obj;
	int ret = 0;

	while (!via_fence_seq_signaled(fence, fence->seq.key)) {
		if (time_after(jiffies, fence->timeout)) {
			DRM_INFO("The fence wait timeout timeout = %lu, jiffies = %lu.\n",
				fence->timeout, jiffies);
			ret = -EBUSY;
			break;
		}

		set_current_state(interruptible ? TASK_INTERRUPTIBLE :
						TASK_UNINTERRUPTIBLE);

		if (interruptible && signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
	}
	set_current_state(TASK_RUNNING);
	return ret;
}

int
via_fence_flush(void *sync_obj)
{
	return 0;
}

void
via_fence_unref(void **sync_obj)
{
	struct via_fence *fence = *sync_obj;

	*sync_obj = NULL;
	if (fence)
		kref_put(&fence->kref, &via_fence_destroy);
}

void *
via_fence_ref(void *sync_obj)
{
	struct via_fence *fence = sync_obj;

	kref_get(&fence->kref);
	return sync_obj;
}

/* We assert 30 * sizeof(uint32_t) is enough for emit fence sequence */
#define FENCE_CMD_BUFFER (256 * sizeof(uint32_t))

int
via_fence_pool_init(struct via_fence_pool *pool, struct drm_device *dev,
			char *name, int num_engines, struct dma_pool *ctx)
{
	int size = sizeof(num_engines * sizeof(struct via_fence_engine *));
	struct drm_via_private *dev_priv = dev->dev_private;
	struct via_fence_engine *eng;
	int ret = 0, i;
	void *par;

	i = sizeof(num_engines * sizeof(struct via_fence_engine));
	par = kzalloc(size + i, GFP_KERNEL);
	if (!par)
		return -ENOMEM;

	pool->engines = par;
	eng = par + size;

	/* allocate fence sync bo */
	ret = ttm_allocate_kernel_buffer(&dev_priv->bdev, PAGE_SIZE, 16,
				TTM_PL_FLAG_VRAM, &pool->fence_sync);
	if (unlikely(ret)) {
		DRM_ERROR("allocate fence sync bo error.\n");
		goto out_err;
	}

	pool->cmd_buffer = kzalloc(FENCE_CMD_BUFFER, GFP_KERNEL);
	if (!pool->cmd_buffer)
		goto out_err;

	spin_lock_init(&pool->lock);
	pool->num_engines = num_engines;
	pool->dev = dev;

	if (!ctx) {
		struct page *page = pool->fence_sync.bo->ttm->pages[0];

		pool->bus_addr = dma_map_page(dev->dev, page, 0, PAGE_SIZE,
						DMA_BIDIRECTIONAL);
	}

	for (i = 0; i < pool->num_engines; i++) {
		eng->write_seq = pool->fence_sync.virtual + VIA_FENCE_SIZE * i;
		if (!ctx) {
			eng->fence_phy_addr = pool->bus_addr + VIA_FENCE_SIZE * i;
			eng->read_seq = eng->write_seq;
		} else {
			eng->read_seq = dma_pool_alloc(ctx, GFP_KERNEL,
							&eng->fence_phy_addr);
		}

		INIT_WORK(&eng->fence_work, via_fence_work);
		eng->fence_wq = create_singlethread_workqueue(name);
		eng->pool = pool;
		pool->engines[i] = eng;
		eng += sizeof(struct via_fence_engine);
	}
	ret = drm_ht_create(&pool->pending, 12);
out_err:
	if (ret)
		via_fence_pool_fini(pool);
	return ret;
}

void
via_fence_pool_fini(struct via_fence_pool *pool)
{
	struct ttm_buffer_object *sync_bo;
	struct via_fence_engine *eng;
	int i;

	drm_ht_remove(&pool->pending);

	kfree(pool->cmd_buffer);

	for (i = 0; i < pool->num_engines; i++) {
		eng = pool->engines[i];

		destroy_workqueue(eng->fence_wq);
	}
	kfree(pool->engines);

	sync_bo = pool->fence_sync.bo;
	if (sync_bo) {
		ttm_bo_unpin(sync_bo, &pool->fence_sync);
		ttm_bo_unref(&sync_bo);
	}

	if (pool->bus_addr)
		dma_unmap_page(pool->dev->dev, pool->bus_addr, PAGE_SIZE,
				DMA_BIDIRECTIONAL);

	kfree(pool);
}
