#include <stdio.h>
#include "cy_pdl.h"
#include "cy_sysint.h"
#include "cybsp.h"
#include "mk_cpu_load.h"
#include "mk_rtos.h"

/* Application constants */
#define CY_ASSERT_FAILED        (0u)
#define MK_MS_TO_TICKS(ms)      (((ms) * MK_RTOS_TICK_HZ) / 1000u)
#define TASK_STACK_WORDS        (192u)

/* Button Interrupt Configuration */
#define BTN_IRQ                 ioss_interrupts_gpio_3_IRQn
#define BTN_IRQ_PRIORITY        (1u)
#define BTN_DEBOUNCE_TICKS      (200u)   /* 200 ms */

static const cy_stc_sysint_t btn_irq_cfg =
{
    .intrSrc      = BTN_IRQ,
    .intrPriority = BTN_IRQ_PRIORITY,
};

/* Below enum is used in ou Application and not for the kernel */
typedef enum
{
    APP_PHASE_SUSPEND = 0u,
    APP_PHASE_RESUME  = 1u
} app_cycle_phase_t;

/* ------------------------------------------------------------------ */
/* UART context                                                         */
/* ------------------------------------------------------------------ */
static cy_stc_scb_uart_context_t g_uart_context;

/* ------------------------------------------------------------------ */
/* Task stacks (this Kernel is statically allocated, not dynamically) */
/* ------------------------------------------------------------------ */
static uint32_t g_task1_stack[TASK_STACK_WORDS];
static uint32_t g_task2_stack[TASK_STACK_WORDS];
static uint32_t g_task3_stack[TASK_STACK_WORDS];
static uint32_t g_task4_stack[TASK_STACK_WORDS];
static uint32_t g_supervisor_stack[TASK_STACK_WORDS];

/* ------------------------------------------------------------------ */
/* Task handles managed by the button-driven suspend/resume cycle      */
/* ------------------------------------------------------------------ */
static volatile mk_tcb_t *g_task1_handle = NULL;
static volatile mk_tcb_t *g_task2_handle = NULL;
static volatile mk_tcb_t *g_task3_handle = NULL;
static volatile mk_tcb_t *g_task4_handle = NULL;

static volatile mk_tcb_t *g_managed_handles[4] = { NULL, NULL, NULL, NULL };
static const char *g_managed_names[4] = { "Task1", "Task2", "Task3", "Task4" };

/* ------------------------------------------------------------------ */
/* Button press events are counted in ISR and handled in supervisor    */
/* ------------------------------------------------------------------ */
static volatile uint32_t g_btn_pending_presses = 0u;
static app_cycle_phase_t g_cycle_phase = APP_PHASE_SUSPEND;
static uint8_t g_cycle_index = 0u;

