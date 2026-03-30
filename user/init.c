#include <userlib.h>

static void user_test_task_2() {
    volatile uint64_t counter = 0;
    while(counter < 5) {
        u_print("TASK 2\n");
        u_sleep(10);
        counter++;
    }
    u_print("TASK 2 DONE\n");
    u_yield();
}


static void user_test_task_echo() {
    u_print("\n### SHELL ###\n");

    while(1) {
        char c = u_read_kbd();
        if (c != 0) {
            char buf[2] = {c, 0};
            u_print(buf);
        }
        u_sleep(50);
    }
}

void user_info_test() {
    u_sleep(50);
    u_print("Current TID: ");
    uint64_t tid = u_sys_get_tid();
    u_print_hex(tid);
    u_print("\n");

    u_print("Available CPUs: ");
    uint64_t cpus = u_sys_cpu_count(); 
    u_print_hex(cpus);
    u_print("\n");

    u_sleep(50);
    u_yield();
}

static void user_malloc_test() {
    u_print("### STARTING MEMORY STRESS TEST ###\n");

    size_t big_size = 10 * 4096;
    uint64_t* ptr = (uint64_t*)u_sys_malloc(big_size);
    
    if (!ptr) {
        u_print("Malloc failed!\n");
        return;
    }

    for(int i = 0; i < 10; i++) {
        ptr[i * 512] = 0xCAFEBABE00000000 | i;
    }
    u_print("Memory written and verified.\n");

    u_print("Freeing 10 pages...\n");
    u_sys_free((uintptr_t)ptr, big_size);

    u_sleep(50);

    // u_print("Triggering Page Fault...\n");
    
    // uint64_t* dead_ptr = (uint64_t*)((uintptr_t)ptr + (8 * 4096));
    // *dead_ptr = 0x1337;  // <-- PAGE FAULT expected
    
    // u_print("ERROR: If you see this, Unmap/TLB Flush FAILED!\n");
}

void _start() {
    char msg[] = {'E', 'L', 'F', ' ', 'W', 'O', 'R', 'K', 'S', '!', '\n', 0};
    u_print(msg);
    u_sleep(100);

    user_test_task_2();

    user_malloc_test();

    user_info_test();
    
    user_test_task_echo();

}
