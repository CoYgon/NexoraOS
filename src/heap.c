#include "heap.h"
#include <stdint.h>
#include <stddef.h>

/* A bounded, coalescing heap for the early single-core kernel. This heap is
 * deliberately independent of page_alloc: the latter returns physical pages,
 * not a virtually mapped, general-purpose allocation region. */
#define HEAP_BYTES (2u * 1024u * 1024u)
#define ALIGNMENT 16u
#define ALIGN_UP(n) (((n) + (ALIGNMENT - 1u)) & ~(size_t)(ALIGNMENT - 1u))
#define HEAP_MAGIC 0x4E584852u

typedef struct __attribute__((aligned(16))) block {
    size_t size;
    struct block *next;
    uint32_t magic;
    uint8_t free;
    uint8_t reserved[3];
} block_t;

static union { max_align_t align; uint8_t bytes[HEAP_BYTES]; } g_heap;
static block_t *g_first;
static size_t g_used;
static int g_ready;

void heap_init(void) {
    if (g_ready) return;
    g_first = (block_t *)g_heap.bytes;
    g_first->size = HEAP_BYTES - sizeof(block_t);
    g_first->next = 0;
    g_first->magic = HEAP_MAGIC;
    g_first->free = 1;
    g_used = 0;
    g_ready = 1;
}

void *nexora_malloc(size_t size) {
    if (!size || size > HEAP_BYTES - sizeof(block_t)) return 0;
    heap_init();
    size = ALIGN_UP(size);
    for (block_t *b = g_first; b; b = b->next) {
        if (b->magic != HEAP_MAGIC) return 0;
        if (!b->free || b->size < size) continue;
        if (b->size >= size + sizeof(block_t) + ALIGNMENT) {
            block_t *split = (block_t *)((uint8_t *)(b + 1) + size);
            split->size = b->size - size - sizeof(block_t);
            split->next = b->next;
            split->magic = HEAP_MAGIC;
            split->free = 1;
            b->next = split;
            b->size = size;
        }
        b->free = 0;
        g_used += b->size;
        return (void *)(b + 1);
    }
    return 0;
}

void *nexora_calloc(size_t count, size_t size) {
    if (size && count > (size_t)-1 / size) return 0;
    size_t total = count * size;
    void *p = nexora_malloc(total);
    if (p) { uint8_t *d = p; for (size_t i = 0; i < total; ++i) d[i] = 0; }
    return p;
}

void nexora_free(void *ptr) {
    if (!ptr || !g_ready) return;
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t start = (uintptr_t)g_heap.bytes;
    if (p < start + sizeof(block_t) || p >= start + HEAP_BYTES) return;
    block_t *target = ((block_t *)ptr) - 1;
    if (target->magic != HEAP_MAGIC || target->free) return;
    target->free = 1;
    g_used = g_used >= target->size ? g_used - target->size : 0;
    for (block_t *b = g_first; b && b->next;) {
        if (b->magic != HEAP_MAGIC || b->next->magic != HEAP_MAGIC) return;
        if (b->free && b->next->free && (uint8_t *)(b + 1) + b->size == (uint8_t *)b->next) {
            block_t *next = b->next;
            b->size += sizeof(block_t) + next->size;
            b->next = next->next;
        } else b = b->next;
    }
}

void *nexora_realloc(void *ptr, size_t size) {
    if (!ptr) return nexora_malloc(size);
    if (!size) { nexora_free(ptr); return 0; }
    block_t *b = ((block_t *)ptr) - 1;
    if (b->magic != HEAP_MAGIC || b->free) return 0;
    if (b->size >= size) return ptr;
    void *next = nexora_malloc(size);
    if (!next) return 0;
    uint8_t *dst = next, *src = ptr;
    for (size_t i = 0; i < b->size; ++i) dst[i] = src[i];
    nexora_free(ptr);
    return next;
}
size_t heap_capacity(void) { return HEAP_BYTES; }
size_t heap_used(void) { return g_used; }
