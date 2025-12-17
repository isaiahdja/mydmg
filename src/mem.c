#include "mem.h"
#include "main.h"
#include <stdio.h>
#include <stdint.h>
#include "input.h"

#define MEM_SIZE 0x10000
static uint8_t MEMORY[MEM_SIZE];

#define BANK_0_START  0x0000
#define BANK_0_SIZE          0x4000
#define BANK_1_START  0x4000
#define BANK_1_SIZE          0x4000
#define VRAM_START    0x8000
#define VRAM_SIZE            0x2000
#define EXT_RAM_START 0xA000
#define EXT_RAM_SIZE         0x2000
#define WRAM_START    0xC000
#define WRAM_SIZE            0x1000
#define ECHO_START    0xE000
#define ECHO_SIZE            0x1E00
#define OAM_START     0xFE00
#define OAM_SIZE             0x00A0
#define UNUSED_START  0xFEA0
#define UNUSED_SIZE          0x0060
#define IO_REGS_START 0xFF00
#define IO_REGS_SIZE         0x0080
#define HRAM_START    0xFF80
#define HRAM_SIZE            0x007F

/* R/W bitmasks : 0 = R, 1 = R/W. */
#define JOYP_REG 0xFF00
#define JOYP_RW_MASK    0x30
/* ... */
#define DIV_REG  0xFF04
#define DIV_RW_MASK     0x00
#define TIMA_REG 0xFF05
#define TIMA_RW_MASK    0xFF
#define TMA_REG  0xFF06
#define TMA_RW_MASK     0xFF
#define TAC_REG  0xFF07
#define TAC_RW_MASK     0x07
#define IF_REG   0xFF0F
/* ... */
#define LCDC_REG 0xFF40
#define STAT_REG 0xFF41
#define SCY_REG  0xFF42
#define SCX_REG  0xFF43
#define LY_REG   0xFF44
#define LYC_REG  0xFF45
#define DMA_REG  0xFF46
#define BGP_REG  0xFF47
#define OBP0_REG 0xFF48
#define OBP1_REG 0xFF49
#define WY_REG   0xFF4A
#define WX_REG   0xFF4B
/* ... */
#define IE_REG   0xFFFF

static uint64_t get_bits(uint64_t num, int high, int low);
static inline uint64_t get_bit(uint64_t num, int idx);

static void inline write_masked(uint16_t addr, uint8_t byte, uint8_t rw_mask);

static void io_write(uint16_t, uint8_t byte);
static void update_tac_caches(void);

/* Used as the internal system counter (counts M-cycles).
Game Boy logic is only ever concerned with the lower 14 bits. */
uint64_t counter;

/* Caches TAC bit 2. */
bool tac_enabled;
/* Caches the bit that should be read from the system counter
according to TAC bits 1 through 0 (clock select). */
int tac_counter_bit_idx;
/* Used to detect falling edge for timer behavior. */
int prev_timer_signal;
/* Used to implement overflow logic on the next M-cycle. */
bool timer_overflowed;
/* Used to save the value of TMA at overflow. */
uint8_t tma_overflow_save;

/* Initialize memory. */
bool mem_init()
{
    /* Map first two ROM banks to memory. */
    rom_copy(MEMORY, 0, BANK_0_SIZE + BANK_1_SIZE);

    /* DMG boot handoff state. */
    MEMORY[JOYP_REG] = 0xCF;
    /* ... */
    MEMORY[DIV_REG] = 0x18;
    MEMORY[TIMA_REG] = 0x00;
    MEMORY[TMA_REG] = 0x00;
    MEMORY[TAC_REG] = 0xF8; update_tac_caches();
    MEMORY[IF_REG] = 0xE1;
    /* ... */
    MEMORY[LCDC_REG] = 0x91;
    MEMORY[STAT_REG] = 0x81;
    MEMORY[SCY_REG] = 0x00;
    MEMORY[SCX_REG] = 0x00;
    MEMORY[LY_REG] = 0x91;
    MEMORY[LYC_REG] = 0x00;
    MEMORY[DMA_REG] = 0xFF;
    MEMORY[BGP_REG] = 0xFC;
    MEMORY[OBP0_REG] = 0x00;
    MEMORY[OBP1_REG] = 0x00;
    MEMORY[WY_REG] = 0x00;
    MEMORY[WX_REG] = 0x00;
    /* ... */
    MEMORY[IE_REG] = 0x00;

#if DEBUG
    mem_dump();
#endif

    return true;
}

/* Dump the full contents of memory. */
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

/* Read one byte from memory at addr. */
uint8_t mem_read(uint16_t addr)
{
    return MEMORY[addr];
}

