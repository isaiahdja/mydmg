#include "dma.h"
#include "bus.h"

/* Direct Memory Access controller. */

static byte dma_reg;
static byte dma_latched;

static byte base;
static int start;
static bool active;

bool dma_init(void)
{
    /* DMG boot handoff state. */
    dma_reg = 0xFF;

    start = 0;
    active = false;

    return true;
}

void dma_tick()
{
    if (start > 0 && --start == 0) {
        active = true;
        dma_latched = dma_reg;
        base = 0x00;
    }
    if (base == 0xA0)
        active = false;
    if (!active)
        return;

    uint16_t src = ((uint16_t)dma_latched << 8) | base;
    uint16_t dst = (OAM_START & 0xFF00) | base;
    bus_copy_dma(src, dst);
    base++;
}

bool dma_is_active() {
    return active;
}

byte dma_dma_read(void) {
    return dma_reg;
}

void dma_dma_write(byte val) {
    dma_reg = val;
    start = 2;
}