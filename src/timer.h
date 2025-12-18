#pragma once
#include "byte.h"
#include <stdbool.h>

bool timer_init(void);
void timer_tick(void);

byte timer_div_read(void);
void timer_div_write(byte val);
byte timer_tima_read(void);
void timer_tima_write(byte val);
byte timer_tma_read(void);
void timer_tma_write(byte val);
byte timer_tac_read(void);
void timer_tac_write(byte val);