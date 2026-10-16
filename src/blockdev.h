#ifndef NEXORAOS_BLOCKDEV_H
#define NEXORAOS_BLOCKDEV_H
#include <stdint.h>
#include <stddef.h>
#define BLOCKDEV_SECTOR_SIZE 512u

typedef int (*blockdev_read_fn)(void *context, uint64_t lba, uint32_t count, void *buffer);
typedef int (*blockdev_write_fn)(void *context, uint64_t lba, uint32_t count, const void *buffer);

struct blockdev_info {
    int present;
    uint64_t sectors;
    char name[16];
    char model[41];
};

void blockdev_init(void);
int blockdev_register(const char *name, uint64_t sectors, blockdev_read_fn read_fn, void *context);
int blockdev_register_rw(const char *name, uint64_t sectors, blockdev_read_fn read_fn, blockdev_write_fn write_fn, void *context);
int blockdev_read(uint64_t lba, uint32_t count, void *buffer);
int blockdev_write(uint64_t lba, uint32_t count, const void *buffer);
int blockdev_is_writable(void);
const struct blockdev_info *blockdev_get_info(void);
int blockdev_is_ready(void);
#endif
