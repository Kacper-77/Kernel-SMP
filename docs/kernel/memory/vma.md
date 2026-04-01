# Virtual Memory Area (VMA) Management

## Overview
The `vma` module is the high-level memory manager of the Kernel. While the VMM handles hardware page tables, the VMA subsystem tracks **why** and **how** a specific range of virtual addresses is being used by a process. It manages regions such as the stack, heap, code segments, and memory-mapped files.

---

## Design Decisions

### 1. Red-Black Tree for O(log n) Lookups
A process can have hundreds of individual memory mappings. To ensure that the kernel can quickly find which VMA an address belongs to (especially during a Page Fault), Kernel uses a **Balanced Red-Black Tree**.
* **Efficiency:** Unlike a simple linked list ($O(n)$), the RB-Tree guarantees $O(\log n)$ search time, ensuring consistent performance regardless of the number of mappings.
* **Self-Balancing:** The `vma_insert_fixup` logic performs rotations and color swaps to keep the tree height minimal.



### 2. VMA Merging (Expansion Logic)
To reduce metadata overhead and tree complexity, the `vma_map` function implements **VMA Merging**.
* **Adjacency Check:** If a new mapping request is physically adjacent to an existing VMA and shares the same protection flags (Read/Write/Exec), the kernel simply expands the existing VMA's boundary.
* **Benefits:** This prevents the "fragmentation of descriptors," particularly useful during consecutive heap expansions.

### 3. Protection and Security
Each VMA stores permission flags that are strictly enforced:
* **NX (No-Execute):** Data and stack regions are marked as non-executable to prevent stack-smashing attacks.
* **Information Leak Prevention:** Every new VMA allocation is automatically zeroed out by the kernel before being mapped to user-space, ensuring no "stale" data from other processes or the kernel is leaked.

### 4. SMP-Safe Resource Reclamation
* **Spinlocks:** Each task has its own `vma_lock`, allowing multiple CPUs to manage different processes' memory simultaneously without contention.
* **TLB Synchronization:** The `vma_unmap` function triggers a `sync_tlb()`, ensuring that after memory is freed, no CPU continues to use cached (and now invalid) translations.

---

## Technical Details

### VMA Descriptor Structure
The descriptor tracks both the tree hierarchy and a flat doubly-linked list for easy iteration during task destruction:
```c
typedef struct vma_area {
    uintptr_t vm_start;    // Starting virtual address
    uintptr_t vm_end;      // Ending virtual address (exclusive)
    uint32_t  vm_flags;    // Permissions (R, W, X, User)

    struct vma_area *left, *right, *parent; // RB-Tree nodes
    vma_node_color_t color;                 // RB-Tree color

    struct vma_area *next, *prev;           // Linked list pointers
} vma_area_t;
```

### Mapping Workflow
The process of mapping a new memory region follows a strict sequence to ensure system integrity:

* **Validation:** Addresses are strictly aligned to 4KB page boundaries to satisfy hardware requirements.
* **Merging:** The allocator first attempts to append the new range to an existing neighbor (VMA Merging). This minimizes the number of descriptors in the kernel heap.
* **Allocation:** If merging is not possible, a new VMA descriptor is allocated from the `kmalloc` heap.
* **Physical Backing:** The system requests the necessary number of physical frames from the **PMM**.
* **Page Table Update:** The **VMM** maps these physical frames into the task's unique PML4 table with the requested permissions.
* **Tree Insertion:** Finally, the node is placed in the Red-Black Tree, followed by a rebalancing fixup to maintain $O(\log n)$ efficiency.

### Future Improvements
To further enhance the memory subsystem, the following features are planned:

* **Demand Paging:** Instead of allocating physical frames immediately (`pmm_alloc_frames`), VMAs will be marked as "on-demand". Physical memory will be allocated only when a Page Fault (`#PF`) actually occurs, saving RAM for unused allocations.
* **Copy-on-Write (CoW):** This will allow the `fork()` system call to share physical frames between parent and child processes. A new frame is only allocated and copied when one of the processes attempts a write operation.
* **Shared Memory:** Implementing a mechanism to allow different VMAs in separate tasks to point to the same physical frames, enabling high-performance Inter-Process Communication (IPC).
