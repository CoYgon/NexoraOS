#ifndef NEXORAOS_FREESTANDING_STDIO_H
#define NEXORAOS_FREESTANDING_STDIO_H
#include <stddef.h>
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, __builtin_va_list ap);
#endif
