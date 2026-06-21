#include "mk_port.h"
#include "cy_pdl.h"

#define MK_PORT_INITIAL_XPSR          (0x01000000u)

void mk_port_task_exit_error(void)
{
    for (;;)
    {
        __BKPT(0);
    }
}

/* Initialize stack frame with R0-R15, xPSR for task execution */
uint32_t *mk_port_init_stack(uint32_t *stack_top, mk_task_fn_t task_fn, void *arg)
{
    uint32_t *sp = stack_top;

    sp = (uint32_t *)((uint32_t)sp & ~((uint32_t)0x7u));
    sp -= 16;

    sp[0] = 0u;
    sp[1] = 0u;
    sp[2] = 0u;
    sp[3] = 0u;
    sp[4] = 0u;
    sp[5] = 0u;
    sp[6] = 0u;
    sp[7] = 0u;

    sp[8] = (uint32_t)arg;
    sp[9] = 0x01010101u;
    sp[10] = 0x02020202u;
    sp[11] = 0x03030303u;
    sp[12] = 0x12121212u;
    sp[13] = (uint32_t)mk_port_task_exit_error;
    sp[14] = (uint32_t)task_fn;
    sp[15] = MK_PORT_INITIAL_XPSR;

    return sp;
}

/* Configure SysTick timer at configured period and set interrupt priorities */
void mk_port_setup_timer_interrupt(void)
{
    uint32_t tick_hz = MK_RTOS_TICK_HZ;
    uint32_t cpu_hz = (uint32_t)MK_RTOS_CPU_CLOCK_HZ;

    NVIC_SetPriority(PendSV_IRQn, (1u << __NVIC_PRIO_BITS) - 1u);
    NVIC_SetPriority(SysTick_IRQn, (1u << __NVIC_PRIO_BITS) - 2u);

    (void)SysTick_Config(cpu_hz / tick_hz);
}

/* Trigger PendSV exception for deferred context switching */
void mk_port_trigger_context_switch(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}

/* Disable all interrupts (enter critical section) */
void mk_port_enter_critical(void)
{
    __disable_irq();
}

/* Enable all interrupts (exit critical section) */
void mk_port_exit_critical(void)
{
    __enable_irq();
}

/* Save interrupt state and disable interrupts (for nested critical sections) */
uint32_t mk_port_critical_save(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

/* Restore previous interrupt state (pairs with mk_port_critical_save) */
void mk_port_critical_restore(uint32_t primask)
{
    __set_PRIMASK(primask);
}

/* Start first task by triggering SVC exception to enter Thread mode with PSP */
__attribute__((naked)) void mk_port_start_first_task(void)
{
    __asm volatile
    (
        "svc 0                        \n"
        "bx lr                        \n"
    );
}

/* SysTick interrupt handler (1 ms tick, calls kernel tick handler) */
void SysTick_Handler(void)
{
    mk_kernelTickHandler();
}

/* SVC exception handler (loads first task context and enables PSP) */
__attribute__((naked)) void SVC_Handler(void)
{
    __asm volatile
    (
        "ldr r3, =mk_current_tcb      \n"
        "ldr r1, [r3]                 \n"
        "ldr r0, [r1]                 \n"

        "ldmia r0!, {r4-r7}           \n"
        "ldmia r0!, {r1-r4}           \n"
        "mov r8, r1                   \n"
        "mov r9, r2                   \n"
        "mov r10, r3                  \n"
        "mov r11, r4                  \n"
        "msr psp, r0                  \n"

        "movs r0, #2                  \n"
        "msr CONTROL, r0              \n"
        "isb                          \n"

        "ldr r0, =0xFFFFFFFD          \n"
        "mov lr, r0                   \n"
        "bx lr                        \n"
    );
}

/* PendSV exception handler (saves context, calls task selector, restores context) */
__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile
    (
        "mrs r0, psp                  \n"
        "sub r0, #32                  \n"
        "stmia r0!, {r4-r7}           \n"
        "mov r1, r8                   \n"
        "mov r2, r9                   \n"
        "mov r3, r10                  \n"
        "mov r4, r11                  \n"
        "stmia r0!, {r1-r4}           \n"

        "sub r0, #32                  \n"

        "ldr r3, =mk_current_tcb      \n"
        "ldr r2, [r3]                 \n"
        "str r0, [r2]                 \n"

        "push {r3, lr}                \n"
        "cpsid i                      \n"
        "bl mk_port_select_next_task  \n"
        "cpsie i                      \n"
        "pop {r3, r4}                 \n"
        "mov lr, r4                   \n"

        "ldr r3, =mk_current_tcb      \n"
        "ldr r1, [r3]                 \n"
        "ldr r0, [r1]                 \n"

        "ldmia r0!, {r4-r7}           \n"
        "ldmia r0!, {r1-r4}           \n"
        "mov r8, r1                   \n"
        "mov r9, r2                   \n"
        "mov r10, r3                  \n"
        "mov r11, r4                  \n"
        "msr psp, r0                  \n"

        "bx lr                        \n"
    );
}
