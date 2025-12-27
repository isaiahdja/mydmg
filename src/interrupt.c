#include "interrupt.h"
#include "bus.h"
#include "cpu.h"

/* Interrupt controller. */

#define JUMP_VEC_VBLANK 0x0040
#define JUMP_VEC_STAT   0x0048
#define JUMP_VEC_TIMER  0x0050
#define JUMP_VEC_SERIAL 0x0058
#define JUMP_VEC_JOYPAD 0x0060

#define IF_RW_MASK 0x1F
#define IE_RW_MASK 0x1F

static byte if_reg, ie_reg;

#define NUM_INTERRUPTS 5

static uint16_t jump_vecs[NUM_INTERRUPTS] = {
    [INT_VBLANK] = JUMP_VEC_VBLANK,
    [INT_STAT]   = JUMP_VEC_STAT,
    [INT_TIMER]  = JUMP_VEC_TIMER,
    [INT_SERIAL] = JUMP_VEC_SERIAL,
    [INT_JOYPAD] = JUMP_VEC_JOYPAD
};

bool interrupt_init(void)
{
    /* DMG boot handoff state. */
    if_reg = 0xE1, ie_reg = 0x00;

    return true;
}

byte interrupt_if_read() {
    return if_reg;
}
void interrupt_if_write(byte val) {
    if_reg = overlay_masked(if_reg, val, IF_RW_MASK);
}

byte interrupt_ie_read() {
    return ie_reg;
}
void interrupt_ie_write(byte val) {
    ie_reg = overlay_masked(ie_reg, val, IE_RW_MASK);
}

bool interrupt_send_interrupt(uint16_t *jump_vec)
{
    for (int i = 0; i < NUM_INTERRUPTS; i++) {
        if ((get_bit(ie_reg, i) & get_bit(if_reg, i )) == 1) {
            if_reg = set_bit(if_reg, i, 0);
            *jump_vec = jump_vecs[i];
            return true;
        }
    }
    return false;
}

bool interrupt_pending() {
    return if_reg & ie_reg != 0x00;
}

void request_interrupt(interrupt_type type) {
    if_reg = set_bit(if_reg, type, 1);
}