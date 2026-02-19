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
    // u_exit();
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

void _start() {
    char msg[] = {'E', 'L', 'F', ' ', 'W', 'O', 'R', 'K', 'S', '!', '\n', 0};
    u_print(msg);
    u_sleep(100);

    user_test_task_2();
    
    user_test_task_echo();
}
