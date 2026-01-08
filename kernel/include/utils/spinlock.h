#ifndef SPINLOCK_H
#define SPINLOCK_H

extern int g_lock_enabled;

typedef struct {
    volatile int lock;
} __attribute__((aligned(16))) spinlock_t;  // Avoid "False Sharing"

static inline void spin_lock(spinlock_t* target) {
    if (!g_lock_enabled) return;

    while (__sync_lock_test_and_set(&(target->lock), 1)) {
        __asm__ volatile("pause");
    }
}

static inline void spin_unlock(spinlock_t* target) {
    if (!g_lock_enabled) return;
    __sync_lock_release(&(target->lock));
}

#endif
