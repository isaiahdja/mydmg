#pragma once
#include "byte.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t af_reg;
    uint16_t bc_reg;
    uint16_t de_reg;
    uint16_t hl_reg;
    uint16_t sp_reg;
    uint16_t pc_reg;

    bit ime_flag;
} cpu_state;

typedef uint8_t (*read_fn)(uint16_t);
typedef void (*write_fn)(uint16_t, uint8_t);
typedef bool (*pending_int_fn)(void);
typedef bool (*receive_int_fn)(uint16_t*);

#ifndef CPU_TEST
bool cpu_init(void);
#endif
void cpu_tick(void);

#ifndef CPU_TEST
byte hram_read(uint16_t addr);
void hram_write(uint16_t addr, byte val);
#endif

bool cpu_test_init(read_fn _read, write_fn _write,
    pending_int_fn _pending_int, receive_int_fn _receive_int);
cpu_state cpu_test_get_state(void);
void cpu_test_set_state(cpu_state _state);