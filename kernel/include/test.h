#ifndef TEST_H
#define TEST_H

#include <boot_info.h>
#include <stdint.h>

extern BootInfo* g_bi;

static inline uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b,
                                uint32_t rm, uint32_t gm, uint32_t bm) {
    if (rm == 0 && gm == 0 && bm == 0) return (r << 16) | (g << 8) | b;
    int rs = __builtin_ctz(rm), gs = __builtin_ctz(gm), bs = __builtin_ctz(bm);
    return ((uint32_t)r << rs) | ((uint32_t)g << gs) | ((uint32_t)b << bs);
}

static inline void draw_test_squares_safe(uint64_t cpu_id, uint32_t* fb_base, uint32_t stride) {
    if (!fb_base) return;

    uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00};
    uint32_t color = colors[cpu_id % 4];

    uint32_t offset_y = 200; 
    uint32_t offset_x = 100 + (cpu_id * 100);

    for (uint32_t y = 0; y < 50; y++) {
        volatile uint32_t* row = (volatile uint32_t*)&fb_base[(offset_y + y) * stride + offset_x];
        for (uint32_t x = 0; x < 50; x++) {
            row[x] = color;
        }
    }
}

#endif
