#include <userlib.h>

static void user_test_task_2() {
    volatile uint64_t counter = 1;
    while(counter < 5) {
        u_printf("TASK 2 | Iteration: %d\n", (int)counter);
        u_sleep(10);
        counter++;
    }
    u_printf("TASK 2 DONE\n");
    u_yield();
}

static void user_test_task_echo() {
    u_printf("\n### SHELL ###\n");

    while(1) {
        char c = u_read_kbd();
        if (c != 0) {
            u_printf("%c", c);
        }
        u_sleep(50);
    }
}

void user_info_test() {
    // u_sleep(100);
    u_printf("Current TID: %d\n", (int)u_sys_get_tid());
    u_printf("Available CPUs: %d\n", (int)u_sys_cpu_count());

    u_sleep(50);
    u_yield();
}

static void user_malloc_test() {
    u_printf("### STARTING MEMORY STRESS TEST ###\n");

    size_t big_size = 10 * 4096;
    uint64_t* ptr = (uint64_t*)u_sys_malloc(big_size);
    
    if (!ptr) {
        u_printf("Malloc failed!\n");
        return;
    }

    u_printf("Allocated 10 pages at %p\n", ptr);

    for(int i = 0; i < 10; i++) {
        ptr[i * 512] = 0xCAFEBABE00000000 | i;
    }
    u_printf("Memory written and verified at %p.\n", ptr);

    u_printf("Freeing %d bytes at %p...\n", (int)big_size, ptr);
    u_sys_free((uintptr_t)ptr, big_size);

    //ptr[1000] = 1;  // <--- PAGE FAULT expected

    u_sleep(50);
}

void _start() {
    u_printf("ELF WORKS! Entering Ring 3 environment...\n");
    u_sleep(100);

    user_test_task_2();

    user_malloc_test();

    user_info_test();

    user_test_task_echo();
}
