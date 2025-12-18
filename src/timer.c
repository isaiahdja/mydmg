#include "timer.h"
#include "system.h"
#include <stdint.h>

/* Internal T-cycle counter. */
static uint64_t system_counter;

#define DIV_RW_MASK  0x00
#define TIMA_RW_MASK 0xFF
#define TMA_RW_MASK  0xFF
#define TAC_RW_MASK  0x07

static byte div_reg;
static byte tima_reg, tma_reg, tac_reg;

/* Caches bit 2 of TAC. */
static bit tac_enable;
/* Effectively caches bits 1 through 0 of TAC by storing the corresponding bit
   of the system counter to check. */
static unsigned int tac_counter_bit_idx;

/* Used for falling-edge detection. */
static bit prev_timer_signal = 0;
/* Used to delay overflow logic to the next M-cycle. */
static bool timer_overflowed = false;
static byte tma_overflow_save;

static void update_tac_caches();

bool timer_init(void)
{
    /* DMG boot handoff state. */
    div_reg = 0xAB;
    tima_reg = 0x00, tma_reg = 0x00, tac_reg = 0xF8;
    update_tac_caches();

    return true;
}

void timer_tick(void)
{
    system_counter += T_M_RATIO;
    div_reg = system_counter >> 8;

    if (timer_overflowed) {
        timer_overflowed = false;
        tima_reg = tma_overflow_save;
        /* TODO: Request timer interrupt here. */
    }

    int next_timer_signal =
        get_bit(system_counter, tac_counter_bit_idx) & tac_enable;
    if (next_timer_signal == 0 && prev_timer_signal == 1) {
        /* Falling edge detected. */
        if (++tima_reg == 0) {
            /* Timer overflow.
            We load the current value of TMA to TIMA on the *next* M-cycle. */
            timer_overflowed = true;
            tma_overflow_save = tma_reg;
        }
    }
    prev_timer_signal = next_timer_signal;
}

static void update_tac_caches()
{
    /* For the enable bit, 0 = disabled, 1 = enabled. */
    tac_enable = get_bit(tac_reg, 2) == 1;
    switch (get_bits(tac_reg, 1, 0)) {
        case 0x0: tac_counter_bit_idx = 9; break; /* 2^12 Hz | 256 M-cycles */
        case 0x1: tac_counter_bit_idx = 3; break; /* 2^18 Hz |   4 M-cycles */
        case 0x2: tac_counter_bit_idx = 5; break; /* 2^16 Hz |  16 M-cycles */
        case 0x3: tac_counter_bit_idx = 7; break; /* 2^14 Hz |  64 M-cycles */
    }
}

byte timer_div_read() {
    return div_reg;
}
void timer_div_write(byte val) {
    /* Writing any value resets the system counter. */
    system_counter = 0;
}

byte timer_tima_read() {
    return tima_reg;
}
void timer_tima_write(byte val) {
    tima_reg = overlay_masked(tima_reg, val, TIMA_RW_MASK);
}

byte timer_tma_read() {
    return tma_reg;
}
void timer_tma_write(byte val) {
    tma_reg = overlay_masked(tma_reg, val, TMA_RW_MASK);
}

byte timer_tac_read() {
    return tac_reg;
}
void timer_tac_write(byte val) {
    tac_reg = overlay_masked(tac_reg, val, TAC_RW_MASK);
    update_tac_caches();
}