#ifndef MK_SYNC_H
#define MK_SYNC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    volatile uint8_t locked;
} mk_mutex_t;

typedef struct
{
    volatile uint32_t count;
    uint32_t max_count;
} mk_semaphore_t;

/* Initialize a mutex to unlocked state */
void mk_mutexInit(mk_mutex_t *mutex);

/* Lock a mutex with timeout (returns true if locked, false if timeout/error) */
bool mk_lock_mutex(mk_mutex_t *mutex, uint32_t timeout_ticks);

/* Unlock a mutex (mark as unlocked) */
void mk_unlock_mutex(mk_mutex_t *mutex);

/* Initialize a semaphore with initial and maximum count */
void mk_semaphoreInit(mk_semaphore_t *sem, uint32_t initial_count, uint32_t max_count);

/* Decrement semaphore count if available (returns true if taken, false on timeout) */
bool mk_semaphoreTake(mk_semaphore_t *sem, uint32_t timeout_ticks);

/* Increment semaphore count (up to max_count) */
void mk_semaphoreGive(mk_semaphore_t *sem);

#endif /* MK_SYNC_H */
