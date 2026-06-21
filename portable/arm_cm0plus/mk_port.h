#ifndef MK_PORT_H
#define MK_PORT_H

#include <stdint.h>

#include "mk_rtos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize task stack frame with entry point and argument */
uint32_t *mk_port_init_stack(uint32_t *stack_top, mk_task_fn_t task_fn, void *arg);

/* Setup SysTick timer and interrupt priorities for kernel scheduling */
void mk_port_setup_timer_interrupt(void);

/* Start the first task in Thread mode with PSP */
void mk_port_start_first_task(void);

/* Trigger PendSV interrupt to perform context switch */
void mk_port_trigger_context_switch(void);

/* Disable interrupts (enter critical section) */
void mk_port_enter_critical(void);

/* Enable interrupts (exit critical section) */
void mk_port_exit_critical(void);

/* Save PRIMASK register and disable interrupts */
uint32_t mk_port_critical_save(void);

/* Restore PRIMASK register to saved state */
void mk_port_critical_restore(uint32_t primask);

/* Select next task to run based on priority and state (called from PendSV) */
void mk_port_select_next_task(void);

#ifdef __cplusplus
}
#endif

#endif /* MK_PORT_H */
