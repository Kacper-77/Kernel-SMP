#ifndef VMA_H
#define VMA_H

#include <stdint.h>

struct task;

/* VMA flags */
#define VMA_READ    (1 << 0)
#define VMA_WRITE   (1 << 1)
#define VMA_EXEC    (1 << 2)
#define VMA_USER    (1 << 3)
#define VMA_STACK   (1 << 4) 
#define VMA_HEAP    (1 << 5)

/* Colors */
typedef enum {
    VMA_RED = 0,
    VMA_BLACK = 1
} vma_node_color_t;

/* Main structure of Red/Black VMA area */
typedef struct vma_area {
    uintptr_t vm_start;    // Start (Aligned)
    uintptr_t vm_end;      // End (Exclusive)
    uint32_t  vm_flags;    

    // RB-Tree O(log n)
    struct vma_area *left;
    struct vma_area *right;
    struct vma_area *parent;
    vma_node_color_t color;

    struct vma_area *next;
    struct vma_area *prev;
} vma_area_t;

void vma_init_task(struct task* t);
vma_area_t* vma_find(struct task* t, uintptr_t addr);
int vma_map(struct task* t, uintptr_t addr, uint64_t size, uint32_t flags);
int vma_unmap(struct task* t, uintptr_t addr);
void vma_destroy_all(struct task* t);

#endif
