#pragma once
#include "byte.h"

bool input_init(void);
void input_poll_and_load(void);

byte input_joyp_read(void);
void input_joyp_write(byte val);