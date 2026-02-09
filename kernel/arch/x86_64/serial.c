#include <serial.h>
#include <spinlock.h>
#include <io.h>

#define COM1 0x3f8

spinlock_t kprint_lock_ = { .ticket = 0, .current = 0, .last_cpu = -1 };

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

void kprint_raw(const char* s) {
    if (!s) return;
    while (*s) {
        if (*s == '\n') write_serial('\r');
        write_serial(*s++);
    }
}

void kprint_hex_raw(uint64_t value) {
    const char* hex_chars = "0123456789ABCDEF";
    write_serial('0');
    write_serial('x');
    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (value >> (i * 4)) & 0xF;
        write_serial(hex_chars[nibble]);
    }
}

void kprint(const char* s) {
    if (!s) return;
    
    uint64_t f = spin_irq_save();
    spin_lock(&kprint_lock_);

    kprint_raw(s);

    spin_unlock(&kprint_lock_);
    spin_irq_restore(f); 
}

void kprint_hex(uint64_t value) {
    uint64_t f = spin_irq_save();
    spin_lock(&kprint_lock_);

    kprint_hex_raw(value);

    spin_unlock(&kprint_lock_);
    spin_irq_restore(f);
}
