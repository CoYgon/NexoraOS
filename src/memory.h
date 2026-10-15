#ifndef EMREOS_MEMORY_H
#define EMREOS_MEMORY_H

#include <stdint.h>
#include "limine.h"

#define EMREOS_PAGE_SIZE 4096ULL

void memory_init(struct limine_memmap_response *map);
uintptr_t page_alloc(void);
int page_free(uintptr_t address);
uint64_t memory_usable_bytes(void);

#endif
