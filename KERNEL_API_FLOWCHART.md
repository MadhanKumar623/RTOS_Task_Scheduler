# MK RTOS Kernel API Flowchart

This flowchart is designed for beginners to understand when each API is used and how control moves between application code, scheduler logic, and interrupts.

```mermaid
flowchart TD
    A[Application Start main] --> B[mk_kernelInit]
    B --> C{Create each task?}
    C -->|Yes| D[mk_taskCreate task_fn arg stack_mem stack_words name priority]
    D --> C
    C -->|No| E[mk_kernelStart]

    E --> F[Kernel creates idle task internally\nmk_taskCreate idle]
    F --> G[mk_port_setup_timer_interrupt]
    G --> H[mk_port_start_first_task]

    H --> I[Tasks Running]

    %% Runtime API usage from tasks
    I --> J{Task behavior}
    J --> K[mk_taskDelay ticks\nstate RUNNING to BLOCKED]
    J --> L[mk_taskYield\nrequest context switch]
    J --> M[mk_taskSuspend task_handle]
    J --> N[mk_taskResume task_handle]
    J --> O[mk_taskDelete task_handle]
    J --> P[mk_taskGetState task_handle]
    J --> Q[mk_taskGetHandle name]
    J --> R[mk_taskGetTickCount]

    %% Sync APIs
    I --> S{Shared resource needed?}
    S --> T[mk_mutexInit]
    S --> U[mk_lock_mutex timeout]
    U --> V[mk_unlock_mutex]
    S --> W[mk_semaphoreInit initial max]
    S --> X[mk_semaphoreTake timeout]
    X --> Y[mk_semaphoreGive]

    %% Tick ISR path
    G --> Z[SysTick Interrupt]
    Z --> AA[mk_kernelTickHandler]
    AA --> AB[Decrement delay ticks\nBLOCKED to READY when timeout expires]
    AA --> AC[mk_cpuLoadOnTick idle_running]
    AA --> AD{Need preemption or time slice?}
    AD -->|Yes| AE[mk_port_trigger_context_switch]
    AD -->|No| I

    %% Context switch path
    L --> AE
    K --> AE
    M --> AE
    N --> AE
    O --> AE
    AE --> AF[PendSV Handler]
    AF --> AG[mk_port_select_next_task]
    AG --> AH[Pick highest-priority READY active task\nround-robin among equal priority]
    AH --> AI[Selected task state READY to RUNNING]
    AI --> AJ[mk_cpuLoadOnContextSwitch idle_running]
    AJ --> I

    %% Idle behavior
    I --> AK{No user task READY?}
    AK -->|Yes| AL[idle task runs]
    AL --> AM[mk_idleHook optional]
    AL --> AN[__WFI low-power wait]
    AN --> Z

    %% Critical-section helpers used by kernel internals
    AG --> AO[mk_port_enter_critical or exit_critical]
    U --> AO
    X --> AO

    %% CPU load read APIs from app
    I --> AP[mk_cpuLoadGetPercent]
    I --> AQ[mk_cpuLoadGetPercentX10]
    I --> AR[mk_cpuLoadGetPercentX1000]
```

## Quick Reading Guide

- Startup order: mk_kernelInit -> mk_taskCreate repeated -> mk_kernelStart.
- Task stack ownership: each task stack is provided by the application using stack_mem and stack_words.
- Scheduling rule: highest priority READY active task runs; same priority tasks are round-robin.
- Deletion model: mk_taskDelete marks slot inactive, so scheduler skips it in future selections.
- Sync model: mutex and semaphore APIs are cooperative with timeout-based waiting.
- CPU load APIs: read from application; update functions are called by kernel internals.
