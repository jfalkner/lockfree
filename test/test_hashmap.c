#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "free_later.h"
#include "hashmap.h"

// global hash map
hashmap *map = NULL;

// how many threads should run in parallel
#define NUM_THREADS 10
// how many times the work loop should repeat
#define NUM_WORK 100
// state for the threads
static pthread_t threads[NUM_THREADS];

extern volatile uint32_t hashmap_put_retries;
extern volatile uint32_t hashmap_put_replace_fail;
extern volatile uint32_t hashmap_put_head_fail;

uint8_t
cmp_uint32(const void *x, const void *y) {
	uint32_t xi = *(uint32_t *)x;
	uint32_t yi = *(uint32_t *)y;
	if (xi > yi) {
		return -1;
	}
	if (xi < yi) {
		return 1;
	}
	return 0;
}

uint64_t
hash_uint32(const void *key) {
	return *(uint32_t *)key;
}

/**
 * Simulates work that is quick and uses the hashtable once per loop.
 */
void *
add_vals(void *args)
{
	int *offset = (int *)args;
	for (int j=0;j<NUM_WORK;j++) {
		int *val = malloc(sizeof(int));
		*val = (*offset * NUM_WORK) + j;
		// same key/val
		hashmap_put(map, val, val);
	}
	return NULL;
}

bool
multi_thread_add_vals(void) {
	for (int i=0;i<NUM_THREADS;i++) {
		int *offset = malloc(sizeof(int));
		*offset = i;
		int ret = pthread_create(&threads[i], NULL, add_vals, offset);
		if (ret != 0) {
			printf("Failed to create thread %d\n", i);
			exit(1);
		}
	}
	// wait for work to finish
	for (int i=0;i<NUM_THREADS;i++) {
		int ret = pthread_join(threads[i], NULL);
		if (ret != 0) {
			printf("Failed to join thread %d\n", i);
			exit(1);
		}
	}
	return true;
}

int
main (int argc, char **argv)
{
	free_later_init();

	map = (hashmap *)hashmap_new(10, cmp_uint32, hash_uint32);

	int loops = 0;
	while (hashmap_put_retries == 0) {
		loops += 1;
		if (!multi_thread_add_vals()) {
			printf("Error. Failed to add values!\n");
			return -1;
		}

		// check all the list entries
		uint32_t TOTAL = NUM_THREADS * NUM_WORK;
		uint32_t found = 0;
		for (uint32_t i=0;i<TOTAL;i++) {
			uint32_t *v = (uint32_t *)hashmap_get(map, &i);
			if (v && *v == i) {
				found++;
			}
			else {
				printf("Cound not find %d in the map\n", i);
			}
		}
		if (found == TOTAL) {
			printf("Loop %d. All values found. hashmap_put_retries=%u, hashmap_put_head_fail=%u, hashmap_put_replace_fail=%u\n",
				loops, hashmap_put_retries, hashmap_put_head_fail, hashmap_put_replace_fail);
		}
		else {
			printf("Found %d of %d values. Where are the missing ones?", found, TOTAL);
		}

	}
	free_later_term();

	printf("Done. Loops=%u\n", loops);
}
