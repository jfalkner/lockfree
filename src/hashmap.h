/**
 * Lock-Free Hashmap
 *
 * This implementation is thread safe and lock free. It will perform well as long as
 * the initial bucket size is large enough.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// links in the linked lists that each bucket uses
typedef struct hashmap_keyval_s {
	struct hashmap_keyval_s *next;
	const void *key;
	void *value;
} hashmap_keyval;

// main hashmap struct with buckets of linked lists
typedef struct hashmap_s {
	// buckets
	hashmap_keyval **buckets;
	uint32_t num_buckets;

	// total count of entries
	uint32_t length;

	// pointer to the hash and comparison functions
	uint64_t (*hash)(const void *key);
	uint8_t (*cmp)(const void *x, const void *y);
	
	// custom memory management of internal linked lists 
	void *opaque;
	hashmap_keyval * (*create_node)(void *opaque, const void *key, void *data);
	void (*destroy_node)(void *opaque, hashmap_keyval *node);
} hashmap;


/**
 * Creates and initializes a new hashmap
 */
void * hashmap_new(uint32_t hint, uint8_t cmp(const void *x, const void *y), uint64_t hash(const void *key));

/**
 * Returns a value mapped to the key or NULL, if no entry exists for the given key
 */
extern void * hashmap_get(hashmap *map, const void *key);

/**
 * Puts the given key, value pair in the map
 *
 * Returns true if an existing matching key was replaced. Otherwise, false.
 */
extern bool hashmap_put(hashmap *map, const void *key, void *value);

/**
 * Removes the given key, value pair in the map
 *
 * Returns true if a key was found. Otherwise, false. This method is guaranteed to
 * return true just once, if multiple threads are attempting to delete the same key.
 */
extern bool hashmap_del(hashmap *map, const void *key);
