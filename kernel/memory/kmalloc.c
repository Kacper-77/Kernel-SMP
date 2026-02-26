#include <kmalloc.h>
#include <pmm.h>
#include <vmm.h>
#include <spinlock.h>
#include <std_funcs.h>
#include <serial.h>

#define KMALLOC_MAGIC 0xCAFEBABE
#define HEAP_MIN_BLOCK_SIZE 16

static spinlock_t heap_lock_ = { .ticket = 0, .current = 0, .last_cpu = -1 };
static m_header_t* heap_start = NULL;

//
// Helper AND LOGS
//
static size_t align(size_t size) {
    return (size + 15) & ~15;
}

static void kpanic(const char* message) {
    kprint("\n!!! KERNEL PANIC !!!\n");
    kprint(message);
    kprint("\nSystem halted.");
    __asm__ volatile("cli");
    for (;;) { __asm__ volatile("hlt"); }
}

//
// DIAGNOSTIC
//
void kmalloc_dump() {
    uint64_t f = spin_irq_save();
    spin_lock(&heap_lock_); 

    kprint("\n--- DUMP START ---\n");
    m_header_t* curr = heap_start;
    while (curr) {
        kprint("Block: ");   kprint_hex((uintptr_t)curr);
        kprint(" | Size: "); kprint_hex(curr->size);
        kprint(" | Free: "); kprint(curr->is_free ? "YES" : "NO");
        kprint("\n");
        curr = curr->next;
    }
    kprint("--- DUMP END ---\n");

    spin_unlock(&heap_lock_);
    spin_irq_restore(f);
}

//
// Initializes the kernel heap by allocating the first physical frame.
// Sets up the initial free block header and protects the operation with a spinlock
// to ensure BSP/AP synchronization during early boot.
//
void kmalloc_init() {
    void* first_frame = pmm_alloc_frame();
    if (!first_frame) {
        kpanic("KMALLOC: Failed to allocate first frame for heap!");
        return;
    } 

    uintptr_t virt_addr = phys_to_virt((uintptr_t)first_frame);
    memset((void*)virt_addr, 0, 4096);

    heap_start = (m_header_t*)virt_addr;
    heap_start->magic = KMALLOC_MAGIC;
    heap_start->size = 4096 - sizeof(m_header_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;
}

//
// Allocates a block of kernel memory using the First Fit algorithm.
// If no suitable free block is found, it automatically expands the heap 
// by requesting new frames from the Physical Memory Manager (PMM).
// Includes block splitting logic to minimize internal fragmentation.
//
void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    size = align(size);
    uint64_t f = spin_irq_save();
    spin_lock(&heap_lock_);

    m_header_t* current = heap_start;
    m_header_t* last = NULL;

    // 1. FINDING FREE BLOCK (First Fit)
    while (current) {
        if (current->is_free && current->size >= size) {
            if (current->size >= size + sizeof(m_header_t) + HEAP_MIN_BLOCK_SIZE) {
                m_header_t* next_block = (m_header_t*)((uintptr_t)current + sizeof(m_header_t) + size);
                next_block->magic = KMALLOC_MAGIC;
                next_block->size = current->size - size - sizeof(m_header_t);

                next_block->is_free = 1;
                next_block->next = current->next;
                next_block->prev = current;
                if (next_block->next)
                    next_block->next->prev = next_block;

                current->size = size;
                current->next = next_block;
            }

            current->is_free = 0;

            void* ptr = (void*)((uintptr_t)current + sizeof(m_header_t));
            spin_unlock(&heap_lock_);
            spin_irq_restore(f);
            
            memset(ptr, 0, current->size);
            return ptr;
        }
        last = current;
        current = current->next;
    }

    // 2. EXPANDING HEAP
    spin_unlock(&heap_lock_);  // Unlock because PMM uses lock
    spin_irq_restore(f);

    // Get new frame
    size_t required_with_header = size + sizeof(m_header_t);
    size_t num_frames = (required_with_header + 4095) / 4096;
    void* new_frames = pmm_alloc_frames(num_frames);

    if (!new_frames) return NULL;

    uintptr_t virt_addr = phys_to_virt((uintptr_t)new_frames);
    memset((void*)virt_addr, 0, num_frames * 4096);

    f = spin_irq_save();
    spin_lock(&heap_lock_);

    m_header_t* new_block = (m_header_t*)virt_addr;
    new_block->magic = KMALLOC_MAGIC;
    new_block->size = (num_frames * 4096) - sizeof(m_header_t);
    new_block->is_free = 0;
    new_block->next = NULL;
    new_block->prev = last;

    // Match size
    if (new_block->size >= size + sizeof(m_header_t) + HEAP_MIN_BLOCK_SIZE) {
        m_header_t* split_block = (m_header_t*)((uintptr_t)new_block + sizeof(m_header_t) + size);
        split_block->magic = KMALLOC_MAGIC;
        split_block->size = new_block->size - size - sizeof(m_header_t);
        split_block->is_free = 1;
        split_block->next = NULL;

        new_block->size = size;
        new_block->next = split_block;
        split_block->prev = new_block;
    }

    if (last) { 
        last->next = new_block;
    } else {
        heap_start = new_block;
        heap_start->prev = NULL;
    }

    spin_unlock(&heap_lock_);
    spin_irq_restore(f);

    return (void*)((uintptr_t)new_block + sizeof(m_header_t));
}

//
// Frees a previously allocated memory block and performs immediate coalescing.
// Checks for heap corruption via magic numbers and guards against double-free.
// Adjacent free blocks are merged to maintain large contiguous memory regions.
//
void kfree(void* ptr) {
    if (!ptr) return;

    // 1. Get header
    m_header_t* header = (m_header_t*)((uintptr_t)ptr - sizeof(m_header_t));

    // 2. Verify magic number 
    if (header->magic != KMALLOC_MAGIC)
        kpanic("KMALLOC: Heap corruption detected (Invalid Magic Number)!");

    if (header->is_free)  // Additional protection
        kpanic("KMALLOC: Double free detected!");

    uint64_t f = spin_irq_save();
    spin_lock(&heap_lock_);
    
    // 3. Set block as free
    header->is_free = 1;

    // 4. COALESCING
    // Merge with next block
    if (header->next && header->next->is_free) {
        uintptr_t end = (uintptr_t)header + sizeof(m_header_t) + header->size;
        if (end == (uintptr_t)header->next) {
            header->size += header->next->size + sizeof(m_header_t);
            header->next = header->next->next;
            if (header->next)
                header->next->prev = header;
        }
    }

    // Merge with previous block
    if (header->prev && header->prev->is_free) {
        uintptr_t end = (uintptr_t)header->prev + sizeof(m_header_t) + header->prev->size;
        if (end == (uintptr_t)header) {
            header->prev->size += header->size + sizeof(m_header_t);
            header->prev->next = header->next;
            if (header->next)
                header->next->prev = header->prev;
            
            header = header->prev;
        }
    }

    spin_unlock(&heap_lock_);
    spin_irq_restore(f);
}
