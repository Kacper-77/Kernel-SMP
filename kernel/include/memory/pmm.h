#ifndef PMM_H
#define PMM_H

#include <boot_info.h>
#include <efi_descriptor.h>
#include <stdint.h>

#define PAGE_SIZE 4096

extern uint8_t* bitmap;
extern uint64_t bitmap_size;

void pmm_init(BootInfo* boot_info);
void* pmm_alloc_frame();
void* pmm_alloc_frames(size_t count);
void pmm_free_frame(void* frame);
void pmm_move_to_high_half();

#endif
