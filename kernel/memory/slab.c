#include <slab.h>
#include <pmm.h>
#include <vmm.h>
#include <panic.h>

#define NUM_KMALLOC_CACHES 8

static slab_cache_t kmalloc_caches[NUM_KMALLOC_CACHES];
static size_t cache_sizes[] = { 16, 32, 64, 128, 256, 512, 1024, 2048 };

/*
 * INTERNAL HELPERS
 */
static void slab_list_remove(slab_t** list, slab_t* slab) {
    if (slab->prev) slab->prev->next = slab->next;
    if (slab->next) slab->next->prev = slab->prev;
    if (*list == slab) *list = slab->next;
    slab->next = slab->prev = NULL;
}

static void slab_list_push(slab_t** list, slab_t* slab) {
    slab->next = *list;
    slab->prev = NULL;
    if (*list) (*list)->prev = slab;
    *list = slab;
}

/*
 * Allocates a new physical frame, sets up the slab header, 
 * and carves it into equal-sized objects linked in a free list.
 */
void slab_grow(slab_cache_t* cache) {
    void* frame = pmm_alloc_frame();
    uintptr_t virt = phys_to_virt((uintptr_t)frame);
    
    slab_t* slab = (slab_t*)virt;
    slab->magic = SLAB_MAGIC;
    slab->parent_cache = cache;
    
    uintptr_t start = virt + sizeof(slab_t);
    start = (start + 15) & ~15;  // Optional align

    slab->free_list = (slab_obj_t*)start;
    size_t max_objs = (virt + 4096 - start) / cache->obj_size;
    slab->free_count = max_objs;

    // Merge objects (Free List)
    slab_obj_t* curr = slab->free_list;
    for (size_t i = 0; i < max_objs - 1; i++) {
        curr->next = (slab_obj_t*)((uintptr_t)curr + cache->obj_size);
        curr = curr->next;
    }
    curr->next = NULL;

    slab_list_push(&cache->empty_slabs, slab);
}

void slab_init() {
    for (int i = 0; i < NUM_KMALLOC_CACHES; i++) {
        kmalloc_caches[i].name = "kmalloc_cache";
        kmalloc_caches[i].obj_size = cache_sizes[i];
        kmalloc_caches[i].partial_slabs = NULL;
        kmalloc_caches[i].full_slabs = NULL;
        kmalloc_caches[i].empty_slabs = NULL;
        kmalloc_caches[i].lock = (spinlock_t){ .ticket = 0, .current = 0, .last_cpu = -1 };
        
        // Optional: starting with ready to use objects
        slab_grow(&kmalloc_caches[i]);
    }
}

void* slab_alloc(size_t size) {
    // 1. Find cache with enough space
    for (int i = 0; i < NUM_KMALLOC_CACHES; i++) {
        if (size <= cache_sizes[i]) {
            return slab_cache_alloc(&kmalloc_caches[i]);
        }
    }
    return NULL;  // If to big, kmalloc will do it itself
}

/*
 * Uses page-alignment mask to locate the slab_t header 
 * at the beginning of the 4KB page.
 */
void slab_free(void* ptr) {
    if (!ptr) return;

    // Get slab header
    slab_t* slab = (slab_t*)((uintptr_t)ptr & ~0xFFF);

    // Security check
    if (slab->magic != SLAB_MAGIC) {
        kpanic("SLAB: Attempted to free a pointer that is not a valid slab object!");
    }

    slab_cache_free(slab->parent_cache, ptr);
}

void* slab_cache_alloc(slab_cache_t* cache) {
    uint64_t f = spin_irq_save();
    spin_lock(&cache->lock);

    // 1. Finding page with free space
    slab_t* slab = cache->partial_slabs;
    if (!slab) {
        slab = cache->empty_slabs;
        if (!slab) {
            slab_grow(cache);  // Adds new page: cache->empty_slabs
            slab = cache->empty_slabs;
        }
        // empty -> partial
        slab_list_remove(&cache->empty_slabs, slab);
        slab_list_push(&cache->partial_slabs, slab);
    }

    // 2. Get object
    slab_obj_t* obj = slab->free_list;
    slab->free_list = obj->next;
    slab->free_count--;

    // 3. If full, send to fulls slabs
    if (slab->free_count == 0) {
        slab_list_remove(&cache->partial_slabs, slab);
        slab_list_push(&cache->full_slabs, slab);
    }

    spin_unlock(&cache->lock);
    spin_irq_restore(f);

    return (void*)obj;
}

void slab_cache_free(slab_cache_t* cache, void* ptr) {
    // Get page with "to free" object 
    slab_t* slab = (slab_t*)((uintptr_t)ptr & ~0xFFF); // Align

    uint64_t f = spin_irq_save();
    spin_lock(&cache->lock);

    // Add object back to "free list" inside this page
    slab_obj_t* obj = (slab_obj_t*)ptr;
    obj->next = slab->free_list;
    slab->free_list = obj;
    slab->free_count++;

    // Move slab to appropriate list based on occupancy
    size_t max_objs = (4096 - sizeof(slab_t)) / cache->obj_size;

    if (slab->free_count == 1) {
        slab_list_remove(&cache->full_slabs, slab);
        slab_list_push(&cache->partial_slabs, slab);
    } 
    else if (slab->free_count == max_objs) {
        slab_list_remove(&cache->partial_slabs, slab);
        slab_list_push(&cache->empty_slabs, slab);
    }

    spin_unlock(&cache->lock);
    spin_irq_restore(f);
}
