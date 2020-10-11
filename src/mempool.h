#ifndef JFALKNER_MEMPOOL_H
#define JFALKNER_MEMPOOL_H


// chunks of preallocated memory
typedef struct mempool_chunk_s {
	struct mempool_chunk_s *next;
	uint8_t * ptr;
	uint8_t * avail;
	uint8_t * limit;
} mempool_chunk;

// tracks allocated memory from a contiguous block
typedef struct mempool_s {
	mempool_chunk *chunks;
	uint32_t chunk_size;
	uint8_t lookback;
	// tracking of CAS failures for tests and estimating thread contention
	uint32_t cas_alloc_retries;
	uint32_t cas_chunk_append_retries;
} mempool;


mempool * mempool_new_default();

mempool * mempool_new(uint32_t chunk_size, uint8_t lookback);

void * mempool_alloc(mempool *pool, uint32_t size);

void * mempool_calloc(mempool *pool, uint32_t count, uint32_t size);

void mempool_free(mempool **pool);

#endif // JFALKNER_MEMPOOL_H
