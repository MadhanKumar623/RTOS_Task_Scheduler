#include <stdint.h>
#include <stdio.h>
#include "main.h"
#include "led.h"

uint8_t current_task = 1;
uint32_t g_tick_count = 0;

typedef struct
{
	uint32_t psp_value; //storing PSP value
	uint32_t block_count;
	uint8_t current_state;
	void (*task_handler)(void);
}TCB_t;
TCB_t user_tasks[MAX_TASKS];

int main(void)
{
	enable_processor_faults(); //To know about various faults such as Busfault, Memmanagefault etc...
	init_scheduler_stack(SCHED_STACK_START);
	init_tasks_stack();
	led_init_all();
	init_systick_timer(TICK_HZ);
	switch_sp_to_psp(); //switching from MSP to PSP
	task1_handler();
	for(;;);
}

void idle_task(void)
{
	while(1);
}

void task1_handler()
{
	while(1)
	{
		printf("This is task1\n");
		led_on(LED_GREEN);
		task_delay(1000);
		led_off(LED_GREEN);
		task_delay(1000);
	}
}

void task2_handler()
{
	while(1)
	{
		printf("This is task2\n");
		led_on(LED_ORANGE);
		task_delay(500);
		led_off(LED_ORANGE);
		task_delay(500);
	}
}

void task3_handler()
{
	while(1)
	{
		printf("This is task3\n");
		led_on(LED_BLUE);
		task_delay(250);
		led_off(LED_BLUE);
		task_delay(250);
	}
}

void task4_handler()
{
	while(1)
	{
		printf("This is task4\n");
		led_on(LED_RED);
		task_delay(125);
		led_off(LED_RED);
		task_delay(125);
	}
}

void init_systick_timer(uint32_t tick_hz)
{
	uint32_t *pSRVR = (uint32_t *)0xE000E014; //SysTick Reload  Value Register -> specifies the start value(ie)reload value to load in systick current value register.
	uint32_t count_value = (SYSTICK_TIM_CLK/tick_hz)-1; //For 1 ms delay, count value = (1ms/(1/16000000));-1 bcoz 0 to 15999 counts 16000
	*pSRVR &= ~(0x00FFFFFF);
	//Loading the Systick Reload register
	*pSRVR |= count_value;
	uint32_t *pSCSR = (uint32_t *)0xE000E010; //Systick Control and Status Register
	*pSCSR |= (1 << 1); //Enable Systick exception request
	*pSCSR |= (1 << 2); //Indicates the clock source is processor clk source
	*pSCSR |= (1 << 0); //Enabling the systick counter
}

__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack)
{
	__asm volatile("MSR MSP,R0"); //Since first argument is automatically stored in R0
	__asm volatile("BX LR"); //Link Register stores the value of return address
}

void init_tasks_stack(void)
{
	user_tasks[0].current_state = TASK_READY_STATE;
	user_tasks[1].current_state = TASK_READY_STATE;
	user_tasks[2].current_state = TASK_READY_STATE;
	user_tasks[3].current_state = TASK_READY_STATE;
	user_tasks[4].current_state = TASK_READY_STATE;
	user_tasks[0].psp_value = IDLE_STACK_START;
	user_tasks[1].psp_value = T1_STACK_START;
	user_tasks[2].psp_value = T2_STACK_START;
	user_tasks[3].psp_value = T3_STACK_START;
	user_tasks[4].psp_value = T4_STACK_START;
	user_tasks[0].task_handler = idle_task;
	user_tasks[1].task_handler = task1_handler;
	user_tasks[2].task_handler = task2_handler;
	user_tasks[3].task_handler = task3_handler;
	user_tasks[4].task_handler = task4_handler;
	uint32_t *pPSP;
	for(int i=0;i<MAX_TASKS;i++)
	{
		pPSP = (uint32_t *)user_tasks[i].psp_value;
		pPSP--;
		*pPSP = DUMMY_XPSR; //Set T bit as 1. In this register it is 24 bit. so value will be 0x01000000
		pPSP--;
		*pPSP =(uint32_t) user_tasks[i].task_handler; //This field is for PC
		pPSP--;
		*pPSP = 0xFFFFFFFD; //This is for LR
		for(int j=0;j<13;j++)
		{
			pPSP--;
			*pPSP = 0;
		}
		user_tasks[i].psp_value = (uint32_t)pPSP;
	}
}

