#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>
#include <stdint.h>

typedef struct m_header {
    uint32_t magic;
    size_t size;
    int is_free;
    struct m_header* next;
} m_header_t;

void kmalloc_init();
void* kmalloc(size_t size);
void kfree(void* ptr);

void kmalloc_dump();

#endif