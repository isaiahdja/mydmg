#include "ppu.h"
#include "bus.h"
#include "system.h"
#include "interrupt.h"

#include <stdio.h>

/* LCD Controller / Picture Processing Unit. */

/* Video RAM. */
static byte vram[VRAM_SIZE];
/* Object Attribute Memory. */
static byte oam[OAM_SIZE];

static byte lcdc_reg;
static inline bit lcd_enable(void) {
    return get_bit(lcdc_reg, 7);
}
static inline bit window_map_area(void) {
    return get_bit(lcdc_reg, 6);
}
static inline bit window_enable(void) {
    return get_bit(lcdc_reg, 5);
}
static inline bit bg_window_data_area(void) {
    return get_bit(lcdc_reg, 4);
}
static inline bit bg_map_area(void) {
    return get_bit(lcdc_reg, 3);
}
static inline bit obj_size(void) {
    return get_bit(lcdc_reg, 2);
}
static inline bit obj_enable(void) {
    return get_bit(lcdc_reg, 1);
}
static inline bit bg_window_enable(void) {
    return get_bit(lcdc_reg, 0);
}

#define STAT_RW_MASK 0x78
static byte stat_reg;
static inline bit lyc_int_select(void) {
    return get_bit(stat_reg, 6);
}
static inline bit mode2_int_select(void) {
    return get_bit(stat_reg, 5);
}
static inline bit mode1_int_select(void) {
    return get_bit(stat_reg, 4);
}
static inline bit mode0_int_select(void) {
    return get_bit(stat_reg, 3);
}
static bit prev_stat_int_signal = 0;

static byte scy_reg, scx_reg;
static byte ly_reg, lyc_reg;
static byte bgp_reg;
static byte obp0_reg, obp1_reg;
static byte wy_reg, wx_reg;

static uint32_t dmg_palette[4] = {
    0xFFFFFFFF, /* White      */
    0xFFAAAAAA, /* Light gray */
    0xFF555555, /* Dark gray  */
    0xFF000000  /* Black      */
};
static uint32_t frame_buffer[GB_HEIGHT * GB_WIDTH];

static ppu_mode mode;
static void set_mode(ppu_mode _mode) {
    mode = _mode;
    stat_reg = overlay_masked(stat_reg, mode, 0x03);
}
static bit prev_vblank_int_signal = 0;

#define T_CYCLES_PER_SCANLINE 456
#define SCANLINES_PER_FRAME 154
#define MODE2_OAM_T_CYCLES 80
static bool mode3_draw_complete;
/* Internal register. */
static byte lx_reg;
static int scanline_counter;

static void mode0_dot(void);
static void mode1_dot(void);
static void mode2_dot(void);
static void mode3_dot(void);

bool ppu_init(void)
{
    /* DMG boot handoff state. */
    lcdc_reg = 0x91;
    stat_reg = 0x85; /* Implies Mode 1 : Vertical blank. */
    /* Note that the mode is changed to Mode 2: OAM scan to begin a new frame. */
    set_mode(MODE2_OAM);
    scy_reg = 0x00, scx_reg = 0x00;
    ly_reg = 0x00, lyc_reg = 0x00;
    bgp_reg = 0x00;
    obp0_reg = 0xFF, obp1_reg = 0xFF;
    wy_reg = 0x00, wx_reg = 0x00;

    scanline_counter = 0;

    return true;
}

