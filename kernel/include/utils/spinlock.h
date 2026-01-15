#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <apic.h>
#include <panic.h>

extern int g_lock_enabled;

typedef struct {
    volatile int lock;
    volatile int owner;     // ID (-1 is none)
    volatile int recursion;
} __attribute__((aligned(64))) spinlock_t;

static inline void spin_lock(spinlock_t* target) {
    if (!g_lock_enabled) return;

    int cpu_id = get_current_cpu_id();

    if (target->owner == cpu_id) {
        target->recursion++;
        return;
    }

    while (__sync_lock_test_and_set(&(target->lock), 1)) {
        __asm__ volatile("pause");
    }

    __sync_synchronize();
    target->owner = cpu_id;
    target->recursion = 1;
}

static inline void spin_unlock(spinlock_t* target) {
    if (!g_lock_enabled) return;

    int cpu_id = get_current_cpu_id();
    
    if (target->owner != cpu_id) {
        panic("spin_unlock: CPU tried to unlock foreign spinlock");
    }

    if (target->recursion <= 0) {
        panic("spin_unlock: recursion underflow");
    }

    target->recursion--;

    if (target->recursion == 0) {
        target->owner = -1;
        __sync_synchronize();
        __sync_lock_release(&(target->lock));
    }
}

#endif
