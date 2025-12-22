#include <serial.h>

#define COM1 0x3f8

void init_serial() {
    outb(COM1 + 1, 0x00);    // Disable interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (38400 baud)
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them
}

int is_transmit_empty() {
    return inb(COM1 + 5) & 0x20;
}

void write_serial(char a) {
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)a), "Nd"((uint16_t)0xE9));
}

void kprint(const char* s) {
    while (*s) {
        if (*s == '\n') write_serial('\r');
        write_serial(*s++);
    }
}