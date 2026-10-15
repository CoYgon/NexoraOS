#include "serial.h"
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}
void serial_putc(char c) {
    unsigned int spin = 1000000;
    while ((inb(0x3F8 + 5) & 0x20) == 0 && --spin) { }
    outb(0x3F8, (uint8_t)c);
}
void serial_write(const char *s) {
    if (!s) return;
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}
void serial_write_hex(unsigned long long value) {
    static const char digits[] = "0123456789ABCDEF";
    serial_write("0x");
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        unsigned int n = (unsigned int)((value >> i) & 0xF);
        if (n || started || i == 0) {
            serial_putc(digits[n]);
            started = 1;
        }
    }
}
void serial_write_dec(unsigned long long value) {
    char buf[24];
    int n = 0;
    if (!value) { serial_putc('0'); return; }
    while (value && n < (int)sizeof(buf)) {
        buf[n++] = (char)('0' + value % 10);
        value /= 10;
    }
    while (n) serial_putc(buf[--n]);
}
