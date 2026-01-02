#include "system.h"
#include "bus.h"
#include "timer.h"
#include "cpu.h"
#include "interrupt.h"
#include "ppu.h"
#include "input.h"
#include "dma.h"

/* Work RAM. */
static byte wram[WRAM_SIZE];

bool sys_init()
{
    /* TODO: Ordering (?) */
    return (
        timer_init() &&
        cpu_init() &&
        interrupt_init() &&
        ppu_init() &&
        input_init() &&
        dma_init()
    );
}

void sys_tick()
{
    /* Order is significant. */
    dma_tick();
    cpu_tick();
    ppu_tick();
    timer_tick();
}

void sys_start_frame()
{
    input_poll_and_load();
}

uint8_t *sys_get_frame_buffer() {
    return ppu_get_frame_buffer();
}

byte wram_read(uint16_t addr) {
    return wram[addr - WRAM_START];
}

void wram_write(uint16_t addr, byte val) {
    wram[addr - WRAM_START] = val;
}