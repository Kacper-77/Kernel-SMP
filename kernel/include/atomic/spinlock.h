#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include <stdbool.h>

extern int g_lock_enabled;

typedef struct spinlock {
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

void spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t* lock);
bool spin_trylock(spinlock_t* lock);

#endif
