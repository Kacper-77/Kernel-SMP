#ifndef GDT_H
#define GDT_H

#include <stdint.h>

//
// Segment selectors offsets in our GDT.
// These are used to load segment registers (CS, DS, etc.)
//
#define KERNEL_CODE_SEL 0x08
#define KERNEL_DATA_SEL 0x10

//
// Initializes the Global Descriptor Table.
// Sets up Null, Kernel Code, and Kernel Data segments,
// then flushes the segment registers.
//
void gdt_init(void);

#endif