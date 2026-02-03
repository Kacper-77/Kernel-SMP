#include <timer.h>
#include <cpu.h>
#include <sched.h>

volatile uint64_t system_uptime_ms = 0;
// global spinlock flag
extern int g_lock_enabled;

//
// Blocks execution for a given number of milliseconds.
// Before the scheduler starts, it performs a busy-wait loop (polling).
// After activation, it puts the current task to sleep and yields the CPU.
//
void msleep(uint64_t ms) {
    task_t* current = get_cpu()->current_task;
    
    // Only before scheduler activation
    if (!current || !g_lock_enabled) {
        uint64_t start = get_uptime_ms();
        while ((get_uptime_ms() - start) < ms) __asm__ volatile("pause");
        return;
    }
    current->sleep_until = get_uptime_ms() + ms;
    current->state = TASK_SLEEPING;
    sched_yield(); 
}
