#include "mk_sync.h"

#include "mk_port.h"
#include "mk_rtos.h"

/* Initialize a mutex to unlocked state */
void mk_mutexInit(mk_mutex_t *mutex)
{
    if (mutex != NULL)
    {
        mutex->locked = 0u;
    }
}

/* Lock a mutex with optional timeout (spins and yields if locked) */
bool mk_lock_mutex(mk_mutex_t *mutex, uint32_t timeout_ticks)
{
    uint32_t start_tick;

    if (mutex == NULL)
    {
        return false;
    }

    start_tick = mk_taskGetTickCount();

    while (true)
    {
        uint32_t key = mk_port_critical_save();
        if (mutex->locked == 0u)
        {
            mutex->locked = 1u;
            mk_port_critical_restore(key);
            return true;
        }
        mk_port_critical_restore(key);

        if (timeout_ticks == 0u)
        {
            return false;
        }

        if ((mk_taskGetTickCount() - start_tick) >= timeout_ticks)
        {
            return false;
        }

        if (mk_current_tcb != NULL)
        {
            ((mk_tcb_t *)mk_current_tcb)->state = MK_TASK_BLOCKED;
        }
        mk_taskYield();
        if (mk_current_tcb != NULL)
        {
            ((mk_tcb_t *)mk_current_tcb)->state = MK_TASK_RUNNING;
        }
    }
}

/* Unlock a mutex (mark as unlocked) */
void mk_unlock_mutex(mk_mutex_t *mutex)
{
    if (mutex != NULL)
    {
        uint32_t key = mk_port_critical_save();
        mutex->locked = 0u;
        mk_port_critical_restore(key);
    }
}

/* Initialize a semaphore with initial and maximum count */
void mk_semaphoreInit(mk_semaphore_t *sem, uint32_t initial_count, uint32_t max_count)
{
    if ((sem == NULL) || (max_count == 0u))
    {
        return;
    }

    if (initial_count > max_count)
    {
        initial_count = max_count;
    }

    sem->count = initial_count;
    sem->max_count = max_count;
}

/* Decrement semaphore count if available (with optional timeout) */
bool mk_semaphoreTake(mk_semaphore_t *sem, uint32_t timeout_ticks)
{
    uint32_t start_tick;

    if (sem == NULL)
    {
        return false;
    }

    start_tick = mk_taskGetTickCount();

    while (true)
    {
        uint32_t key = mk_port_critical_save();
        if (sem->count > 0u)
        {
            sem->count--;
            mk_port_critical_restore(key);
            return true;
        }
        mk_port_critical_restore(key);

        if (timeout_ticks == 0u)
        {
            return false;
        }

        if ((mk_taskGetTickCount() - start_tick) >= timeout_ticks)
        {
            return false;
        }

        if (mk_current_tcb != NULL)
        {
            ((mk_tcb_t *)mk_current_tcb)->state = MK_TASK_BLOCKED;
        }
        mk_taskYield();
        if (mk_current_tcb != NULL)
        {
            ((mk_tcb_t *)mk_current_tcb)->state = MK_TASK_RUNNING;
        }
    }
}

/* Increment semaphore count (does not exceed max_count) */
void mk_semaphoreGive(mk_semaphore_t *sem)
{
    if (sem != NULL)
    {
        uint32_t key = mk_port_critical_save();
        if (sem->count < sem->max_count)
        {
            sem->count++;
        }
        mk_port_critical_restore(key);
    }
}
