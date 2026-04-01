# System Call Interface (Syscalls)

## Overview
The `syscall` module provides a secure and controlled gateway for user-space applications to request services from the kernel. By transitioning from **Ring 3** to **Ring 0**, can perform restricted operations like memory allocation, process management, and hardware I/O on behalf of the user.

---

## Design Decisions

### 1. The Fast Path (Syscall Entry)
Kenrel utilizes the x86_64 `syscall` and `sysret` architectural features for low-latency transitions.
* **Stack Switching:** Upon entry, the kernel uses the `swapgs` instruction to access CPU-local data and retrieve the kernel-mode stack pointer. This ensures that user-space cannot corrupt the kernel stack.
* **Context Preservation:** The `syscall_entry` assembly stub manually builds an `interrupt_frame_t`. This allows the kernel to treat system calls similarly to interrupts, simplifying the logic for context switching during calls like `sys_sleep` or `sys_exit`.


### 2. Security & Pointer Validation
A critical responsibility of the syscall layer is protecting the kernel from malicious or buggy user code.
* **Boundary Checks:** Functions like `is_user_range` verify that any pointer passed from user-space (e.g., a string for `kprint`) resides strictly within the user-land memory range (`< 0x00007FFFFFFFFFFF`).
* **Higher-Half Protection:** The kernel explicitly blocks any attempts to pass pointers pointing to the kernel's virtual address space (Higher Half), preventing "Confused Deputy" attacks where the kernel could be tricked into leaking its own data.

### 3. Dynamic Heap Management (brk/sbrk logic)
The `sys_malloc` and `sys_free` handlers implement a kernel-side backing for userspace allocators:
* **VMA Integration:** If the user-space heap reaches its limit, the kernel dynamically expands the process's address space using the **VMA (Virtual Memory Area)** subsystem.
* **Page-Aligned Allocation:** While the user-land allocator (like `malloc`) handles small bytes, the kernel syscall layer manages memory in 4KB page increments to ensure hardware-level protection.

---

## Technical Details

### The System Call Table (`sys_table`)
Uses a dispatch table (Jump Table) for $O(1)$ lookup speed. Each index corresponds to a specific service:
| ID | Call | Description |
|:---|:---|:---|
| `SYS_KPRINT` | `sys_kprint`   | Securely prints a user-space string to the serial console. 
| `SYS_MALLOC` | `sys_malloc`   | Expands the process heap via VMA mapping. 
| `SYS_SLEEP`  | `sys_sleep`    | Suspends the task and triggers the scheduler. 
| `SYS_KBD_PS2`| `sys_read_kbd` | Blocks the task until a key is available in the buffer. 

### Task Blocking & Yielding
Unlike simple functions, syscalls like `sys_read_kbd` can be **blocking**:
1. If no key is present, the kernel marks the task as `TASK_BLOCKED` with `REASON_KEYBOARD`.
2. The kernel then calls `sched_yield()`, allowing other tasks to run.
3. The task is only moved back to the `READY` state when the keyboard IRQ handler triggers a wakeup event.

---

## Future Improvements
* **Sysret Implementation:** Transitioning from `iretq` back to `sysret` in the assembly stub to further reduce the overhead of returning to user-space.
* **Signal System:** Implementing a way for the kernel to asynchronously notify user tasks (e.g., `SIGSEGV` on page faults).
* **Extended File I/O:** Adding `sys_open`, `sys_read`, and `sys_write` once the Virtual File System (VFS) is integrated.
