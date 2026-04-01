# Physical Memory Manager (PMM)

## Overview
The `pmm` module is responsible for managing the system's physical RAM at the lowest level. It treats physical memory as a series of **4KB frames** and uses a **Bitmap** to track the allocation status of every page in the system. This manager is essential for providing physical backing for virtual memory pages and kernel structures.

---

## Design Decisions

### 1. Bitmap-Based Tracking
Instead of using complex structures like Buddy Allocators in the early boot stage, uses a simple and predictable **Bitmap**.
* **Memory Overhead:** Each bit represents one 4KB frame. This results in a very low overhead (approximately 1 byte of bitmap for every 32KB of physical RAM).
* **Search Efficiency:** The allocator uses the `__builtin_ctz` (Count Trailing Zeros) instruction to find free bits (frames) within a byte or QWORD extremely quickly.

### 2. Multi-Pass EFI Initialization
Since the kernel starts in an environment provided by UEFI, the PMM must parse the **EFI Memory Map** to understand the hardware layout:
1. **Pass 1:** Determines the total amount of RAM to calculate the required bitmap size.
2. **Pass 2:** Finds a large enough contiguous free region to store the bitmap itself.
3. **Pass 3:** Marks usable RAM (Conventional Memory) as free, while ensuring that critical regions (Kernel code, UEFI data, and the Bitmap) are reserved.

### 3. SMP-Aware Allocation (Per-CPU Hints)
In a multicore environment, lock contention on a global bitmap can become a bottleneck. 
* **Last Index Hint:** Each CPU maintains a `pmm_last_index` hint in its local context. 
* **Performance:** When a CPU requests a frame, it starts searching from the last place it found a free bit, rather than the beginning of the bitmap. This spreads the allocation load across the bitmap and reduces the time spent holding the global spinlock.

### 4. Multiprocessor Safety
* **Spinlocks:** A global `pmm_lock_` protects the bitmap from race conditions.
* **IRQ Preservation:** All PMM operations are wrapped in `spin_irq_save/restore`. This is critical because a page fault occurring during an interrupt could otherwise lead to a deadlock if the PMM was already locked by the same CPU.

---

## Technical Details

### Frame Allocation Logic
The PMM provides two main allocation functions:
* `pmm_alloc_frame()`: Optimized for single-page requests (common for paging). It searches byte-by-byte using `__builtin_ctz`.
* `pmm_alloc_frames(count)`: Optimized for contiguous allocations (required for kernel stacks). It uses a 64-bit jump optimization, skipping entire QWORDs if they are fully occupied (`0xFFFFFFFFFFFFFFFF`).

### Higher Half Transition
During early boot, the bitmap is accessed via its physical address. After the Virtual Memory Manager (VMM) is initialized, `pmm_move_to_high_half()` is called to remap the bitmap pointer into the **Higher Half Direct Map (HHDM)**. This allows the PMM to continue functioning after the kernel switches to its final virtual address space.

---

## Future Improvements
* **Buddy Allocator:** Transitioning to a Buddy System for better handling of fragmented contiguous allocation requests.
* **NUMA Awareness:** Partitioning the bitmap based on physical memory nodes to improve performance on multi-socket systems.
* **Atomic Bitmaps:** Using atomic bitwise operations (Lock-Free) for single frame allocations to further reduce spinlock overhead.
