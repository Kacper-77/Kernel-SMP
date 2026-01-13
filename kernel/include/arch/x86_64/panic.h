#ifndef PANIC_H
#define PANIC_H

void panic(const char* message);
void smp_halt_others(void);

#endif
