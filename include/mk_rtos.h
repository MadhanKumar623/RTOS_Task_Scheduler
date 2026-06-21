#ifndef MK_RTOS_H
#define MK_RTOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mk_rtos_config.h"

typedef void (*mk_task_fn_t)(void *arg);

typedef enum
{
    MK_OK = 0,
    MK_ERR_INVALID_ARG = -1,
    MK_ERR_NO_SLOT = -2,
    MK_ERR_STACK_TOO_SMALL = -3,
    MK_ERR_KERNEL_RUNNING = -4
} mk_status_t;

typedef enum
{
    MK_TASK_READY = 0u,      /* Task is ready to run but not currently executing */
    MK_TASK_BLOCKED = 1u,    /* Task is waiting on delay or resource availability */
    MK_TASK_RUNNING = 2u,    /* Task is currently executing on the CPU */
    MK_TASK_SUSPENDED = 3u   /* Task is explicitly suspended */
} mk_task_state_t;

typedef struct
{
    uint32_t *sp;
    uint32_t *stack_bottom;
    uint32_t stack_words;
    uint32_t delay_ticks;
    mk_task_fn_t entry;
    void *arg;
    const char *name;
    uint8_t priority;        /* 0 = lowest (idle), MK_RTOS_MAX_PRIORITY-1 = highest */
    mk_task_state_t state;   /* Current task state (READY, BLOCKED, RUNNING, SUSPENDED) */
} mk_tcb_t;

/* Initialize the RTOS kernel (must be called before mk_taskCreate) */
void mk_kernelInit(void);

/* Create a new task with specified priority and stack */
mk_status_t mk_taskCreate(mk_task_fn_t task_fn,
                          void *arg,
                          uint32_t *stack_mem,
                          uint32_t stack_words,
                          const char *name,
                          uint8_t priority);

/* Start the kernel and begin task scheduling */
void mk_kernelStart(void);

/* Delay current task for specified ticks */
void mk_taskDelay(uint32_t ticks);

/* Yield CPU to other ready tasks */
void mk_taskYield(void);

/* Suspend a task (transitions to SUSPENDED state) */
void mk_taskSuspend(volatile mk_tcb_t *tcb);

/* Resume a suspended task (transitions to READY state) */
void mk_taskResume(volatile mk_tcb_t *tcb);

/* Get current state of a task (READY, BLOCKED, RUNNING, or SUSPENDED) */
mk_task_state_t mk_taskGetState(volatile mk_tcb_t *tcb);

/* Delete a task permanently (marks slot inactive, triggers switch if self) */
mk_status_t mk_taskDelete(volatile mk_tcb_t *tcb);

/* Find and return a task handle by name (search task table for matching name) */
volatile mk_tcb_t *mk_taskGetHandle(const char *name);

/* Get current system tick count */
uint32_t mk_taskGetTickCount(void);

/* Kernel tick handler (called by SysTick ISR) */
void mk_kernelTickHandler(void);

/* Idle hook for idle task (empty by default, user can override) */
void mk_idleHook(void);

extern volatile mk_tcb_t *mk_current_tcb;

#endif /* MK_RTOS_H */
