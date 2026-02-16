#include <userlib.h>

void _start() {
    char msg[] = {'E', 'L', 'F', ' ', 'W', 'O', 'R', 'K', 'S', '!', '\n', 0};
    while(1) {
        u_print(msg);
        u_sleep(100);
        u_exit();
    }
}
