# ELF Loader and TAR Ramdisk Implementation

## Overview
The `elf` module implements a **64-bit Executable and Linkable Format (ELF)** loader. It parses program headers to set up the process address space, integrates with the **Virtual Memory Area (VMA)** subsystem to manage memory permissions, and initializes the user-space heap. To facilitate testing and early boot, a minimal **TAR reader** is used as a read-only ramdisk provider.

---

## Design Decisions

### 1. Program Header Parsing (`PT_LOAD`)
The loader iterates through the ELF Program Header Table, specifically looking for `PT_LOAD` segments. 

* **Segment Mapping:** Each segment is registered as a unique **VMA**. This ensures that the kernel's memory manager is aware of which regions of the virtual address space are occupied by code, data, or BSS.
* **Permission Translation:** ELF flags (`PF_R`, `PF_W`, `PF_X`) are translated into VMA flags (`VMA_READ`, `VMA_WRITE`, `VMA_EXEC`). This allows the hardware to enforce **NX (No-Execute)** on data segments and **RO (Read-Only)** on code segments, preventing common security vulnerabilities.

### 2. Physical Memory Population
The `elf_copy_segment` function handles the critical task of filling the process's memory.

* **Virtual Context Isolation:** Since the loader may be running in a different address space than the target task, it uses `vmm_virtual_to_physical` to find the allocated frames and `phys_to_virt` to access them directly via the kernel's higher-half direct mapping.
* **Page Boundary Handling:** The copy logic correctly handles segments that do not start or end on page boundaries, ensuring no data corruption occurs during the transition from file to memory.



### 3. Automatic Heap Initialization
After loading all program segments, the loader identifies the highest virtual address used (`max_vaddr`) and initializes the process heap immediately after it.

* **Deterministic Layout:** This provides a consistent memory layout for the user-space `malloc` implementation, starting the heap at the first page-aligned address following the data segment.

### 4. USTAR Ramdisk (TAR)
Instead of a complex filesystem driver, uses a **USTAR (Uniform Standard TAR)** implementation for its initial ramdisk (InitRD).

* **Simplicity:** The `tar_lookup` function performs simple octal-to-integer conversion and string comparison, allowing the kernel to find executables and data files embedded in a TAR archive provided by the bootloader.

---

## Technical Details

### ELF Structure and Parsing
The loader operates directly on the `Elf64_Ehdr` (**Executable Header**) and `Elf64_Phdr` (**Program Header**) structures defined in the ELF64 specification.

**Header Verification:** Before parsing, the kernel validates the `e_ident` array:
* **Magic:** `0x7F 'E' 'L' 'F'`
* **Class:** Must be `ELFCLASS64` (2) for 64-bit support.
* **Data:** Must be `ELFDATA2LSB` (1) for Little-Endian.

**Segment Iteration:** The loader calculates the location of the Program Header Table using `header->e_phoff`. It then iterates `header->e_phnum` times, skipping non-loadable segments (e.g., `PT_NOTE` or `PT_INTERP`) to focus exclusively on `PT_LOAD`.

### Memory Population and BSS Logic
One of the most critical parts of the loader is handling the difference between `p_filesz` (size in the file) and `p_memsz` (size in memory):

* **Segment Loading:** The kernel only copies `p_filesz` bytes from the TAR archive.
* **BSS Zeroing:** If `p_memsz > p_filesz`, the remaining area (the **BSS section**) is automatically zero-initialized. Since `vma_map` requests fresh physical frames from the PMM, which are zeroed by default, this satisfies the C standard requirement for uninitialized global variables without extra overhead.
* **Alignment:** The loader respects the `p_align` field, ensuring that segments are mapped to page-aligned virtual addresses to satisfy hardware requirements for memory protection.

### TAR Implementation Details
The `tar_lookup` function implements a linear search over the USTAR archive:

* **Header Parsing:** Each file in the TAR archive starts with a 512-byte header.
* **Size Calculation:** File sizes in USTAR are stored as octal ASCII strings. The `octal_to_int` helper converts these to `uint64_t`.
* **Traversing:** The loader moves to the next file by skipping the header and the file data, aligned to the 512-byte block boundary:
    > `offset = 512 + ((size + 511) & ~511)`

### Execution Handoff
The `elf_load` function returns the `header->e_entry` virtual address. This value is used by the scheduler to:

1.  Set the initial `RIP` register of the new task.
2.  Initialize the user-space stack pointer (`RSP`) at the top of the mapped stack VMA.
3.  Prepare the `RFLAGS` register (enabling interrupts for user-space).

---

## Future Improvements
* **Dynamic Linking:** Implement a basic dynamic linker (RTLD) or a way to load an interpreter (like `ld-linux.so`) to support shared libraries.
* **Demand Paging:** Instead of loading the entire ELF file into memory at once, implement **"Lazy Loading"** by marking VMAs as `NOT_PRESENT` and loading pages only when a Page Fault (`#PF`) occurs.
* **Position Independent Executables (PIE):** Add support for relocating segments to enable **ASLR (Address Space Layout Randomization)**.
* **VFS Integration:** Replace the TAR implementation with a proper **Virtual File System (VFS)** layer and an Ext2 or FAT32 driver.
