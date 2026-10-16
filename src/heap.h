#ifndef NEXORAOS_HEAP_H
#define NEXORAOS_HEAP_H
#include <stddef.h>
#include <stdint.h>
void heap_init(void);
void *nexora_malloc(size_t size);
void *nexora_calloc(size_t count, size_t size);
void *nexora_realloc(void *ptr, size_t size);
void nexora_free(void *ptr);
size_t heap_capacity(void);
size_t heap_used(void);
#endif
