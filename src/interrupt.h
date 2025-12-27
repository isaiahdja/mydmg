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

bool interrupt_init(void);

byte interrupt_if_read(void);
void interrupt_if_write(byte val);
byte interrupt_ie_read(void);
void interrupt_ie_write(byte val);

bool interrupt_send_interrupt(uint16_t *jump_vec);
bool interrupt_pending(void);
void request_interrupt(interrupt_type type);