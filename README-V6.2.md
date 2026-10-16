# NexoraOS Classic V6.2

NexoraOS Classic V6.2 is the next version of the existing Limine/x86-64 OS.
This release carries the project rename from EmreOS to NexoraOS and preserves
the existing kernel, desktop, storage prototype, and bundled third-party sources.

## Renaming
- Project-owned source comments, include guards, build messages, and documentation
  use NexoraOS naming.
- The upstream lwIP source and third-party header contents are intentionally not
  rewritten; their upstream attribution and license text must remain intact.

## Networking (lwIP)
- Configuration/architecture headers are under `src/ports/lwip/`.
- The selected configuration is `NO_SYS=1`, the correct direction for a kernel
  without a scheduler or POSIX sockets.
- **Not yet active:** the project has no integrated Ethernet NIC driver and
  lwIP sources are not part of the kernel link. To activate it, implement a NIC
  driver (e.g. virtio-net for QEMU), RX/TX queues, a `netif` adapter, ARP input,
  and periodic `sys_check_timeouts()` calls. Add only the needed lwIP core sources
  to the build after those pieces are in place.

## Images and TrueType fonts
- `src/stb_image.h` and `src/stb_truetype.h` remain vendored.
- `src/assets.h` defines a small kernel-facing image/font data contract.
- **Not yet active:** the current page allocator is not a general heap and the
  kernel has no VFS-to-memory asset loader or complete libc/math compatibility
  layer. Defining stb implementation macros prematurely can introduce unresolved
  symbols and uncontrolled allocations.
- Next steps: bounded heap (`malloc`/`realloc`/`free` equivalents), bounded VFS
  file reads, a single implementation translation unit, compile/link checks,
  framebuffer pixel-format conversion, clipping, and malformed-file tests.

## Validation
Run the host tests with `./tests/run_all_tests.sh`, then build with
`./scripts/build.sh` in an environment containing clang, ld.lld, nasm, make,
xorriso and the required Limine boot files. A successful kernel link alone does
not prove NIC or asset features work on real hardware.
