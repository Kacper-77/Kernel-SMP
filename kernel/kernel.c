#include <stdint.h>
#include <stddef.h>
#include "include/boot_info.h"

static inline uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b,
                                uint32_t rm, uint32_t gm, uint32_t bm) {
    int rs = __builtin_ctz(rm), gs = __builtin_ctz(gm), bs = __builtin_ctz(bm);
    return ((uint32_t)r << rs) | ((uint32_t)g << gs) | ((uint32_t)b << bs);
}

__attribute__((ms_abi, section(".text.entry")))
void kernel_main(BootInfo *info) {
    volatile uint32_t *fb = (volatile uint32_t*)(uintptr_t)info->fb.framebuffer_base;
    uint32_t w = info->fb.width, h = info->fb.height, stride = info->fb.pixels_per_scanline;

    uint32_t BLACK = pack_rgb(0,0,0, info->fb.red_mask, info->fb.green_mask, info->fb.blue_mask);
    uint32_t WHITE = pack_rgb(255,255,255, info->fb.red_mask, info->fb.green_mask, info->fb.blue_mask);

    for (uint32_t y = 0; y < h; ++y) {
        volatile uint32_t *row = fb + y * stride;
        for (uint32_t x = 0; x < w; ++x) row[x] = BLACK;
    }

    for (uint32_t y = 40; y < 120; ++y)
        for (uint32_t x = 40; x < 120; ++x)
            fb[y * stride + x] = WHITE;

    for (;;) { __asm__ __volatile__("hlt"); }
}
