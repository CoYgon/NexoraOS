#ifndef NEXORAOS_FREESTANDING_STDLIB_H
#define NEXORAOS_FREESTANDING_STDLIB_H
#include <stddef.h>
#include "heap.h"
#define malloc nexora_malloc
#define realloc nexora_realloc
#define free nexora_free
int abs(int);
int atoi(const char *);
long strtol(const char *, char **, int);
#endif
