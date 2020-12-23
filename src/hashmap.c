#include "free_later.h"
#include "hashmap.h"

// used for testing CAS-retries in tests
volatile uint32_t hashmap_put_retries = 0;
volatile uint32_t hashmap_put_replace_fail = 0;
volatile uint32_t hashmap_put_head_fail = 0;
volatile uint32_t hashmap_del_fail = 0;
volatile uint32_t hashmap_del_fail_new_head = 0;


hashmap_keyval *
hashmap_create_node_malloc(void *opaque, const void *key, void *value) {
	hashmap_keyval *next = NULL;

	next = malloc (sizeof *(next));
	next->key = key;
	next->value = value;

	return next;
}

void
hashmap_destroy_node_later(void *opaque, hashmap_keyval *node) {
	// free all of these later in case other threads are using them
	free_later((void *)node->key, free);
	free_later(node->value, opaque);
	free_later(node, free);
}

void
hashmap_destroy_value_later(void *opaque, hashmap_keyval *node) {
        // free later in case other threads are using them
        free_later(node->value, opaque);
        free_later(node, free);
}
void *
hashmap_new(uint32_t num_buckets, uint8_t cmp(const void *x, const void *y), uint64_t hash(const void *key))
{
	hashmap *map = calloc(1, sizeof(hashmap));
	map->num_buckets = num_buckets;
	map->buckets = calloc(num_buckets, sizeof(hashmap_keyval *));
	// keep local reference of the two utility functions
	map->hash = hash;
	map->cmp = cmp;
	// custom memory management hook
	map->opaque = NULL;
	map->create_node = hashmap_create_node_malloc;
	map->destroy_node = hashmap_destroy_node_later;
	return map;
}

void *
hashmap_get(hashmap *map, const void *key)
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

bool
hashmap_put(hashmap *map, const void *key, void *value)
{
	// sanity checks
	if (!map) return NULL;

	// hash to convert the key to a bucket index where the value would be stored
	uint32_t bucket_index = map->hash(key) % map->num_buckets;

	hashmap_keyval *kv = NULL;
	hashmap_keyval *prev = NULL;
	
	// known head and next entry to add to the list
	hashmap_keyval *head = NULL;
	hashmap_keyval *next = NULL;

	while (true) {
		// copy the head of the list before checking entries for equality
		head = map->buckets[bucket_index];

		// find any existing matches to this key
		prev = NULL;
		if (head) {
			for (kv = head; kv; kv = kv->next) {
				if (map->cmp(key, kv->key) == 0) break;
				prev = kv;
			}
		}
		// if the key exists, update and return it
		if (kv) {

			// lazy make the next key-value pair to append
			if (!next) {
				next = map->create_node(map->opaque, key, value);
			}
			// ensure the linked-list's existing node chain persists
			next->next = kv->next;

			// CAS-update the reference in the previous node
			if (prev) {
				// replace this link, assuming it hasn't changed by another thread
				bool success = __atomic_compare_exchange(&prev->next, &kv, &next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
				if (success) {
					// this node, key and value are never again used by this data structure
					map->destroy_node(map->opaque, kv);
					return true;
				}
				hashmap_put_replace_fail += 1;
			}
			// no previous link, update the head of the list
			else {
				// set the head of the list to be whatever this node points to (NULL or other links)
				bool success = __atomic_compare_exchange(&map->buckets[bucket_index], &kv, &next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
				if (success) {
					map->destroy_node(map->opaque, kv);
					return true;
				}

				// failure means at least one new entry was added, retry the whole match/del process
				hashmap_put_head_fail += 1;
			}
		}
		// if the key doesn't exist, try adding it
		else {
			// make the next key-value pair to append
			if (!next) {
				next = map->create_node(map->opaque, key, value);
			}
			next->next = NULL;

			// make sure the reference to existing nodes is kept
			if (head) next->next = head;

			// prepend the kv-pair or lazy-make the bucket
			bool success = __atomic_compare_exchange(&map->buckets[bucket_index], &head, &next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
			if (success) {
				__atomic_fetch_add(&map->length, 1, __ATOMIC_SEQ_CST);
				return false;
			}
			// failure means another thead updated head before this one
			else {
				// track the CAS failure for tests -- non-atomic to minimize thread contention
				hashmap_put_retries += 1;
			}
		}
	}
}

bool hashmap_del(hashmap *map, const void *key) {
	hashmap_keyval *match;
	hashmap_keyval *prev = NULL;

	if (!map) return false;

	uint32_t bucket_index = (*map->hash)(key) % map->num_buckets;
	
	// try to find a match, loop in case a delete attempt fails
	while (true) {
		prev = NULL;
		for (match = map->buckets[bucket_index]; match; match = match->next) {
			if ((*map->cmp)(key, match->key) == 0) break;
			prev = match;
		}

		// exit if no match was found
		if (!match) return false;

		// previous means this not the head but a link in the list
		if (prev) {
			// try the delete but fail if another thread did the delete
			bool success = __atomic_compare_exchange(&prev->next, &match, &match->next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
			if (success) {
				__atomic_fetch_sub(&map->length, 1, __ATOMIC_SEQ_CST);
				map->destroy_node(map->opaque, match);
				return true;
			}
			hashmap_del_fail += 1;
		}
		// no previous link means this needs to leave an empty bucket
		else {
			// copy the next link in the list (may be NULL) to the head
			bool success = __atomic_compare_exchange(&map->buckets[bucket_index], &match, &match->next, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
			if (success) {
				__atomic_fetch_sub(&map->length, 1, __ATOMIC_SEQ_CST);
				map->destroy_node(map->opaque, match);
				return true;
			}

			// failure means the whole match/del process needs another attempt 
			hashmap_del_fail_new_head += 1;
		}
	}

	return false;
}
