# NexoraOS Classic V6.6 — Desktop assets and personalization

V6.6 builds on the V6.5 desktop with a fifth Settings app, FAT32-backed wallpaper/font loading, a scaled image renderer that respects framebuffer clipping, and a TTF-aware UI text path with the built-in bitmap font as a fallback.

## Desktop changes

- Added the **Settings** app to the desktop cards and Start menu.
- Settings can toggle the wallpaper, toggle TrueType rendering, and cycle the active accent color.
- Added `assets/WALLPAPER.PNG` (dark abstract Nexora wallpaper) and the DejaVu Sans Mono TrueType font in `assets/DejaVuSansMono.ttf`.
- If a FAT32 data disk is mounted, the kernel looks for `/NEXORA/WALLPAP.PNG` and `/NEXORA/FONT.TTF`. Missing or invalid assets safely fall back to the gradient and built-in bitmap font.
- The scaled wallpaper renderer respects the current framebuffer clip region so dragging a window does not need to iterate over the entire wallpaper for every clipped repaint.
- App-card spacing now fits the available desktop content width when five apps are shown.

## Optional FAT32 asset disk

The current legacy ATA block driver exposes one read-only disk. The asset disk is therefore only read by the OS; settings changes affect the current session and are not persisted.

On Ubuntu/WSL, install the tools:

```sh
sudo apt update
sudo apt install dosfstools mtools
```

Create a 128 MiB FAT32 data disk containing the bundled wallpaper and font:

```sh
./scripts/make-assets-disk.sh
```

Then build and launch as usual:

```sh
./scripts/build.sh
./scripts/run.sh
```

`run.sh` automatically attaches `build/nexoraos-assets.img` when it exists. The kernel will log whether it loaded the wallpaper and TTF font over the serial console. If the image file cannot be created or the disk is not detected, the desktop continues using its built-in fallbacks.

## Validation status

- `./tests/run_all_tests.sh`: all current host test groups pass, including the scaled image drawing regression.
- `make kernel-c-check`: all kernel and lwIP C sources compile and the C-only ELF link check succeeds.
- A full bootable ISO and actual QEMU screen/ATA asset-disk run have not been tested in the current environment because NASM, xorriso, and QEMU are unavailable. The FAT32 asset-disk creation script also requires `dosfstools` and `mtools` on the target machine.

The current UI text is primarily ASCII. The TrueType path is used for printable ASCII strings; the asset API itself accepts Unicode codepoints, but UTF-8 decoding and internationalized UI text are future work. There is no graphics acceleration or persistent settings database yet.
