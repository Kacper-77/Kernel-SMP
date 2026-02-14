#ifndef TAR_H
#define TAR_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char name[100];     
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];     // (ASCII)
    char mtime[12];
    char chksum[8];
    char typeflag;      // '0' - for common file
    char linkname[100];
    char magic[6];      // "ustar"
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
} __attribute__((packed)) tar_header_t;

void tar_init(void* address, size_t size);
void* tar_lookup(const char* filename, size_t* out_size);

#endif