#ifndef MAIN_H_
#define MAIN_H_

void task1_handler(void);
void task2_handler(void);
void task3_handler(void);
void task4_handler(void);

void init_systick_timer(uint32_t);
__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack);
void init_tasks_stack(void);
void enable_processor_faults(void);
__attribute__((naked)) void switch_sp_to_psp(void);
uint32_t get_psp_value(void);
void save_psp_value(uint32_t);
void update_next_task(void);
void task_delay(uint32_t);
void update_global_tick_count(void);
void unblock_tasks(void);
void schedule(void);


#define SIZE_TASK_STACK         1024U
#define SIZE_SCHED_STACK        1024U
#define SRAM_START              0x20000000U
#define SIZE_SRAM 				((128) * (1024))
#define SRAM_END 				((SRAM_START) + (SIZE_SRAM))
#define MAX_TASKS 				5
#define T1_STACK_START 			SRAM_END
#define T2_STACK_START          ((SRAM_END) - (SIZE_TASK_STACK))
#define T3_STACK_START          ((SRAM_END) - (2*SIZE_TASK_STACK))
#define T4_STACK_START          ((SRAM_END) - (3*SIZE_TASK_STACK))
#define IDLE_STACK_START        ((SRAM_END) - (4*SIZE_TASK_STACK))
#define SCHED_STACK_START       ((SRAM_END) - (5*SIZE_TASK_STACK))
#define TICK_HZ                 1000
#define HSI_CLOCK               16000000U
#define SYSTICK_TIM_CLK         HSI_CLOCK
#define DUMMY_XPSR              0x01000000U
#define TASK_READY_STATE        0x00
#define TASK_BLOCKED_STATE      0xFF


#define INTERRUPT_DISABLE() do{                                   \
	                           __asm volatile ("MOV R0,#0x1");    \
							   __asm volatile ("MSR PRIMASK,R0"); \
							}while(0)	
#define INTERRUPT_ENABLE() do{                                    \
	                           __asm volatile ("MOV R0,#0x0");    \
							   __asm volatile ("MSR PRIMASK,R0"); \
							}while(0)
#endif /* MAIN_H_ *