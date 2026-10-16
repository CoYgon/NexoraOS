#ifndef NEXORAOS_LIMINE_H
#define NEXORAOS_LIMINE_H

#include <stdint.h>
#include <stddef.h>

#define LIMINE_REQUESTS_START_MARKER \
    __attribute__((used, section(".limine_requests_start"))) \
    volatile uint64_t limine_requests_start_marker[4] = { \
        0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf, \
        0x785c6ed015d3e316, 0x181e920a7852b9d9 \
    }

#define LIMINE_REQUESTS_END_MARKER \
    __attribute__((used, section(".limine_requests_end"))) \
    volatile uint64_t limine_requests_end_marker[2] = { \
        0xadc0e0531bb10d03, 0x9572709f31764c62 \
    }

#define LIMINE_REQUEST \
    __attribute__((used, section(".limine_requests"), aligned(8)))

#define LIMINE_BASE_REVISION(N) \
    __attribute__((used, section(".limine_requests"), aligned(8))) \
    volatile uint64_t limine_base_revision[3] = { \
        0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) \
    }

#define LIMINE_COMMON_MAGIC \
    0xc7b1dd30df4c8b88, 0x0a82e883a194f07b

#define LIMINE_FRAMEBUFFER_REQUEST_ID \
    { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b }

#define LIMINE_MEMMAP_REQUEST_ID \
    { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }

#define LIMINE_KERNEL_ADDRESS_REQUEST_ID \
    { LIMINE_COMMON_MAGIC, 0x71ba76863cc55f63, 0xb2644a48c516a487 }

#define LIMINE_MEMMAP_USABLE 0
#define LIMINE_MEMMAP_RESERVED 1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE 2
#define LIMINE_MEMMAP_ACPI_NVS 3
#define LIMINE_MEMMAP_BAD_MEMORY 4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_EXECUTABLE_AND_MODULES 6
#define LIMINE_MEMMAP_FRAMEBUFFER 7
#define LIMINE_MEMMAP_RESERVED_MAPPED 8

#define LIMINE_FRAMEBUFFER_RGB 1

struct limine_framebuffer {
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;
    uint64_t mode_count;
    void *modes;
};

struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_memmap_response *response;
};

struct limine_kernel_address_response {
    uint64_t revision;
    uint64_t physical_base;
    uint64_t virtual_base;
};
struct limine_kernel_address_request {
    uint64_t id[4];
    uint64_t revision;
    volatile struct limine_kernel_address_response *response;
};

#endif
