# Virtual Memory Manager (VMM)

## Overview
The `vmm` module is the core of the Kernel memory management. It implements a **4-level Paging System** for the x86_64 architecture, providing isolation between processes and protecting the kernel from user-space interference. It handles the transition to the **Higher Half Kernel**, Direct Physical Mapping (HHDM), and hardware-accelerated memory caching via PAT.

---

## Design Decisions

### 1. 4-Level Paging (PML4)
The VMM manages the hierarchical structure of page tables:
* **PML4 (Page Map Level 4):** The root table, indexed by bits 39-47 of the virtual address.
* **PDPT, PD, PT:** Intermediate tables that eventually point to a 4KB physical frame.
* **On-Demand Table Allocation:** Intermediate tables are only created when a mapping is requested, saving significant memory for sparse address spaces.



### 2. Higher Half Kernel & HHDM
To simplify kernel development and hardware access, employs two specific mapping strategies:
* **Higher Half Kernel:** The kernel code and data are mapped to the top of the virtual address space (`0xFFFFFFFF80000000`). This keeps user-space (bottom half) separate and consistent.
* **HHDM (Higher Half Direct Map):** The entire physical RAM is mapped into the kernel's virtual space with a constant offset (`0xFFFF800000000000`). This allows the kernel to access any physical frame simply by adding the HHDM offset to its physical address.

### 3. Page Attribute Table (PAT) & MMIO
For high-performance hardware access, the VMM configures the **PAT MSR**:
* **Write-Combining (WC):** By configuring PAT entry 4, the kernel can map the Framebuffer in a way that allows the CPU to combine multiple small writes into a single burst. This significantly boosts graphics performance.
* **Device Mapping:** MMIO devices are mapped with **NX (No-Execute)** and **Cache Disable** (or WC) bits to ensure system security and correct hardware behavior.

### 4. SMP TLB Synchronization
In a multicore environment, when one CPU changes a page table, other CPUs might still have the old mapping in their **TLB (Translation Lookaside Buffer)**.
* **TLB Shootdown:** implements a synchronization mechanism where an IPI (Inter-Processor Interrupt) is broadcast to all cores to force a TLB flush (`invlpg` or CR3 reload) after critical mapping changes.
* **Spinlocks:** A global `vmm_lock_` ensures that only one CPU modifies the kernel page tables at a time.

---

## Technical Details

### Page Table Entry (PTE) Flags
The VMM utilizes hardware-defined bits to enforce security:
* `PTE_PRESENT`: Ensures the page is actually backed by RAM.
* `PTE_WRITABLE`: Controls read-only vs read-write access.
* `PTE_USER`: Prevents user-mode code from accessing kernel memory.
* `PTE_NX`: The "No-Execute" bit (bit 63) prevents code execution from data pages, mitigating buffer overflow exploits.

### Critical Boot Sequence (The CR3 Switch)
The `vmm_init` function performs a delicate "handover":
1. **Identity Mapping:** Maps the current execution range (0x0 -> 0x0) so the CPU doesn't crash immediately after enabling paging.
2. **Table Setup:** Allocates and fills the PML4 with Kernel, HHDM, and Framebuffer mappings.
3. **The Leap:** Loads the new PML4 into `CR3`, adjusts `RSP` and `RBP` to the Higher Half, and jumps to high-address code.
4. **Cleanup:** Disables the temporary identity mappings to protect the lower half for future user-space processes.

---

## Future Improvements
* **Huge Pages (2MB/1GB):** Implementing 2MB or 1GB pages for the HHDM and Kernel segments to reduce TLB pressure and improve performance.
* **ASLR (Address Space Layout Randomization):** Randomizing the kernel base address during boot to increase resistance against ROP-based attacks.
* **PCID (Process-Context Identifiers):** Utilizing the PCID feature of modern Intel/AMD CPUs to reduce the cost of TLB flushes during context switches.
