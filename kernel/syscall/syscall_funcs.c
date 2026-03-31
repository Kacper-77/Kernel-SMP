#include <syscall.h>
#include <sched.h>
#include <timer.h>
#include <serial.h>
#include <ps2_kbd.h>
#include <cpu.h>
#include <kmalloc.h>
#include <smp.h>
#include <vmm.h>
#include <pmm.h>
#include <vma.h>
#include <std_funcs.h>

#include <stdint.h>
#include <stddef.h>

#define KERNEL_SPACE_START 0xFFFF800000000000
#define USER_SPACE_END     0x00007FFFFFFFFFFF

/*
 * HELPERS
 */
static bool is_user_range(const void* addr, size_t size) {
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + size;

    if (start > USER_SPACE_END || end > USER_SPACE_END || end < start) {
        return false;
    }
    return true;
}

static uint64_t sys_kprint(const char* str) {
    if (!str) return -1;

    if ((uintptr_t)str >= KERNEL_SPACE_START) {
        kprintf("\n[SECURITY] Blocked KPRINT: Pointer %p is in Higher Half!\n", str); 
        return -1;
    }

    size_t len = 0;
    while (str[len] != '\0') {
        len++;
        if (len > 4096) return -2; 
    }

    if (!is_user_range(str, len + 1)) {
        kprintf("\n[SECURITY] Blocked KPRINT: String at %p crosses kernel boundary!\n", str);
        return -1;
    }

    kprint(str);
    return 0;
}

static uint64_t sys_kprint_hex(uint64_t value) {
    kprintf("%p\n", value);
    return 0;
}

/*
 * FUNCS WRAPPERS
 */
uint64_t sys_kprint_handler(interrupt_frame_t* frame) {
    return sys_kprint((const char*)frame->rdi);
}

uint64_t sys_kprint_hex_handler(interrupt_frame_t* frame) {
    return sys_kprint_hex((uint64_t)frame->rdi);
}

uint64_t sys_get_cpuid_handler(interrupt_frame_t* frame) {
    (void)frame;
    return get_cpu()->cpu_id;
}

uint64_t sys_exit_handler(interrupt_frame_t* frame) {
    task_exit();
    return (uint64_t)schedule(frame);
}

uint64_t sys_yield_handler(interrupt_frame_t* frame) {
    (void)frame;
    sched_yield();
    return 0;
}

uint64_t sys_get_uptime_handler(interrupt_frame_t* frame) {
    (void)frame;
    return get_uptime_ms();
}

uint64_t sys_sleep_handler(interrupt_frame_t* frame) {
    uint64_t ms = frame->rdi;
    if (ms > 0) {
        sched_make_task_sleep(ms);
    }
    return (uint64_t)schedule(frame);
}

uint64_t sys_read_kbd_handler(interrupt_frame_t* frame) {
    (void)frame;
    char c;

    while (!kbd_pop_char(&c)) {
        sched_block_current(REASON_KEYBOARD); 
        sched_yield();
    }
    return (uint64_t)c;
}

uint64_t sys_malloc_handler(interrupt_frame_t* frame) {
    size_t size = frame->rdi;
    if (size == 0) return 0;
    
    task_t* current = sched_get_current();

    size_t aligned_size = (size + 0x0F) & ~0X0FULL;

    if (current->heap_curr + aligned_size <= current->heap_end) {
        uintptr_t allocated_addr = current->heap_curr;
        current->heap_curr += aligned_size;

        return allocated_addr;
    }

    size_t needed = (current->heap_curr + size) - current->heap_end;
    size_t aligned_needed = (needed + 0xFFF) & ~0xFFFULL;
    
    int res = vma_map(current, current->heap_end, aligned_needed, 
                      VMA_READ | VMA_WRITE | VMA_USER | VMA_HEAP);
    
    if (res != 0) return 0;

    current->heap_end += aligned_needed;
    
    uintptr_t addr = current->heap_curr;
    current->heap_curr += aligned_size;
    
    return (uint64_t)addr;
}

uint64_t sys_free_handler(interrupt_frame_t* frame) {
    uintptr_t addr = (uintptr_t)frame->rdi;
    size_t size = (size_t)frame->rsi;

    if (addr == 0 || size == 0) return 0;

    task_t* current = sched_get_current();
    size_t aligned_size = (size + 0x0F) & ~0x0FULL;

    if (addr + aligned_size == current->heap_curr) {
        current->heap_curr = addr;
    } else {
        // For now, shrink only the 'logical' heap if the top is freed.
        // Middle holes are managed by the userspace allocator.
        return 0; 
    }

    size_t gap = current->heap_end - current->heap_curr;
    if (gap >= (4 * PAGE_SIZE)) {
        // Keep 1 page as a buffer to prevent "thrashing"
        size_t to_release = (gap - PAGE_SIZE) & ~0xFFFULL;

        uintptr_t unmap_start = current->heap_end - to_release;
        if (unmap_start < current->heap_curr) 
            unmap_start = (current->heap_curr + 0xFFF) & ~0xFFFULL;

        if (vma_unmap(current, unmap_start, to_release) == 0)
            current->heap_end = unmap_start;
    }
    return 0;
}

uint64_t sys_get_tid_handler(interrupt_frame_t* frame) {
    (void)frame;
    return (uint64_t)sched_get_current()->tid;
}

uint64_t sys_cpu_count_handler(interrupt_frame_t* frame) {
    (void)frame;
    return (uint64_t)get_cpu_count_test();
}

/*
 * INIT SYS TABLE
 */
void init_sys_table() {
    sys_table[SYS_KPRINT]     = sys_kprint_handler;
    sys_table[SYS_EXIT]       = sys_exit_handler;
    sys_table[SYS_YIELD]      = sys_yield_handler;
    sys_table[SYS_GET_UPTIME] = sys_get_uptime_handler;
    sys_table[SYS_SLEEP]      = sys_sleep_handler;
    sys_table[SYS_KBD_PS2]    = sys_read_kbd_handler;
    sys_table[SYS_KPRINT_HEX] = sys_kprint_hex_handler;
    sys_table[SYS_CPU_ID]     = sys_get_cpuid_handler;
    sys_table[SYS_MALLOC]     = sys_malloc_handler;
    sys_table[SYS_FREE]       = sys_free_handler;
    sys_table[SYS_GET_TID]    = sys_get_tid_handler;
    sys_table[SYS_CPU_COUNT]  = sys_cpu_count_handler;
}
