# EmreOS Desktop Update

## What changed
- Mouse movement now updates only the software cursor rather than clearing and repainting the entire framebuffer. This avoids the CRT-like flicker caused by full-screen redraws on every mouse packet.
- The cursor saves and restores the pixels beneath it, respecting the framebuffer pitch and clipping at screen edges.
- Desktop colors were refreshed to a restrained Windows-inspired slate/blue palette with clearer panel contrast, a top system bar, app cards, and a taskbar.

## Build
Run `bash scripts/build.sh` in an environment with Clang, LLD, NASM, xorriso, and the Limine tools installed.

The kernel C sources and linker step were checked in this environment. Full QEMU visual testing was not available here, so please verify mouse behavior in QEMU on your machine.
