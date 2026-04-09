#include <atomic.h>
#include <cpu.h>
#include <sched.h>
#include <sched_utils.h>

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
void mutex_lock(mutex_t* m) {
    if (atomic_dec_return(&m->count) == 0) {
        m->owner = sched_get_current();
        return;
    }

    task_t* current = sched_get_current();
    uint64_t f = spin_irq_save();
    spin_lock(&m->wait_lock);

    current->state = TASK_BLOCKED;
    current->wait_reason = REASON_MUTEX;
    
    current->sched_next = m->wait_list;
    m->wait_list = current;

    spin_unlock(&m->wait_lock);
    
    sched_yield(); 

    spin_irq_restore(f);
}

void mutex_unlock(mutex_t* m) {
    m->owner = NULL;

    if (atomic_inc_return(&m->count) <= 0) {
        uint64_t f = spin_irq_save();
        spin_lock(&m->wait_lock);

        if (m->wait_list) {
            task_t* t = m->wait_list;
            m->wait_list = t->sched_next;
            
            t->state = TASK_READY;
            t->sched_next = NULL;
            
            cpu_context_t* target_cpu = get_cpu_by_id(t->cpu_id);
            enqueue_task(target_cpu ? target_cpu : get_cpu(), t);
        }

        spin_unlock(&m->wait_lock);
        spin_irq_restore(f);
    }
}
