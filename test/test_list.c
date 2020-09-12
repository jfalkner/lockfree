#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "list.h"

// global hash map
list_t *list = NULL;

// how many threads should run in parallel
#define NUM_THREADS 10
// how many times the work loop should repeat
#define NUM_WORK 10
// state for the threads
static pthread_t threads[NUM_THREADS];

/**
 * Simulates work that is quick and will exercise CAS failure retry logic.
 */
void *
add_vals(void *args)
{
	int *offset = (int *)args;
	for (int j=0;j<NUM_WORK;j++) {
		int *val = malloc(sizeof(int));
		*val = (*offset * NUM_WORK) + j;
		list_add(list, *offset, val);
	}
	return NULL;
}

void *
del_vals(void *args)
{
	int *offset = (int *)args;
	for (int j=0;j<NUM_WORK;j++) {
		int *val = malloc(sizeof(int));
		*val = (*offset * NUM_WORK) + j;
		list_del(list, *offset, val);
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

bool
multi_thread_del_vals(void) {
	for (int i=0;i<NUM_THREADS;i++) {
		int *offset = malloc(sizeof(int));
		*offset = i;
		int ret = pthread_create(&threads[i], NULL, del_vals, offset);
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
	list = (list_t *)list_new(NUM_THREADS);

	printf("Adding Values\n");
	int loops = 0;
	while (list->retries < 100) {
		loops += 1;
		printf("Trying for CAS-fail retry: %d\n", loops);
		if (!multi_thread_add_vals()) {
			printf("Error. Failed to add values!");
			return -1;
		}
	}
	printf("Done\n");

	// check all the list entries
	node_t *n = list->next;
	uint32_t TOTAL = NUM_THREADS * NUM_WORK;
	uint8_t *checks = calloc(TOTAL, sizeof(uint8_t));
	while (n) {
		checks[*(int *)n->val] += 1;
		n = n->next;
	}

	printf("\nChecks\n");
	int valid_checks = 0;
	int invalid_checks = 0;
	for (int i=0;i<TOTAL;i++) {
		if (checks[i] != loops) {
			printf("check[%d]: %d\n", i, checks[i]);
			invalid_checks += 1;
		}
		else {
			valid_checks += 1;
		}
	}
	printf("Valid: %d, Invalid: %d\n", valid_checks, invalid_checks);

	// try removing all the values until they are done
	printf("Removing Values\n");
	int del_loops = 0;
	//while (list->retries < 1) {
		del_loops += 1;
		if (!multi_thread_del_vals()) {
			printf("Error. Failed to del values!");
			return -1;
		}
	//}
	printf("Done\n");
}
