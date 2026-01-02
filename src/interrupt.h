#pragma once
#include "byte.h"
#include <stdbool.h>

typedef enum {
    INT_VBLANK = 0,
    INT_STAT   = 1,
    INT_TIMER  = 2,
    INT_SERIAL = 3,
    INT_JOYPAD = 4
} interrupt_type;

bool int_init(void);

byte int_if_read(void);
void int_if_write(byte val);
byte int_ie_read(void);
void int_ie_write(byte val);

bool int_send_interrupt(uint16_t *jump_vec);
bool pending_interrupt(void);
void request_interrupt(interrupt_type type);