void enable_processor_faults(void)
{
	uint32_t *pSHCSR = (uint32_t *)0xE000ED24;
	*pSHCSR |= (1 << 16); //Enabling the Mem Manage Fault
	*pSHCSR |= (1 << 17); //Bus Fault
	*pSHCSR |= (1 << 18); //usage Fault
}

void HardFault_Handler(void)
{
	printf("Exception : HardFault\n");
	while(1);
}

void MemManage_Handler(void)
{
	printf("Exception : MemManageFault\n");
	while(1);
}

void BusFault_Handler(void)
{
	printf("Exception: BusFault\n");
	while(1);
}

void UsageFault_Handler(void)
{
	printf("Exception: UsageFault\n");
	while(1);
}

uint32_t get_psp_value(void)
{
	return user_tasks[current_task].psp_value;
}

void save_psp_value(uint32_t current_psp_value)
{
	user_tasks[current_task].psp_value=current_psp_value;
}

void update_next_task(void)
{
	int state = TASK_BLOCKED_STATE;
	for(int i=0;i<MAX_TASKS;i++)
	{
		current_task++;
		current_task = current_task % MAX_TASKS;
		state = user_tasks[current_task].current_state;
		if(state == TASK_READY_STATE && current_task!=0)
		{
			break;
		}
	}
	if(state!=TASK_READY_STATE)
	current_task=0;
}

__attribute__((naked)) void switch_sp_to_psp(void)
{
	__asm volatile("PUSH {LR}"); //Saving the LR value for future reference(go back to main fun where it is actually called from)
	__asm volatile("BL get_psp_value"); //TO come back to this fun itself use BL, If you want only go to that func use BX
	__asm volatile ("MSR PSP,R0");
	__asm volatile ("POP {LR}");
	//Till now it only uses MSP as a stack pointer. Now only we are going to change it
	__asm volatile("MOV R0,#0x02");
	__asm volatile("MSR CONTROL,R0"); //Changing the setting of control registers to change to psp
	__asm volatile("BX LR");
}

void schedule(void)
{
	uint32_t *pICSR = (uint32_t *)0xE000ED04;
	*pICSR |= (1<<28); //Pending the PendSV Exception
}

void task_delay(uint32_t tick_count)
{
	//Disable Interrupt
	INTERRUPT_DISABLE(); //By using PRIMASK register.Priority Mask Reg
	if(current_task)
	{
		user_tasks[current_task].block_count = g_tick_count + tick_count;
		user_tasks[current_task].current_state = TASK_BLOCKED_STATE;
		schedule();
	}
	//Enable Interrupt
	INTERRUPT_ENABLE();
}

__attribute__((naked)) void PendSV_Handler(void)
{
	__asm volatile ("MRS R0,PSP");
	__asm volatile ("STMDB R0!,{R4-R11}"); //Instruction to store value of registers from R4 to R11[Data Movement from Register to Memory]
	__asm volatile("PUSH {LR}"); //As we are going to use BL instr
	__asm volatile("BL save_psp_value");
	__asm volatile("BL update_next_task");
	__asm volatile("BL get_psp_value");
	__asm volatile ("LDMIA R0!,{R4-R11}");
	__asm volatile ("MSR PSP,R0");
	__asm volatile("POP {LR}");
	__asm volatile("BX LR"); //As it is naked fun, we should manually made it to return back to the place where it is called from
}

void update_global_tick_count(void)
{
	g_tick_count++;
}

void unblock_tasks(void)
{
	for(int i=1;i<MAX_TASKS;i++)
	{
		if(user_tasks[i].current_state != TASK_READY_STATE)
		{
			if(user_tasks[i].block_count == g_tick_count)
			{
				user_tasks[i].current_state = TASK_READY_STATE;
			}
		}
	}
}

void SysTick_Handler(void) //for pending the pendSV
{
	update_global_tick_count();
	unblock_tasks();
	uint32_t *pICSR = (uint32_t *)0xE000ED04;
	*pICSR |= (1<<28);
}