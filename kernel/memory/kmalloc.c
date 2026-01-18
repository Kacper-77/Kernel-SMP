#include <kmalloc.h>
#include <pmm.h>
#include <vmm.h>
#include <spinlock.h>
#include <std_funcs.h>
#include <serial.h>

#define KMALLOC_MAGIC 0xCAFEBABE
#define HEAP_MIN_BLOCK_SIZE 16

static spinlock_t heap_lock_ = { .lock = 0, .owner = -1, .recursion = 0 };
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
    extern spinlock_t kprint_lock_;

    spin_lock(&kprint_lock_);
    spin_lock(&heap_lock_); 

    kprint("\n--- DUMP START ---\n");
    m_header_t* curr = heap_start;
    while (curr) {
        kprint("Block: "); kprint_hex((uintptr_t)curr);
        kprint(" | Size: "); kprint_hex(curr->size);
        kprint(" | Free: "); kprint(curr->is_free ? "YES" : "NO");
        kprint("\n");
        curr = curr->next;
    }
    kprint("--- DUMP END ---\n");

    spin_unlock(&heap_lock_);
    spin_unlock(&kprint_lock_);
}

//
// INIT
//
void kmalloc_init() {
    spin_lock(&heap_lock_);

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

    spin_unlock(&heap_lock_);
}

//
// KMALLOC AND KFREE
//
void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    size = align(size);
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

                current->size = size;
                current->next = next_block;
            }

            current->is_free = 0;

            void* ptr = (void*)((uintptr_t)current + sizeof(m_header_t));
            memset(ptr, 0, current->size);
            spin_unlock(&heap_lock_);
            return ptr;
        }
        last = current;
        current = current->next;
    }

    // 2. EXPANDING HEAP
    // Get new frame
    size_t required_with_header = size + sizeof(m_header_t);
    size_t num_frames = (required_with_header + 4095) / 4096;
    void* new_frames = pmm_alloc_frames(num_frames);

    uintptr_t virt_addr = phys_to_virt((uintptr_t)new_frames);
    memset((void*)virt_addr, 0, num_frames * 4096);

    m_header_t* new_block = (m_header_t*)virt_addr;
    new_block->magic = KMALLOC_MAGIC;
    new_block->size = (num_frames * 4096) - sizeof(m_header_t);
    new_block->is_free = 0;
    new_block->next = NULL;

    // Match size
    if (new_block->size >= size + sizeof(m_header_t) + HEAP_MIN_BLOCK_SIZE) {
        m_header_t* split_block = (m_header_t*)((uintptr_t)new_block + sizeof(m_header_t) + size);
        split_block->magic = KMALLOC_MAGIC;
        split_block->size = new_block->size - size - sizeof(m_header_t);
        split_block->is_free = 1;
        split_block->next = NULL;

        new_block->size = size;
        new_block->next = split_block;
    }

    if (last) { 
        last->next = new_block; 
    } else {
        heap_start = new_block;
    }

    spin_unlock(&heap_lock_);
    return (void*)((uintptr_t)new_block + sizeof(m_header_t));
}

void kfree(void* ptr) {
    if (!ptr) return;

    // Get header
    m_header_t* header = (m_header_t*)((uintptr_t)ptr - sizeof(m_header_t));

    // Verify magic number
    if (header->magic != KMALLOC_MAGIC) {
        kpanic("KMALLOC: Heap corruption detected (Invalid Magic Number)!");
    }

    spin_lock(&heap_lock_);
    
    // Set block as free
    header->is_free = 1;

    // COALESCING
    m_header_t* current = heap_start;
    while (current != NULL && current->next != NULL) {
        if (current->is_free && current->next->is_free) {
        uintptr_t end_of_current = (uintptr_t)current + sizeof(m_header_t) + current->size;
        if (end_of_current == (uintptr_t)current->next) {
            current->size += current->next->size + sizeof(m_header_t);
            current->next = current->next->next;
            continue;
        }
    }
        current = current->next;
    }

    spin_unlock(&heap_lock_);
}
