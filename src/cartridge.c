#include "cartridge.h"
#include "bus.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <stdio.h>

/* Cartridge, including any extra hardware. */

typedef enum {
    ROM_ONLY = 0x00,

    MBC1             = 0x01,
    MBC1_RAM         = 0x02,
    MBC1_RAM_BATTERY = 0x03,
    /* ... */
    MBC3_TIMER_BATTERY     = 0x0F,
    MBC3_TIMER_RAM_BATTERY = 0x10,
    MBC3                   = 0x11,
    MBC3_RAM               = 0x12,
    MBC3_RAM_BATTERY       = 0x13,
    /* ... */
} cart_type;
static cart_type type;

static byte *cart_rom;
static size_t cart_rom_size;
static unsigned int rom_banks_16kib;

static byte *cart_ram;
static size_t cart_ram_size;
static unsigned int ram_banks_8kib;
static bool has_ram, has_ram_battery, has_rtc;

static byte (*read_fn)(uint16_t);
static void (*write_fn)(uint16_t, byte);

/* */

static byte mbc0_read(uint16_t addr);
static void mbc0_write(uint16_t addr, byte val);

static void mbc1_init(void);
static byte mbc1_read(uint16_t addr);
static void mbc1_write(uint16_t addr, byte val);

typedef struct {
    byte secs;
    byte mins;
    byte hours;
    byte day_lo; byte day_hi;
} rtc_regs;
static rtc_regs mbc3_rtc_regs;
static void mbc3_init(void);
static byte mbc3_read(uint16_t addr);
static void mbc3_write(uint16_t addr, byte val);

static char *sav_path;
static char *rtc_path;

bool cart_init(const char *rom_path)
{
    cart_rom = SDL_LoadFile(rom_path, &cart_rom_size);
    if (cart_rom == NULL) {
        SDL_SetError("Failed to read the provided game file");
        return false;
    }
    if (cart_rom_size < (1 << 15)) {
        SDL_SetError("Provided game file is too small");
        return false;
    }

    /* Read title from cartridge header. */
    char title[17] = {0};
    strncpy((char *)&title, (const char *)(cart_rom + 0x134), 16);
    SDL_Log("Opened %s", title);
    
    /* Read MBC from cartridge header. */
    type = cart_rom[0x147];
    has_ram = false; has_ram_battery = false; has_rtc = false;
    switch (type) {
        case ROM_ONLY:
            SDL_Log("No MBC");
            read_fn = &mbc0_read;
            write_fn = &mbc0_write;
            break;
        
        case MBC1_RAM_BATTERY: has_ram_battery = true;
        case MBC1_RAM: has_ram = true;
        case MBC1:
            SDL_Log("MBC1");
            read_fn = &mbc1_read;
            write_fn = &mbc1_write;
            mbc1_init();
            break;

        case MBC3_TIMER_RAM_BATTERY: has_rtc = true;
        case MBC3_RAM_BATTERY: has_ram_battery = true;
        case MBC3_RAM: has_ram = true;
        case MBC3_TIMER_BATTERY: /* has_rtc cannot be set here due to previous fallthroughs. */
        case MBC3:
            if (type == MBC3_TIMER_BATTERY)
                has_rtc = true;
            SDL_Log("MBC3");
            read_fn = &mbc3_read;
            write_fn = &mbc3_write;
            mbc3_init();
            break;

        default:
            SDL_SetError("Unsupported cartridge type ([0x147] = %02X)", type);
            return false;
    }

    byte header_rom_val = cart_rom[0x148];
    rom_banks_16kib = 1 << (header_rom_val + 1);
    if (rom_banks_16kib > 512) {
        SDL_SetError("Invalid header ROM size (0x148 = %02X)", header_rom_val);
        return false;
    }
    if (rom_banks_16kib * (1 << 14) != cart_rom_size) {
        SDL_SetError("Header ROM size does not match the provided file");
        return false;
    }

    byte header_ram = cart_rom[0x149];
    switch (header_ram) {
        case 0x00: ram_banks_8kib =  0; break;
        case 0x02: ram_banks_8kib =  1; break;
        case 0x03: ram_banks_8kib =  4; break;
        case 0x04: ram_banks_8kib = 16; break;
        case 0x05: ram_banks_8kib =  8; break;
        default:
            SDL_SetError("Invalid header RAM size ([0x149] = %02X)", header_ram);
            return false;
    }
    if (has_ram != (ram_banks_8kib > 0)) {
        SDL_SetError("Header is inconsistent");
            return false;
    } 
    cart_ram_size = ram_banks_8kib * (1 << 13);
    cart_ram = malloc(cart_ram_size);

    /* Define save paths. */
    int rom_path_len = strlen(rom_path);

    int sav_path_len = rom_path_len + 4 + 1; /* .sav + \0 */
    sav_path = malloc(sav_path_len);
    strcpy(sav_path, rom_path);

    int rtc_path_len = rom_path_len + 4 + 1; /* .rtc + \0 */
    rtc_path = malloc(rtc_path_len);
    strcpy(rtc_path, rom_path);

    int last_dot = -1;
    for (int i = 0; i < rom_path_len; i++) {
        if (rom_path[i] == '.')
            last_dot = i;
        else if (rom_path[i] == '/' || rom_path[i] == '\\')
            last_dot = -1;
    }
    if (last_dot != -1) {
        strcpy(sav_path + last_dot, ".sav");
        strcpy(rtc_path + last_dot, ".rtc");
    }
    else {
        strcpy(sav_path + rom_path_len, ".sav");
        strcpy(rtc_path + rom_path_len, ".rtc");
    }

    SDL_Log("%ld KiB ROM", cart_rom_size / (1 << 10));
    if (has_ram)
        SDL_Log("+ %ld KiB RAM", cart_ram_size / (1 << 10));
    if (has_ram_battery) {
        SDL_Log("+ Battery");
        
        /* Detect .sav file. */
        size_t sav_data_size;
        byte *sav_data = SDL_LoadFile(sav_path, &sav_data_size);
        if (sav_data != NULL) {
            SDL_Log("Detected .sav file %s", sav_path);
            if (sav_data_size == cart_ram_size) {
                memcpy(cart_ram, sav_data, cart_ram_size);
                SDL_Log("Loaded .sav file");
            }
            else {
                SDL_Log("Invalid .sav file");
            }
            SDL_free(sav_data);
        }
    }
    if (has_rtc) {
        SDL_Log("+ Real-time clock");
        
        /* TODO: Detect .rtc file. */
    }

    return true;
}

