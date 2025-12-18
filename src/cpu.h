#pragma once
#include "byte.h"
#include <stdint.h>
#include <stdbool.h>

bool cpu_init(void);
void cpu_tick(void);

byte hram_read(uint16_t addr);
void hram_write(uint16_t addr, byte val);