#ifndef EMREOS_MOUSE_H
#define EMREOS_MOUSE_H

#include <stdint.h>

struct mouse_state {
    int32_t x;
    int32_t y;
    uint8_t buttons;
};

int mouse_init(void);
int mouse_poll(void);
struct mouse_state mouse_get_state(void);

#endif
