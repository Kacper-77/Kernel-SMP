# System Timer and Sleep Services

## Overview
The `timer` module provides time-keeping and delay services for the kernel and user-space tasks. It maintains a global system uptime counter and abstracts the difference between synchronous busy-waiting (early boot) and asynchronous task sleeping (multitasking).

## Design Decisions

1. Dual-Phase Blocking (msleep)
The `msleep` function is designed to be polymorphic based on the current state of the system:
- Pre-Scheduler Phase: If the scheduler is not yet active or the current CPU has no task assigned, the function performs a "busy-wait" using the `pause` instruction. This is essential for early hardware initialization where timing is required but the tasking subsystem is not ready.
- Post-Activation Phase: Once the multitasking environment is live, `msleep` transitions to a non-blocking model. It marks the current task as sleeping and yields the CPU, allowing other threads to execute while the timer increments.

2. Global Uptime Tracking
The `system_uptime_ms` variable serves as the monotonic clock for the entire system.
- Tick Source: It is updated by the Local APIC (LAPIC) timer interrupt handler (vector 32).
- Resolution: Currently configured for a 5ms tick interval (updated in `interrupt_dispatch`).
- Volatile Access: Marked as `volatile` to ensure that the compiler always fetches the latest value from memory, preventing incorrect optimizations in polling loops.

3. Integration with Scheduler
The timer module acts as the primary trigger for the scheduler's blocking logic.
- sched_make_task_sleep: This function (called by `msleep`) moves the task from the 'Running' state to a 'Sleeping' state and records the target wake-up time.
- sched_update_sleepers: During every timer interrupt on the BSP (Bootstrap Processor), the scheduler checks the list of sleeping tasks and moves those whose time has expired back to the 'Ready' queue.

## Technical Details

Global State:
- system_uptime_ms: A 64-bit counter representing milliseconds since boot. 
- g_lock_enabled: A global flag used to detect if the kernel has reached a state where spinlocks and multitasking are safe to use.

Polling Mechanism:
- The busy-wait loop uses the formula `(get_uptime_ms() - start) < ms`. This handles potential (though distant) 64-bit wrap-around scenarios correctly due to unsigned integer overflow properties.
- The `pause` instruction is utilized to hint to the CPU that a spin-loop is occurring, improving power efficiency and pipeline management on Intel/AMD processors.

## Future Improvements
- High-Resolution Timers: Implement microsecond-level delays using the CPU's TSC (Time Stamp Counter) or HPET (High Precision Event Timer).
- Per-CPU Uptime: Move uptime tracking to per-CPU structures to reduce cache contention on the global `system_uptime_ms` variable in high-core-count SMP systems.
- Dynamic Tick (Tickless): Implement a tickless kernel mode where the timer interrupt is only scheduled for the next known event, reducing power consumption and unnecessary context switches.
- User-space Syscall: Wrap `msleep` into a formal `sys_nanosleep` or `sys_sleep` system call for Ring 3 applications.
