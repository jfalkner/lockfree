/**
 * Lock-free hashmap
 *
 * This implementation is thread safe and lock free. It will perform well as long as
 * the initial bucket size is large enough.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "list.h"

typedef struct keyval_s {
	void *key;
	void *value;
} keyval_t;

typedef struct hashmap_s {
	// buckets
	list_t **buckets;
	uint32_t num_buckets;

	// pointer to the hash and comparison functions
	uint32_t (*hash)(const void *key);
	uint8_t (*cmp)(const void *x, const void *y);

	// memory management functions
	// TODO: ...
} hashmap_t;


void * hashmap_new(uint8_t num_threads, uint32_t hint, uint8_t cmp(const void *x, const void *y), uint32_t hash(const void *key));
void hashmap_free(hashmap_t *map);

void * hashmap_get(hashmap_t *map, uint8_t thread_id, void *key);
bool hashmap_put(hashmap_t *map, uint8_t thread_id, void *key, void *value);
bool hashmap_del(hashmap_t *map, uint8_t thread_id, void *key);

