#include <spinlock.h>
#include <apic.h>
#include <cpu.h>

void spin_lock(spinlock_t* lock) {
    if (!g_lock_enabled) return;

    // Get ticket by atomic increment
    uint32_t my_ticket = __atomic_fetch_add(&lock->ticket, 1, __ATOMIC_RELAXED);

    // Wait for current == ticket
    while (__atomic_load_n(&lock->current, __ATOMIC_ACQUIRE) != my_ticket) {
        __asm__ volatile("pause");
    }

    lock->last_cpu = get_current_cpu_id();
}

void spin_unlock(spinlock_t* lock) {
    if (!g_lock_enabled) return;

    lock->last_cpu = -1;

    uint32_t next = lock->current + 1;
    __atomic_store_n(&lock->current, next, __ATOMIC_RELEASE);
}

bool spin_trylock(spinlock_t* lock) {
    uint32_t current_ticket = __atomic_load_n(&lock->ticket, __ATOMIC_RELAXED);
    uint32_t next_ticket = current_ticket + 1;

    if (__atomic_load_n(&lock->current, __ATOMIC_ACQUIRE) != current_ticket) {
        return false;
    }

    if (__atomic_compare_exchange_n(&lock->ticket, &current_ticket, next_ticket, 
                                   false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        lock->last_cpu = get_cpu()->cpu_id;
        return true;
    }

    return false;
}
