#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <apic.h>
#include <cpu.h>

#include <stdint.h>

extern int g_lock_enabled;

typedef struct {
    volatile uint32_t ticket;
    volatile uint32_t current;
    int last_cpu;
} __attribute__((aligned(64))) spinlock_t;

static inline uint64_t spin_irq_save() {
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0; cli" : "=rm"(rflags) :: "memory");
    return rflags;
}

static inline void spin_irq_restore(uint64_t rflags) {
    __asm__ volatile("push %0; popfq" :: "rm"(rflags) : "memory");
}

static inline void spin_lock(spinlock_t* lock) {
    if (!g_lock_enabled) return;

    // Get ticket by atomic increment
    uint32_t my_ticket = __atomic_fetch_add(&lock->ticket, 1, __ATOMIC_RELAXED);

    // Wait for current == ticket
    while (__atomic_load_n(&lock->current, __ATOMIC_ACQUIRE) != my_ticket) {
        __asm__ volatile("pause");
    }

    lock->last_cpu = get_current_cpu_id();
}

static inline void spin_unlock(spinlock_t* lock) {
    if (!g_lock_enabled) return;

    lock->last_cpu = -1;

    uint32_t next = lock->current + 1;
    __atomic_store_n(&lock->current, next, __ATOMIC_RELEASE);
}

#endif
