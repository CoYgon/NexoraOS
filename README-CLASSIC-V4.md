# NexoraOS Classic V4 — development update

This update preserves the existing freestanding x86-64/Limine codebase and adds a classic desktop theme plus safer, more useful RAM filesystem operations.

## Included

- Classic teal desktop, gray panels/taskbar, blue window title bars, and a Start-style program menu.
- Start menu entries for memory/system information, file explorer, display settings, and terminal.
- Existing software cursor save/restore and clipped redraw path retained to reduce mouse-movement flicker.
- RAM VFS paths now show nested directories correctly (for example `/docs/notes`).
- New VFS operations: `rm <file>`, `rmdir <empty-dir>`, `mv <old-name> <new-name>`.
- `rmdir` refuses to remove a non-empty directory or the current directory; `rm` refuses directories.
- `df` reports used/available VFS node slots.
- Updated build/run scripts check dependencies before destructive cleanup and print clear missing-tool messages.
- Host-side regression test for nested paths, file contents, rename, removal and directory safeguards.

## Terminal commands

`help`, `clear`, `echo <text>`, `uname`, `mem`, `pages`, `df`, `ls`, `pwd`, `cd <dir>`, `mkdir <dir>`, `touch <file>`, `cat <file>`, `write <file> <text>`, `rm <file>`, `rmdir <empty-dir>`, `mv <old-name> <new-name>`, `history`, `version`, `about`, `exit`.

The filesystem is still RAM-only. Names are single path components rather than arbitrary absolute/relative paths; file data and directory changes are lost on reboot. The current shell is a built-in command interpreter, not a POSIX shell.

## Build and test

Run the host-side VFS regression test:

```sh
./tests/run_vfs_tests.sh
```

Build a bootable ISO on a machine with `clang`, `ld.lld`, `nasm`, `make`, `xorriso`, and the required Limine tools installed:

```sh
./scripts/build.sh
```

Run in QEMU after building the ISO:

```sh
./scripts/run.sh
```

## Verification status for this archive

- All C translation units compiled with Clang for `x86_64-unknown-none-elf`, `-Wall -Wextra -Werror`.
- Kernel linked successfully as a statically linked x86-64 ELF using the project's existing `build/entry.o`.
- VFS regression test passed.
- A fresh ISO build and interactive QEMU visual test were not performed in the build environment because `nasm`, `xorriso`, and `qemu-system-x86_64` are unavailable. The desktop behavior and flicker improvement therefore still need runtime confirmation.

## Still not implemented

Hardware IRQ-driven input, USB input, virtual memory/page-table setup, user/kernel privilege separation, process scheduling, executable loading, a disk driver, and persistent storage are not implemented by this update. The page allocator and framebuffer desktop are early OS-development components, not a production-secure kernel.

## NexoraPkg (V0.1)

The shell now includes a custom package catalog/manager inspired by apt and pacman:

- `pkg help`
- `pkg search <term>`
- `pkg list`
- `pkg info <name>`
- `pkg install <name>`
- `pkg remove <name>`
- `pkg update`
- `pkg upgrade`
- `pkg repos`
- `pkg autoremove`

The current repository is a built-in offline catalog (`emre-core`). Package state lives in RAM and resets at reboot. At this stage `install`/`remove` manage package metadata only; package payload downloading, signature verification, persistent package database, and launching installed user-mode applications are not implemented. `pkg update` checks the built-in index and does not access the network. Run `./tests/run_pkg_tests.sh` to test package state and command behavior.
