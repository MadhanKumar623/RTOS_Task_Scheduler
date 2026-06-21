# Custom RTOS Kernel

## 1. Overview
This folder contains a small RTOS kernel for ARM Cortex-M0+ class MCUs.

Current kernel features:
- Static task creation
- Priority-based scheduling
- Round-robin among equal-priority ready tasks
- Optional preemption
- Delay and yield APIs
- Explicit task suspend and resume
- Task deletion support
- Task handle lookup by name
- Basic mutex and semaphore primitives
- CPU load measurement
- Cortex-M0+ context switching through SysTick, PendSV, and SVC

The kernel is intentionally small and uses simple data structures so the control flow can be followed easily.

## 2. Folder Structure
- `include/`: public headers and configuration macros
- `kernel/`: scheduler, task state management, sync primitives, CPU load logic
- `portable/arm_cm0plus/`: architecture-specific context switch and tick setup

## 3. Main Files
### include/mk_rtos.h
Public kernel API and task data types.

Key contents:
- `mk_status_t`
- `mk_task_state_t`
- `mk_tcb_t`
- task and kernel APIs

### include/mk_rtos_config.h
Compile-time kernel configuration.

Important settings:
- maximum task count
- preemption enable/disable
- CPU clock used for SysTick timing
- SysTick period and tick frequency
- maximum priority
- CPU load averaging window

### include/mk_sync.h
Public APIs for mutexes and semaphores.

### include/mk_cpu_load.h
Public APIs for CPU load measurement.

### kernel/mk_rtos.c
Core scheduler implementation.

Responsibilities:
- task table initialization
- task creation
- delay and yield behavior
- tick processing
- task state transitions
- next-task selection
- suspend, resume, delete, and handle lookup
- idle task management

### kernel/mk_sync.c
Basic mutex and semaphore operations.

### kernel/mk_cpu_load.c
CPU load measurement using tick events and context-switch timing.

### portable/arm_cm0plus/mk_port.c
Low-level Cortex-M0+ port.

Responsibilities:
- initial stack frame preparation
- SysTick configuration
- PendSV trigger
- critical section helpers
- first-task startup through SVC
- context save/restore through PendSV

## 4. Kernel Data Model
The kernel stores tasks in a static table:
- one slot per user task
- one extra slot reserved for the idle task

Each slot contains:
- a task control block (`mk_tcb_t`)
- an `active` flag

The TCB stores:
- current stack pointer
- stack base and size
- delay counter
- entry function and argument
- task name
- priority
- task state

## 5. Task States
The current task-state model is:
- `MK_TASK_READY`: task can run but is not currently executing
- `MK_TASK_BLOCKED`: task is waiting on delay or resource availability
- `MK_TASK_RUNNING`: task is currently executing on the CPU
- `MK_TASK_SUSPENDED`: task is explicitly removed from scheduling

### State transitions
Common transitions:
- `READY -> RUNNING`: scheduler selects the task
- `RUNNING -> BLOCKED`: task calls delay or waits for a resource
- `BLOCKED -> READY`: delay expires or resource wait loop gets another chance to run
- `RUNNING -> READY`: scheduler switches away from the running task
- `READY/RUNNING -> SUSPENDED`: task is explicitly suspended
- `SUSPENDED -> READY`: task is resumed

The idle task is also represented through the same state model.

## 6. Scheduling Model
The scheduler is priority-based.

Rules:
- higher priority ready tasks run before lower priority ready tasks
- among ready tasks with the same priority, round-robin selection is used
- if no user task is ready, the idle task runs

### Preemption
Controlled by `MK_RTOS_PREEMPTION_ENABLE`:
- `1`: the tick handler can request a context switch when another ready task should run
- `0`: task switching is cooperative except for wake-from-idle behavior

## 7. Core Kernel Flow
### Startup flow
Typical kernel flow:
1. `mk_kernelInit()` clears the task table and resets counters
2. application creates tasks using `mk_taskCreate()`
3. `mk_kernelStart()` creates the idle task and starts scheduling
4. SysTick drives `mk_kernelTickHandler()`
5. PendSV performs task switching when requested

### Delay flow
When a task calls `mk_taskDelay(ticks)`:
1. its delay counter is set
2. its state becomes `BLOCKED`
3. a context switch is triggered

During each tick:
1. delay counters are decremented
2. when a counter reaches zero, the task becomes `READY`

### Yield flow
When a task calls `mk_taskYield()`:
1. it does not become blocked
2. a context switch is requested
3. the scheduler may move it from `RUNNING` back to `READY` and choose another task

## 8. Task Control APIs
### `mk_taskCreate(...)`
Creates a new task before the kernel starts.

Behavior:
- validates inputs
- initializes stack frame
- stores name, priority, and initial state
- marks the task slot active

