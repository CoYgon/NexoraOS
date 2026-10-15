# EmreOS V2 development build

This is a freestanding x86-64 Limine kernel and a desktop/shell prototype.

## Implemented in this revision
- Small bounded RAM-only VFS: `ls`, `cd`, `pwd`, `mkdir`, `touch`, `cat`, `write`.
- Bounded command history and Up/Down history navigation.
- Existing physical page allocator, framebuffer, PS/2 polling keyboard and mouse.
- Input remains a fixed command allow-list; no arbitrary host command execution.

## Important limitations (not represented as complete features)
- The VFS is volatile and limited to 48 entries; it is not a disk-backed filesystem and is lost at reboot.
- Keyboard handling is still PS/2 polling, not IDT/PIC interrupt-driven input. USB keyboards are not supported.
- The kernel does not yet establish isolated user address spaces or a ring-3 syscall boundary. Physical page allocation is not virtual memory protection.
- There is no executable loader, process scheduler, or safe user-process execution yet. Do not run untrusted code on this kernel.
- `cd` currently supports simple child names, `/`, and `..`; this is not a POSIX shell/path resolver.

## Build
Requirements: clang, lld, NASM, xorriso, make, and the bundled Limine files.

```sh
./scripts/build.sh
./scripts/run.sh
```

Build validation in the development environment: all modified C translation units compiled with the freestanding target and the kernel ELF linked successfully. QEMU boot and interactive hardware behavior were not verified in this environment.
