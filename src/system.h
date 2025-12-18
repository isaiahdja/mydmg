#pragma once
#include "byte.h"
#include <stdint.h>
#include <stdbool.h>

#define GB_WIDTH 160
#define GB_HEIGHT 144

#define T_CYCLES_PER_SEC (1 << 22);
#define T_CYCLES_PER_FRAME 70244
#define T_M_RATIO 4
#define M_CYCLES_PER_FRAME (T_CYCLES_PER_FRAME / T_M_RATIO)

bool sys_init();
void sys_tick(void);
void sys_start_frame(void);

byte vram_read(uint16_t addr);
void vram_write(uint16_t addr, byte val);

byte wram_read(uint16_t addr);
void wram_write(uint16_t addr, byte val);