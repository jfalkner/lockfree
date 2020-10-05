#include <stdio.h>

#include "hashmap.h"


void *
hashmap_new(uint32_t num_buckets, uint8_t cmp(const void *x, const void *y), uint32_t hash(const void *key))
{
	hashmap *map = calloc(1, sizeof(hashmap));
	map->num_buckets = num_buckets;
	map->buckets = calloc(num_buckets, sizeof(hashmap_keyval *));
	// keep local reference of the two utility functions
	map->hash = hash;
	map->cmp = cmp;
	// tracked CAS retries for tests and debugging
	map->put_retries = 0;
	return map;
}

void *
hashmap_get(hashmap *map, void *key)
{
	// hash to convert the key to a bucket index where the value would be stored
	uint32_t index = map->hash(key) % map->num_buckets;

	// walk the linked list nodes to find any matches
	hashmap_keyval *n = map->buckets[index];
	while (n) {
		if (map->cmp(n->key, key) == 0) {
			return n->value;
		}

		n = n->next;
	}

	// no matches found
	return NULL;
}

void *
hashmap_put(hashmap *map, void *key, void *value)
{
	// sanity checks
	if (!map || !key) {
		return NULL;
	}

	// hash to convert the key to a bucket index where the value would be stored
	uint32_t bucket_index = map->hash(key) % map->num_buckets;

	hashmap_keyval *kv = NULL;
	
	// known head and next entry to add to the list
	hashmap_keyval *head = NULL;
	hashmap_keyval *next = NULL;

	while (true) {
		// copy the head of the list before checking entries for equality
		head = map->buckets[bucket_index];

		// find any existing matches to this key
		if (head) {
			for (kv = head; kv; kv = kv->next) {
				if (map->cmp(key, kv->key) == 0) {
					break;
				}
			}
		}
		// if the key exists, update and return it
		if (kv) {
			while (true) {
				void *prev = kv->value;
				bool success = __atomic_compare_exchange(&kv->value, &prev, &value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
				if (success) {
					return prev;
				}
			}
		}
		// if the key doesn't exist, try adding it
		else {
			// make the next key-value pair to append
			if (!next) {
				next = malloc (sizeof (hashmap_keyval));
				next->key = key;
				next->value = value;
				next->next = NULL;
			}

			// make sure the reference to existing nodes is kept
			if (head) {
				next->next = head;
			}

			// prepend the kv-pair or lazy-make the bucket
			bool success = __atomic_compare_exchange(&map->buckets[bucket_index], &head, &next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
			if (success) {
				__atomic_fetch_add(&map->length, 1, __ATOMIC_SEQ_CST);
				return NULL;
			}
			// failure means another thead updated head before this one
			else {
				// track the CAS failure for tests -- non-atomic to minimize thread contention
				map->put_retries += 1;
			}
		}
	}
}
