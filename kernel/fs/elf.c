#include <elf.h>
#include <elf_info.h>
#include <vmm.h>
#include <pmm.h>
#include <vma.h>
#include <cpu.h>
#include <std_funcs.h>
#include <kmalloc.h>
#include <serial.h>
#include <sched.h>

/*
 * Helper to copy data into the newly mapped VMA regions.
 * We use phys_to_virt to write directly to the physical frames.
 */
static void elf_copy_segment(task_t* t, uintptr_t vaddr, void* src, size_t filesz) {
    page_table_t* pml4_virt = vmm_get_table(t->cr3);
    size_t copied = 0;

    while (copied < filesz) {
        uintptr_t curr_vaddr = vaddr + copied;
        uintptr_t offset = curr_vaddr % PAGE_SIZE;
        size_t to_copy = PAGE_SIZE - offset;
        if (to_copy > (filesz - copied)) to_copy = filesz - copied;

        // Since vma_map already mapped these pages, we just need the phys address
        uintptr_t dest_phys = vmm_virtual_to_physical(pml4_virt, curr_vaddr);
        if (!dest_phys) return; 

        void* dest_virt = (void*)(phys_to_virt(dest_phys) + offset);
        memcpy(dest_virt, (uint8_t*)src + copied, to_copy);
        
        copied += to_copy;
    }
}

/*
 * Loads an ELF64 executable and registers segments as VMAs.
 */
uintptr_t elf_load(task_t* t, void* elf_data) {
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_data;

    if (memcmp(header->e_ident, "\x7F" "ELF", 4) != 0) return 0;

    Elf64_Phdr* phdr = (Elf64_Phdr*)((uintptr_t)elf_data + header->e_phoff);
    uintptr_t max_vaddr = 0;

    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            // 1. Convert ELF flags to VMA flags
            uint32_t vma_flags = VMA_USER;
            if (phdr[i].p_flags & PF_R) vma_flags |= VMA_READ;
            if (phdr[i].p_flags & PF_W) vma_flags |= VMA_WRITE;
            if (phdr[i].p_flags & PF_X) vma_flags |= VMA_EXEC;

            // 2. Use vma_map to reserve memory and register it in the task's tree
            // This also handles PMM allocation and VMM mapping internally.
            int res = vma_map(t, phdr[i].p_vaddr, phdr[i].p_memsz, vma_flags);
            if (res != 0) {
                kprint("[ELF] Failed to map segment at ");
                kprint_hex(phdr[i].p_vaddr);
                kprint("\n");
                return 0;
            }

            // 3. Copy data from the ELF file to the allocated memory
            if (phdr[i].p_filesz > 0) {
                elf_copy_segment(t, phdr[i].p_vaddr, 
                                 (void*)((uintptr_t)elf_data + phdr[i].p_offset), 
                                 phdr[i].p_filesz);
            }

            // Track the end of the program for heap initialization
            uintptr_t seg_end = phdr[i].p_vaddr + phdr[i].p_memsz;
            if (seg_end > max_vaddr) max_vaddr = seg_end;
        }
    }

    // Initialize heap pointers right after the last ELF segment
    t->heap_start = (max_vaddr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
    t->heap_curr  = t->heap_start;
    t->heap_end   = t->heap_start + 4 * PAGE_SIZE;

    vma_map(t, t->heap_start, 4 * PAGE_SIZE,
            VMA_READ | VMA_WRITE | VMA_USER | VMA_HEAP);

    return header->e_entry;
}
