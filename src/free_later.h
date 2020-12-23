/**
 * Memory cleanup for lockfree deletes
 *
 * Several data structures such as `hashmap_del` will remove data; however, it may not
 * be possible to free data until later. For example, if many threads are using the
 * same hashmap, more than one may be using a reference when `hashmap_del` is called.
 * The solution here is to have `hashmap_del` register data that can be deleted later
 * and let the application notify when worker threads are done with old references.
 *
 * `free_later(void *var, void release(void *))` will register a pointer to have the
 * `release` method called on it later, when it is safe to free memory.
 *
 * `free_later_init()` must be called before using `free_later`, and `free_later_term()`
 * should be called before application termination. It'll ensure all registerd vars
 * have their `release()` callback invoked.
 *
 * `free_later_stage()` should be called before a round of work starts. It'll stage
 * all buffered values to a list that can't be updated, and make a new list to
 * register any new `free_later()` invocations. After all worker threads have
 * progressed with work, call `free_later_run()` to have every value in the staged
 * buffer released.
 */
#ifndef JFALKNER_FREE_LATER_H
#define JFALKNER_FREE_LATER_H


// lifecycle events. _init() must be called before use and _term() once at the end
int free_later_init(void);
int free_later_term(void);

// signals that worker threads are done with old references
void free_later_ready(void);
void free_later_stage(void);

// adds a var to the cleanup later list
void free_later(void *var, void release(void *var));

#endif // JFALKNER_FREE_LATER_H
