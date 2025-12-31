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
bool just_enabled = false;

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
static inline int get_palette_color(byte palette, int idx) {
    return get_bits(palette, (idx * 2) + 1, (idx * 2));
}

static byte wy_reg, wx_reg;

static uint32_t dmg_colors[4] = {
    0xFF89C177, /* 100% */
    0xFF4DA350, /*  66% */
    0xFF36774A, /*  33% */
    0xFF224939  /*   0% */
};
static uint32_t frame_buffer[GB_HEIGHT * GB_WIDTH];
static uint32_t off_buffer[GB_HEIGHT * GB_WIDTH];

static void set_mode(ppu_mode _mode);
static ppu_mode mode;
static bit prev_vblank_int_signal = 0;

#define T_CYCLES_PER_SCANLINE 456
#define SCANLINES_PER_FRAME 154
#define MODE2_OAM_T_CYCLES 80
static int scanline_counter;
static bool mode3_draw_complete;
/* Internal register. */
static byte lx_reg;

static void mode0_dot(void);
static void mode1_dot(void);
static void mode2_dot(void);
static void mode3_dot(void);

typedef struct {
    uint16_t addr;
    byte obj_x;
    byte obj_y;
} obj_slot_type;
static obj_slot_type scanline_objs[10];
static int scanline_objs_count;
static uint16_t mode2_addr;
typedef enum {
    CHECK,
    PUSH, SKIP
} mode2_cycle_type;
static mode2_cycle_type mode2_cycle;

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

static fifo bg_fifo;

static fifo obj_fifo;

static void fifo_clear(fifo *f);
static bool fifo_pop(fifo *f, pixel *p);
static bool bg_fifo_fill(pixel *p);

/* */

typedef struct {
    int dot;

    byte tile_id;
    uint16_t data_addr;
    byte data_lo;
    byte data_hi;
    pixel pixels[8];

    /* BG/WIN only. */
    byte fetch_x;
    uint16_t id_addr;

    /* OBJ only. */
    byte attributes;
} fetcher;

static int first_fetch_disregard;
static int scx_disregard;
static fetcher bg_fetcher;

bool need_to_fetch_obj;
static obj_slot_type fetch_obj;
static fetcher obj_fetcher;

static void fetcher_clear(fetcher *f);
static void bg_fetcher_dot(void);
static void check_objs_lx(void);
static void obj_fetcher_dot(void);

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
    /* TODO: Change blank LCD color. */
    for (int i = 0; i < GB_WIDTH * GB_HEIGHT; i++)
        off_buffer[i] = dmg_colors[0];

    return true;
}

void ppu_tick(void)
{
    if (mode == LCD_DISABLED)
        return;

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
            /* Begin new scanline. */
            scanline_counter = 0;
            if (++ly_reg == SCANLINES_PER_FRAME) {
                /* Begin new frame. */
                just_enabled = false;
                ly_reg = 0;
            }
            
            if (ly_reg >= GB_HEIGHT)
                set_mode(MODE1_VBLANK);
            else
                set_mode(MODE2_OAM);
        }
        else if (mode == MODE2_OAM && scanline_counter == MODE2_OAM_T_CYCLES) {
            set_mode(MODE3_DRAW);
        }
        else if (mode == MODE3_DRAW && mode3_draw_complete) {
            set_mode(MODE0_HBLANK);
        }
    }

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
}

