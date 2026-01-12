#include <timer.h>

volatile uint64_t system_uptime_ms = 0;

void msleep(uint64_t ms) {
    if (ms == 0) return;
    uint64_t start = get_uptime_ms();
    while ((get_uptime_ms() - start) < ms) {
        __asm__ volatile("pause");
    }
}