/* ------------------------------------------------------------------ */
/* P3.7 GPIO Port-3 interrupt handler                                   */
/* ------------------------------------------------------------------ */
static void btn_gpio_isr(void)
{
    /* Check if pin 7 triggered the interrupt */
    if (Cy_GPIO_GetInterruptStatus(BTN_PORT, BTN_PIN) != 0u)
    {
        Cy_GPIO_ClearInterrupt(BTN_PORT, BTN_PIN);

        /* Software debounce using RTOS tick count */
        static uint32_t last_press_tick = 0u;
        uint32_t        now_tick        = mk_taskGetTickCount();

        if ((now_tick - last_press_tick) >= BTN_DEBOUNCE_TICKS)
        {
            last_press_tick = now_tick;
            g_btn_pending_presses++;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Task 1: Toggle LEDs 1-3 every 2000 ms                               */
/* ------------------------------------------------------------------ */
static void task_uart_1(void *arg)
{
    (void)arg;
    for (;;)
    {
        Cy_GPIO_Inv(LED1_PORT, LED1_NUM);
        Cy_GPIO_Inv(LED2_PORT, LED2_NUM);
        Cy_GPIO_Inv(LED3_PORT, LED3_NUM);
        mk_taskDelay(MK_MS_TO_TICKS(2000u));
    }
}

/* ------------------------------------------------------------------ */
/* Task 2: Toggle LEDs 4-6 every 1000 ms                               */
/* ------------------------------------------------------------------ */
static void task_uart_2(void *arg)
{
    (void)arg;
    for (;;)
    {
        Cy_GPIO_Inv(LED4_PORT, LED4_NUM);
        Cy_GPIO_Inv(LED5_PORT, LED5_NUM);
        Cy_GPIO_Inv(LED6_PORT, LED6_NUM);
        mk_taskDelay(MK_MS_TO_TICKS(1000u));
    }
}

/* ------------------------------------------------------------------ */
/* Task 3: Toggle LEDs 7-9 every 1000 ms                               */
/* ------------------------------------------------------------------ */
static void task_uart_3(void *arg)
{
    (void)arg;
    for (;;)
    {
        Cy_GPIO_Inv(LED7_PORT, LED7_NUM);
        Cy_GPIO_Inv(LED8_PORT, LED8_NUM);
        Cy_GPIO_Inv(LED9_PORT, LED9_NUM);
        mk_taskDelay(MK_MS_TO_TICKS(1000u));
    }
}

/* ------------------------------------------------------------------ */
/* Task 4: Toggle user LED every 500 ms                                */
/* ------------------------------------------------------------------ */
static void task_uart_4(void *arg)
{
    (void)arg;
    for (;;)
    {
        Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
        mk_taskDelay(MK_MS_TO_TICKS(500u));
    }
}

static const char *task_state_to_str(mk_task_state_t state)
{
    switch (state)
    {
        case MK_TASK_READY:
            return "READY";
        case MK_TASK_BLOCKED:
            return "BLOCKED";
        case MK_TASK_RUNNING:
            return "RUNNING";
        case MK_TASK_SUSPENDED:
            return "SUSPENDED";
        default:
            return "UNKNOWN";
    }
}

/* ------------------------------------------------------------------ */
/* Supervisor task: handles button events and prints continuous status */
/* ------------------------------------------------------------------ */
static void task_supervisor(void *arg)
{
    char msg[128];
    uint32_t last_status_tick = 0u;

    (void)arg;

    for (;;)
    {
        while (g_btn_pending_presses > 0u)
        {
            uint32_t irq_state;
            uint8_t task_idx;
            volatile mk_tcb_t *task_handle;

            irq_state = Cy_SysLib_EnterCriticalSection();
            g_btn_pending_presses--;
            Cy_SysLib_ExitCriticalSection(irq_state);

            task_idx = g_cycle_index;
            task_handle = g_managed_handles[task_idx];

            if ((g_cycle_phase == APP_PHASE_SUSPEND) && (task_handle != NULL))
            {
                mk_taskSuspend(task_handle);
                snprintf(msg, sizeof(msg),
                               "[BTN] %s Suspended\r\n",
                               g_managed_names[task_idx]);
                Cy_SCB_UART_PutString(CYBSP_UART_HW, msg);
            }
            else if ((g_cycle_phase == APP_PHASE_RESUME) && (task_handle != NULL))
            {
                mk_taskResume(task_handle);
                snprintf(msg, sizeof(msg),
                               "[BTN] %s Resumed\r\n",
                               g_managed_names[task_idx]);
                Cy_SCB_UART_PutString(CYBSP_UART_HW, msg);
            }

            g_cycle_index++;
            if (g_cycle_index >= 4u)
            {
                g_cycle_index = 0u;
                g_cycle_phase = (g_cycle_phase == APP_PHASE_SUSPEND) ?
                                APP_PHASE_RESUME : APP_PHASE_SUSPEND;
            }
        }

        /* Printing the Load status */
        if ((mk_taskGetTickCount() - last_status_tick) >= MK_MS_TO_TICKS(1000u))
        {
            uint32_t load_x1000 = mk_cpuLoadGetPercentX1000();

            last_status_tick = mk_taskGetTickCount();

            snprintf(msg, sizeof(msg),
                           "CPU Load: %lu.%03lu%%\r\n",
                           (unsigned long)(load_x1000 / 1000u),
                           (unsigned long)(load_x1000 % 1000u));
            Cy_SCB_UART_PutString(CYBSP_UART_HW, msg);

            snprintf(msg, sizeof(msg),
                           "Task1=%s, Task2=%s, Task3=%s, Task4=%s\r\n",
                           task_state_to_str(mk_taskGetState(g_task1_handle)),
                           task_state_to_str(mk_taskGetState(g_task2_handle)),
                           task_state_to_str(mk_taskGetState(g_task3_handle)),
                           task_state_to_str(mk_taskGetState(g_task4_handle)));
            Cy_SCB_UART_PutString(CYBSP_UART_HW, msg);
        }

        mk_taskDelay(MK_MS_TO_TICKS(50u));
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void)
{
    cy_rslt_t  result;
    mk_status_t mk_status;

    /* Initialize board peripherals (clocks, UART, LEDs, etc.) */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Configure and enable UART */
    Cy_SCB_UART_Init(CYBSP_UART_HW, &CYBSP_UART_config, &g_uart_context);
    Cy_SCB_UART_Enable(CYBSP_UART_HW);

    /* Register and enable the Port-3 GPIO interrupt */
    Cy_SysInt_Init(&btn_irq_cfg, btn_gpio_isr);
    NVIC_EnableIRQ(BTN_IRQ);

    /* ---- Create RTOS tasks ---- */
    mk_kernelInit();

    /* Task 1 creation */
    mk_status = mk_taskCreate(task_uart_1, NULL, g_task1_stack, TASK_STACK_WORDS, "Task1", 2u);
    if (mk_status != MK_OK) { CY_ASSERT(CY_ASSERT_FAILED); }

    /* Store Task1 handle immediately after creation */
    g_task1_handle = mk_taskGetHandle("Task1");

    /* Task 2 creation */
    mk_status = mk_taskCreate(task_uart_2, NULL, g_task2_stack, TASK_STACK_WORDS, "Task2", 2u);                           
    if (mk_status != MK_OK) { CY_ASSERT(CY_ASSERT_FAILED); }

    /* Store Task2 handle immediately after creation */
    g_task2_handle = mk_taskGetHandle("Task2");

    /* Task 3 creation */
    mk_status = mk_taskCreate(task_uart_3, NULL, g_task3_stack, TASK_STACK_WORDS, "Task3", 2u);
    if (mk_status != MK_OK) { CY_ASSERT(CY_ASSERT_FAILED); }

    /* Store Task3 handle immediately after creation */
    g_task3_handle = mk_taskGetHandle("Task3");

    /* Task 4 creation */
    mk_status = mk_taskCreate(task_uart_4, NULL, g_task4_stack, TASK_STACK_WORDS, "Task4", 2u);
    if (mk_status != MK_OK) { CY_ASSERT(CY_ASSERT_FAILED); }

    /* Store Task4 handle immediately after creation */
    g_task4_handle = mk_taskGetHandle("Task4");

    /* Store managed task handles to be used by supervisor task */
    g_managed_handles[0] = g_task1_handle;
    g_managed_handles[1] = g_task2_handle;
    g_managed_handles[2] = g_task3_handle;
    g_managed_handles[3] = g_task4_handle;

    /* Supervisor task creation */
    mk_status = mk_taskCreate(task_supervisor, NULL, g_supervisor_stack, TASK_STACK_WORDS, "Supervisor", 1u);
    if (mk_status != MK_OK) { CY_ASSERT(CY_ASSERT_FAILED); }

    __enable_irq();

    Cy_SCB_UART_PutString(CYBSP_UART_HW, "mk_rtos start\r\nPress BTN (P3.7): 1-4 suspend Task1-Task4, next 1-4 resume them\r\n");

    mk_kernelStart();

    /* mk_kernelStart never returns in normal operation */
    for (;;)
    {
    }
}

/* [] END OF FILE */

