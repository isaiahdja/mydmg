#include "system.h"
#include "bus.h"
#include "timer.h"
#include "cpu.h"
#include "interrupt.h"
#include "ppu.h"
#include "input.h"

static byte vram[VRAM_SIZE];
static byte wram[WRAM_SIZE];

bool sys_init()
{
    /* Order is insignificant. */
    return (
        timer_init() &&
        cpu_init() &&
        interrupt_init() &&
        ppu_init()
    );
}

void sys_tick()
{
    /* TODO: Place interrupt_tick() (?) */
    /* Order is significant. */
    cpu_tick();
    ppu_tick();
    timer_tick();
}

void sys_start_frame()
{
    input_poll_and_load();
}

byte vram_read(uint16_t addr) {
    return vram[addr - VRAM_START];
}

void vram_write(uint16_t addr, byte val) {
    vram[addr - VRAM_START] = val;
}

byte wram_read(uint16_t addr) {
    return wram[addr - WRAM_START];
}

void wram_write(uint16_t addr, byte val) {
    wram[addr - WRAM_START] = val;
}