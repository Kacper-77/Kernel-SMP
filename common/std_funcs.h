#ifndef STD_FUNCS_H
#define STD_FUNCS_H

#include <stdint.h>
#include <stddef.h>

int memcmp(const void *s1, const void *s2, size_t n);
void* memset(void* dest, int ch, size_t count);
void* memcpy(void* dest, const void* src, size_t n);
char* strcpy(char* dest, const char* src);
int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
char* strncpy(char* dest, const char* src, size_t n);

#endif
