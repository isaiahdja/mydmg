#include "cart.h"
#include "SDL3/SDL.h"
#include <string.h>
#include <SDL3/SDL.h>

#include "mbc/mbc0.h"
#include "mbc/mbc1.h"

/* Cartridge. */

typedef enum {
    ROM_ONLY = 0x00,
    MBC1     = 0x01,
    /* ... */
} cart_type;

byte *cart_rom;
size_t cart_rom_size;
uint32_t rom_banks;

static cart_type type;

bool cart_init(const char *rom_path)
{
    cart_rom = SDL_LoadFile(rom_path, &cart_rom_size);
    if (cart_rom == NULL) {
        SDL_SetError("Failed to the read provided file");
        return false;
    }

    /* Read title from cartridge header. */
    char title[17] = {0};
    strncpy((char *)&title, cart_rom + 0x134, 16);
    SDL_Log("Opened %s", title);
    
    /* Read MBC from cartridge header. */
    type = cart_rom[0x147];
    switch (type) {
        case ROM_ONLY: SDL_Log("No MBC"); break;
        case MBC1:     SDL_Log("MBC1");  break;
        default:
            SDL_SetError("Unsupported cartridge type ([0x147] = %02X)", type);
            return false;
    }
    rom_banks = 1 << (cart_rom[0x148] + 1);

    return true;
}

void cart_deinit()
{
    SDL_free(cart_rom);
}

/* Route read to the current MBC. */
byte cart_read(uint16_t addr)
{
    switch (type) {
        case ROM_ONLY: return mbc0_read(addr);
        case MBC1:     return mbc1_read(addr);
    }
    return 0xFF;
}

/* Route write to the current MBC. */
void cart_write(uint16_t addr, byte val)
{
    switch (type) {
        case ROM_ONLY: mbc0_write(addr, val); break;
        case MBC1:     mbc1_write(addr, val); break;
    }
}