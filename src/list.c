#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "list.h"


static const list_node *empty = NULL;


list * list_new()
{
	list *l = calloc(1, sizeof(list_node));
	l->head = (list_node *)empty;
	// all CAS retry tracking starts at 0
	l->retries_empty = 0;
	l->retries_populated = 0;
	return l;
}

void list_add(list *l, void *val)
{
	// wrap the value as a node in the linked list
	list_node *v = calloc(1, sizeof(list_node));
	v->val = val;

	// try adding to the front of the list
	while (true) {
		list_node *n = l->head;
		// case for if this is the first link in the list
		if (n == empty) {
			v->next = NULL;
	        	bool b = __atomic_compare_exchange(&l->head, &empty, &v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
			if (b) {
				return;
			}
			l->retries_empty++;
		}
		// case for inserting when an existing link is present
		else {
			v->next = n;
	        	bool b = __atomic_compare_exchange(&l->head, &n, &v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
			if (b) {
				return;
			}
			l->retries_populated++;
		}

	}
}
