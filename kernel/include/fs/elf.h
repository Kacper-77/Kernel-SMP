#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define ELF_MAGIC 0x464C457F // "\x7FELF"

#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    Elf64_Addr e_entry;   
    Elf64_Off e_phoff;    
    Elf64_Off e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;     
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t p_type;      
    uint32_t p_flags;     
    Elf64_Off p_offset;   
    Elf64_Addr p_vaddr;   
    Elf64_Addr p_paddr;
    uint64_t p_filesz;    
    uint64_t p_memsz;     
    uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

typedef struct {
    uintptr_t entry;      
    uintptr_t pml4_phys;  
    uintptr_t stack_top;  
} elf_info_t;

uintptr_t elf_load(uintptr_t pml4_phys, void* elf_data);

#endif