void cart_deinit()
{
    if (has_ram_battery) {
        /* Write .sav file. */
        /* TODO: Check success status (?) */
        SDL_SaveFile(sav_path, cart_ram, cart_ram_size);
    }
    if (has_rtc) {
        /* TODO: Write to .rtc file. */
        /* TODO: Check success status (?) */
    }

    SDL_free(cart_rom);
    free(cart_ram);
    free(sav_path);
    free(rtc_path);
}

byte cart_read(uint16_t addr) {
    return read_fn(addr);
}
void cart_write(uint16_t addr, byte val) {
    write_fn(addr, val);
}

/* No MBC - 2 ROM banks are directly mapped to memory. */
/* "Optionally up to 8 KiB of RAM could be connected at $A000-BFFF, using a
   discrete logic decoder in place of a full MBC chip." */

static byte mbc0_read(uint16_t addr) {
    region_type region = get_addr_region(addr);
    if (region == BANK0 || region == BANK1)
        return cart_rom[addr];
    if (has_ram && region == EXT_RAM) {
        return cart_ram[addr - EXT_RAM_START];
    }
    
    return 0xFF;
}
static void mbc0_write(uint16_t addr, byte val) {
    region_type region = get_addr_region(addr);
    if (has_ram && region == EXT_RAM) {
        cart_ram[addr - EXT_RAM_START] = val;
    }
    return;
}

/* MBC1. */
/* TODO: MBC1M (Multi-cart) (?) */

static bool mbc1_ram_enabled;
static byte mbc1_bank0_reg;
static byte mbc1_bank1_reg;
static bit mbc1_mode;

static void mbc1_init()
{
    mbc1_ram_enabled = false;
    mbc1_bank0_reg = 0x00;
    mbc1_bank1_reg = 0x00;
    mbc1_mode = 0;
}
static uint16_t mbc1_cart_ram_addr(uint16_t addr) {
    uint8_t bank = mbc1_mode == 1 ? mbc1_bank1_reg : 0x00;
    bank &= (ram_banks_8kib - 1);
    uint16_t ram_addr = ((uint16_t)bank << 13) | get_bits(addr, 12, 0);
    return ram_addr;
}
static byte mbc1_read(uint16_t addr)
{
    byte mbc1_bank0_reg_adj = mbc1_bank0_reg;
    if (mbc1_bank0_reg_adj == 0x00)
        mbc1_bank0_reg_adj = 0x01;

    region_type region = get_addr_region(addr);
    if (region == BANK0 || region == BANK1) {
        uint8_t bank = 0x00;
        if (region == BANK0 && mbc1_mode == 1) {
            bank = ((uint32_t)mbc1_bank1_reg << 5);
        }
        else if (region == BANK1) {
            bank = ((uint32_t)mbc1_bank1_reg << 5) | mbc1_bank0_reg_adj;
        }
        bank &= (rom_banks_16kib - 1);
        uint32_t rom_addr = ((uint32_t)bank << 14) | get_bits(addr, 13, 0);
        return cart_rom[rom_addr];
    }
    else if (region == EXT_RAM && has_ram && mbc1_ram_enabled) {
        return cart_ram[mbc1_cart_ram_addr(addr)];
    }
    else
        return 0xFF;
}
static void mbc1_write(uint16_t addr, byte val)
{
    switch (get_bits(addr, 15, 13)) {
        case 0:
            mbc1_ram_enabled = ((val & 0x0F) == 0xA);
            break;
        case 1: mbc1_bank0_reg   = val & 0x1F; break;
        case 2: mbc1_bank1_reg   = val & 0x03; break;
        case 3: mbc1_mode        = val & 0x01; break;
        case 5: /* External RAM region */
            if (has_ram && mbc1_ram_enabled) {
                cart_ram[mbc1_cart_ram_addr(addr)] = val;
            }
            break;
    }
}

