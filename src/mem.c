#include "mem.h"
#include "main.h"
#include <stdio.h>
#include <stdint.h>

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
#define UNUSED_SIZE                0x0060
#define IO_REGS_START   0xFF00
#define IO_REGS_SIZE                0x0080
#define HRAM_START      0xFF80
#define HRAM_SIZE                   0x007F
#define IE_REG_START    0xFFFF
#define IE_REG_SIZE                 0x0001

#define MEM_SIZE 0x10000
static uint8_t MEMORY[MEM_SIZE];

#define mem_dump_default() mem_dump(16, "memdump")

bool mem_init()
{
#if DEBUG
    SDL_RemovePath("memdump");
#endif

    /* Map ROM into memory */
    size_t read_size = BANK_0_SIZE + BANK_1_SIZE;
    size_t read = SDL_ReadIO(
        rom_io, MEMORY + BANK_0_START, read_size);
    if (read < read_size) {
        SDL_SetError("Failed reading ROM file");
        return false;
    }
    mem_dump_default();

    char title[16];
    mem_read((void*)&title, 0x0134, 16);
    SDL_Log("Opened %s", title);

    return true;
}

void mem_dump(int bytes_per_line, const char *file_path)
{
    static int count = 0;
    count++;

    SDL_IOStream *io = SDL_IOFromFile(file_path, "a");
    if (io == NULL)
        return;

    for (int i = 0; i < MEM_SIZE; i++) {
        if (i == 0) 
            SDL_IOprintf(io, "MEMORY DUMP %d:",count );
        if (i % bytes_per_line == 0)
            SDL_IOprintf(io, "\n0x%04X ", i);
        SDL_IOprintf(io, "%02X ", MEMORY[i]);
    }
    SDL_IOprintf(io, "\n\n");

    SDL_CloseIO(io);
}

void mem_read(void *dst, uint16_t start, size_t bytes)
{
    if (start + bytes > MEM_SIZE) {
        SDL_LogCritical(
            SDL_LOG_CATEGORY_APPLICATION, "Memory read is out-of-bounds");
        return;
    }
    memcpy(dst, MEMORY + start, bytes);
}

void mem_write(void *src, uint16_t start, size_t bytes)
{
    if (start + bytes > MEM_SIZE) {
        SDL_LogCritical(
            SDL_LOG_CATEGORY_APPLICATION, "Memory write is out-of-bounds");
        return;
    }
    memcpy(MEMORY + start, src, bytes);
}