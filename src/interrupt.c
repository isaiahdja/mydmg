#include "interrupt.h"
#include "bus.h"

#define IF_RW_MASK 0x1F
#define IE_RW_MASK 0x1F

static byte if_reg, ie_reg;

bool interrupt_init(void)
{
    /* DMG boot handoff state. */
    if_reg = 0xE1, ie_reg = 0x00;

    return true;
}

byte interrupt_if_read() {}
void interrupt_if_write(byte val) {}

byte interrupt_ie_read() {}
void interrupt_ie_write(byte val) {}