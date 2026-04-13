#ifndef ATOMIC_H
#define ATOMIC_H

#include <stdint.h>
#include <stdbool.h>

extern int g_lock_enabled;

struct task;

typedef struct spinlock {
    volatile uint32_t ticket;
    volatile uint32_t current;
    int last_cpu;
} __attribute__((aligned(64))) spinlock_t;

typedef struct mutex {
    int          count;
    spinlock_t   wait_lock;
    struct task* wait_list;
    struct task* owner;
} mutex_t;

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

void mutex_lock(mutex_t* m);
void mutex_unlock(mutex_t* m);

#endif
