#include "memory.h"
#include "serial.h"

#define PAGE_SIZE EMREOS_PAGE_SIZE
#define FREE_STACK_CAPACITY 4096

static struct limine_memmap_response *g_map;
static uint64_t g_usable;
static uint64_t g_region_index;
static uint64_t g_cursor;

static uintptr_t free_stack[FREE_STACK_CAPACITY];
static uint64_t free_count;

/* Track only pages actually handed out by this allocator. This prevents
   freeing arbitrary usable RAM and catches double-free attempts. */
#define ALLOC_TRACK_CAPACITY 8192
static uintptr_t allocated_pages[ALLOC_TRACK_CAPACITY];
static uint64_t allocated_count;

static int track_alloc(uintptr_t address) {
    if (allocated_count >= ALLOC_TRACK_CAPACITY)
        return 0;
    allocated_pages[allocated_count++] = address;
    return 1;
}

static int untrack_alloc(uintptr_t address) {
    for (uint64_t i = 0; i < allocated_count; ++i) {
        if (allocated_pages[i] == address) {
            allocated_pages[i] = allocated_pages[--allocated_count];
            return 1;
        }
    }
    return 0;
}

static int is_usable_page(uintptr_t address) {
    if (!g_map || address == 0 || (address & (PAGE_SIZE - 1)))
        return 0;

    for (uint64_t i = 0; i < g_map->entry_count; ++i) {
        struct limine_memmap_entry *e = g_map->entries[i];
        if (!e || e->type != LIMINE_MEMMAP_USABLE)
            continue;

        if (address >= e->base &&
            address - e->base <= e->length &&
            e->length - (address - e->base) >= PAGE_SIZE)
            return 1;
    }

    return 0;
}

void memory_init(struct limine_memmap_response *map) {
    g_map = map;
    g_usable = 0;
    g_region_index = 0;
    g_cursor = 0;
    free_count = 0;
    allocated_count = 0;

    if (!map)
        return;

    for (uint64_t i = 0; i < map->entry_count; ++i) {
        struct limine_memmap_entry *e = map->entries[i];
        if (e && e->type == LIMINE_MEMMAP_USABLE)
            g_usable += e->length;
    }
}

uint64_t memory_usable_bytes(void) {
    return g_usable;
}

uintptr_t page_alloc(void) {
    if (!g_map)
        return 0;

    if (free_count) {
        uintptr_t reused = free_stack[--free_count];
        if (!track_alloc(reused)) {
            serial_write("[memory] allocation tracking capacity reached\n");
            return 0;
        }
        return reused;
    }

    while (g_region_index < g_map->entry_count) {
        struct limine_memmap_entry *e =
            g_map->entries[g_region_index];

        if (!e || e->type != LIMINE_MEMMAP_USABLE ||
            e->length < PAGE_SIZE ||
            e->base > UINT64_MAX - e->length) {
            ++g_region_index;
            g_cursor = 0;
            continue;
        }

        uint64_t start =
            (e->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t end =
            (e->base + e->length) & ~(PAGE_SIZE - 1);

        if (!g_cursor)
            g_cursor = start;

        if (g_cursor <= end && end - g_cursor >= PAGE_SIZE) {
            uintptr_t result = (uintptr_t)g_cursor;
            g_cursor += PAGE_SIZE;
            if (!track_alloc(result)) {
                serial_write("[memory] allocation tracking capacity reached\n");
                return 0;
            }
            return result;
        }

        ++g_region_index;
        g_cursor = 0;
    }

    serial_write("[memory] out of physical pages\n");
    return 0;
}

int page_free(uintptr_t address) {
    if (!is_usable_page(address)) {
        serial_write("[memory] rejected invalid page address\n");
        return 0;
    }

    if (free_count >= FREE_STACK_CAPACITY) {
        serial_write("[memory] free-page stack full\n");
        return 0;
    }

    if (!untrack_alloc(address)) {
        serial_write("[memory] rejected unallocated or double-freed page\n");
        return 0;
    }

    free_stack[free_count++] = address;
    return 1;
}
