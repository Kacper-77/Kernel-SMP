#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Gloabl uptime 
extern volatile uint64_t system_uptime_ms;

void msleep(uint64_t ms);

static inline uint64_t get_uptime_ms() {
    return system_uptime_ms;
}

#endif
