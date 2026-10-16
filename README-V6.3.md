# NexoraOS Classic V6.3

V6.3 is the asset-runtime integration stage for the existing Limine/x86-64 kernel. It keeps the prior desktop, VFS, package and storage code, and makes the bundled stb image/font libraries callable from kernel code without linking a hosted libc.

## Implemented

- `src/heap.c`, `src/heap.h`: fixed 2 MiB, 16-byte-aligned heap with split/coalesce, calloc/realloc/free, bounds checks, and usage/capacity reporting. It is an early single-core allocator, not a replacement for the physical page allocator.
- `src/assets.c`, `src/assets.h`: one implementation translation unit for `stb_image.h` and `stb_truetype.h`.
- `assets_image_decode()`: decodes in-memory image data to RGBA and rejects invalid/oversized inputs before the full decode.
- `assets_image_draw()`: clips to framebuffer bounds and alpha blends RGBA pixels.
- `assets_font_draw_codepoint()`: validates the SFNT table directory and draws a Unicode codepoint from an in-memory TTF/OTF font, with framebuffer clipping and alpha blending.
- Framebuffer pixels are converted between canonical `0xRRGGBB` and the channel masks supplied by Limine.
- The entry stub enables the x86-64 SSE2 state needed by the isolated TrueType translation unit. The kernel has no scheduler yet, so it does not yet save/restore SIMD state between tasks.
- `tests/run_all_tests.sh` now includes heap, image decode/draw, and framebuffer mask/pitch tests.

## Example API usage

```c
#include "assets.h"

nexoraos_image_t image;
if (assets_image_decode(encoded_bytes, encoded_size, &image)) {
    assets_image_draw(&image, 40, 80);
    assets_image_release(&image);
}

nexoraos_font_t font = {
    .data = font_bytes,
    .size = font_size,
    .pixel_height = 24.0f
};
assets_font_draw_codepoint(&font, 'A', 60, 120, 0xFFFFFF);
```

The caller must provide a valid buffer containing the encoded image or font file. A general file-to-buffer loader from VFS/FAT32 is not implemented yet. Fonts should be trusted input: table-directory bounds are checked, but this is not a complete hostile-font sandbox.

## Build and test

Run the host-side regression suite:

```sh
./tests/run_all_tests.sh
```

Build the bootable image in an environment with `clang`, `ld.lld`, `nasm`, `make`, `xorriso`, and the expected Limine boot files:

```sh
./scripts/build.sh
```

In this development environment, all C translation units compiled and the full kernel ELF linked with an equivalent x86-64 entry stub. The complete NASM/ISO/QEMU boot flow was not run because `nasm`, `xorriso`, and `qemu-system-x86_64` were unavailable here.

## Still not implemented

- lwIP is configured but not linked into a working network path. A supported NIC driver (e.g. QEMU virtio-net), RX/TX queues, netif glue and timer polling are still required.
- No VFS/FAT32 asset-file loader is exposed as a memory buffer API.
- No scheduler or SIMD context switching exists.