static void set_mode(ppu_mode _mode) {
    mode = _mode;
    
    switch (mode) {
        case MODE0_HBLANK:
            break;
        case MODE1_VBLANK:
            break;
        case MODE2_OAM:
            scanline_objs_count = 0;
            mode2_addr = OAM_START;
            mode2_cycle = CHECK;
            break;
        case MODE3_DRAW:
            lx_reg = 0xF8;
            fifo_clear(&bg_fifo); fetcher_clear(&bg_fetcher);
            fifo_clear(&obj_fifo); fetcher_clear(&obj_fetcher);
            scx_disregard = scx_reg % 8;
            check_objs_lx();
            mode3_draw_complete = false;
            break;
        case LCD_DISABLED:
            stat_reg &= 0xFC;
            return;
    }

    stat_reg = overlay_masked(stat_reg, mode, 0x03);
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
    /* "When re-enabling the LCD, the PPU will immediately start drawing again,
       but the screen will stay blank during the first frame." "*/
    if (mode == LCD_DISABLED || just_enabled)
        return off_buffer;
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
    if (scanline_objs_count == 10)
        return;
    
    switch (mode2_cycle) {
        case CHECK:
            scanline_objs[scanline_objs_count].addr = mode2_addr;
            byte obj_y = bus_read_ppu(mode2_addr);
            scanline_objs[scanline_objs_count].obj_y = obj_y;
            int screen_start = obj_y - 16;
            int screen_end = screen_start + (obj_size() == 0 ? 8 : 16);
            bool on_scanline = (ly_reg >= screen_start) && (ly_reg < screen_end);
            mode2_cycle = on_scanline ? PUSH : SKIP;
            break;
        case PUSH:
            scanline_objs[scanline_objs_count].obj_x = bus_read_ppu(mode2_addr + 1);
            scanline_objs_count++;
        case SKIP:
            mode2_addr += 4;
            mode2_cycle = CHECK;
            break;
    }
}

static void mode3_dot()
{
    if (obj_enable() && need_to_fetch_obj)
        obj_fetcher_dot();
    bg_fetcher_dot();

    if (need_to_fetch_obj)
        return;

    pixel bg_pixel;
    if (fifo_pop(&bg_fifo, &bg_pixel)) {
        pixel obj_pixel;
        bool obj_popped = fifo_pop(&obj_fifo, &obj_pixel);

        if (scx_disregard > 0) {
            scx_disregard--;
            return;
        }

        if (!bg_win_enable())
            bg_pixel.palette_idx = 0;
        if (!obj_enable())
            obj_pixel.palette_idx = 0;

        /* Default -- Choose BG/WIN pixel if the object FIFO wasn't popped. */
        int pick = 0;
        if (obj_popped)
        {
            if (!bg_win_enable())
                pick = 1;
            else if (!obj_enable())
                pick = 0;
            else if (obj_pixel.priority == 1 && bg_pixel.palette_idx != 0)
                pick = 0;
            else if (obj_pixel.palette_idx == 0)
                pick = 0;
            else
                pick = 1;
        }

        int color;
        if (pick == 0) {
            color = get_palette_color(bgp_reg, bg_pixel.palette_idx);
        }
        else {
            color = get_palette_color(
                (obj_pixel.palette == 0 ? obp0_reg : obp1_reg),
                obj_pixel.palette_idx);
        }

        if (lx_reg < GB_WIDTH)
            frame_buffer[ly_reg * GB_WIDTH + lx_reg] = dmg_colors[color];

        if (++lx_reg == GB_WIDTH) {
            //printf("Mode 3 length = %d\n", scanline_counter - 80 + 1);
            mode3_draw_complete = true;
        }
        else
            check_objs_lx();
    }
}

byte ppu_lcdc_read() {
    return lcdc_reg;
}
void ppu_lcdc_write(byte val) {
    bit prev_lcd_enable = lcd_enable();
    lcdc_reg = val;

    if (lcd_enable() == 0 && prev_lcd_enable == 1) {
        set_mode(LCD_DISABLED);
    }
    else if (lcd_enable() == 1 && prev_lcd_enable == 0) {
        just_enabled = true;
        scanline_counter = 0;
        ly_reg = 0;
        set_mode(MODE2_OAM);
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

static void fifo_clear(fifo *f) {
    f->head = 8;
}

static bool fifo_pop(fifo *f, pixel *p)
{
    if (f->head == 8)
        return false;

    *p = f->pixels[f->head++];
    return true;
}

static bool bg_fifo_fill(pixel *p)
{
    if (bg_fifo.head != 8)
        return false;
    
    memcpy(bg_fifo.pixels, p, 8 * sizeof(pixel));
    bg_fifo.head = 0;
    return true;
}

static bool obj_fifo_fill(pixel *p)
{
    /* Merge. */
    int j = 0;
    while (j < 8) {
        pixel new = p[j];
        pixel old;
        if ((fifo_pop(&obj_fifo, &old) && old.palette_idx != 0))
            obj_fifo.pixels[j] = old;
        else
            obj_fifo.pixels[j] = new;

        j++;
    }

    obj_fifo.head = 0;
    return true;
}

/* */

static void fetcher_clear(fetcher *f) {
    f->dot = 0;
    f->fetch_x = 0xF8;
}

static void bg_fetcher_dot()
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
                bg_fetcher.fetch_x += 8;
                bg_fetcher.dot = 0;
            }
            break;
    }
}

