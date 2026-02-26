#include <serial.h>
#include <spinlock.h>
#include <io.h>

#define COM1 0x3f8
#define BUFFER_SIZE (128 * 1024)  // 128 KB

spinlock_t kprint_lock_ = { .ticket = 0, .current = 0, .last_cpu = -1 };

static char log_buffer[BUFFER_SIZE];
static uint32_t log_head = 0;
static uint32_t log_tail = 0;

// Puts char in buffer
static void log_putc(char c) {
    uint32_t next = (log_head + 1) % BUFFER_SIZE;
    
    if (next == log_tail) {
        log_tail = (log_tail + 1) % BUFFER_SIZE;
    }
    
    log_buffer[log_head] = c;
    log_head = next;
}

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

    while (*s) {
        if (*s == '\n') log_putc('\r');
        log_putc(*s++);
    }

    spin_unlock(&kprint_lock_);
    spin_irq_restore(f); 
}

void kprint_hex(uint64_t value) {
    uint64_t f = spin_irq_save();
    spin_lock(&kprint_lock_);

    const char* hex_chars = "0123456789ABCDEF";

    log_putc('0'); log_putc('x');
    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (value >> (i * 4)) & 0xF;
        log_putc(hex_chars[nibble]);
    }

    spin_unlock(&kprint_lock_);
    spin_irq_restore(f);
}

void log_flush() {
    if (log_head == log_tail) return;

    while (log_head != log_tail && (inb(COM1 + 5) & 0x20)) {
        outb(COM1, log_buffer[log_tail]);
        log_tail = (log_tail + 1) % BUFFER_SIZE;
    }
}
