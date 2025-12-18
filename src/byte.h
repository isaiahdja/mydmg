#ifndef BYTE_H
#define BYTE_H

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t byte;
typedef bool bit;

static inline uintmax_t get_bits(
    uintmax_t num, unsigned int high, unsigned int low) {
    int width = high - low + 1;
    if (width <= 0)
        return 0;
    if (width >= 64)
        return num >> low;
    return (num >> low) & (((uintmax_t)0x1 << width) - 1);
}

static inline bit get_bit(uintmax_t num, unsigned int idx) {
    return (num >> idx) & 0x1;
}

static inline uintmax_t overlay_masked(
    uintmax_t base, uintmax_t over, uintmax_t mask) {
    return (base & ~mask) | (over & mask);
}

static inline uintmax_t set_bit(uintmax_t num, unsigned int idx, bit val) {
    if (val == 1)
        return  num | (uintmax_t)1 << idx;
    else
        return num &= ~((uintmax_t)1 << idx);
}

#endif