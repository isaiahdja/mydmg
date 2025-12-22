#pragma once
#include "byte.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    byte a_reg;
    bit z_flag, n_flag, h_flag, c_flag;
    byte b_reg, c_reg;
    byte d_reg, e_reg;
    byte h_reg, l_reg;
    uint16_t sp_reg;
    uint16_t pc_reg;

    bit ime_flag;
} cpu_state;

typedef uint8_t (*read_fn)(uint16_t);
typedef void (*write_fn)(uint16_t, uint8_t);
typedef bool (*receive_int_fn)(uint16_t*);

#ifndef CPU_TEST
bool cpu_init(void);
#endif
void cpu_tick(void);

#ifndef CPU_TEST
byte hram_read(uint16_t addr);
void hram_write(uint16_t addr, byte val);
#endif

bool cpu_test_init(read_fn _read, write_fn _write, receive_int_fn _receive_int);
cpu_state cpu_test_get_state(void);
void cpu_test_set_state(cpu_state _state);