#ifndef MK_CPU_LOAD_H
#define MK_CPU_LOAD_H

#include <stdint.h>

/* Initialize CPU load counters. */
void mk_cpuLoadInit(void);

/* Get CPU load as rounded integer percent (0..100). */
uint32_t mk_cpuLoadGetPercent(void);

/* Get CPU load in 0.1% units (0..1000), e.g., 755 = 75.5%. */
uint32_t mk_cpuLoadGetPercentX10(void);

/* Get CPU load in 0.001% units (0..100000), e.g., 1234 = 1.234%. */
uint32_t mk_cpuLoadGetPercentX1000(void);

/* Kernel-internal: update CPU load counters once per SysTick. */
void mk_cpuLoadOnTick(uint8_t idle_running);

/* Kernel-internal: update CPU load counters on context switch. */
void mk_cpuLoadOnContextSwitch(uint8_t idle_running);

#endif /* MK_CPU_LOAD_H */
