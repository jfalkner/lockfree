#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "list.h"


static const node_t *empty = NULL;

// free resources only when all threads aren't using it
void safe_cleanup(list_t *l, uint16_t thread_index) {
	// skip if a cleanup list
	if (thread_index == -1) {
		return;
	}

	// increment lazy cleanup index, if this thread is next to declare cleanup safety
	uint16_t next_thread_index = thread_index + 1;
       	__atomic_compare_exchange(&l->safe_cleanup_index, &thread_index, &next_thread_index, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);

	// if all threads declared safe to cleanup, do it
	if (l->safe_cleanup_index == l->max_threads && l->cleanup_now->next) {
		// reset the safe-to-cleanup index, ensuring it'll not trigger since this thread is working on it
		l->safe_cleanup_index = 0;
		
		// walk to nodes to free and free them
		node_t *n = l->cleanup_now->next;
		bool success = __atomic_compare_exchange(&l->cleanup_now->next, &n, &empty, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
		if (success) {
			node_t *next = NULL;
			while (n) {
				next = n->next;
				free(n);
				n = next;
			}
			// make eventual list the new now
			l->cleanup_now->next = l->cleanup_eventually->next;
			l->cleanup_eventually->next = NULL;
		}
		// failure means a different thread did the cleanup, nothing left to do

		// safe for this thread to lazy-cleanup
		safe_cleanup(l, thread_index);
	}
}


list_t * list_new(uint16_t max_threads)
{
	list_t *l = calloc(1, sizeof(node_t));
	l->next = (node_t *)empty;
	// needed for knowing when cleanup is safe to perform
	l->max_threads = max_threads;
	// the cleanup lists purposely have max_threads left at 0
	l->cleanup_eventually = calloc(1, sizeof(list_t));
	l->cleanup_now = calloc(1, sizeof(list_t));
	// all CAS retry tracking starts at 0
	l->retries = 0;
	return l;
}

void list_add(list_t *l, uint16_t thread_index, void *val)
{
	// wrap the value as a node in the linked list
	node_t *v = calloc(1, sizeof(node_t));
	v->next = NULL;
	v->val = val;

	// if the first node is free, try to use it
	node_t *n = l->next;
	if (n == empty) {
        	bool b = __atomic_compare_exchange(&l->next, &empty, &v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
		if (b) {
			l->last = v;
			safe_cleanup(l, thread_index);
			return;
		}
	}

	// retry until the update is done
	n = l->last ? l->last : l->next;
	while (true) {
		// follow notes until an empty spot is found
		while (n->next) {
			n = (node_t *)n->next;
		}
        	bool b = __atomic_compare_exchange(&n->next, &empty, &v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
		if (b) {
			l->last = v;
			safe_cleanup(l, thread_index);
			return;
		}
		else {
			l->retries += 1;
		}
	}
}

void list_del(list_t *l, uint16_t thread_index, void *val)
{
	// start with the first node in the linked list
	node_t *prev = NULL;
	node_t *n = l->next;

	// retry until the update is done
	while (n) {
		if (n->val == val) {
			// if the next pointer is already set, straight-forward to point previous to it
			if (n->next) {
				prev->next = n->next;
			}
			// no next pointer is a race condition. it is possible another
			// thread will update n->next before prev->next is set to NULL
			else {
				// make a temporary circular refernce so anything using n->next will now extend prev->next
        			bool b = __atomic_compare_exchange(n->next, empty, prev, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
				// if successfull, n->next is no longer available and prev->next can be set to NULL to allow extension
				if (b) {
					prev->next = NULL;
					l->last = prev;
				}
				// if something else already set n->next, just use it
				else {
					prev->next = n->next;
				}
			}
			// signal that this thread can be free'd when safe to do so
			list_add(l->cleanup_eventually, -1, n);
			break;
		}
		n = n->next;
	}
	safe_cleanup(l, thread_index);
}
