#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

void init_serial();
int is_transmit_empty();
void write_serial(char a);
void kprint(const char* s);
void kprint_hex(uint64_t value);
void kprint_raw(const char* s);

#endif
