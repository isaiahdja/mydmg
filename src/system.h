#pragma once
#include "byte.h"
#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h>

#define GB_WIDTH 160
#define GB_HEIGHT 144

#define T_CYCLES_PER_SEC (1 << 22)
#define T_CYCLES_PER_FRAME 70224
#define T_M_RATIO 4
#define M_CYCLES_PER_FRAME (T_CYCLES_PER_FRAME / T_M_RATIO)

typedef struct {
    const char *rom_path;
    SDL_Mutex *frame_mux;
} system_args;

bool sys_init(system_args args);
void sys_tick(void);
void sys_start_frame(void);
void sys_deinit(void);
uint8_t *sys_get_frame_buffer(void);

byte wram_read(uint16_t addr);
void wram_write(uint16_t addr, byte val);