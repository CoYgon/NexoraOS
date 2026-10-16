#ifndef NEXORAOS_FRAMEBUFFER_H
#define NEXORAOS_FRAMEBUFFER_H

#include <stdint.h>
#include "limine.h"

void fb_init(struct limine_framebuffer *fb);
void fb_set_clip(int enabled, int x, int y, int w, int h);
int fb_get_clip(int *x, int *y, int *w, int *h);
void fb_clear(uint32_t color);
void fb_rect(uint64_t x, uint64_t y, uint64_t w,
             uint64_t h, uint32_t color);
void fb_pixel(uint64_t x, uint64_t y, uint32_t color);
uint32_t fb_read_pixel(uint64_t x, uint64_t y);
void fb_text(uint64_t x, uint64_t y, const char *text,
             uint32_t color, uint32_t scale);
int fb_available(void);
uint64_t fb_width(void);
uint64_t fb_height(void);

#endif