### `mk_taskSuspend(tcb)`
Explicitly moves a task into `SUSPENDED` state.

### `mk_taskResume(tcb)`
Moves a suspended task back to `READY` state.

### `mk_taskDelete(tcb)`
Permanently disables a task slot by clearing its `active` flag.

Notes:
- intended for permanent removal, not temporary stop/start cycling
- idle task deletion is rejected
- if a task deletes itself, the kernel forces a context switch

### `mk_taskGetHandle(name)`
Searches the active task table by task name and returns a handle.

In this kernel, a task handle is simply a pointer to the task’s TCB.

### `mk_taskGetState(tcb)`
Returns the current task state from the TCB.

## 9. Synchronization Model
The current mutex and semaphore implementation is simple and polling-based.

### Mutex
`mk_lock_mutex()`:
- checks the lock inside a critical section
- if unavailable, it repeatedly yields until timeout or success
- while waiting, the current task state is marked `BLOCKED`

`mk_unlock_mutex()`:
- clears the lock in a critical section

### Semaphore
`mk_semaphoreTake()`:
- checks count inside a critical section
- if zero, repeatedly yields until timeout or success
- while waiting, the current task state is marked `BLOCKED`

`mk_semaphoreGive()`:
- increments the count up to the configured maximum

### Important note
Resource waiting is not implemented with explicit wait lists yet.
The task state is updated to `BLOCKED`, but the actual behavior is still retry-and-yield rather than full event-driven blocking.

## 10. CPU Load Measurement
CPU load is measured in `kernel/mk_cpu_load.c`.

### Idea
The kernel measures elapsed runtime and classifies it as:
- idle time: idle task was running
- busy time: any non-idle task was running

### Inputs used
CPU load updates happen from two paths:
- tick updates from `mk_kernelTickHandler()`
- task-switch updates from `mk_port_select_next_task()`

This improves accuracy for short task bursts between ticks.

### Reported values
- `mk_cpuLoadGetPercent()` -> rounded integer percent
- `mk_cpuLoadGetPercentX10()` -> 0.1% units
- `mk_cpuLoadGetPercentX1000()` -> 0.001% units

### Accuracy note
Very light workloads can still produce extremely small percentages. The x1000 API is the best choice for fine observation.

## 11. Port Layer
The Cortex-M0+ port uses:
- SysTick for periodic kernel tick generation
- PendSV for deferred context switching
- SVC to launch the first task

The port is responsible for:
- building the initial software stack frame
- saving and restoring context during task switches
- setting interrupt priorities
- entering and leaving critical sections

## 12. Configuration Summary
Main kernel configuration points are in `include/mk_rtos_config.h`.

Important macros:
- `MK_RTOS_MAX_TASKS`
- `MK_RTOS_PREEMPTION_ENABLE`
- `MK_RTOS_CPU_CLOCK_HZ`
- `MK_RTOS_SYSTICK_PERIOD_MS`
- `MK_RTOS_TICK_HZ`
- `MK_RTOS_MAX_PRIORITY`
- `MK_RTOS_DEFAULT_STACK_WORDS`
- `MK_RTOS_CPU_LOAD_WINDOW_TICKS`

## 13. Current Limitations
This kernel currently does not include:
- dynamic task creation after kernel start
- queue or mailbox objects
- event groups
- ISR-safe synchronization APIs
- true wait-list based mutex/semaphore blocking
- task priority change API
- stack usage diagnostics

## 14. Public API Summary
### Kernel control
- `mk_kernelInit`
- `mk_kernelStart`
- `mk_kernelTickHandler`

### Task control
- `mk_taskCreate`
- `mk_taskDelay`
- `mk_taskYield`
- `mk_taskSuspend`
- `mk_taskResume`
- `mk_taskDelete`
- `mk_taskGetHandle`
- `mk_taskGetState`
- `mk_taskGetTickCount`

### CPU load
- `mk_cpuLoadInit`
- `mk_cpuLoadGetPercent`
- `mk_cpuLoadGetPercentX10`
- `mk_cpuLoadGetPercentX1000`

### Synchronization
- `mk_mutexInit`
- `mk_lock_mutex`
- `mk_unlock_mutex`
- `mk_semaphoreInit`
- `mk_semaphoreTake`
- `mk_semaphoreGive`

## 15. Target Controller and Sample Application
This RTOS integration is developed and validated for Cypress Infineon PSoC 4100S Plus.

The `main.c` in this project is an application-level sample showing how to use the kernel on this controller.
In short, it demonstrates:
- board and UART initialization
- static task stack allocation and `mk_taskCreate()` usage
- task priorities and periodic delays
- button interrupt event handling through a supervisor task
- task suspend and resume control flow
- runtime UART status output including CPU load and task states

So, `main.c` is not part of the kernel internals; it is a reference app that shows practical kernel usage on PSoC 4100S Plus.