void ppu_tick(void)
{
    /* We can check for interrupts every M-cycle as that is how the rest of the
       system steps.
       Note that LY will only change on an M-cycle boundary -- the number of
       dots to render one scanline is a multiple of the T:M ratio, i.e.
       divisible by 4. */
    
    bit next_stat_int_signal = 0;

    if (ly_reg == lyc_reg) {
        stat_reg = set_bit(stat_reg, 2, 1);
        if (lyc_int_select() == 1)
            next_stat_int_signal = 1;
    }
    else
        stat_reg = set_bit(stat_reg, 2, 0);

    if (mode2_int_select() == 1 && mode == MODE2_OAM)
        next_stat_int_signal = 1;
    if (mode1_int_select() == 1 && mode == MODE1_VBLANK)
        next_stat_int_signal = 1;
    if (mode0_int_select() == 1 && mode == MODE0_HBLANK)
        next_stat_int_signal = 1;

    if (next_stat_int_signal == 1 && prev_stat_int_signal == 0) {
        /* Rising edge detected. */
        request_interrupt(INT_STAT);
    }
    prev_stat_int_signal = next_stat_int_signal;

    bit next_vblank_int_signal = (mode == MODE1_VBLANK) ? 1 : 0;
    if (next_vblank_int_signal == 1 && prev_vblank_int_signal == 0) {
        /* Rising edge detected. */
        request_interrupt(INT_VBLANK);
    }
    prev_vblank_int_signal = next_vblank_int_signal;

    /* Pixel FIFO steps by T-cycle. */
    for (int _ = 0; _ < T_M_RATIO; _++) {
        switch (mode) {
            case MODE0_HBLANK:
                mode0_dot();
                break;
            case MODE1_VBLANK:
                mode1_dot();
                break;
            case MODE2_OAM:
                mode2_dot();
                break;
            case MODE3_DRAW:
                mode3_dot();
                break;
        }

        if (++scanline_counter == T_CYCLES_PER_SCANLINE) {
            scanline_counter = 0;
            if (++ly_reg == SCANLINES_PER_FRAME)
                ly_reg = 0;
            if (ly_reg >= GB_HEIGHT)
                set_mode(MODE1_VBLANK);
            else
                set_mode(MODE2_OAM);
        }
        else if (mode == MODE2_OAM && scanline_counter == MODE2_OAM_T_CYCLES)
            set_mode(MODE3_DRAW);
        else if (mode == MODE3_DRAW && mode3_draw_complete) {
            set_mode(MODE0_HBLANK);
            mode3_draw_complete = false;
        }
    }
}

byte vram_read(uint16_t addr) {
    return vram[addr - VRAM_START];
}
void vram_write(uint16_t addr, byte val) {
    vram[addr - VRAM_START] = val;
}

byte oam_read(uint16_t addr) {
    return oam[addr - OAM_START];
}
void oam_write(uint16_t addr, byte val) {
    oam[addr - OAM_START] = val;
}

ppu_mode ppu_get_mode() {
    return mode;
}
uint32_t *ppu_get_frame_buffer() {
    return frame_buffer;
}

static void mode0_dot()
{
    return;
}

static void mode1_dot()
{
    return;
}

static void mode2_dot()
{

}

static void mode3_dot()
{
    /* TODO: Model Mode 3 penalties. */

    if (++lx_reg == GB_WIDTH) {
        lx_reg = 0;
        mode3_draw_complete = true;
    }
}

byte ppu_lcdc_read() {
    return lcdc_reg;
}
void ppu_lcdc_write(byte val) {
    lcdc_reg = val;

    /* TODO: Logic for re-enable (?) */
    if (lcd_enable() == 0) {
        /* Set lower-2 bits of STAT to 0b00, enable CPU reads/writes to VRAM
           and OAM. */
        set_mode(MODE0_HBLANK);

        ly_reg = 0x00;
    }
}

byte ppu_stat_read() {
    return stat_reg;
}
void ppu_stat_write(byte val) {
    stat_reg = overlay_masked(stat_reg, val, STAT_RW_MASK);
}

byte ppu_scy_read() {
    return scy_reg;
}
void ppu_scy_write(byte val) {
    scy_reg = val;
}

byte ppu_scx_read() {
    return scx_reg;
}
void ppu_scx_write(byte val) {
    scx_reg = val;
}

byte ppu_ly_read() {
    return ly_reg;
}
void ppu_ly_write(byte val) {
    return;
}

byte ppu_lyc_read() {
    return lyc_reg;
}
void ppu_lyc_write(byte val) {
    lyc_reg = val;
}

byte ppu_bgp_read() {
    return bgp_reg;
}
void ppu_bgp_write(byte val) {
    bgp_reg = val;
}

byte ppu_obp0_read() {
    return obp0_reg;
}
void ppu_obp0_write(byte val) {
    obp0_reg = val;
}

byte ppu_obp1_read() {
    return obp1_reg;
}
void ppu_obp1_write(byte val) {
    obp1_reg = val;
}

byte ppu_wy_read() {
    return wy_reg;
}
void ppu_wy_write(byte val) {
    wy_reg = val;
}

byte ppu_wx_read() {
    return wx_reg;
}
void ppu_wx_write(byte val) {
    wx_reg = val;
}