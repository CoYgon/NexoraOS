# NexoraOS Classic V6 — storage foundations

V6 builds on the V5 source tree and adds a conservative, read-only storage path.

## Added modules

- `src/blockdev.c/.h`: sector-based block-device abstraction and a legacy primary-channel ATA PIO probe/read driver. It probes primary master then slave, uses bounded polling, exposes one device, supports LBA28 reads only, and contains **no sector-write path**.
- `src/fat32.c/.h`: FAT32 BPB validation, MBR primary partition scan (types 0x0B/0x0C), superfloppy recognition, root-directory listing and root-level 8.3 file reading. It deliberately rejects FAT12/16 and supports 512-byte sectors and clusters up to 4 KiB.
- `src/emrefs.c/.h`: strict validator for the proposed EmreFS V1 superblock, including bounds and CRC-32 checks. This is a format prototype only; it is not a mountable filesystem yet.
- `tests/storage_test.c`: memory-backed synthetic FAT32 volume and EmreFS superblock tests.

## Shell commands

- `disk` — show detected legacy ATA device details.
- `mounts` — show RAM VFS and FAT32 mount state.
- `fatls` — list the mounted FAT32 root directory.
- `fatcat HELLO.TXT` — print a root-level short-name file.
- `fsinfo` — describe the EmreFS prototype status.
- `pkg store` — list the built-in EmreStore text catalog (metadata only, not a graphical store).

Existing RAM VFS and NexoraPkg commands remain. NexoraPkg still manages built-in metadata only; packages are not downloaded or executed.

## EmreFS V1 superblock proposal

The first sector uses `EMREFS1\0` at byte 0, version at 8, header size at 10, sector size at 12, total sectors at 16, metadata start/count at 24/32, data start at 40, generation at 48, and CRC-32 of bytes 0..59 at byte 60. All integers are little-endian. This release validates this structure only; no EmreFS allocator, directories, file I/O, journal, or formatter is included.

## Build and tests

```sh
./tests/run_vfs_tests.sh
./tests/run_pkg_tests.sh
./tests/run_storage_tests.sh
./scripts/build.sh
```

## Hardware and safety limitations

- The ATA implementation supports only legacy IDE-compatible primary-channel devices. It does not support AHCI, NVMe, USB mass storage, SATA controllers exposed only through AHCI, GPT partition maps, or LBA48.
- FAT32 access is **read-only**. There are no disk writes, format commands, or repair commands. Root directory only, short 8.3 names; no long filenames, subdirectory traversal, file writes, timestamps, or Unicode.
- ATA PIO access and the desktop have not been tested on physical hardware in this environment. The test suite uses a synthetic memory-backed disk, not an actual disk/controller.
- Do not connect a disk containing important data expecting general-purpose compatibility. No real disk writes are implemented, but hardware detection itself remains platform-dependent.
- The scheduler, user mode, virtual memory, network stack, graphical EmreStore, signed package archives and executable loader are not implemented. `pkg install` remains metadata-only.
