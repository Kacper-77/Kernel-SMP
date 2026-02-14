#include <tar.h>
#include <std_funcs.h>

#include <stdint.h>

static void* tar_base = NULL;
static size_t tar_limit = 0;

void tar_init(void* address, size_t size) {
    tar_base = address;
    tar_limit = size;
}

static uint64_t octal_to_int(const char *s, int size) {
    uint64_t res = 0;
    for (int i = 0; i < size && s[i] >= '0' && s[i] <= '7'; i++) {
        res = res * 8 + (s[i] - '0');
    }
    return res;
}

void* tar_lookup(const char* filename, size_t* out_size) {
    tar_header_t* header = (tar_header_t*)tar_base;
    uintptr_t end = (uintptr_t)tar_base + tar_limit;

    while ((uintptr_t)header < end && header->name[0] != '\0') {
        if (memcmp(header->magic, "ustar", 5) == 0) {
            uint64_t size = octal_to_int(header->size, 12);
            
            if (strcmp(header->name, filename) == 0) {
                *out_size = size;
                return (void*)((uintptr_t)header + 512);
            }

            uintptr_t offset = 512 + ((size + 511) & ~511);
            header = (tar_header_t*)((uintptr_t)header + offset);
        } else {
            break;
        }
    }
    return NULL;
}
