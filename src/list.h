/**
 * Lock-Free Linked List
 *
 * This is an add-only linked list that does not use user-space mutexes. It relies on
 * hardware specific memory locking, typically via compare-and-swap (CAS) operations.
 * 
 * The implementation currently is add-only.
 */
#ifndef JFALKNER_LIST_H
#define JFALKNER_LIST_H

#include <stdint.h>


typedef struct list_node_s {
	struct list_node_s *next;
	void *val;
} list_node;

typedef struct list_s {
	// list of nodes in the linked-list
	list_node *head;
	// tracking for CAS retries that is used by tests
	uint32_t retries_empty;
	uint32_t retries_populated;
} list;

list * list_new();

void list_add(list *list, void *val);

#endif // JFALKNER_LIST_H
