#ifndef EMREOS_EMREFS_H
#define EMREOS_EMREFS_H
#include <stdint.h>
#define EMREFS_SUPERBLOCK_BYTES 512u
void emrefs_init(void);
int emrefs_validate_superblock(const uint8_t *sector, uint64_t device_sectors);
int emrefs_mount(uint64_t partition_lba, uint64_t partition_sectors);
int emrefs_auto_mount(void);
int emrefs_is_mounted(void);
int emrefs_is_writable(void);
int emrefs_format(uint64_t partition_lba, uint64_t partition_sectors);
int emrefs_list_root(char *out, unsigned cap);
int emrefs_mkdir(const char *name);
int emrefs_write_file(const char *name, const void *data, uint32_t size);
int emrefs_read_file(const char *name, void *out, uint32_t cap, uint32_t *bytes_read);
#endif
