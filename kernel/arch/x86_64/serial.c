#include <serial.h>
#include <spinlock.h>
#include <io.h>

#define COM1 0x3f8

spinlock_t kprint_lock_ = { .lock = 0, .owner = -1, .recursion = 0 };

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
    while (is_transmit_empty() == 0);
    outb(COM1, a);
}

void kprint(const char* s) {
    if (!s) return;
    
    uint64_t f = save_interrupts_and_cli();
    spin_lock(&kprint_lock_);

    while (*s) {
        if (*s == '\n') write_serial('\r');
        write_serial(*s++);
    }

    spin_unlock(&kprint_lock_);
    restore_interrupts(f);
}

void kprint_hex(uint64_t value) {
    uint64_t f = save_interrupts_and_cli();
    spin_lock(&kprint_lock_);

    const char* hex_chars = "0123456789ABCDEF";
    write_serial('0');
    write_serial('x');

    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (value >> (i * 4)) & 0xF;
        write_serial(hex_chars[nibble]);
    }

    spin_unlock(&kprint_lock_);
    restore_interrupts(f);
}

void kprint_raw(const char* s) {
    if (!s) return;
    while (*s) {
        if (*s == '\n') write_serial('\r');
        write_serial(*s++);
    }
}
