# NexoraOS Classic V6.5 — Desktop UI refresh

V6.5 refreshes the existing framebuffer desktop while preserving the V6.4 kernel, lwIP/VirtIO networking path, memory heap, asset APIs, VFS, package manager, and storage code.

## Desktop changes

- Replaced the legacy teal/gray palette with a dark navy desktop, cyan/blue/purple accents, and higher-contrast text.
- Added a subtle stepped-gradient wallpaper, NexoraOS brand mark, compact system status indicator, welcome heading, and four more distinct app tiles.
- Redesigned the overview panel as three status tiles.
- Updated the taskbar, pinned app buttons, start menu, and application window chrome.
- Corrected the app-card, Start menu, and taskbar mappings so Files opens File Explorer and Display opens Display Settings.
- Kept the existing lightweight framebuffer renderer and software cursor; no graphics acceleration or compositing manager is claimed.

## Build and tests

Run `./tests/run_all_tests.sh` and `make kernel-c-check` to validate host regressions and compile/link all kernel C + lwIP objects. Build a bootable ISO with `./scripts/build.sh` on a machine with clang, ld.lld, nasm, make, xorriso, git, and network access for fetching Limine.

The current environment has no NASM, xorriso, or QEMU, so this UI refresh has not been visually verified by booting the ISO. It compiles and links through the C-only kernel validation target; actual boot and mouse interaction should still be checked in QEMU.
