#include <vma.h>
#include <sched.h>
#include <kmalloc.h>
#include <vmm.h>
#include <pmm.h>
#include <atomic.h>
#include <std_funcs.h>

/*
 * Internal Red/Black helpers
 */
static void vma_rotate_left(struct task* t, vma_area_t* x) {
    vma_area_t* y = x->right;
    x->right = y->left;
    
    if (y->left) y->left->parent = x;
    y->parent = x->parent;
    
    if (!x->parent) t->vma_tree_root = y;
    else if (x == x->parent->left) x->parent->left = y;
    else x->parent->right = y;
    
    y->left = x;
    x->parent = y;
}

static void vma_rotate_right(struct task* t, vma_area_t* y) {
    vma_area_t* x = y->left;
    y->left = x->right;
    
    if (x->right) x->right->parent = y;
    x->parent = y->parent;
    
    if (!y->parent) t->vma_tree_root = x;
    else if (y == y->parent->right) y->parent->right = x;
    else y->parent->left = x;
    
    x->right = y;
    y->parent = x;
}

static void vma_insert_fixup(struct task* t, vma_area_t* z) {
    while (z->parent && z->parent->color == VMA_RED) {
        if (z->parent == z->parent->parent->left) {
            vma_area_t* y = z->parent->parent->right;

            if (y && y->color == VMA_RED) {
                z->parent->color = VMA_BLACK;
                y->color = VMA_BLACK;
                z->parent->parent->color = VMA_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    vma_rotate_left(t, z);
                }
                z->parent->color = VMA_BLACK;
                z->parent->parent->color = VMA_RED;
                vma_rotate_right(t, z->parent->parent);
            }
        } else {
            // Right side
            vma_area_t* y = z->parent->parent->left;
            if (y && y->color == VMA_RED) {
                z->parent->color = VMA_BLACK;
                y->color = VMA_BLACK;
                z->parent->parent->color = VMA_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    vma_rotate_right(t, z);
                }
                z->parent->color = VMA_BLACK;
                z->parent->parent->color = VMA_RED;
                vma_rotate_left(t, z->parent->parent);
            }
        }
    }
    t->vma_tree_root->color = VMA_BLACK;
}

/*
 * Initializes VMA-related fields for a new task.
 * Called during task_alloc_base() form context.c.
 */
void vma_init_task(struct task* t) {
    t->vma_tree_root = NULL;
    t->vma_list_head = NULL;
    t->vma_count = 0;
    t->vma_lock  = (spinlock_t){ .ticket = 0, .current = 0, .last_cpu = -1 };
}

/*
 * Searches the RB-Tree for an area containing 'addr'.
 */
