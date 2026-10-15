# EmreOS Desktop V3

## Desktop updates
- Windows-inspired top system bar and bottom taskbar with pinned Files and Terminal launchers.
- Desktop application tiles for This PC, Files, Display, and Terminal.
- File Explorer window displays the current RAM filesystem path and directory entries.
- Window dragging repaints only the dirty region (the union of old and new window bounds), rather than clearing/redrawing the full screen for every mouse packet.
- Framebuffer drawing now supports a clipping rectangle so dirty-region redraws do not touch unrelated pixels.
- Cursor background is restored before repaint and redrawn after the clipped update.

## Limitations
- Files are held in a volatile RAM filesystem and are lost on reboot.
- This is a small freestanding kernel desktop, not a Windows-compatible environment.
- Verify on QEMU or the target machine; full visual QEMU testing was not available in the build environment.

## Build
Run `bash scripts/build.sh` in an environment with Clang, LLD, NASM, xorriso, Git, and the Limine tools available.
