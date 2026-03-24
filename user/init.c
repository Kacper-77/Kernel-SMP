#include <userlib.h>

static void user_test_task_2() {
    char msg[] = {'T', 'a', 's', 'k', ' ', '2', '\n', 0};
    char msg2[] = {'D', 'o', 'n', 'e', '\n', 0};
    
    volatile uint64_t counter = 0;
    while(counter < 5) {
        u_print(msg);
        u_sleep(10);
        counter++;
    }
    u_print(msg2);
    u_yield();
}


static void user_test_task_echo() {
    char msg[] = {'S','H','E','L','L','\n', 0};
    u_print(msg);

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
    char m_tid[]  = {'C', 'u', 'r', 'r', 'e', 'n', 't', ' ', 'T', 'I', 'D', ':', ' ', 0};
    char m_cpus[] = {'A', 'v', 'a', 'i', 'l', 'a', 'b', 'l', 'e', ' ', 'C', 'P', 'U', 's', ':', ' ', 0};
    char nl[]     = {'\n', 0};

    u_print(m_tid);
    uint64_t tid = u_sys_get_tid();
    u_print_hex(tid);
    u_print(nl);

    u_print(m_cpus);
    uint64_t cpus = u_sys_cpu_count(); 
    u_print_hex(cpus);
    u_print(nl);

    u_sleep(50);
    u_yield();
}

static void user_malloc_test() {
    char m_start[] = {'T', 'e', 's', 't', 'i', 'n', 'g', ' ', 'M', 'a', 'l', 'l', 'o', 'c', '.', '.', '.', '\n', 0};
    char m_fail[]  = {'M', 'a', 'l', 'l', 'o', 'c', ' ', 'F', 'A', 'I', 'L', 'E', 'D', '!', '\n', 0};
    char m_ok[]    = {'M', 'a', 'l', 'l', 'o', 'c', ' ', 'O', 'K', ',', ' ', 'A', 'd', 'r', ':', ' ', 0};
    char m_succ[]  = {'W', 'r', 'i', 't', 'e', '/', 'R', 'e', 'a', 'd', ':', ' ', 'S', 'U', 'C', 'C', 'E', 'S', 'S', '\n', 0};
    char nl[]      = {'\n', 0};

    u_print(m_start);
    
    uint64_t* ptr = (uint64_t*)u_sys_malloc(8192);
    
    if (ptr == 0) {
        u_print(m_fail);
        u_exit();
        return;
    }

    u_print(m_ok);
    u_print_hex((uintptr_t)ptr);
    u_print(nl);

    ptr[0] = 0xDEADC0DECAFEBABE;
    ptr[1023] = 0x1337133713371337;

    if (ptr[0] == 0xDEADC0DECAFEBABE) {
        u_print(m_succ);
    }

    u_sleep(100);
    u_yield();
}

void _start() {
    char msg[] = {'E', 'L', 'F', ' ', 'W', 'O', 'R', 'K', 'S', '!', '\n', 0};
    u_print(msg);
    u_sleep(100);

    user_test_task_2();

    // user_malloc_test();

    user_info_test();
    
    user_test_task_echo();

}
