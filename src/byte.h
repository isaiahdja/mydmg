#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Bit and byte utilities. */

typedef uint8_t byte;
typedef bool bit;

/* Retrieve bits high through low from num (fully right-shifted). */
static inline uintmax_t get_bits(
    uintmax_t num, unsigned int high, unsigned int low) {
    int width = high - low + 1;
    if (width <= 0)
        return 0;
    return (num >> low) & (((uintmax_t)0x1 << width) - 1);
}
static inline bit get_bit(uintmax_t num, unsigned int idx) {
    return (num >> idx) & 0x1;
}

/* For every bit in base, replace it with the corresponding bit in over iff. the
   corresponding bit in mask is 1.
   Can be used to enforce R/W bits within a byte. */
static inline uintmax_t overlay_masked(
    uintmax_t base, uintmax_t over, uintmax_t mask) {
    return (base & ~mask) | (over & mask);
}
static inline uintmax_t set_bit(uintmax_t num, unsigned int idx, bit val) {
    uintmax_t mask = (uintmax_t)1 << idx;
    if (val == 1)
        return  num | mask;
    else
        return num & (~mask);
}

/* Returns true if there exists a bit in prev such that the bit in prev is 1 and
   the corresponding bit in next is 0. */
static inline bool detect_falling_edge(uintmax_t prev, uintmax_t next) {
    return (prev & ~next) != (uintmax_t)0;
}

static inline byte get_hi_byte(uint16_t num) {
    return (num >> 8) & 0xFF;
}
static inline byte get_lo_byte(uint16_t num) {
    return num & 0xFF;
}
static inline uint16_t set_hi_byte(uint16_t num, byte hi) {
    return (num & 0x00FF) | ((uint16_t)hi << 8);
}
static inline uint16_t set_lo_byte(uint16_t num, byte lo) {
    return (num & 0xFF00) | lo;
}

static inline byte get_hi_nibble(byte num) {
    return (num >> 4) & 0xF;
}
static inline byte get_lo_nibble(byte num) {
    return num & 0xF;
}
static inline byte set_hi_nibble(byte num, byte hi) {
    return (num & 0xF) | ((hi & 0xF) << 4);
}
static inline byte set_lo_nibble(byte num, byte lo) {
    return (num & 0xF0) | (lo & 0xF);
}