#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

void init_serial();
int is_transmit_empty();
void write_serial(char a);
void kprint(const char* s);
void kprint_hex(uint64_t value);

#endif
