#include "mk_cpu_load.h"

#include "cy_pdl.h"
#include "mk_rtos.h"
#include "mk_rtos_config.h"

#if (MK_RTOS_CPU_LOAD_WINDOW_TICKS == 0u)
#error "MK_RTOS_CPU_LOAD_WINDOW_TICKS must be greater than 0"
#endif

static volatile uint32_t mk_cpu_window_ticks;
static volatile uint32_t mk_cpu_idle_ticks;
static volatile uint32_t mk_cpu_load_x10;
static volatile uint32_t mk_cpu_load_x1000;
static uint32_t mk_cpu_last_stamp;
static uint8_t mk_cpu_last_idle;
static uint8_t mk_cpu_stamp_valid;

/* Return elapsed SysTick clocks since boot in modulo-32bit space. */
static uint32_t mk_cpuLoadGetStamp(void)
{
    uint32_t tick_1;
    uint32_t tick_2;
    uint32_t val;
    uint32_t per_tick = (uint32_t)(MK_RTOS_CPU_CLOCK_HZ / MK_RTOS_TICK_HZ);

    do
    {
        tick_1 = mk_taskGetTickCount();
        val = SysTick->VAL;
        tick_2 = mk_taskGetTickCount();
    } while (tick_1 != tick_2);

    return (tick_1 * per_tick) + (per_tick - val);
}

/* Accumulate elapsed runtime into idle or busy bucket. */
static void mk_cpuLoadAccumulate(uint8_t current_idle)
{
    uint32_t now_stamp = mk_cpuLoadGetStamp();
    uint32_t elapsed;

    if (mk_cpu_stamp_valid == 0u)
    {
        mk_cpu_last_stamp = now_stamp;
        mk_cpu_last_idle = current_idle;
        mk_cpu_stamp_valid = 1u;
        return;
    }

    elapsed = now_stamp - mk_cpu_last_stamp;
    mk_cpu_window_ticks += elapsed;
    if (mk_cpu_last_idle != 0u)
    {
        mk_cpu_idle_ticks += elapsed;
    }

    if (mk_cpu_window_ticks >= (uint32_t)(MK_RTOS_CPU_LOAD_WINDOW_TICKS * (MK_RTOS_CPU_CLOCK_HZ / MK_RTOS_TICK_HZ)))
    {
        uint32_t busy_ticks = mk_cpu_window_ticks - mk_cpu_idle_ticks;
        uint64_t busy_u64 = (uint64_t)busy_ticks;
        uint64_t window_u64 = (uint64_t)mk_cpu_window_ticks;

        mk_cpu_load_x10 = (uint32_t)((busy_u64 * 1000ull + (window_u64 / 2ull)) / window_u64);
        mk_cpu_load_x1000 = (uint32_t)((busy_u64 * 100000ull + (window_u64 / 2ull)) / window_u64);

        mk_cpu_window_ticks = 0u;
        mk_cpu_idle_ticks = 0u;
    }

    mk_cpu_last_stamp = now_stamp;
    mk_cpu_last_idle = current_idle;
}

/* Reset CPU load counters and last computed value. */
void mk_cpuLoadInit(void)
{
    mk_cpu_window_ticks = 0u;
    mk_cpu_idle_ticks = 0u;
    mk_cpu_load_x10 = 0u;
    mk_cpu_load_x1000 = 0u;
    mk_cpu_last_stamp = 0u;
    mk_cpu_last_idle = 0u;
    mk_cpu_stamp_valid = 0u;
}

/* Update CPU load accounting on every tick using idle-task activity. */
void mk_cpuLoadOnTick(uint8_t idle_running)
{
    mk_cpuLoadAccumulate(idle_running);
}

/* Update CPU load accounting on every context switch for sub-tick precision. */
void mk_cpuLoadOnContextSwitch(uint8_t idle_running)
{
    mk_cpuLoadAccumulate(idle_running);
}

/* Return CPU load in 0.1% units (for example 755 = 75.5%). */
uint32_t mk_cpuLoadGetPercentX10(void)
{
    return mk_cpu_load_x10;
}

/* Return CPU load in 0.001% units (for example 1234 = 1.234%). */
uint32_t mk_cpuLoadGetPercentX1000(void)
{
    return mk_cpu_load_x1000;
}

/* Return CPU load as rounded integer percent (0..100). */
uint32_t mk_cpuLoadGetPercent(void)
{
    return (mk_cpu_load_x10 + 5u) / 10u;
}
