#include <i8042.h>
#include <io.h>
#include <spinlock.h>
#include <serial.h>

static spinlock_t i8042_lock_ = { .ticket = 0, .current = 0, .last_cpu = -1 };

void i8042_wait_write() {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(PS2_STATUS_REG) & PS2_STATUS_INPUT_FULL)) return;
        __asm__ volatile("pause");
    }
}

void i8042_wait_read() {
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS_REG) & PS2_STATUS_OUTPUT_FULL) return;
        __asm__ volatile("pause");
    }
}

void i8042_write_command(uint8_t cmd) {
    uint64_t f = spin_irq_save();
    spin_lock(&i8042_lock_);
    
    i8042_wait_write();
    outb(PS2_COMMAND_REG, cmd);
    
    spin_unlock(&i8042_lock_);
    spin_irq_restore(f);
}

void i8042_write_data(uint8_t data) {
    uint64_t f = spin_irq_save();
    spin_lock(&i8042_lock_);

    i8042_wait_write();
    outb(PS2_DATA_PORT, data);

    spin_unlock(&i8042_lock_);
    spin_irq_restore(f);
}

uint8_t i8042_read_data() {
    uint64_t f = spin_irq_save();
    spin_lock(&i8042_lock_);
    
    i8042_wait_read();
    uint8_t data = inb(PS2_DATA_PORT);
    
    spin_unlock(&i8042_lock_);
    spin_irq_restore(f);
    return data;
}

void i8042_init() {
    // 1. Disable ports P1 & P2
    i8042_write_command(I8042_CMD_DISABLE_P1);
    i8042_write_command(I8042_CMD_DISABLE_P2);

    // 2. Flush output buffer
    while (inb(PS2_STATUS_REG) & PS2_STATUS_OUTPUT_FULL) {
        inb(PS2_DATA_PORT);
    }

    // 3. Self-test
    i8042_write_command(I8042_CMD_SELF_TEST);
    uint8_t res = i8042_read_data();
    if (res != 0x55) {
        kprint("i8042: Self-test FAILED!\n");  // TO DELETE IN FUTURE
        return;
    }

    // 4. Enable interrupts for P1 (Keyboard)
    // Read bits -> Modify -> Save
    i8042_write_command(I8042_CMD_READ_CONFIG);
    uint8_t config = i8042_read_data();
    config |=  (1 << 0); // Enable IRQ 1
    config &= ~(1 << 1); // Disable IRQ 12 (mouse) for now
    config &= ~(1 << 6); // Disable translation
    
    i8042_write_command(I8042_CMD_WRITE_CONFIG);
    i8042_write_data(config);

    // 5. Enable keyboard port
    i8042_write_command(I8042_CMD_ENABLE_P1);

    // 6. Tell the keyboard to start sending scancodes (Enable Scanning)
    i8042_wait_write();
    outb(PS2_DATA_PORT, 0xF4);

    // 7. Final Flush
    while (inb(PS2_STATUS_REG) & PS2_STATUS_OUTPUT_FULL) {
        inb(PS2_DATA_PORT);
    }
}
