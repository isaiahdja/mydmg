#pragma once
#include "byte.h"
#include <stdint.h>
#include <stdbool.h>

extern byte *cart_rom;
extern uint32_t rom_banks;

bool cart_init(const char *rom_path);
void cart_deinit(void);

byte cart_read(uint16_t addr);
void cart_write(uint16_t addr, byte val);