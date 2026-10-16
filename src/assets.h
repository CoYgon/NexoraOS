#ifndef NEXORAOS_ASSETS_H
#define NEXORAOS_ASSETS_H
#include <stdint.h>
#include <stddef.h>

typedef struct nexoraos_image {
    uint32_t width, height, channels;
    uint8_t *pixels; /* RGBA, 4 bytes per pixel */
} nexoraos_image_t;

typedef struct nexoraos_font {
    const uint8_t *data;
    size_t size;
    float pixel_height;
} nexoraos_font_t;

/* Decode encoded image bytes already read into memory (PNG/JPEG/BMP/TGA).
 * Returns 1 on success. The caller owns image.pixels and must release it. */
int assets_image_decode(const uint8_t *data, size_t size, nexoraos_image_t *out);
void assets_image_release(nexoraos_image_t *image);
void assets_image_draw(const nexoraos_image_t *image, int x, int y);
void assets_image_draw_scaled(const nexoraos_image_t *image, int x, int y, uint32_t width, uint32_t height);
/* Render one Unicode codepoint from an in-memory TTF/OTF. Returns pixel advance,
 * or -1 for invalid/unreadable font data. This API accepts a codepoint, not UTF-8. */
int assets_font_draw_codepoint(const nexoraos_font_t *font, uint32_t codepoint,
                               int x, int baseline_y, uint32_t color);
#endif
