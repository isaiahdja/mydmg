#include "mbc1.h"
#include "cart.h"
#include "bus.h"

#include <stdio.h>

/* MBC1 (No RAM). */

#define BANK1_RW_MASK 0x1F
byte bank1_reg = 0x00;
#define BANK2_RW_MASK 0x03
byte bank2_reg = 0x00;
#define MODE_RW_MASK 0x01
byte mode_reg = 0x00;

byte mbc1_read(uint16_t addr)
{
    region_type region = get_addr_region(addr);
    byte bank_number = 0x00;
    if (region == BANK0) {
        if (mode_reg == 0x01)
            bank_number |= bank2_reg << 5;
    }
    else if (region == BANK1)
        bank_number |= (bank2_reg << 5) | (bank1_reg);
    else
        return 0xFF;

    bank_number &= (rom_banks - 1);
    uint32_t rom_addr = ((uint32_t)bank_number << 14) | get_bits(addr, 13, 0);

    if (rom_addr >= cart_rom_size) {
        printf("MBC1: Attempted to read beyond cartridge size!\n");
        return 0xFF;
    }
    return cart_rom[rom_addr];
}

void mbc1_write(uint16_t addr, byte val)
{
    int b_15_13 = get_bits(addr, 15, 13);
    if (b_15_13 == 1) {
        bank1_reg = overlay_masked(bank1_reg, val, BANK1_RW_MASK);
        if (bank1_reg == 0x00)
            bank1_reg = 0x01;
    }
    else if (b_15_13 == 2) {
        bank2_reg = overlay_masked(bank2_reg, val, BANK2_RW_MASK);
    }
    else if (b_15_13 == 3) {
        mode_reg = overlay_masked(mode_reg, val, MODE_RW_MASK);
    }
}