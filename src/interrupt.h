#pragma once
#include "byte.h"
#include <stdbool.h>

typedef enum {
    ITR_VBLANK = 0,
    ITR_STAT   = 1,
    ITR_TIMER  = 2,
    ITR_SERIAL = 3,
    ITR_JOYPAD = 4
} interrupt_type;

bool interrupt_init(void);
void interrupt_tick(void);

byte interrupt_if_read(void);
void interrupt_if_write(byte val);
byte interrupt_ie_read(void);
void interrupt_ie_write(byte val);

void request_interrupt(interrupt_type type);