vma_area_t* vma_find(struct task* t, uintptr_t addr) {
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

/*
 * High-level mapping function:
 * 1. Attempts VMA Merging: If the new range is adjacent to an existing VMA with 
 * matching flags, it expands the existing one to save memory and tree nodes.
 * 2. Overlap Check: Ensures the requested virtual range is not already occupied.
 * 3. Allocation: If merging fails, allocates a new VMA descriptor via kmalloc.
 * 4. Physical Backing: Allocates physical frames via PMM and maps them into 
 * the task's specific PML4.
 * 5. Security: Zeroes out new memory to prevent information leakage.
 * 6. RB-Tree Integration: Inserts the new VMA into the balanced Red-Black Tree 
 * and performs a fixup to maintain O(log n) search complexity.
 */
int vma_map(struct task* t, uintptr_t addr, size_t size, uint32_t flags) {
    if (size == 0) return -1;
    
    // Page align size and address
    size = (size + 0xFFF) & ~0xFFFULL;
    if (addr & 0xFFF) return -1;

    uint64_t f = spin_irq_save();
    spin_lock(&t->vma_lock);

    // 1. VMA Merging (Expansion Logic)
    // Check if we can just expand an existing VMA (usually the heap)
    // This prevents fragmentation of VMA descriptors in the kernel heap.
    vma_area_t* curr = t->vma_list_head;
    while (curr) {
        if (curr->vm_end == addr && curr->vm_flags == flags) {
            // Found a neighbor with identical flags!
            uint64_t pages = size / 4096;
            void* phys = pmm_alloc_frames(pages);
            if (!phys) {
                spin_unlock(&t->vma_lock);
                spin_irq_restore(f);
                return -1;
            }

            // Map the new physical frames into the existing virtual range
            uint32_t vmm_opts = PTE_PRESENT | PTE_USER;
            if (flags & VMA_WRITE) vmm_opts |= PTE_WRITABLE;
            if (!(flags & VMA_EXEC)) vmm_opts |= PTE_NX;

            vmm_map_range(vmm_get_table(t->cr3), addr, (uintptr_t)phys, size, vmm_opts);
            memset((void*)phys_to_virt((uintptr_t)phys), 0, size);

            // Expand the boundary of the existing VMA
            curr->vm_end += size;

            spin_unlock(&t->vma_lock);
            spin_irq_restore(f);
            return 0; // Success via expansion, no new VMA descriptor created
        }
        curr = curr->next;
    }

    // 2. Overlap Check
    vma_area_t* check = t->vma_list_head;
    while (check) {
        if (addr < check->vm_end && (addr + size) > check->vm_start) {
            spin_unlock(&t->vma_lock);
            spin_irq_restore(f);
            return -2;
        }
        check = check->next;
    }

    // 3. Allocate New VMA descriptor
    vma_area_t* new_vma = kmalloc(sizeof(vma_area_t));
    if (!new_vma) {
        spin_unlock(&t->vma_lock);
        spin_irq_restore(f);
        return -3;
    }
    memset(new_vma, 0, sizeof(vma_area_t));

    new_vma->vm_start = addr;
    new_vma->vm_end   = addr + size;
    new_vma->vm_flags = flags;
    new_vma->color    = VMA_RED; // New nodes are always red

    // 4. Physical memory allocation for the new region
    uint64_t pages = size / 4096;
    void* phys = pmm_alloc_frames(pages);
    if (!phys) {
        kfree(new_vma);
        spin_unlock(&t->vma_lock);
        spin_irq_restore(f);
        return -4;
    }

    // 5. Update Page Tables
    uint32_t vmm_opts = PTE_PRESENT | PTE_USER;
    if (flags & VMA_WRITE) vmm_opts |= PTE_WRITABLE;
    if (!(flags & VMA_EXEC)) vmm_opts |= PTE_NX; 
    
    vmm_map_range(vmm_get_table(t->cr3), addr, (uintptr_t)phys, size, vmm_opts);
    memset((void*)phys_to_virt((uintptr_t)phys), 0, size);

    // 6. Linked List Insertion (Keep it for easy iteration/destruction)
    new_vma->next = t->vma_list_head;
    if (t->vma_list_head) t->vma_list_head->prev = new_vma;
    t->vma_list_head = new_vma;

    // 7. RB-Tree Insertion
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

    // 8. Rebalance the Tree
    vma_insert_fixup(t, new_vma);

    t->vma_count++;
    spin_unlock(&t->vma_lock);
    spin_irq_restore(f);
    return 0;
}

/*
 * Unmaps a specific range within a VMA, releases physical frames, 
 * and either trims the VMA or destroys its descriptor if fully unmapped.
 * Handles SMP synchronization and TLB invalidation.
 */
int vma_unmap(struct task* t, uintptr_t addr, size_t size) {
    if (size == 0) return 0;

    uint64_t f = spin_irq_save();
    spin_lock(&t->vma_lock);

    // 1. Locate the VMA area containing the start address
    vma_area_t* vma = vma_find(t, addr);
    if (!vma) {
        spin_unlock(&t->vma_lock);
        spin_irq_restore(f);
        return -1;
    }

    uintptr_t unmap_end = addr + size;
    page_table_t* pml4 = vmm_get_table(t->cr3);

    // 2. Iterate through the requested range and release resources page-by-page
    // We only unmap the 'size' requested, not necessarily the whole VMA
    for (uintptr_t vaddr = addr; vaddr < unmap_end; vaddr += PAGE_SIZE) {
        uintptr_t phys = vmm_virtual_to_physical(pml4, vaddr);
        if (phys) {
            // Remove entry from Page Tables (VMM)
            vmm_unmap(pml4, vaddr);
            // Return the physical frame to the PMM allocator
            pmm_free_frame((void*)phys);
        }
    }

    // 3. CASE A: Partial Unmap (Trimming the tail of the VMA)
    // Common for heap shrinkage: unmapping from some point to the very end
    if (addr > vma->vm_start && unmap_end >= vma->vm_end) {
        vma->vm_end = addr; // Just move the boundary
        goto finalize;
    }

    // 4. CASE B: Full Unmap (Removing the entire VMA descriptor)
    if (addr <= vma->vm_start && unmap_end >= vma->vm_end) {
        // Remove from the doubly-linked list
        if (vma->prev) vma->prev->next = vma->next;
        if (vma->next) vma->next->prev = vma->prev;
        if (t->vma_list_head == vma) {
            t->vma_list_head = vma->next;
        }

        // Remove from the Binary Search Tree
        vma_area_t* replacement = NULL;
        if (vma->left) {
            replacement = vma->left;
            if (vma->right) {
                vma_area_t* rightmost = vma->left;
                while (rightmost->right) rightmost = rightmost->right;
                rightmost->right = vma->right;
                vma->right->parent = rightmost;
            }
        } else {
            replacement = vma->right;
        }

        if (!vma->parent) {
            t->vma_tree_root = replacement;
        } else {
            if (vma->parent->left == vma) vma->parent->left = replacement;
            else vma->parent->right = replacement;
        }
        
        if (replacement) replacement->parent = vma->parent;

        t->vma_count--;
        
        // Free the VMA descriptor from the Kernel Heap
        kfree(vma);
    } 
finalize:
    spin_unlock(&t->vma_lock);
    spin_irq_restore(f);

    // 5. Invalidate TLB to ensure no CPU uses stale mappings
    sync_tlb(); 

    return 0;
}

/*
 * Iterates through the VMA list and releases EVERYTHING.
 * Crucial for the Reaper/task_exit to prevent memory leaks.
 */
void vma_destroy_all(struct task* t) {
    uint64_t f = spin_irq_save();
    spin_lock(&t->vma_lock);

    vma_area_t* curr = t->vma_list_head;

    while (curr) {
        vma_area_t* next = curr->next;
        kfree(curr);
        curr = next;
    }

    t->vma_list_head = NULL;
    t->vma_tree_root = NULL;
    t->vma_count = 0;

    if (t->cr3 != 0) {
        vmm_destroy_user_pml4(t->cr3, true);
        t->cr3 = 0;
    }
    
    spin_unlock(&t->vma_lock);
    spin_irq_restore(f);
}
