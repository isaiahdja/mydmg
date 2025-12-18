#include "mbc0.h"
#include "cart.h"
#include "bus.h"

/* No MBC - Two ROM banks are directly mapped to memory. */

byte mbc0_read(uint16_t addr) {
    region_type region = get_addr_region(addr);
    if (region == BANK0 || region == BANK1)
        return cart_rom[addr];
    
    return 0xFF;
}

void mbc0_write(uint16_t addr, byte val) {
    return;
}