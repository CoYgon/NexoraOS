# EmreOS Classic V6.1

V6.1 extends the V6 storage prototype while preserving the existing Limine/x86-64 kernel and desktop.

## Changes

- FAT32 read-only path traversal: `fatls [path]`, `fatcat <path>`; root and nested directories work with 8.3 names. Long File Names (LFN), FAT32 writes, and Unicode names are not implemented.
- Block device API now supports optional write callbacks. Existing ATA PIO hardware registration remains read-only; this release deliberately does not write to real disks.
- EmreFS V1 on-disk prototype: superblock CRC validation, bitmap-based contiguous sector allocation, root directory entries, root-level directory placeholders, file create/overwrite/read/list, and format support through an explicitly writable block-device callback. Directory entries cannot yet contain child files or subdirectories.
- EmreFS automatic discovery only considers MBR primary partitions of type `0xDA`. EmreFS writes are disabled on the built-in ATA driver because that driver is read-only.
- Tests exercise FAT32 nested paths and EmreFS format/list/read/write using a memory-backed disk, plus a read-only block-device guard.

## Commands

- `fatls` or `fatls SUBDIR`
- `fatcat SUBDIR/FILE.TXT`
- `disk`, `mounts`, `fsinfo`
- `emrelist`, `emrecat <name>`, `emremkdir <name>`, `emrewrite <name> <text>` (mutating commands only work when a writable device has been registered; current physical ATA driver does not register one).

## Build/test

```sh
./tests/run_all_tests.sh
./scripts/build-kernel.sh
```

The test suite uses host-compiled tests and an in-memory test disk. A kernel ELF build does not prove real hardware boot. AHCI/NVMe drivers, FAT32 writes/LFN, nested EmreFS directories, journaling/crash consistency, and real-hardware testing are not included. The ATA PIO driver is legacy primary-channel LBA28 and read-only. Do not use `emrefs_format` against a real disk until a separately reviewed writable driver and confirmation path exist.
