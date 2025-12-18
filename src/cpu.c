#include "cpu.h"
#include "bus.h"

static byte hram[HRAM_SIZE];

static byte a_reg;
bit z_flag, n_flag, h_flag, c_flag;
static byte b_reg, c_reg;
static byte d_reg, e_reg;
static byte h_reg, l_reg;
static uint16_t sp_reg;
static uint16_t pc_reg;

bool cpu_init(void)
{
    /* DMG boot handoff state. */
    a_reg = 0x01;
    /* TODO: Check header checksum (?) */
    z_flag = 1; n_flag = 0, h_flag = 0; c_flag = 0;
    b_reg = 0x00; c_reg = 0x13;
    d_reg = 0x00; e_reg = 0xD8;
    h_reg = 0x01; l_reg = 0x4D;
    sp_reg = 0xFFFE;
    pc_reg = 0x0100;

    return true;
}

void cpu_tick(void)
{

}

byte hram_read(uint16_t addr) {
    return hram[addr - HRAM_START];
}

void hram_write(uint16_t addr, byte val) {
    hram[addr - HRAM_START] = val;
}