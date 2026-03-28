#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <sched.h>

#define ELF_MAGIC 0x464C457F

#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4

uintptr_t elf_load(task_t* t, void* elf_data);

#endif
