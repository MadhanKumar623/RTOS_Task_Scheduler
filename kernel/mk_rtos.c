#include "mk_rtos.h"

#include "cy_pdl.h"
#include "mk_cpu_load.h"
#include "mk_port.h"

typedef struct
{
    mk_tcb_t tcb;
    uint8_t active;
} mk_task_slot_t;

static mk_task_slot_t mk_task_table[MK_RTOS_MAX_TASKS + 1u];
static uint32_t mk_task_count;
static uint32_t mk_current_index;
static volatile uint32_t mk_tick_count;
static uint8_t mk_kernel_started;

volatile mk_tcb_t *mk_current_tcb;

static void mk_idleTask(void *arg);
static void mk_prepare_current_for_switch(void);
#if (MK_RTOS_PREEMPTION_ENABLE == 0u)
static uint8_t mk_has_ready_user_task(void);
#endif

static void mk_prepare_current_for_switch(void)
{
    if ((mk_current_tcb != NULL) &&
        (mk_task_table[mk_current_index].active != 0u) &&
        (((mk_tcb_t *)mk_current_tcb)->state == MK_TASK_RUNNING))
    {
        ((mk_tcb_t *)mk_current_tcb)->state = MK_TASK_READY;
    }
}

/* Initialize kernel data structures and task table */
void mk_kernelInit(void)
{
    uint32_t i;

    mk_task_count = 0u;
    mk_current_index = 0u;
    mk_tick_count = 0u;
    mk_kernel_started = 0u;
    mk_current_tcb = NULL;
    mk_cpuLoadInit();

    for (i = 0u; i < (MK_RTOS_MAX_TASKS + 1u); i++)
    {
        mk_task_table[i].active = 0u;
        mk_task_table[i].tcb.sp = NULL;
        mk_task_table[i].tcb.stack_bottom = NULL;
        mk_task_table[i].tcb.stack_words = 0u;
        mk_task_table[i].tcb.delay_ticks = 0u;
        mk_task_table[i].tcb.entry = NULL;
        mk_task_table[i].tcb.arg = NULL;
        mk_task_table[i].tcb.name = NULL;
        mk_task_table[i].tcb.priority = 0u;
        mk_task_table[i].tcb.state = MK_TASK_READY;
    }
}

/* Create and register a new task with stack, priority, size and entry function */
mk_status_t mk_taskCreate(mk_task_fn_t task_fn,
                          void *arg,
                          uint32_t *stack_mem,
                          uint32_t stack_words,
                          const char *name,
                          uint8_t priority)
{
    mk_tcb_t *tcb;
    uint32_t *stack_top;

    if ((task_fn == NULL) || (stack_mem == NULL))
    {
        return MK_ERR_INVALID_ARG;
    }

    if (mk_kernel_started != 0u)
    {
        return MK_ERR_KERNEL_RUNNING;
    }

    if (stack_words < 64u)
    {
        return MK_ERR_STACK_TOO_SMALL;
    }

    if (mk_task_count >= MK_RTOS_MAX_TASKS)
    {
        return MK_ERR_NO_SLOT;
    }

    tcb = &mk_task_table[mk_task_count].tcb;
    stack_top = &stack_mem[stack_words];

    /* Clamp priority to valid range */
    if (priority >= (uint8_t)MK_RTOS_MAX_PRIORITY)
    {
        priority = (uint8_t)(MK_RTOS_MAX_PRIORITY - 1u);
    }

    tcb->sp = mk_port_init_stack(stack_top, task_fn, arg);
    tcb->stack_bottom = stack_mem;
    tcb->stack_words = stack_words;
    tcb->delay_ticks = 0u;
    tcb->entry = task_fn;
    tcb->arg = arg;
    tcb->name = name;
    tcb->priority = priority;

    mk_task_table[mk_task_count].active = 1u;
    mk_task_count++;

    return MK_OK;
}

/* Start kernel scheduling (creates idle task, sets up hardware, enters scheduler) */
void mk_kernelStart(void)
{
    static uint32_t mk_idle_stack[MK_RTOS_DEFAULT_STACK_WORDS];

    if ((mk_kernel_started != 0u) || (mk_task_count == 0u))
    {
        return;
    }

    (void)mk_taskCreate(mk_idleTask,
                        NULL,
                        mk_idle_stack,
                        MK_RTOS_DEFAULT_STACK_WORDS,
                        "mk_idle",
                        0u);

    mk_current_index = 0u;
    mk_current_tcb = &mk_task_table[mk_current_index].tcb;
    ((mk_tcb_t *)mk_current_tcb)->state = MK_TASK_RUNNING;
    mk_kernel_started = 1u;

    mk_port_setup_timer_interrupt();
    mk_port_start_first_task();

    for (;;)
    {
    }
}

