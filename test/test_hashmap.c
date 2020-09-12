#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// include example use of libcii -- not thread safe
#include "hashmap.h"

// global hash map
hashmap_t *map = NULL;

// flag to indicate if workers should stop
bool run = true;

// how many threads should run in parallel
#define NUM_THREADS 10
// how many times the work loop should repeat
#define NUM_WORK 10
// state for the threads
static pthread_t threads[NUM_THREADS];

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

uint32_t
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
		hashmap_put(map, *offset, val, val);
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
	map = (hashmap_t *)hashmap_new(NUM_THREADS, 10, cmp_uint32, hash_uint32);

	// multi-thread add values
	printf("Adding Values\n");
	if (!multi_thread_add_vals()) {
		printf("Error. Failed to add values!");
		return -1;
	}
	printf("Done\n");

	// check all the list entries
	uint32_t TOTAL = NUM_THREADS * NUM_WORK;
	uint32_t found = 0;
	for (uint32_t i=0;i<TOTAL;i++) {
		uint32_t *v = (uint32_t *)hashmap_get(map, -1, &i);
		if (v && *v == i) {
			found++;
		}
		else {
			printf("Cound not find %d in the map\n", i);
		}
	}
	if (found == TOTAL) {
		printf("\nAll values found.\n");
	}
	else {
		printf("Found %d of %d values. Where are the missing ones?", found, TOTAL);
	}

	// TODO: check deletes
}
