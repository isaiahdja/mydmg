#include "mem.h"
#include "main.h"
#include <stdio.h>
#include <stdint.h>

#define MEM_SIZE 0x10000
static uint8_t MEMORY[MEM_SIZE];

#define BANK_0_START    0x0000
#define BANK_0_SIZE                 0x4000
#define BANK_1_START    0x4000
#define BANK_1_SIZE                 0x4000
#define VRAM_START      0x8000
#define VRAM_SIZE                   0x2000
#define EXT_RAM_START   0xA000
#define EXT_RAM_SIZE                0x2000
#define WRAM_START      0xC000
#define WRAM_SIZE                   0x1000
#define ECHO_START      0xE000
#define ECHO_SIZE                   0x1E00
#define OAM_START       0xFE00
#define OAM_SIZE                    0x00A0
#define UNUSED_START    0xFEA0
#define UNUSED_SIZE                 0x0060
#define IO_REGS_START   0xFF00
#define IO_REGS_SIZE                0x0080
#define HRAM_START      0xFF80
#define HRAM_SIZE                   0x007F
#define IE_REG          0xFFFF

#define JOYP_REG    0xFF00
/* ... */
#define DIV_REG     0xFF40
#define TIMA_REG    0xFF05
#define TMA_REG     0xFF06
#define TAC_REG     0xFF07
#define IF_REG      0xFF0F
/* ... */
#define LCDC_REG    0xFF40
#define STAT_REG    0xFF41
#define SCY_REG     0xFF42
#define SCX_REG     0xFF43
#define LY_REG      0xFF44
#define LYC_REG     0xFF45
#define DMA_REG     0xFF46
#define BGP_REG     0xFF47
#define OBP0_REG    0xFF48
#define OBP1_REG    0xFF49
#define WY_REG      0xFF4A
#define WX_REG      0xFF4B
/* ... */

/* Initialize memory. */
bool mem_init()
{
    /* Map first two ROM banks to memory. */
    size_t read_size = BANK_0_SIZE + BANK_1_SIZE;
    size_t read = SDL_ReadIO(
        rom_io, MEMORY + BANK_0_START, read_size);
    if (read < read_size) {
        SDL_SetError("Failed reading ROM file");
        return false;
    }

    char title[16];
    memcpy(&title, MEMORY + 0x0134, 16);
    SDL_Log("Opened %s", title);

    /* TODO: DMG boot handoff state. */

#if DEBUG
    mem_dump();
#endif

    return true;
}

/* Dump the full contents of the memory. */
void mem_dump()
{
    static int count = 0;
    count++;

    size_t line_max = 64;
    char line[line_max];
    int written = 0;
    for (int i = 0; i < MEM_SIZE; i++) {
        if (i == 0) 
            written += snprintf(line, line_max, "MEMORY DUMP %d:", count);
        if (i % 16 == 0) {
            SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "%s", line);
            memset(line, 0, line_max);
            written = snprintf(line, line_max, "%04X |", i);
        }
        written += snprintf(line + written, line_max, " %02X", MEMORY[i]);
    }
}

/* Read one byte from memory at [addr]. */
uint8_t mem_read(uint16_t addr)
{
    if (addr >= MEM_SIZE) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Invalid memory read");
        return 0;
    }
    return MEMORY[addr];
}

/* Write byte to memory at [addr]. */
void mem_write(uint16_t addr, uint8_t byte)
{
    if (addr >= MEM_SIZE) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Invalid memory write");
        return;
    }
    MEMORY[addr] = byte;
}