/* Delay current task for specified number of ticks (transitions to BLOCKED) */
void mk_taskDelay(uint32_t ticks)
{
    if ((mk_current_tcb == NULL) || (ticks == 0u))
    {
        return;
    }

    mk_port_enter_critical();
    ((mk_tcb_t *)mk_current_tcb)->delay_ticks = ticks;
    ((mk_tcb_t *)mk_current_tcb)->state = MK_TASK_BLOCKED;
    mk_port_exit_critical();
 
    /* Triggering for the context switch as the current task gets blocked */
    mk_port_trigger_context_switch();
}

/* Yield CPU to other ready tasks of equal or higher priority */
void mk_taskYield(void)
{
    mk_port_trigger_context_switch();
}

/* Get current system tick counter */
uint32_t mk_taskGetTickCount(void)
{
    return mk_tick_count;
}

/* Handle system tick: decrement delays, check for preemption, trigger context switch */
void mk_kernelTickHandler(void)
{
    uint32_t i;

    mk_tick_count++;

    if (mk_task_count > 0u)
    {
        mk_cpuLoadOnTick((mk_current_index == (mk_task_count - 1u)) ? 1u : 0u);
    }

    for (i = 0u; i < mk_task_count; i++)
    {
        if (mk_task_table[i].active != 0u)
        {
            if (mk_task_table[i].tcb.delay_ticks > 0u)
            {
                mk_task_table[i].tcb.delay_ticks--;
                /* Transition from BLOCKED to READY when delay expires */
                if ((mk_task_table[i].tcb.delay_ticks == 0u) &&
                    (mk_task_table[i].tcb.state == MK_TASK_BLOCKED))
                {
                    mk_task_table[i].tcb.state = MK_TASK_READY;
                }
            }
        }
    }

#if (MK_RTOS_PREEMPTION_ENABLE == 1u)
    /*
     * Trigger context switch if a higher-priority task just became ready
     * (preemption), or if another task of equal priority is ready
     * (time-slicing among equal-priority tasks, like FreeRTOS).
     */
    {
        uint8_t cur_prio = ((mk_tcb_t *)mk_current_tcb)->priority;
        uint8_t j;
        for (j = 0u; j < (uint8_t)mk_task_count; j++)
        {
            if ((mk_task_table[j].active != 0u) &&
                (mk_task_table[j].tcb.delay_ticks == 0u) &&
                (mk_task_table[j].tcb.state != MK_TASK_SUSPENDED) &&
                ((uint32_t)j != mk_current_index) &&
                (mk_task_table[j].tcb.priority >= cur_prio))
            {
                mk_port_trigger_context_switch();
                break;
            }
        }
    }
#else
    /* Cooperative: only wake from idle when a user task is ready */
    if ((mk_task_count > 0u) &&
        (mk_current_index == (mk_task_count - 1u)) &&
        (mk_has_ready_user_task() != 0u))
    {
        mk_port_trigger_context_switch();
    }
#endif
}

/* Select next task using priority and round-robin scheduling (called from PendSV) */
void mk_port_select_next_task(void)
{
    uint32_t i;
    uint32_t candidate;
    uint8_t highest_prio;

    if (mk_task_count == 0u)
    {
        return;
    }

    mk_prepare_current_for_switch();

    /* --- Step 1: find the highest priority among ALL ready tasks --- */
    highest_prio = 0u;
    for (i = 0u; i < mk_task_count; i++)
    {
        if ((mk_task_table[i].active != 0u) &&
            (mk_task_table[i].tcb.delay_ticks == 0u) &&
            (mk_task_table[i].tcb.state != MK_TASK_SUSPENDED))
        {
            if (mk_task_table[i].tcb.priority > highest_prio)
            {
                highest_prio = mk_task_table[i].tcb.priority;
            }
        }
    }

    /*
     * --- Step 2: round-robin among tasks at that priority level ---
     * Start searching AFTER the current index so tasks of equal priority
     * share CPU fairly (same behaviour as FreeRTOS time-slicing).
     */
    candidate = mk_current_index;
    for (i = 0u; i < mk_task_count; i++)
    {
        candidate++;
        if (candidate >= mk_task_count)
        {
            candidate = 0u;
        }

        if ((mk_task_table[candidate].active != 0u) &&
            (mk_task_table[candidate].tcb.delay_ticks == 0u) &&
            (mk_task_table[candidate].tcb.state != MK_TASK_SUSPENDED) &&
            (mk_task_table[candidate].tcb.priority == highest_prio))
        {
            mk_current_index = candidate;
            mk_current_tcb = &mk_task_table[mk_current_index].tcb;
            ((mk_tcb_t *)mk_current_tcb)->state = MK_TASK_RUNNING;
            mk_cpuLoadOnContextSwitch((mk_current_index == (mk_task_count - 1u)) ? 1u : 0u);
            return;
        }
    }

    /* Fallback: idle task (last slot) */
    mk_current_index = mk_task_count - 1u;
    mk_current_tcb = &mk_task_table[mk_current_index].tcb;
    ((mk_tcb_t *)mk_current_tcb)->state = MK_TASK_RUNNING;
    mk_cpuLoadOnContextSwitch(1u);
}

