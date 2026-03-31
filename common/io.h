#ifndef IO_H
#define IO_H

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

int vsnprintf(char* buf, size_t n, const char* fmt, va_list args);

#endif
