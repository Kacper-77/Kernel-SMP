#include <elf.h>
#include <vmm.h>
#include <pmm.h>
#include <cpu.h>
#include <std_funcs.h>
#include <kmalloc.h>
#include <serial.h>

//
// Helper to copy data from ELF buffer to the newly allocated physical frames
//
static void elf_copy_segment(page_table_t* pml4_virt, uintptr_t vaddr, void* src, size_t filesz) {
    size_t copied = 0;
    while (copied < filesz) {
        uintptr_t current_vaddr = vaddr + copied;
        uintptr_t offset_in_page = current_vaddr % PAGE_SIZE;
        size_t to_copy = PAGE_SIZE - offset_in_page;
        if (to_copy > (filesz - copied)) to_copy = filesz - copied;

        // Get the physical address of the page we just mapped
        uintptr_t dest_phys = vmm_virtual_to_physical(pml4_virt, current_vaddr);
        if (!dest_phys) return; 

        // Copy using the higher-half direct mapping (phys_to_virt)
        void* dest_virt = (void*)(phys_to_virt(dest_phys) + offset_in_page);
        memcpy(dest_virt, (uint8_t*)src + copied, to_copy);
        
        copied += to_copy;
        kprint("Copying segment to Phys: "); 
        kprint_hex(dest_phys + offset_in_page); 
        kprint("\n");
    }
}

uintptr_t elf_load(uintptr_t pml4_phys, void* elf_data) {
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_data;
    if (memcmp(header->e_ident, "\x7F" "ELF", 4) != 0) return 0;

    page_table_t* pml4_virt = (page_table_t*)phys_to_virt(pml4_phys);
    Elf64_Phdr* phdr = (Elf64_Phdr*)((uintptr_t)elf_data + header->e_phoff);

    disable_wp_cr0();

    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t vmm_flags = PTE_PRESENT | PTE_USER;
            if (phdr[i].p_flags & PF_W) vmm_flags |= PTE_WRITABLE;
            if (!(phdr[i].p_flags & PF_X)) vmm_flags |= PTE_NX;

            uintptr_t start_page = PAGE_ALIGN_DOWN(phdr[i].p_vaddr);
            uintptr_t end_page = PAGE_ALIGN_UP(phdr[i].p_vaddr + phdr[i].p_memsz);
            
            for (uintptr_t vpage = start_page; vpage < end_page; vpage += PAGE_SIZE) {
                uintptr_t phys = vmm_virtual_to_physical(pml4_virt, vpage);
                if (phys == 0) {
                    uintptr_t frame = (uintptr_t)pmm_alloc_frame();
                    vmm_map(pml4_virt, vpage, frame, vmm_flags);
                    memset((void*)phys_to_virt(frame), 0, PAGE_SIZE);
                } else {
                    vmm_map(pml4_virt, vpage, phys, vmm_flags); 
                }
            }

            if (phdr[i].p_filesz > 0) {
                elf_copy_segment(pml4_virt, phdr[i].p_vaddr, (void*)((uintptr_t)elf_data + phdr[i].p_offset), phdr[i].p_filesz);
            }
        }
    }

    enable_wp_cr0();

    return header->e_entry;
}