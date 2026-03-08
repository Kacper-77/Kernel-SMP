#ifndef SLAB_H
#define SLAB_H

#include <spinlock.h>

#include <stddef.h>

#define SLAB_MAGIC 0x51AB51AB

typedef struct slab_obj {
    struct slab_obj* next;
} slab_obj_t;

typedef struct slab {
    uint32_t magic;
    struct slab_cache* parent_cache;
    slab_obj_t* free_list;      // List of free objects inside this page
    size_t free_count;          // How much free left
    struct slab* next;
    struct slab* prev;
    void* data;
} slab_t;

typedef struct slab_cache {
    const char* name;
    size_t obj_size;            
    slab_t* partial_slabs;      // Full and empty pages
    slab_t* full_slabs;         // Fully filled
    slab_t* empty_slabs;        // Fully empty
    spinlock_t lock;
} slab_cache_t;

void slab_init();
void* slab_alloc(size_t size);
void slab_free(void* ptr);
void slab_grow(slab_cache_t* cache);

void* slab_cache_alloc(slab_cache_t* cache);
void slab_cache_free(slab_cache_t* cache, void* ptr);

static inline slab_t* get_slab_from_ptr(void* ptr) {
    // Every slab starts at the beginning of page
    return (slab_t*)((uintptr_t)ptr & ~0xFFF);
}

#endif
