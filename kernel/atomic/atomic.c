#include <atomic.h>
#include <cpu.h>
#include <sched.h>
#include <sched_utils.h>

/*
 * SPINLOCK
 */
void spin_lock(spinlock_t* lock) {
    if (!g_lock_enabled) return;

    // Get ticket by atomic increment
    uint32_t my_ticket = __atomic_fetch_add(&lock->ticket, 1, __ATOMIC_RELAXED);

    // Wait for current == ticket
    while (__atomic_load_n(&lock->current, __ATOMIC_ACQUIRE) != my_ticket) {
        __asm__ volatile("pause");
    }

    lock->last_cpu = get_cpu()->cpu_id;
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

/*
 * MUTEX
 */
void mutex_lock(mutex_t* m) {
    if (!g_lock_enabled) return;

    task_t* current = sched_get_current();

    while (1) {
        uint64_t f = spin_irq_save();
        spin_lock(&m->wait_lock);

        if (m->count > 0) {
            m->count = 0;
            m->owner = current;

            spin_unlock(&m->wait_lock);
            spin_irq_restore(f);
            return;
        }

        // enqueue
        current->state = TASK_BLOCKED;
        current->wait_reason = REASON_MUTEX;

        current->sched_next = m->wait_list;
        m->wait_list = current;

        spin_unlock(&m->wait_lock);
        spin_irq_restore(f);

        sched_yield();
    }
}

void mutex_unlock(mutex_t* m) {
    if (!g_lock_enabled) return;

    uint64_t f = spin_irq_save();
    spin_lock(&m->wait_lock);

    m->owner = NULL;

    task_t* task_to_wake = NULL;

    if (m->wait_list) {
        // dequeue
        task_to_wake = m->wait_list;
        m->wait_list = task_to_wake->sched_next;
        task_to_wake->sched_next = NULL;
    } else {
        m->count = 1;
    }

    spin_unlock(&m->wait_lock);
    spin_irq_restore(f);

    if (task_to_wake) {
        task_to_wake->state = TASK_READY;
        cpu_context_t* target_cpu = get_cpu_by_id(task_to_wake->cpu_id);
        enqueue_task(target_cpu, task_to_wake);
    }
}
