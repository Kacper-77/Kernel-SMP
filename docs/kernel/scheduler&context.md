# SMP Multilevel Round-Robin Scheduler

## Overview
The `sched` module is the engine of the Kernel, responsible for multiplexing the CPU across multiple tasks. It is a **Symmetric Multiprocessing (SMP)** aware scheduler that supports multiple priority levels, task blocking/sleeping, and dynamic load balancing across all available CPU cores.

---

## Design Decisions

### 1. Multi-Level Feedback Queue (MLFQ) with Quanta
To balance between high-priority system tasks and background idle work, the scheduler uses four priority levels (High, Normal, Low, Idle).
* **Quanta System:** Each priority level is assigned a specific "quantum" (time slice ratio). High-priority tasks get more CPU time before the scheduler even considers checking lower-priority queues.
* **Fairness:** The `dequeue_task` logic ensures that lower-priority tasks aren't completely starved, but strictly prioritizes the execution of critical system services.



### 2. SMP Work Stealing (Load Balancing)
To prevent "hot cores" (one CPU being overloaded while others are idle), the scheduler implements **Work Stealing**:
* **Mechanism:** When a CPU's local runqueue is empty, it attempts to "steal" a migratable user task from another CPU's runqueue.
* **Lock-Free Approach:** It uses `spin_trylock` on other CPUs' runqueues to avoid deadlocks and minimize latency during the stealing process.

### 3. Asynchronous Task Cleanup (The Reaper)
Destroying a task (especially a user process with a complex VMA tree) is an expensive operation.
* **Zombie State:** When `task_exit()` is called, the task is marked as a `TASK_ZOMBIE` and moved to a global "dead list."
* **Kernel Reaper:** The actual memory deallocation (freeing stacks, destroying page tables) is offloaded to the **Idle Task** on CPU 0. This allows the dying task to be swapped out instantly without blocking the CPU for cleanup.

### 4. Sleep and Block Mechanisms
The scheduler supports efficient waiting without busy-looping:
* **Sleeping List:** Tasks calling `msleep` are placed in a sorted `sleeping_task_list`. The timer interrupt updates this list, waking tasks only when their `sleep_until` timestamp is reached.
* **Event Blocking:** Tasks can be blocked with a `task_reason_t` (e.g., waiting for Keyboard/I/O). They are moved to a blocked list and only re-enter the runqueue when a specific `sched_wakeup` event is triggered.

---

## Technical Details

### Task Control Block (TCB)
The `task_t` structure is the heart of every process. It is **aligned to 64 bytes** to prevent "false sharing" in CPU caches during SMP operations.
* **Context:** Stores `RSP`, `CR3`, and the kernel stack base.
* **Memory Map:** Contains the VMA tree root and spinlock for thread-safe memory operations.

### The Context Switch Flow
1. **Interrupt:** An IRQ (Timer or `int $32`) triggers the `schedule` function.
2. **State Save:** The CPU pushes the `interrupt_frame_t` onto the current task's stack.
3. **Queueing:** The current task's `RSP` is saved, and it's put back into the runqueue (if still ready).
4. **Selection:** A new task is picked from the local runqueue or stolen from another core.
5. **Environment Update:** The `TSS.rsp0` is updated to point to the new task's kernel stack (for the next interrupt).
6. **Address Space Switch:** If the new task has a different `CR3`, the MMU is updated.
7. **Restoration:** The function returns the new `RSP`, and the assembly stub performs an `iretq` into the new context.

---

## Future Improvements
* **Wait Queues:** Replacing the global blocked list with specific wait queues for every device to improve `sched_wakeup` performance.
* **Affinity Groups:** Allowing users to "pin" certain processes to specific cores for real-time performance.
* **Preemption:** Implementing full kernel preemption to allow high-priority tasks to interrupt lower-priority tasks even while they are executing in kernel-mode.
