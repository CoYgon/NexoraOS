#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "framebuffer.h"

int main(void) {
    uint32_t raw[6] = {0}; /* 2x2 pixels, 12-byte pitch with one padding word per row */
    struct limine_framebuffer fb = {0};
    fb.address = raw; fb.width = 2; fb.height = 2; fb.pitch = 12; fb.bpp = 32;
    fb.red_mask_size=8; fb.red_mask_shift=0;
    fb.green_mask_size=8; fb.green_mask_shift=8;
    fb.blue_mask_size=8; fb.blue_mask_shift=16;
    fb_init(&fb);
    assert(fb_available());
    fb_pixel(1,0,0x123456u);
    assert(raw[1]==0x563412u);
    assert(fb_read_pixel(1,0)==0x123456u);
    fb_pixel(0,1,0xABCDEFu);
    assert(raw[3]==0xEFCDABu); /* second row begins after pitch padding */
    assert(fb_read_pixel(0,1)==0xABCDEFu);
    puts("NexoraOS framebuffer mask/pitch tests passed");
    return 0;
}
