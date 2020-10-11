/**
 * CAS-based spinlock
 *
 * This is sometimes faster than a mutex and is an alternative to lock/unlock portions
 * of code to ensure that just one thread is executing it at a time.
 *
 * Do not use this in lockfree algorithms. It is still effectively a user-space mutex.
 */
#ifndef JFALKNER_SPINLOCK_H
#define JFALKNER_SPINLOCK_H


#define acquire_lock(lock) while (!__sync_bool_compare_and_swap(&(lock), 0, 1));
#define release_lock(lock) __sync_bool_compare_and_swap(&(lock), 1, 0);


#endif // JFALKNER_SPINLOCK_H
