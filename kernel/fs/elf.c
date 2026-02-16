#include <elf.h>
#include <vmm.h>
#include <pmm.h>
#include <std_funcs.h>
#include <kmalloc.h>

// Helper to copy data from ELF buffer to the newly allocated physical frames
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
    }
}

elf_info_t elf_load(void* elf_data) {
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_data;
    elf_info_t info = {0};

    // 1. Basic ELF header validation
    if (memcmp(header->e_ident, "\x7F" "ELF", 4) != 0) return info;

    // 2. Create a clean address space for the user process
    uintptr_t user_pml4_phys = vmm_create_user_pml4();
    page_table_t* user_pml4_virt = (page_table_t*)phys_to_virt(user_pml4_phys);

    Elf64_Phdr* phdr = (Elf64_Phdr*)((uintptr_t)elf_data + header->e_phoff);

    // 3. Iterate through Program Headers (Segments)
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            // Determine page permissions
            uint64_t vmm_flags = PTE_PRESENT | PTE_USER;
            if (phdr[i].p_flags & PF_W) vmm_flags |= PTE_WRITABLE;
            if (!(phdr[i].p_flags & PF_X)) vmm_flags |= PTE_NX;

            // Align the segment to page boundaries
            uintptr_t start_page = PAGE_ALIGN_DOWN(phdr[i].p_vaddr);
            uintptr_t end_page = PAGE_ALIGN_UP(phdr[i].p_vaddr + phdr[i].p_memsz);
            uint64_t num_pages = (end_page - start_page) / PAGE_SIZE;

            // Map and allocate memory for each page in the segment
            for (uint64_t j = 0; j < num_pages; j++) {
                uintptr_t current_vpage = start_page + (j * PAGE_SIZE);
                
                uintptr_t phys = vmm_virtual_to_physical(user_pml4_virt, current_vpage);
                void* frame_virt;

                if (phys == 0) {
                    void* frame = pmm_alloc_frame();
                    vmm_map(user_pml4_virt, current_vpage, (uintptr_t)frame, vmm_flags);
                    frame_virt = (void*)phys_to_virt((uintptr_t)frame);
                    memset(frame_virt, 0, PAGE_SIZE);
                } else {
                    vmm_map(user_pml4_virt, current_vpage, phys, vmm_flags);
                }
            }
            
            // Copy the actual file data into the mapped pages
            if (phdr[i].p_filesz > 0) {
                void* src_data = (void*)((uintptr_t)elf_data + phdr[i].p_offset);
                elf_copy_segment(user_pml4_virt, phdr[i].p_vaddr, src_data, phdr[i].p_filesz);
            }
        }
    }

    // 4. Setup User Stack
    uintptr_t stack_top = 0x00007FFFF0004000; 
    size_t stack_size = 4 * PAGE_SIZE;
    uintptr_t stack_bottom = stack_top - stack_size;

    void* stack_frames = pmm_alloc_frames(4);
    vmm_map_range(user_pml4_virt, stack_bottom, (uintptr_t)stack_frames, stack_size,
                  PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NX);

    // 5. Fill return structure
    info.entry = header->e_entry;
    info.pml4_phys = user_pml4_phys;
    info.stack_top = stack_top; 
    
    return info;
}
