#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "hashmap.h"

// global hash map
hashmap *map = NULL;

// how many threads should run in parallel
#define NUM_THREADS 10
// how many times the work loop should repeat
#define NUM_WORK 10
// state for the threads
static pthread_t threads[NUM_THREADS];
// state for the threads that test deletes
static pthread_t threads_del[NUM_THREADS * 2];

static uint32_t MAX_VAL_PLUS_ONE = NUM_THREADS * NUM_WORK + 1;

extern uint32_t hashmap_del_fail;
extern uint32_t hashmap_del_fail_new_head;


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

// adds a value over and over to test the del functionality
void *
add_val(void *args)
{
	for (int j=0;j<NUM_WORK;j++) {
		hashmap_put(map, &MAX_VAL_PLUS_ONE, &MAX_VAL_PLUS_ONE);
	}
	return NULL;
}

void *
del_val(void *args)
{
	for (int j=0;j<NUM_WORK;j++) {
		hashmap_del(map, &MAX_VAL_PLUS_ONE);
	}
	return NULL;
}

bool
multi_thread_del(void) {
	for (int i=0;i<NUM_THREADS;i++) {
		int ret = pthread_create(&threads_del[i], NULL, add_val, NULL);
		if (ret != 0) {
			printf("Failed to create thread %d\n", i);
			exit(1);
		}
		ret = pthread_create(&threads_del[NUM_THREADS + i], NULL, del_val, NULL);
		if (ret != 0) {
			printf("Failed to create thread %d\n", i);
			exit(1);
		}
	}
	// also add normal numbers to ensure they aren't clobbered
	multi_thread_add_vals();
	// wait for work to finish
	for (int i=0;i<NUM_THREADS * 2;i++) {
		int ret = pthread_join(threads_del[i], NULL);
		if (ret != 0) {
			printf("Failed to join thread %d\n", i);
			exit(1);
		}
	}
	return true;
}

bool
test_add ()
{
	map = (hashmap *)hashmap_new(10, cmp_uint32, hash_uint32);

	int loops = 0;
	while (map->put_retries < 10) {
		loops += 1;
		if (!multi_thread_add_vals()) {
			printf("Error. Failed to add values!\n");
			return false;
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
			printf("Loop %d. All values found.\n", loops);
		}
		else {
			printf("Found %d of %d values. Where are the missing ones?", found, TOTAL);
		}

	}
	printf("Done. Loops=%u\n", loops);
	return true;
}

bool
test_del(){
	// keep looping until a CAS retry was needed by hashmap_del
	uint32_t loops = 0;
	// make sure test counters are zeroed
	hashmap_del_fail = 0;
	hashmap_del_fail_new_head = 0;

	while (hashmap_del_fail == 0 || hashmap_del_fail_new_head == 0) {
		map = hashmap_new(10, cmp_uint32, hash_uint32);

		// multi-thread add values
		if (!multi_thread_del()) {
			printf("test_del() is failing. Can't complete multi_thread_del()");
			return false;
		}
		loops++;

		// check all the list entries
		uint32_t TOTAL = NUM_THREADS * NUM_WORK;
		uint32_t found = 0;
		for (uint32_t i=0;i<TOTAL;i++) {
			uint32_t *v = (uint32_t *)hashmap_get(map, &i);
			if (v && *v == i) {
				found++;
			}
			else {
				printf("Cound not find %d in the hashmap\n", i);
			}
		}
		if (found != TOTAL) {
			printf("test_del() is failing. Not all values found!?");
			return false;
		}
	}
	printf("Done. Needed %u loops\n", loops);
}

int
main (int argc, char **argv)
{
	if (!test_add()) {
		printf("Failed multi-threaded add test.");
	}
	if (!test_del()) {
		printf("Failed multi-threaded del test.");
	}
}