static byte flip_bits(byte b) {
    byte flipped = 0;
    for (int i = 0; i < 8; i++)
        flipped |= ((b >> i) & 0x1) << (7 - i);
    return flipped;
}
static void obj_fetcher_dot()
{
    switch (obj_fetcher.dot) {
        /* Get tile ID. */
        case 0:
            obj_fetcher.tile_id = bus_read_ppu(fetch_obj.addr + 2);
            obj_fetcher.dot++;
            break;
        case 1:
            obj_fetcher.attributes = bus_read_ppu(fetch_obj.addr + 3);
            if (obj_size() == 1) {
                /* Default -- First tile of 8 x 16 object. */
                bit override = 0;
                if (ly_reg >= fetch_obj.obj_y - 8)
                    /* Second tile of 8 x 16 object. */
                    override = 1;
                if (get_bit(obj_fetcher.attributes, 6) == 1)
                    override = !override;
                obj_fetcher.tile_id = set_bit(obj_fetcher.tile_id, 0, override);
            }
            obj_fetcher.dot++;
            break;
        /* Get tile data (low). */
        case 2:
            byte data_line = (byte)(ly_reg - fetch_obj.obj_y) % 8;
            if (get_bit(obj_fetcher.attributes, 6))
                data_line = (~data_line) & 0x7;
            obj_fetcher.data_addr = (
                0x8000                               |
                ((uint16_t)obj_fetcher.tile_id << 4) |
                ((uint16_t)data_line           << 1));
            obj_fetcher.dot++;
            break;
        case 3:
            obj_fetcher.data_lo = bus_read_ppu(obj_fetcher.data_addr);
            if (get_bit(obj_fetcher.attributes, 5))
                obj_fetcher.data_lo = flip_bits(obj_fetcher.data_lo);
            obj_fetcher.dot++;
            break;
        /* Get tile data (high). */
        case 4:
            obj_fetcher.data_addr += 1;
            obj_fetcher.dot++;
            break;
        case 5:
            obj_fetcher.data_hi = bus_read_ppu(obj_fetcher.data_addr);
            if (get_bit(obj_fetcher.attributes, 5))
                obj_fetcher.data_hi = flip_bits(obj_fetcher.data_hi);
            obj_fetcher.dot++;
            break;
        /* Push. */
        case 6:
            for (int i = 0; i < 8; i++) {
                int idx = 7 - i;
                pixel p;
                p.palette_idx = (
                    (int)get_bit(obj_fetcher.data_hi, idx) << 1) |
                    (int)get_bit(obj_fetcher.data_lo, idx);
                p.palette = get_bit(obj_fetcher.attributes, 4);
                p.priority = get_bit(obj_fetcher.attributes, 7);
                obj_fetcher.pixels[i] = p;
            }
            obj_fifo_fill(obj_fetcher.pixels);
            obj_fetcher.dot = 0;
            need_to_fetch_obj = false;
            break;
    }
}

static void check_objs_lx()
{
    need_to_fetch_obj = false;
    for (int i = 0; i < scanline_objs_count; i++) {
        obj_slot_type obj_slot = scanline_objs[i];
        if ((byte)(lx_reg + 8) == obj_slot.obj_x) {
            need_to_fetch_obj = true;
            fetch_obj = obj_slot;
            break;
        }
    }
}