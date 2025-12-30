#pragma once
#include "byte.h"
#include <stdint.h>

byte mbc1_read(uint16_t addr);
void mbc1_write(uint16_t addr, byte val);