#ifndef EMREOS_SERIAL_H
#define EMREOS_SERIAL_H
#include <stddef.h>
void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
void serial_write_hex(unsigned long long value);
void serial_write_dec(unsigned long long value);
#endif
