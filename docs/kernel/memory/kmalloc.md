# Kernel Heap Allocator

## Overview
The `kmalloc` module implements a **Hybrid Dynamic Memory Allocator**. It provides efficient memory management by combining two distinct strategies: a **SLAB Allocator** for small, fixed-size objects and a **First-Fit Linked List** with coalescing for larger, variable-sized allocations.

---

## Design Decisions

### 1. Hybrid Allocation Strategy
To maximize performance and minimize internal fragmentation, the allocator routes requests based on size:
* **Small Requests (≤ 2048B):** Handled by the **SLAB Allocator**. This provides near O(1) allocation time and high cache efficiency for common kernel structures.
* **Large Requests (> 2048B):** Managed via a **First-Fit** algorithm on the main kernel heap.

### 2. First-Fit with Block Splitting
For large allocations, the kernel searches the doubly-linked list of blocks:
* **Splitting:** If a chosen free block is significantly larger than the requested size (at least `sizeof(m_header_t) + HEAP_MIN_BLOCK_SIZE` larger), it is split. This creates a new free block for future use and reduces wasted memory.
* **Alignment:** All allocations are aligned to 16-byte boundaries (`(size + 15) & ~15`) to satisfy CPU requirements and optimize memory access.

### 3. Immediate Coalescing
The deallocation process (`kfree`) includes logic to merge adjacent free blocks:
* **Forward Merge:** If the next block in memory is free, it is merged into the current block.
* **Backward Merge:** If the previous block is free, it is merged into the current block.
* This prevents **external fragmentation**, ensuring large contiguous regions of memory remain available.

### 4. Multiprocessor Safety
The allocator is designed for a multicore environment:
* **Spinlocks:** A global `heap_lock_` protects the metadata structures during modification.
* **Interrupt Safety:** Using `spin_irq_save()` and `spin_irq_restore()`, the allocator ensures that an interrupt handler cannot trigger a deadlock if it attempts to allocate memory while the current CPU already holds the lock.

---

## Technical Details

### Memory Block Header
Each heap allocation is preceded by a metadata header:
```c
typedef struct m_header {
uint32_t magic;        // Magic number for corruption detection
    size_t size;       // Usable size of the block (excluding header)
    int is_free;       // Status flag (1 if free, 0 if allocated)
    struct m_header* next; // Pointer to the next block in the list
    struct m_header* prev; // Pointer to the previous block in the list
} m_header_t;
```

### Dynamic Heap Expansion
The heap is not fixed at boot time. If the allocator cannot find a suitable free block:

* **Lock Release:** It releases the spinlock temporarily to allow other cores to interact with memory.
* **PMM Allocation:** It requests new physical frames from the **Physical Memory Manager (PMM)**.
* **Virtual Mapping:** It maps these frames to the kernel's virtual space and appends a new block to the heap chain.

### Corruption Detection
* **Magic Numbers:** Every header contains a magic value (`KMALLOC_MAGIC`). If `kfree` detects an incorrect magic number, it triggers a `kpanic`, as this indicates a buffer overflow has corrupted the heap metadata.
* **Double-Free Checks:** The kernel panics if `kfree` is called on a block that is already marked as free, preventing potential memory corruption.

### Future Improvements
* **Segregated Free Lists:** Grouping free blocks by size ranges to speed up the search process.
* **Per-CPU SLAB Caches:** Reducing global lock contention by giving each CPU its own local pool for small allocations.
* **Deferred Coalescing:** To make deallocation faster, coalescing could be performed periodically by a background kernel thread.
