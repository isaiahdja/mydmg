#include "cpu.h"
#include <stdint.h>
#include <SDL3/SDL.h>
#include "main.h"

static uint8_t reg_A;
static bool flag_Z, flag_N, flag_H, flag_C;
static uint8_t reg_B, reg_C;
static uint8_t reg_D, reg_E;
static uint8_t reg_H, reg_L;
static uint16_t reg_SP;
static uint16_t reg_PC;
static bool flag_IME;

/* Initialize CPU */
bool cpu_init()
{
    /* DMG boot handoff state */
    reg_A = 0x01;
    flag_Z = true;
    flag_N = false;
    flag_H = true; /* TODO: Checksum (?) */
    flag_C = true;
    reg_B = 0x00;
    reg_C = 0x13;
    reg_D = 0x00;
    reg_E = 0xD8;
    reg_H = 0x01;
    reg_L = 0x4D;
    reg_PC = 0x0100;
    reg_SP = 0xFFFE;
    flag_IME = false;

#if DEBUG
    cpu_dump();
#endif
    return true;
}

void cpu_dump()
{
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM,
        "A : %02X\n"
        "B : %02X | C : %02X\n"
        "D : %02X | E : %02X\n"
        "H : %02X | L : %02X\n"
        "SP : %04X\n"
        "PC : %04X\n"
        "Z : %d | N : %d | H : %d | C : %d\n"
        "IME : %d\n",
        reg_A, reg_B, reg_C, reg_D, reg_E, reg_H, reg_L, reg_SP, reg_PC,
        flag_Z, flag_N, flag_H, flag_C, flag_IME
    );
}