#include "system.h"
#include "bus.h"
#include "timer.h"
#include "cpu.h"
#include "interrupt.h"
#include "ppu.h"
#include "input.h"
#include "dma.h"
#include "cartridge.h"

/* Work RAM. */
static byte wram[WRAM_SIZE];

bool sys_init(system_args args)
{
    return (
        cart_init(args.rom_path) &&
        timer_init() &&
        cpu_init() &&
        int_init() &&
        ppu_init(args.frame_mux) &&
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

void sys_deinit()
{
    cart_deinit();
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