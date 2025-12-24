#ifndef PMM_H
#define PMM_H

#include <boot_info.h>
#include <stdint.h>

#define PAGE_SIZE 4096

void pmm_init(BootInfo* boot_info);
void* pmm_alloc_frame();
void pmm_free_frame(void* frame);

// Internal helper functions
void pmm_set_frame(uint64_t frame_addr);
void pmm_unset_frame(uint64_t frame_addr);
int pmm_is_frame_set(uint64_t frame_addr);

#endif
