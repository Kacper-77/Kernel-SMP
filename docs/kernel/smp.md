# Symmetric Multiprocessing (SMP) & AP Bootstrapping

## Overview
The `smp` module is responsible for discovering and activating additional CPU cores, known as **Application Processors (APs)**. In x86_64, only the Bootstrap Processor (BSP) starts executing at boot; all other cores remain in a halted state until explicitly woken up using a specific inter-processor interrupt (IPI) sequence.

---

## Design Decisions

### 1. The Trampoline (Real Mode to Long Mode)
When an AP wakes up, it starts in **16-bit Real Mode**, just like an 8086 processor from 1978. To bring it into the modern 64-bit world, uses a "trampoline" code:
* **Relocation:** The trampoline is copied to a fixed physical address in low memory (`0x8000`) because 16-bit reset vectors have limited addressing range.
* **The Transition:** The code performs a high-speed "evolution":
    1. **16-bit Real Mode** -> Enables Protected Mode.
    2. **32-bit Protected Mode** -> Sets up basic paging and EFER MSR.
    3. **64-bit Long Mode** -> Loads the final GDT, switches to the kernel stack, and jumps to the C entry point (`kernel_main_ap`).



### 2. INIT-SIPI-SIPI Protocol
Follows the standard Intel Multiprocessor Specification to wake up cores:
1. **INIT IPI:** Resets the target AP.
2. **Startup IPI (SIPI) x2:** Sends the "vector" (address) where the AP should start executing (in our case, `0x08` which translates to `0x8000`).
3. **Synchronization:** The BSP waits for a `trampoline_ready` flag in the shared `ap_config_t` structure before moving on to the next CPU, ensuring no race conditions during boot.

### 3. ACPI MADT Parsing
Instead of hardcoding CPU counts, dynamically discovers cores using the **ACPI MADT (Multiple APIC Description Table)**:
* It scans the system for "Local APIC" entries.
* It filters out the BSP and disabled cores.
* This ensures the OS works on anything from a dual-core laptop to a 32-core server.



---

## Technical Details

### AP Configuration Block (`ap_config_t`)
To pass information from the high-level C code to the low-level assembly trampoline, we use a packed structure at a fixed location (`0x7000`):
```c
typedef struct {
    uint64_t trampoline_stack;   // The private kernel stack for this AP
    uint64_t trampoline_pml4;    // Physical address of the kernel page table
    uint64_t trampoline_entry;   // Address of kernel_main_ap()
    uint64_t cpu_context_p;      // Pointer to the cpu_context_t structure
    volatile uint64_t ready;     // Synchronization flag
} ap_config_t;
```

### CPU-Local Initialization
Once an AP reaches the `kernel_main_ap` entry point, it performs a local hardware setup to mirror the environment of the Bootstrap Processor (BSP):

* **GDT/IDT:** Every CPU is assigned its own **Task State Segment (TSS)**. This is critical for SMP stability, as it provides each core with a private, dedicated stack for handling interrupts and exceptions.
* **SSE/PAT:** Advanced CPU features, such as SIMD instructions (SSE/AVX) and the **Page Attribute Table (PAT)** for memory caching control, are enabled on a per-core basis.
* **Local APIC Timer:** Each core initializes its own high-resolution timer. This allows for independent, preemptive scheduling where one CPU can context-switch without being synchronized to a global clock.

### Future Improvements
To further mature the SMP capabilities of Kernel, the following features are on the roadmap:

* **IPI Messaging System:** Implementing a robust framework for **Inter-Processor Interrupts**. This will allow CPUs to send "messages" to each other, which is essential for operations like **TLB Shootdowns** (invalidating memory caches on all cores) or remote function execution.
* **CPU Hotplugging:** Adding support for dynamically adding or removing CPUs at runtime. This is increasingly important for system scalability in modern virtualized and cloud environments.
* **NUMA Awareness:** Optimizing the memory allocator to be aware of **Non-Uniform Memory Access** topologies. The kernel will attempt to allocate RAM from the node physically closest to the executing CPU, significantly reducing memory latency on multi-socket server systems.
