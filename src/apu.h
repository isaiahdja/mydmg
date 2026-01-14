#include "byte.h"
#include <stdint.h>
#include <stdbool.h>

bool apu_init(void);
void apu_tick(void);

void incr_div_apu(void);

byte apu_ch1_read(uint16_t addr);
void apu_ch1_write(uint16_t addr, byte val);

byte apu_ch2_read(uint16_t addr);
void apu_ch2_write(uint16_t addr, byte val);

byte apu_ctrl_read(uint16_t addr);
void apu_ctrl_write(uint16_t addr, byte val);