#include "cart.h"
#include "SDL3/SDL.h"
#include <string.h>

#include "mbc/mbc0.h"

typedef enum {
    ROM_ONLY = 0x00,
    /* ... */
} cart_type;

byte *cart_rom;
size_t cart_rom_size;

static cart_type type;

bool cart_init(const char *rom_path)
{
    cart_rom = SDL_LoadFile(rom_path, &cart_rom_size);
    if (cart_rom == NULL) {
        SDL_SetError("Failed to the read provided file");
        return false;
    }

    char title[17] = {0};
    strncpy((char *)&title, cart_rom + 0x134, 16);
    SDL_Log("Opened %s", title);
    
    type = cart_rom[0x147];
    
    return true;
}

void cart_deinit()
{
    SDL_free(cart_rom);
}

byte cart_read(uint16_t addr)
{
    switch (type) {
        case ROM_ONLY: mbc0_read(addr); break;
    }
}

void cart_write(uint16_t addr, byte val)
{
    switch (type) {
        case ROM_ONLY: mbc0_write(addr, val); break;
    }
}