/* Write byte to memory at addr. */
void mem_write(uint16_t addr, uint8_t byte)
{
    /* HRAM is always writeable. */
    if (addr >= HRAM_START && addr < HRAM_START + HRAM_SIZE) {
        MEMORY[addr] = byte;
        return;
    }

    if (false) /* TODO: Check DMA transfer status. */
        return;

    if (addr < BANK_1_START + BANK_1_SIZE) {
        /* TODO: Pass to MBC. */
        return;
    }
    else if (addr < VRAM_START + VRAM_SIZE) {
        /* TODO: Check PPU mode. */
    }
    else if (addr < EXT_RAM_START + EXT_RAM_SIZE) {
        /* TODO: Pass to MBC. */
        return;
    }
    else if (addr < WRAM_START + WRAM_SIZE)
        MEMORY[addr] = byte;
    else if (addr < ECHO_START + ECHO_SIZE)
        MEMORY[addr - 0x2000] = byte; /* Maps to WRAM */
    else if (addr < OAM_START + OAM_SIZE) {
        /* TODO: Check PPU mode. */
    }
    else if (addr < UNUSED_START + UNUSED_SIZE)
        return;
    else if (addr < IO_REGS_START + IO_REGS_SIZE || addr == IE_REG)
        io_write(addr, byte);
}

/* Simulate one M-cycle for the memory.
- Update I/O registers. */
void mem_tick()
{
    counter++;

    /* Model behavior of timer registers. */
    MEMORY[DIV_REG] = (uint8_t)(counter >> 6);

    if (timer_overflowed) {
        timer_overflowed = false;
        MEMORY[TIMA_REG] = tma_overflow_save;
        /* TODO: Request timer interrupt here. */
    }

    int next_timer_signal = get_bit(counter, tac_counter_bit_idx) & tac_enabled;
    if (next_timer_signal == 0 && prev_timer_signal == 1) {
        /* Falling edge detected. */
        if (++MEMORY[TIMA_REG] == 0) {
            /* Timer overflow.
            We load the current value of TMA to TIMA on the *next* M-cycle. */
            timer_overflowed = true;
            tma_overflow_save = MEMORY[TMA_REG];
        }
    }
    prev_timer_signal = next_timer_signal;
}

/* Retrieve bits high through low (inclusive) from num. */
static uint64_t get_bits(uint64_t num, int high, int low) {
    int width = high - low + 1;
    if (width >= 64)
        return num >> low;
    return (num >> low) & (((uint64_t)0x1 << width) - 1);
}

/* Retrieve bit idx from num. */
static inline uint64_t get_bit(uint64_t num, int idx) {
    return get_bits(num, idx, idx);
}

static void inline write_masked(uint16_t addr, uint8_t byte, uint8_t rw_mask) {
    MEMORY[addr] &= ~rw_mask;
    MEMORY[addr] |= (byte & rw_mask);
}

static void io_write(uint16_t addr, uint8_t byte)
{
    /* Enforce R/W bits and special write behavior for I/O registers. */
    switch (addr) {
        case JOYP_REG:
            write_masked(addr, byte, JOYP_RW_MASK);
            load_joyp_nibble();
            break;
        case DIV_REG:
            counter = 0;
            break;
        case TIMA_REG:
            write_masked(addr, byte, TIMA_RW_MASK);
            break;
        case TMA_REG:
            write_masked(addr, byte, TMA_RW_MASK);
            break;
        case TAC_REG:
            write_masked(addr, byte, TAC_RW_MASK);
            update_tac_caches();
            break;
        default:
            write_masked(addr, byte, 0xFF);
    }
}

void load_joyp_nibble()
{
    uint8_t byte = MEMORY[JOYP_REG];
    /* 0 = selected, 1 = not selected (active-low). */
    bool action = get_bit(byte, 5) == 0;
    bool direction = get_bit(byte, 4) == 0;
    write_masked(JOYP_REG, get_joyp_nibble(action, direction), 0x0F);
}

static void update_tac_caches()
{
    uint8_t byte = MEMORY[TAC_REG];
    /* 0 = disabled, 1 = enabled (active-high).*/
    tac_enabled = get_bit(byte, 2) == 1;
    switch (get_bits(byte, 1, 0)) {
        case 0x0: tac_counter_bit_idx = 7; break;
        case 0x1: tac_counter_bit_idx = 1; break;
        case 0x2: tac_counter_bit_idx = 3; break;
        case 0x3: tac_counter_bit_idx = 5; break;
    }
}

/* TODO: MBC scaffolding. */