#include "mouse.h"
#include <stdint.h>

#define MOUSE_DATA   0x60
#define MOUSE_STATUS 0x64
#define MOUSE_CMD    0x64

static struct mouse_state state = { 100, 100, 0 };
static uint8_t packet[3];
static unsigned packet_index;

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

static int wait_input_clear(void) {
    for (unsigned i = 0; i < 100000; ++i)
        if (!(inb(MOUSE_STATUS) & 2))
            return 1;
    return 0;
}

static int wait_output_full(void) {
    for (unsigned i = 0; i < 100000; ++i)
        if (inb(MOUSE_STATUS) & 1)
            return 1;
    return 0;
}

static int mouse_command(uint8_t command) {
    if (!wait_input_clear()) return 0;

    outb(MOUSE_CMD, 0xD4);

    if (!wait_input_clear()) return 0;
    outb(MOUSE_DATA, command);

    for (unsigned i = 0; i < 100000; ++i) {
        uint8_t status = inb(MOUSE_STATUS);

        if (status & 1) {
            uint8_t data = inb(MOUSE_DATA);

            if ((status & 0x20) && data == 0xFA)
                return 1;
        }
    }

    return 0;
}

int mouse_init(void) {
    /* Enable auxiliary PS/2 port. */
    if (!wait_input_clear()) return 0;
    outb(MOUSE_CMD, 0xA8);

    /* Read controller command byte. */
    if (!wait_input_clear()) return 0;
    outb(MOUSE_CMD, 0x20);

    if (!wait_output_full()) return 0;
    uint8_t command_byte = inb(MOUSE_DATA);

    /* Polling mode: disable IRQs; enable auxiliary clock. */
    command_byte &= (uint8_t)~0x02;
    command_byte &= (uint8_t)~0x20;

    if (!wait_input_clear()) return 0;
    outb(MOUSE_CMD, 0x60);

    if (!wait_input_clear()) return 0;
    outb(MOUSE_DATA, command_byte);

    if (!mouse_command(0xF6)) return 0; /* Defaults */
    if (!mouse_command(0xF4)) return 0; /* Enable reporting */

    packet_index = 0;
    return 1;
}

int mouse_poll(void) {
    uint8_t status = inb(MOUSE_STATUS);

    /* No data, or next byte belongs to the keyboard. */
    if (!(status & 1) || !(status & 0x20))
        return 0;

    uint8_t data = inb(MOUSE_DATA);

    if (packet_index == 0 && !(data & 0x08))
        return 0;

    packet[packet_index++] = data;

    if (packet_index < 3)
        return 0;

    packet_index = 0;

    /* Ignore overflow packets. */
    if (packet[0] & 0xC0)
        return 0;

    int32_t dx = (int8_t)packet[1];
    int32_t dy = (int8_t)packet[2];

    state.x += dx;
    state.y -= dy;
    state.buttons = packet[0] & 7;

    if (state.x < 0) state.x = 0;
    if (state.y < 0) state.y = 0;

    return 1;
}

struct mouse_state mouse_get_state(void) {
    return state;
}
