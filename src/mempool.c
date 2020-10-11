#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mempool.h"

// all data types to ensure data alighment
union align {
	int i;
	long l;
	long *lp;
	void *p;
	void (*fp)(void);
	float f;
	double d;
};

// padded header for allocated chunks
union header {
	mempool_chunk chunk;
	union align a;
};

mempool*
mempool_new_default(void) {
	// default to 10k per block and consider 3x older blocks
	return mempool_new(10 * 1024, 3);
}

mempool*
mempool_new(uint32_t chunk_size, uint8_t lookback) {
	mempool *pool = malloc(sizeof (mempool));
	if (pool == NULL)
		return NULL;
	pool->chunks = NULL;
	pool->chunk_size = chunk_size;
	pool->lookback = lookback;
	return pool;
}

void
mempool_free(mempool **pool) {
	mempool_chunk *c = (*pool)->chunks;
	while (c) {
		// tofree is a linked-list node. copy ->next before free'ing
		mempool_chunk *tofree = c;
		c = c->next;
		// free the old node
		free(tofree);
	}
	(*pool)->chunks = NULL;

	// release memory allocated for the mempool struct and NULL the pointer
	free(*pool);
	*pool = NULL;
}

void *
mempool_alloc(mempool *pool, uint32_t nbytes) {
	uint8_t *avail_bytes;

	// sanity check that a valid pool and non-zero bytes are being allocated
	if (!pool || nbytes <= 0)
		return NULL;

	// pad nbytes to ensure data alignment cause data to go past bounds
	nbytes = ((nbytes + sizeof (union align) - 1) / (sizeof (union align))) * (sizeof (union align));
	while (true) {
		// memory chunk that we're checking
		mempool_chunk *chunk = pool->chunks;
		if (chunk) {
			// try to fake-alloc a section of this memory chunk that fits the data
			while (nbytes < chunk->limit - chunk->avail) {
				// available bytes must not change for this CAS
				avail_bytes = chunk->avail;
				uint8_t *updated_avail_bytes = avail_bytes + nbytes;
				bool success = __atomic_compare_exchange(&chunk->avail, &avail_bytes, &updated_avail_bytes, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
				if (success) {
					return avail_bytes;
				}
				else {
					// this count won't be perfect. no need to impact performance to make it perfect
					pool->cas_alloc_retries++;
				}
			}
		}

		// if another thread already made a new chunk, try it
		if (chunk != pool->chunks)
			continue;

		// make a new chunk of memory for the pool
		uint32_t chunk_size = sizeof (union header) + nbytes + pool->chunk_size;
		mempool_chunk *next_chunk = malloc(chunk_size);
		if (next_chunk == NULL)
			return NULL;
		next_chunk->avail = (uint8_t *)((union header *)next_chunk + 1);
		next_chunk->avail += nbytes;
		next_chunk->limit = (uint8_t *)next_chunk + chunk_size;

		// copy this before another thread might edit it
		avail_bytes = next_chunk->avail - nbytes;

		// prepend this new chunk to the pool's list
		while (true) {
			// cache existing list and point this link at it
			chunk = pool->chunks;
			pool->chunks = chunk;
			// available bytes must not change for this CAS
			bool success = __atomic_compare_exchange(&pool->chunks, &chunk, &next_chunk, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
			if (success) {
				return avail_bytes;
			}
			else {
				// this count won't be perfect. no need to impact performance to make it perfect
				pool->cas_chunk_append_retries++;
			}
		}
	}
}

void *
mempool_calloc(mempool *pool, uint32_t count, uint32_t size) {
	void *ptr;

	// sanity check that a non-zero amount of data is being requested
	if (count <= 0)
		return NULL;

	// allocate and zero out the data
	ptr = mempool_alloc(pool, count * size);
	memset(ptr, '\0', count * size);

	return ptr;
}
