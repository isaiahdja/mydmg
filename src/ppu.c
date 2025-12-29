#include "ppu.h"
#include "bus.h"
#include "system.h"
#include "interrupt.h"
#include <string.h>

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
static inline bit win_map_area(void) {
    return get_bit(lcdc_reg, 6);
}
static inline bit win_enable(void) {
    return get_bit(lcdc_reg, 5);
}
static inline bit bg_win_data_area(void) {
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
static inline bit bg_win_enable(void) {
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

static inline int get_palette_color(byte palette, int idx) {
    return get_bits(palette, (idx * 2) + 1, (idx * 2));
}
static byte bgp_reg;
static byte obp0_reg, obp1_reg;

static byte wy_reg, wx_reg;

static uint32_t dmg_colors[4] = {
    0xFFC9EDFF, /* White      */
    0xFF9ACAE2, /* Light gray */
    0xFF77B1CE, /* Dark gray  */
    0xFF427A96  /* Black      */
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

/* */

typedef struct {
    int palette_idx;

    /* OBJ only. */
    bit palette;
    bit priority;
} pixel;

typedef struct {
    pixel pixels[8];
    int head;
} fifo;

fifo bg_fifo;

void fifo_clear(fifo *f);
bool fifo_pop(fifo *f, pixel *p);
bool bg_fifo_fill(pixel *p);

/* */

typedef struct {
    int dot;
    byte fetch_x;

    uint16_t id_addr;
    byte tile_id;
    uint16_t data_addr;
    byte data_lo;
    byte data_hi;

    pixel pixels[8];
} fetcher;

int first_fetch_disregard;
int scx_disregard;
fetcher bg_fetcher;

void fetcher_clear(fetcher *f);
void bg_fetcher_dot(void);

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
    if (lcd_enable() == 0)
        return;

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
        else if (mode == MODE2_OAM && scanline_counter == MODE2_OAM_T_CYCLES) {
            fifo_clear(&bg_fifo); fetcher_clear(&bg_fetcher);
            first_fetch_disregard = 8; scx_disregard = scx_reg % 8;
            set_mode(MODE3_DRAW);
        }
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
    return;
}

static void mode3_dot()
{
    bg_fetcher_dot();

    pixel p;
    if (fifo_pop(&bg_fifo, &p)) {
        if (first_fetch_disregard > 0) {
            first_fetch_disregard--;
            return;
        }
        if (scx_disregard > 0) {
            scx_disregard--;
            return;
        }

        frame_buffer[ly_reg * GB_WIDTH + lx_reg] = dmg_colors[
            get_palette_color(bgp_reg, p.palette_idx)];

        if (++lx_reg == GB_WIDTH) {
            lx_reg = 0;
            //printf("Mode 3 length = %d\n", scanline_counter - 80 + 1);
            mode3_draw_complete = true;
        }
    }
}

byte ppu_lcdc_read() {
    return lcdc_reg;
}
void ppu_lcdc_write(byte val) {
    bit prev_lcd_enable = lcd_enable();
    lcdc_reg = val;

    if (lcd_enable() == 0 && prev_lcd_enable == 1) {
        stat_reg &= 0xFC;
    }
    else if (lcd_enable() == 1 && prev_lcd_enable == 0) {
        set_mode(MODE2_OAM);
        scanline_counter = 0;
        ly_reg = 0; lx_reg = 0;
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

/* */

void fifo_clear(fifo *f) {
    f->head = 8;
}

bool fifo_pop(fifo *f, pixel *p)
{
    if (f->head == 8)
        return false;

    *p = f->pixels[f->head++];
    return true;
}

bool bg_fifo_fill(pixel *p)
{
    if (bg_fifo.head != 8)
        return false;
    
    memcpy(bg_fifo.pixels, p, 8 * sizeof(pixel));
    bg_fifo.head = 0;
    return true;
}

/* */

void fetcher_clear(fetcher *f) {
    f->dot = 0;
    f->fetch_x = 0;
}

void bg_fetcher_dot()
{
    switch (bg_fetcher.dot) {
        /* Get tile ID. */
        case 0:
            bit map_area = bg_map_area();
            byte tile_y = (byte)(ly_reg + scy_reg) / 8;
            byte tile_x = (byte)(bg_fetcher.fetch_x + scx_reg) / 8;
            bg_fetcher.id_addr = (
                0x9800                     |
                ((uint16_t)map_area << 10) |
                ((uint16_t)tile_y   <<  5) |
                (uint16_t)tile_x);
            bg_fetcher.dot++;
            break;
        case 1:
            bg_fetcher.tile_id = bus_read_ppu(bg_fetcher.id_addr);
            bg_fetcher.dot++;
            break;
        /* Get tile data (low). */
        case 2:
            bit addr_mode = (bg_win_data_area() == 1 ?
                0 : !(get_bit(bg_fetcher.tile_id, 7)));
            byte data_line = (byte)(ly_reg + scy_reg) % 8;
            bg_fetcher.data_addr = (
                0x8000                               |
                ((uint16_t)addr_mode          << 12) |
                ((uint16_t)bg_fetcher.tile_id <<  4) |
                ((uint16_t)data_line          <<  1));
            bg_fetcher.dot++;
            break;
        case 3:
            bg_fetcher.data_lo = bus_read_ppu(bg_fetcher.data_addr);
            bg_fetcher.dot++;
            break;
        /* Get tile data (high). */
        case 4:
            bg_fetcher.data_addr += 1;
            bg_fetcher.dot++;
            break;
        case 5:
            bg_fetcher.data_hi = bus_read_ppu(bg_fetcher.data_addr);
            bg_fetcher.dot++;
            break;
        /* Push. */
        case 6:
            for (int i = 0; i < 8; i++) {
                int idx = 7 - i;
                pixel p;
                p.palette_idx = (
                    (int)get_bit(bg_fetcher.data_hi, idx) << 1) |
                    (int)get_bit(bg_fetcher.data_lo, idx);
                bg_fetcher.pixels[i] = p;
            }
            bg_fetcher.dot++;
        case 7:
            if (bg_fifo_fill(bg_fetcher.pixels)) {
                if (first_fetch_disregard == 0)
                    bg_fetcher.fetch_x += 8;
                bg_fetcher.dot = 0;
            }
            break;
    }
}