/* MBC3. */

static bool mbc3_ram_timer_enabled;
static byte mbc3_rom_bank_reg;
static byte mbc3_ram_timer_select;

static void mbc3_init()
{
    mbc3_ram_timer_enabled = false;
    mbc3_rom_bank_reg = 0x00;
    mbc3_ram_timer_select = 0x00;

    /* TODO: Check if RTC registers were already loaded. */
    mbc3_rtc_regs.secs = 0x00;
    mbc3_rtc_regs.mins = 0x00;
    mbc3_rtc_regs.hours = 0x00;
    mbc3_rtc_regs.day_lo = 0x00; mbc3_rtc_regs.day_hi = 0x00;
}
static byte mbc3_read(uint16_t addr)
{
    byte mbc3_rom_bank_reg_adj = mbc3_rom_bank_reg;
    if (mbc3_rom_bank_reg_adj == 0x00)
        mbc3_rom_bank_reg_adj = 0x01;

    region_type region = get_addr_region(addr);
    if (region == BANK0) {
        return cart_rom[addr];
    }
    else if (region == BANK1) {
        uint8_t bank = mbc3_rom_bank_reg_adj & (rom_banks_16kib - 1);
        uint32_t cart_addr = ((uint32_t)bank << 14) | get_bits(addr, 13, 0);
        return cart_rom[cart_addr];
    }
    else if (region == EXT_RAM && mbc3_ram_timer_enabled) {
        if (has_ram && mbc3_ram_timer_select < 0x08) {
            uint8_t bank = mbc3_ram_timer_select & (ram_banks_8kib - 1);
            uint32_t cart_addr = ((uint32_t)bank << 13) | get_bits(addr, 12, 0);
            return cart_ram[cart_addr];
        }
        else if (has_rtc && mbc3_ram_timer_select >= 0x08) {
            switch (mbc3_ram_timer_select) {
                case 0x08: return mbc3_rtc_regs.secs;
                case 0x09: return mbc3_rtc_regs.mins;
                case 0x0A: return mbc3_rtc_regs.hours;
                case 0x0B: return mbc3_rtc_regs.day_lo;
                case 0x0C: return mbc3_rtc_regs.day_hi;
            }
        }
    }

    return 0xFF;
}
static void mbc3_write(uint16_t addr, byte val)
{
    switch (get_bits(addr, 15, 13)) {
        case 0:
            mbc3_ram_timer_enabled = ((val & 0x0F) == 0x0A);
            break;
        case 1:
            mbc3_rom_bank_reg = val & 0x7F;
            break;
        case 2:
            mbc3_ram_timer_select = val & 0x0F;
            break;
        case 3:
            /* TODO: Latch clock data. */
            break;
        case 5: /* External RAM (or RTC register) region */
            if (!mbc3_ram_timer_enabled)
                return;

            if (has_ram && mbc3_ram_timer_select < 0x08) {
                uint8_t bank = mbc3_ram_timer_select & (ram_banks_8kib - 1);
                uint32_t cart_addr = ((uint32_t)bank << 13) | get_bits(addr, 12, 0);
                cart_ram[cart_addr] = val;
            }
            else if (has_rtc && mbc3_ram_timer_select >= 0x08) {
                /* TODO: Check value validity (?) */
                switch (mbc3_ram_timer_select) {
                    case 0x08: mbc3_rtc_regs.secs   = val; break;
                    case 0x09: mbc3_rtc_regs.mins   = val; break;
                    case 0x0A: mbc3_rtc_regs.hours  = val; break;
                    case 0x0B: mbc3_rtc_regs.day_lo = val; break;
                    case 0x0C: mbc3_rtc_regs.day_hi = val; break;
                }
            }
            break;
    }
}