#include <vma.h>
#include <sched.h>
#include <kmalloc.h>
#include <vmm.h>
#include <pmm.h>
#include <spinlock.h>
#include <std_funcs.h>

//
// Initializes VMA-related fields for a new task.
// Called during task_alloc_base() form context.c.
//
void vma_init_task(task_t* t) {
    t->vma_tree_root = NULL;
    t->vma_list_head = NULL;
    t->vma_count = 0;
    t->vma_lock  = (spinlock_t){ .ticket = 0, .current = 0, .last_cpu = -1 };
}

//
// Searches the RB-Tree (currently BST) for an area containing 'addr'.
//
vma_area_t* vma_find(task_t* t, uintptr_t addr) {
    vma_area_t* curr = t->vma_tree_root;

    while (curr) {
        if (addr >= curr->vm_start && addr < curr->vm_end)
            return curr;
        
        if (addr < curr->vm_start)
            curr = curr->left;
        else
            curr = curr->right;
    }
    return NULL;
}

//
// High-level mapping function:
// 1. Checks for overlaps.
// 2. Allocates VMA descriptor via kmalloc.
// 3. Allocates physical frames via PMM.
// 4. Maps pages in the task's specific PML4.
//
int vma_map(task_t* t, uintptr_t addr, uint64_t size, uint32_t flags) {
    if (size == 0) return -1;
    
    // Page align size and address
    size = (size + 0xFFF) & ~0xFFFULL;
    if (addr & 0xFFF) return -1;

    spin_lock(&t->vma_lock);

    // 1. Simple overlap check using the linked list
    vma_area_t* check = t->vma_list_head;
    while (check) {
        if (addr < check->vm_end && (addr + size) > check->vm_start) {
            spin_unlock(&t->vma_lock);
            return -2; // Collision detected
        }
        check = check->next;
    }

    // 2. Allocate VMA descriptor in Kernel Heap
    vma_area_t* new_vma = kmalloc(sizeof(vma_area_t));
    if (!new_vma) {
        spin_unlock(&t->vma_lock);
        return -3;
    }
    memset(new_vma, 0, sizeof(vma_area_t));

    new_vma->vm_start = addr;
    new_vma->vm_end   = addr + size;
    new_vma->vm_flags = flags;

    // 3. Physical memory allocation
    uint64_t pages = size / 4096;
    void* phys = pmm_alloc_frames(pages);
    if (!phys) {
        kfree(new_vma);
        spin_unlock(&t->vma_lock);
        return -4;
    }

    // 4. Update Page Tables (VMM)
    uint32_t vmm_opts = PTE_PRESENT | PTE_USER;
    if (flags & VMA_WRITE) vmm_opts |= PTE_WRITABLE;
    
    // We map directly into the task's PML4
    vmm_map_range(vmm_get_table(t->cr3), addr, (uintptr_t)phys, size, vmm_opts);
    
    // Zero out the new memory for security (User shouldn't see kernel garbage)
    memset((void*)phys_to_virt((uintptr_t)phys), 0, size);

    // 5. Insert into the list
    new_vma->next = t->vma_list_head;
    if (t->vma_list_head) t->vma_list_head->prev = new_vma;
    t->vma_list_head = new_vma;

    // 6. Insert into Tree
    if (!t->vma_tree_root) {
        t->vma_tree_root = new_vma;
    } else {
        vma_area_t* node = t->vma_tree_root;
        while (1) {
            if (new_vma->vm_start < node->vm_start) {
                if (!node->left) { node->left = new_vma; new_vma->parent = node; break; }
                node = node->left;
            } else {
                if (!node->right) { node->right = new_vma; new_vma->parent = node; break; }
                node = node->right;
            }
        }
    }

    t->vma_count++;
    spin_unlock(&t->vma_lock);
    return 0;
}

//
// Unmaps an area, releases physical frames, and destroys the VMA descriptor.
//
int vma_unmap(task_t* t, uintptr_t addr) {
    spin_lock(&t->vma_lock);

    vma_area_t* vma = vma_find(t, addr);
    if (!vma || vma->vm_start != addr) {
        spin_unlock(&t->vma_lock);
        return -1;
    }

    uint64_t size = vma->vm_end - vma->vm_start;
    
    // 1. Get physical address from VMM to free it in PMM
    uintptr_t phys = vmm_get_phys(vmm_get_table(t->cr3), addr);
    
    // 2. Clear Page Tables
    vmm_unmap_range(vmm_get_table(t->cr3), addr, size);
    
    // 3. Free Physical Frames
    pmm_free_frames((void*)phys, size / 4096);

    // 4. Remove from List
    if (vma->prev) vma->prev->next = vma->next;
    if (vma->next) vma->next->prev = vma->prev;
    if (t->vma_list_head == vma) t->vma_list_head = vma->next;

    // 5. Remove from Tree
    if (vma == t->vma_tree_root) t->vma_tree_root = NULL;

    t->vma_count--;
    kfree(vma);

    spin_unlock(&t->vma_lock);
    return 0;
}

//
// Iterates through the VMA list and releases EVERYTHING.
// Crucial for the Reaper/task_exit to prevent memory leaks.
//
void vma_destroy_all(task_t* t) {
    spin_lock(&t->vma_lock);
    
    vma_area_t* curr = t->vma_list_head;
    while (curr) {
        vma_area_t* next = curr->next;
        
        uint64_t size = curr->vm_end - curr->vm_start;
        uintptr_t phys = vmm_get_phys(vmm_get_table(t->cr3), curr->vm_start);
        
        // Cleanup HW resources
        vmm_unmap_range(vmm_get_table(t->cr3), curr->vm_start, size);
        pmm_free_frames((void*)phys, size / 4096);
        
        // Free descriptor
        kfree(curr);
        curr = next;
    }

    t->vma_list_head = NULL;
    t->vma_tree_root = NULL;
    t->vma_count = 0;
    
    spin_unlock(&t->vma_lock);
}
