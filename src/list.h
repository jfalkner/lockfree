#ifndef JFALKNER_LIST_H
#define JFALKNER_LIST_H

#include <stdint.h>

#define LIST_ONLY_THREAD -1
#define LIST_NOOP_THREAD -2


typedef struct node_s {
	struct node_s *next;
	void *val;
} node_t;

typedef struct list_s {
	// list of nodes in the linked-list
	node_t *next;
	node_t *last;
	// track when deletes can safely be free'd
	uint16_t max_threads;
	uint16_t safe_cleanup_index;
	struct list_s *cleanup_eventually;
	struct list_s *cleanup_now;
	// tracking for CAS retries that is used by tests
	uint32_t retries;
} list_t;

list_t * list_new(uint16_t max_threads);

void list_add(list_t *list, uint16_t thread_index, void *val);
void list_del(list_t *list, uint16_t thread_index, void *val);

#endif // JFALKNER_LIST_H
