#include "dma.h"
#include "bus.h"

/* Direct Memory Access controller. */

static byte dma_reg;

static byte base;
static bool active = false;

bool dma_init(void)
{
    /* DMG boot handoff state. */
    dma_reg = 0xFF;
    return true;
}

void dma_tick()
{
    if (!active)
        return;

    uint16_t src = ((uint16_t)dma_reg << 8) | base;
    uint16_t dst = (OAM_START & 0xFF00) | base;
    bus_copy_dma(src, dst);

    if (++base == 0xA0)
        active = false;
}

bool dma_is_active() {
    return active;
}

byte dma_dma_read(void) {
    return dma_reg;
}

void dma_dma_write(byte val) {
    dma_reg = val;
    base = 0x00;
    active = true;
}