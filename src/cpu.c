#include "cpu.h"
#include "bus.h"

#include <stdio.h>

/* Central Processing Unit core. */

/* High RAM. */
static byte hram[HRAM_SIZE];

static byte a_reg;
static bit z_flag, n_flag, h_flag, c_flag;
static byte b_reg, c_reg;
static byte d_reg, e_reg;
static byte h_reg, l_reg;
static uint16_t sp_reg;
static uint16_t pc_reg;

static bit ime_flag;
static byte z_latch, w_latch;
static byte instr_reg;

static void (*instr_func)(void);
static int instr_cycle;
static bool instr_complete;

static void fetch_and_decode(void);

static void nop(void);

bool cpu_init(void)
{
    /* DMG boot handoff state. */
    a_reg = 0x01;
    /* TODO: Check header checksum (?) */
    z_flag = 1, n_flag = 0, h_flag = 0, c_flag = 0;
    b_reg = 0x00; c_reg = 0x13;
    d_reg = 0x00; e_reg = 0xD8;
    h_reg = 0x01; l_reg = 0x4D;
    sp_reg = 0xFFFE;
    pc_reg = 0x0100;
    ime_flag = 0;

    fetch_and_decode();

    return true;
}

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
        if (ime_flag == 1 && interrupt_send_interrupt(&jump_vec)) {
            /* TODO: Interrupt received. */
        }
        fetch_and_decode(); /* Resets instr_cycle and instr_complete. */
    }
    else
        instr_cycle++;
}

byte hram_read(uint16_t addr) {
    return hram[addr - HRAM_START];
}

void hram_write(uint16_t addr, byte val) {
    hram[addr - HRAM_START] = val;
}

bit cpu_get_ime() {
    return ime_flag;
}

static void fetch_and_decode()
{
    instr_reg = bus_read_cpu(pc_reg++);

    if (instr_reg == 0x00) {
        instr_func = &nop;
    }
     
    instr_cycle = 0;
    instr_complete = false;
}

static void nop() {
    instr_complete = true;
}