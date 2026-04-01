# SLAB Allocator Implementation

## Overview
The `slab` module provides a high-performance object caching layer for the kernel. By pre-allocating pages and dividing them into fixed-size "slots" (objects), it eliminates the overhead of searching for free space and prevents memory fragmentation for small, frequently used kernel structures.

---

## Design Decisions

### 1. Object Caching and Cache Tiers
Kernel maintains **8 predefined cache tiers** (from 16B to 2048B). 
* **Efficiency:** When the kernel requests memory via `kmalloc`, the request is routed to the smallest cache that can fit the object.
* **Speed:** Since all objects in a specific cache are of the same size, allocation is reduced to a simple linked-list pop operation.



### 2. Slab State Management (Full, Partial, Empty)
To optimize memory usage and allocation speed, each `slab_cache_t` organizes its pages (slabs) into three distinct doubly-linked lists:
* **Partial Slabs:** Slabs that have both allocated and free objects. These are the primary targets for new allocations.
* **Empty Slabs:** Slabs where all objects are free. These act as a reserve.
* **Full Slabs:** Slabs where every object is currently in use. They are moved back to the partial list only when an object is freed.



### 3. Page-Aligned Metadata
The metadata for each slab (`slab_t`) is stored at the very beginning of the 4KB physical frame.
* **Direct Access:** This allows the `slab_free` function to find the header for any given pointer using a simple bitmask: `(uintptr_t)ptr & ~0xFFF`. 
* **Zero Overhead:** No extra lookup tables or hashes are needed to find out which cache a pointer belongs to.

### 4. Slab Growth and Free List
When a cache runs out of free objects, `slab_grow` is called:
1. It requests a new frame from the **PMM**.
2. It carves the page into `max_objs` based on the cache's `obj_size`.
3. It links these objects together in a **Free List** located within the page itself.

---

## Technical Details

### Core Structures
```c
typedef struct slab {
    uint32_t magic;           // For safety checks
    struct slab_cache* parent_cache;
    slab_obj_t* free_list;    // Head of free objects in this page
    size_t free_count;        // Counter for state transitions
    struct slab* next;
    struct slab* prev;
} slab_t;
```

### Multiprocessor Safety
Each cache has its own independent `spinlock_t`, ensuring high concurrency:

* **Granular Locking:** Different CPUs can allocate objects from different caches (e.g., one from the 32B cache and another from the 128B cache) simultaneously without waiting for each other.
* **IRQ Context:** Like the PMM and Heap, the SLAB allocator is interrupt-safe. By using `spin_irq_save/restore`, it ensures system stability even during high-frequency kernel events or when allocations occur inside interrupt handlers.

### Future Improvements
* **Per-CPU Caches:** Implementing local object buffers for each CPU to eliminate lock contention entirely for the most common allocation paths.
* **Slab Reclamation:** A background "reaper" thread that monitors memory pressure and returns "Empty Slabs" to the PMM when system-wide memory is low.
* **Object Coloring:** Offsetting the start of objects within a page to improve CPU L1/L2 cache hit rates by preventing multiple slab objects from mapping to the same cache line (Cache Aliasing).
