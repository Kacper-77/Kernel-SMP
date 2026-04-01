#include <io.h>

static char* ksprint_int(char* buf, uint64_t value, int base, int width, char pad) {
    const char* digits = "0123456789abcdef";
    char tmp[65];
    int i = 0;

    do {
        tmp[i++] = digits[value % base];
        value /= base;
    } while (value > 0);

    // Padding
    while (i < width) {
        tmp[i++] = pad;
    }

    while (i > 0) {
        *buf++ = tmp[--i];
    }
    return buf;
}

int vsnprintf(char* buf, size_t n, const char* fmt, va_list args) {
    char* str = buf;
    char* end = buf + n;

    for (; *fmt && str < end - 1; fmt++) {
        if (*fmt != '%') {
            *str++ = *fmt;
            continue;
        }

        fmt++; // Skip '%'
        
        int width = 0;
        char pad = ' ';
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
            case 'c':
                *str++ = (char)va_arg(args, int);
                break;
            case 's': {
                char* s = va_arg(args, char*);
                if (!s) s = "(null)";
                while (*s && str < end - 1) *str++ = *s++;
                break;
            }
            case 'd': {
                int64_t d = va_arg(args, int64_t);
                if (d < 0) {
                    *str++ = '-';
                    d = -d;
                }
                str = ksprint_int(str, d, 10, width, pad);
                break;
            }
            case 'x':
                str = ksprint_int(str, va_arg(args, uint64_t), 16, width, pad);
                break;
            case 'p':
                if (str < end - 19) {
                    *str++ = '0'; *str++ = 'x';
                    str = ksprint_int(str, va_arg(args, uint64_t), 16, 16, '0');
                }
                break;
            case 'b':
                str = ksprint_int(str, va_arg(args, uint64_t), 2, width, pad);
                break;
            default:
                *str++ = *fmt;
                break;
        }
    }
    *str = '\0';
    return str - buf;
}
