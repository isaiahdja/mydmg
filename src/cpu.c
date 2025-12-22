#include "cpu.h"
#ifndef CPU_TEST
#include "bus.h"
#include "interrupt.h"
#endif

#include <stdio.h>

/* Central Processing Unit core. */

/* High RAM. */
#ifndef CPU_TEST
static byte hram[HRAM_SIZE];
#endif

static cpu_state state;
static byte z_latch, w_latch;
static byte instr_reg;

static void (*instr_func)(void);
static int instr_cycle;
static bool instr_complete;

static void fetch_and_decode(void);

static void nop(void);

static read_fn memory_read;
static write_fn memory_write;
static receive_int_fn receive_int;

#ifndef CPU_TEST
bool cpu_init(void)
{
    memory_read = &bus_read_cpu;
    memory_write = &bus_write_cpu;
    receive_int = &interrupt_send_interrupt;

    /* DMG boot handoff state. */
    state = (cpu_state){
        0x01,
        1, 0, 0, 0,
        0x00, 0x13,
        0x00, 0xD8,
        0x01, 0x4D,
        0xFFFE,
        0x0100,
        0
    };

    fetch_and_decode();

    return true;
}
#endif

void cpu_tick(void)
{
    /* The current instruction function will set instr_completed to true if it
       has completed, allowing the next instruction to be fetched. This models
       the CPU fetch/execute overlap.
       Note that the final cycle of an instruction must not use the memory bus,
       as it is reserved for fetching. */
    instr_func();

    if (instr_complete) {
        uint16_t jump_vec;
        if (state.ime_flag == 1 && receive_int(&jump_vec)) {
            /* TODO: Interrupt received. */
        }
        fetch_and_decode(); /* Resets instr_cycle and instr_complete. */
    }
    else
        instr_cycle++;
}

#ifndef CPU_TEST
byte hram_read(uint16_t addr) {
    return hram[addr - HRAM_START];
}

void hram_write(uint16_t addr, byte val) {
    hram[addr - HRAM_START] = val;
}
#endif

static void fetch_and_decode()
{
    instr_cycle = 0;
    instr_complete = false;
    instr_func = &nop;
    
    instr_reg = memory_read(state.pc_reg++);

    if (instr_reg == 0x00) {
        instr_func = &nop;
        return;
    }
}

static void nop() {
    instr_complete = true;
}

bool cpu_test_init(read_fn _read, write_fn _write, receive_int_fn _receive_int)
{
    memory_read = _read;
    memory_write = _write;
    receive_int = _receive_int;

    instr_func = &nop;
    instr_cycle = 0;
    instr_complete = false;

    return true;
}
cpu_state cpu_test_get_state() {
    return state;
}
void cpu_test_set_state(cpu_state _state) {
    state = _state;
}