/* Idle hook called by idle task (user can override for low-power behavior) */
void mk_idleHook(void)
{
}

/* Suspend a task (transitions to SUSPENDED state, excluded from scheduling) */
void mk_taskSuspend(volatile mk_tcb_t *tcb)
{
    if (tcb == NULL)
    {
        return;
    }

    mk_port_enter_critical();
    ((mk_tcb_t *)tcb)->state = MK_TASK_SUSPENDED;
    mk_port_exit_critical();

    /* If suspending the current task, trigger context switch */
    if (tcb == mk_current_tcb)
    {
        mk_port_trigger_context_switch();
    }
}

/* Resume a suspended task (transitions to READY state, becomes available for scheduling) */
void mk_taskResume(volatile mk_tcb_t *tcb)
{
    if (tcb == NULL)
    {
        return;
    }

    mk_port_enter_critical();
    if (((mk_tcb_t *)tcb)->state == MK_TASK_SUSPENDED)
    {
        ((mk_tcb_t *)tcb)->state = MK_TASK_READY;
    }
    mk_port_exit_critical();

    /* Resume may allow a higher-priority task to run, so trigger context switch */
    mk_port_trigger_context_switch();
}

/* Get current state of a task (READY, BLOCKED, RUNNING, or SUSPENDED) */
mk_task_state_t mk_taskGetState(volatile mk_tcb_t *tcb)
{
    if (tcb == NULL)
    {
        return MK_TASK_READY;
    }

    return ((mk_tcb_t *)tcb)->state;
}

/* Delete a task permanently (marks slot inactive, triggers switch if self) */
mk_status_t mk_taskDelete(volatile mk_tcb_t *tcb)
{
    uint32_t i;
    uint32_t task_index = mk_task_count; /* sentinel: invalid */
    uint8_t  is_self;

    if (tcb == NULL)
    {
        return MK_ERR_INVALID_ARG;
    }

    mk_port_enter_critical();

    /* Find the matching slot */
    for (i = 0u; i < mk_task_count; i++)
    {
        if (&mk_task_table[i].tcb == (mk_tcb_t *)tcb)
        {
            task_index = i;
            break;
        }
    }

    /* Slot not found or it is the idle task (always last slot) */
    if ((task_index >= mk_task_count) || (task_index == (mk_task_count - 1u)))
    {
        mk_port_exit_critical();
        return MK_ERR_INVALID_ARG;
    }

    is_self = (tcb == mk_current_tcb) ? 1u : 0u;

    /* Mark slot as permanently inactive */
    mk_task_table[task_index].active    = 0u;
    mk_task_table[task_index].tcb.state = MK_TASK_READY;

    mk_port_exit_critical();

    /* If deleting itself, hand off CPU immediately */
    if (is_self != 0u)
    {
        mk_port_trigger_context_switch();
    }

    return MK_OK;
}

/* Inline string compare to avoid pulling in string.h */
static uint8_t mk_str_eq(const char *a, const char *b)
{
    if ((a == NULL) || (b == NULL))
    {
        return 0u;
    }
    while ((*a != '\0') && (*b != '\0') && (*a == *b))
    {
        a++;
        b++;
    }
    return ((*a == '\0') && (*b == '\0')) ? 1u : 0u;
}

/* Find and return a task handle by name (search task table for matching name) */
volatile mk_tcb_t *mk_taskGetHandle(const char *name)
{
    uint32_t i;

    if (name == NULL)
    {
        return NULL;
    }

    for (i = 0u; i < mk_task_count; i++)
    {
        if ((mk_task_table[i].active != 0u) &&
            (mk_str_eq(mk_task_table[i].tcb.name, name) != 0u))
        {
            return (volatile mk_tcb_t *)&mk_task_table[i].tcb;
        }
    }

    return NULL;
}

/* System idle task (lowest priority, runs when no user tasks are ready) */
static void mk_idleTask(void *arg)
{
    (void)arg;

    for (;;)
    {
#if (MK_RTOS_USE_IDLE_HOOK == 1u)
        mk_idleHook();
#endif
        __WFI();
    }
}

#if (MK_RTOS_PREEMPTION_ENABLE == 0u)
/* Check if any user task is ready and not suspended (cooperative mode) */
static uint8_t mk_has_ready_user_task(void)
{
    uint32_t i;

    if (mk_task_count == 0u)
    {
        return 0u;
    }

    for (i = 0u; i < (mk_task_count - 1u); i++)
    {
        if ((mk_task_table[i].active != 0u) &&
            (mk_task_table[i].tcb.delay_ticks == 0u) &&
            (mk_task_table[i].tcb.state != MK_TASK_SUSPENDED))
        {
            return 1u;
        }
    }

    return 0u;
}
#endif

