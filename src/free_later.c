#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "spinlock.h"
#include "list.h"
#include "free_later.h"

typedef struct free_later_var_s {
	void *var;
	void (*free)(void *var);
} free_later_var;


// track expired vars to cleanup later
static list *buffer = NULL;
static list *buffer_prev = NULL;


int
free_later_init() {
	buffer = list_new();
	return 0;
}

void
free_later (
	void *var,
	void release(void *var)) {
    //register a var for cleanup
    free_later_var *cv = malloc(sizeof(free_later_var));
    cv->var = var;
    cv->free = release;
    list_add(buffer, cv);
}

void
free_later_stage(void) {
	// lock to ensure that multiple threads don't do cleanup simultaneously
	static bool lock = false;

	// CAS-based lock in case multiple threads are calling this method
	acquire_lock(lock);

	if (!buffer_prev || buffer_prev->length == 0) {
		release_lock(lock);
		return;
	}

	// swap the buffers
	buffer_prev = buffer;
	buffer = list_new();

	release_lock(lock);
}

void
free_later_run() {
	// lock to ensure that multiple threads don't do cleanup simultaneously
	static bool lock = false;

	// skip if there is nothing to return
	if (!buffer_prev) return;

	// CAS-based lock in case multiple threads are calling this method
	acquire_lock(lock);

	// At this point, all workers have processed one or more new flow since the 
	// free_later buffer was filled. No threads are using the old, deleted data.
	for (list_node *n = buffer_prev->head; n; n = n->next) {
		free_later_var *v = n->val;
		v->free(v->var);
		free(n);
	}

	free(buffer_prev);
	buffer_prev = NULL;

	release_lock(lock);
}

int
free_later_term() {
	// purge anything that is buffered 
	free_later_run();
	// stage and purge anything that was unbuffered
	free_later_stage();
	free_later_run();
	// release memory for the buffer
	free(buffer);
	buffer = NULL;
	return 0;
}

