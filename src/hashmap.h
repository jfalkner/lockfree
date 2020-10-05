/**
 * Lock-Free Hashmap
 *
 * This implementation is thread safe and lock free. It will perform well as long as
 * the initial bucket size is large enough.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "list.h"

typedef struct hashmap_keyval_s {
	struct hashmap_keyval_s *next;
	void *key;
	void *value;
} hashmap_keyval;

typedef struct hashmap_s {
	// buckets
	hashmap_keyval **buckets;
	uint32_t num_buckets;

	// total count of entries
	uint32_t length;

	// pointer to the hash and comparison functions
	uint32_t (*hash)(const void *key);
	uint8_t (*cmp)(const void *x, const void *y);

	// CAS failures for test
	uint32_t put_retries;
} hashmap;


void * hashmap_new(uint32_t hint, uint8_t cmp(const void *x, const void *y), uint32_t hash(const void *key));

/**
 * Returns a value mapped to the key or NULL, if no entry exists for the given key
 */
void * hashmap_get(hashmap *map, void *key);

/**
 * Puts the given key, value pair in the map
 *
 * Returns the value of the previous entry with the same key, if one existed. Otherwise,
 * returns NULL.
 */
void * hashmap_put(hashmap *map, void *key, void *value);
