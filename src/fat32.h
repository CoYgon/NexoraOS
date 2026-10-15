#ifndef EMREOS_FAT32_H
#define EMREOS_FAT32_H
#include <stdint.h>
#define FAT32_PATH_MAX 128
void fat32_init(void);
int fat32_mount(uint64_t partition_lba, uint64_t partition_sectors);
int fat32_auto_mount(void);
int fat32_is_mounted(void);
uint64_t fat32_partition_lba(void);
int fat32_list_root(char *out, unsigned cap);
int fat32_list_path(const char *path, char *out, unsigned cap);
int fat32_read_root_file(const char *short_name, char *out, unsigned cap, unsigned *bytes_read);
int fat32_read_file(const char *path, char *out, unsigned cap, unsigned *bytes_read);
#endif
