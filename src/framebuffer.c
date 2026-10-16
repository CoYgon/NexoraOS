#include "framebuffer.h"
#include <stdint.h>

static struct limine_framebuffer *g_fb;
static int clip_enabled;
static int clip_x0, clip_y0, clip_x1, clip_y1;

static uint32_t pack_channel(uint32_t value, uint8_t bits, uint8_t shift, uint8_t fallback_shift) {
    if (!bits) { bits = 8; shift = fallback_shift; }
    if (bits > 8) bits = 8;
    uint32_t max = (1u << bits) - 1u;
    return ((value * max + 127u) / 255u) << shift;
}
static uint32_t unpack_channel(uint32_t raw, uint8_t bits, uint8_t shift, uint8_t fallback_shift) {
    if (!bits) { bits = 8; shift = fallback_shift; }
    if (bits > 8) bits = 8;
    uint32_t max = (1u << bits) - 1u;
    uint32_t value = (raw >> shift) & max;
    return (value * 255u + max / 2u) / max;
}
static uint32_t pack_rgb(uint32_t rgb) {
    if (!g_fb) return rgb;
    return pack_channel((rgb >> 16) & 255u, g_fb->red_mask_size, g_fb->red_mask_shift, 16) |
           pack_channel((rgb >> 8) & 255u, g_fb->green_mask_size, g_fb->green_mask_shift, 8) |
           pack_channel(rgb & 255u, g_fb->blue_mask_size, g_fb->blue_mask_shift, 0);
}
static uint32_t unpack_rgb(uint32_t raw) {
    if (!g_fb) return raw;
    return (unpack_channel(raw, g_fb->red_mask_size, g_fb->red_mask_shift, 16) << 16) |
           (unpack_channel(raw, g_fb->green_mask_size, g_fb->green_mask_shift, 8) << 8) |
           unpack_channel(raw, g_fb->blue_mask_size, g_fb->blue_mask_shift, 0);
}

void fb_set_clip(int enabled, int x, int y, int w, int h) {
    clip_enabled = enabled;
    if (!enabled) return;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    clip_x0 = x < 0 ? 0 : x;
    clip_y0 = y < 0 ? 0 : y;
    clip_x1 = x + w;
    clip_y1 = y + h;
    if (g_fb) {
        if (clip_x1 > (int)g_fb->width) clip_x1 = (int)g_fb->width;
        if (clip_y1 > (int)g_fb->height) clip_y1 = (int)g_fb->height;
    }
}

int fb_get_clip(int *x, int *y, int *w, int *h) {
    if (!clip_enabled) return 0;
    if (x) *x=clip_x0;
    if (y) *y=clip_y0;
    if (w) *w=clip_x1-clip_x0;
    if (h) *h=clip_y1-clip_y0;
    return 1;
}

/* 5 sütun x 7 satır; her byte bir sütunun piksellerini taşır. */
static const uint8_t font[36][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}  /* 9 */
};

void fb_init(struct limine_framebuffer *fb) {
    g_fb = fb;
}

int fb_available(void) {
    return g_fb && g_fb->address &&
           g_fb->bpp == 32 &&
           g_fb->width && g_fb->height &&
           g_fb->pitch / 4 >= g_fb->width;
}

uint64_t fb_width(void) {
    return fb_available() ? g_fb->width : 0;
}

uint64_t fb_height(void) {
    return fb_available() ? g_fb->height : 0;
}

void fb_pixel(uint64_t x, uint64_t y, uint32_t color) {
    if (!fb_available() || x >= g_fb->width || y >= g_fb->height)
        return;
    if (clip_enabled && ((int)x < clip_x0 || (int)x >= clip_x1 || (int)y < clip_y0 || (int)y >= clip_y1)) return;

    volatile uint32_t *row =
        (volatile uint32_t *)((uintptr_t)g_fb->address + y * g_fb->pitch);

    row[x] = pack_rgb(color);
}

uint32_t fb_read_pixel(uint64_t x, uint64_t y) {
    if (!fb_available() || x >= g_fb->width || y >= g_fb->height)
        return 0;
    volatile uint32_t *row = (volatile uint32_t *)
        ((uintptr_t)g_fb->address + y * g_fb->pitch);
    return unpack_rgb(row[x]);
}

void fb_rect(uint64_t x, uint64_t y, uint64_t w,
             uint64_t h, uint32_t color) {
    if (!fb_available() || x >= g_fb->width || y >= g_fb->height)
        return;

    if (w > g_fb->width - x) w = g_fb->width - x;
    if (h > g_fb->height - y) h = g_fb->height - y;
    if (clip_enabled) {
        uint64_t x2 = x + w, y2 = y + h;
        if (x < (uint64_t)clip_x0) x = (uint64_t)clip_x0;
        if (y < (uint64_t)clip_y0) y = (uint64_t)clip_y0;
        if (x2 > (uint64_t)clip_x1) x2 = (uint64_t)clip_x1;
        if (y2 > (uint64_t)clip_y1) y2 = (uint64_t)clip_y1;
        if (x2 <= x || y2 <= y) return;
        w = x2 - x; h = y2 - y;
    }

    for (uint64_t yy = y; yy < y + h; ++yy) {
        volatile uint32_t *row =
            (volatile uint32_t *)((uintptr_t)g_fb->address +
                                  yy * g_fb->pitch);

        for (uint64_t xx = x; xx < x + w; ++xx)
            row[xx] = pack_rgb(color);
    }
}

void fb_clear(uint32_t color) {
    if (fb_available())
        fb_rect(0, 0, g_fb->width, g_fb->height, color);
}

static int glyph_index(char c) {
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + c - '0';
    return -1;
}

void fb_text(uint64_t x, uint64_t y, const char *text,
             uint32_t color, uint32_t scale) {
    if (!fb_available() || !text || scale == 0)
        return;

    uint64_t origin_x = x;

    while (*text) {
        char c = *text++;

        if (c == '\n') {
            x = origin_x;
            y += 9 * scale;
            continue;
        }

        if (c == ' ') {
            x += 4 * scale;
            continue;
        }

        int index = glyph_index(c);

        if (index >= 0) {
            for (uint32_t col = 0; col < 5; ++col) {
                uint8_t bits = font[index][col];

                for (uint32_t row = 0; row < 7; ++row) {
                    if (bits & (1u << row))
                        fb_rect(x + col * scale, y + row * scale,
                                scale, scale, color);
                }
            }
        } else if (c == '-') {
            fb_rect(x, y + 3 * scale, 4 * scale, scale, color);
        } else if (c == ':') {
            fb_rect(x + scale, y + scale, scale, scale, color);
            fb_rect(x + scale, y + 5 * scale, scale, scale, color);
        } else if (c == '.') {
            fb_rect(x + scale, y + 6 * scale, scale, scale, color);
        } else if (c == '/') {
            for (uint32_t i = 0; i < 7; ++i)
                fb_rect(x + (4 - (i * 4 / 6)) * scale,
                        y + i * scale, scale, scale, color);
        }

        x += 6 * scale;

        if (x >= g_fb->width || y >= g_fb->height)
            break;
    }
}
