#ifndef MK_RTOS_CONFIG_H
#define MK_RTOS_CONFIG_H

#include <stdint.h>

#define MK_RTOS_MAX_TASKS              (8u)
#define MK_RTOS_PREEMPTION_ENABLE      (0u)
#define MK_RTOS_USE_IDLE_HOOK          (0u)

/* CPU clock used by SysTick reload calculation (48 MHz). */
#define MK_RTOS_CPU_CLOCK_HZ           (48000000UL)

/* SysTick interrupt period in milliseconds (1u => 1 ms). */
#define MK_RTOS_SYSTICK_PERIOD_MS      (1u)

/* RTOS tick frequency derived from SysTick period. */
#define MK_RTOS_TICK_HZ                (1000u / MK_RTOS_SYSTICK_PERIOD_MS)

/* CPU load averaging window in ticks (default: 1 second). */
#define MK_RTOS_CPU_LOAD_WINDOW_TICKS  (MK_RTOS_TICK_HZ)

/*
 * Task priority levels: 0 = lowest (idle), MK_RTOS_MAX_PRIORITY-1 = highest.
 * Pass the number directly to mk_taskCreate(), e.g. 1, 2, 3 ...
 */
#define MK_RTOS_MAX_PRIORITY           (5u)

/* Task stack size is in 32-bit words. */
#define MK_RTOS_DEFAULT_STACK_WORDS    (128u)

#endif /* MK_RTOS_CONFIG_H */
