#include <stdio.h>

#include "list.h"
#include "hashmap.h"

#define cas(dst, old, new) __sync_bool_compare_and_swap((dst), (old), (new))
#define unlock_bucket(index) map->bucket_locks[(index)] = 0


void *
hashmap_new(uint8_t num_threads, uint32_t hint, uint8_t cmp(const void *x, const void *y), uint32_t hash(const void *key))
{
	hashmap_t *map = calloc(1, sizeof(hashmap_t));
	map->num_buckets = hint;
	map->buckets = calloc(hint, sizeof(list_t *));
	for (int i=0;i<hint;i++) {
		map->buckets[i] = list_new(num_threads);
	}
	map->hash = hash;
	map->cmp = cmp;
	return map;
}

void *
hashmap_get(hashmap_t *map, uint8_t thread_id, void *key)
{
	// find the bucket that would have the value
	int index = map->hash(key) % map->num_buckets;
	list_t *l = map->buckets[index];

	// walk the nodes to find any matches
	node_t *n = l->next;
	while (n) {
		keyval_t *kv = (keyval_t *)n->val;
		if (map->cmp(kv->key, key) == 0) {
			return kv->value;
		}

		n = n->next;
	}

	return NULL;
}

/**
 * Puts the given key, value pair in the map
 *
 * Returns true if the value was successfully added. Returns false if the value was not
 * added because it already exists in the map.
 *
 * It is assumed that memory management is occuring outside of the hashmap. If false is
 * returned it is safe to free the key and value that was used with the put.
 */
bool
hashmap_put(hashmap_t *map, uint8_t thread_id, void *key, void *value)
{
	// find the bucket that would have the value
	int index = map->hash(key) % map->num_buckets;

	// make a struct to track this -- TODO: mempool these
	keyval_t *kv = calloc(1, sizeof(keyval_t));
	kv->key = key;
	kv->value = value;

	list_add(map->buckets[index], thread_id, kv);
	
	return NULL;
}

void
hashmap_free(hashmap_t *map) {
	// TODO: reclaim memory
}

bool
hashmap_del(hashmap_t *map, uint8_t thread_id, void *key) {
	// TODO: impplement -- use a callback for thread safety?
	return true;
}
