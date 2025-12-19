#include "ppu.h"
#include "bus.h"

/* LCD Controller / Picture Processing Unit. */

/* Video RAM. */
static byte vram[VRAM_SIZE];
/* Object Attribute Memory. */
static byte oam[OAM_SIZE];

static byte lcdc_reg;
static byte stat_reg;
static byte scy_reg, scx_reg;
static byte ly_reg, lyc_reg;
static byte bgp_reg;
static byte obp0_reg, obp1_reg;
static byte wy_reg, wx_reg;

static ppu_mode mode;

bool ppu_init(void)
{
    /* DMG boot handoff state. */
    lcdc_reg = 0x91;
    stat_reg = 0x85;
    scy_reg = 0x00, scx_reg = 0x00;
    ly_reg = 0x00, lyc_reg = 0x00;
    bgp_reg = 0x00;
    obp0_reg = 0xFF, obp1_reg = 0xFF;
    wy_reg = 0x00, wx_reg = 0x00;
    mode = MODE2_OAM;

    return true;
}

void ppu_tick(void)
{

}

ppu_mode ppu_get_mode() {
    return mode;
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

byte ppu_lcdc_read() {}
void ppu_lcdc_write(byte val) {}

byte ppu_stat_read() {}
void ppu_stat_write(byte val) {}

byte ppu_scy_read() {}
void ppu_scy_write(byte val) {}

byte ppu_scx_read() {}
void ppu_scx_write(byte val) {}

byte ppu_ly_read() {}
void ppu_ly_write(byte val) {}

byte ppu_lyc_read() {}
void ppu_lyc_write(byte val) {}

byte ppu_bgp_read() {}
void ppu_bgp_write(byte val) {}

byte ppu_obp0_read() {}
void ppu_obp0_write(byte val) {}

byte ppu_obp1_read() {}
void ppu_obp1_write(byte val) {}

byte ppu_wy_read() {}
void ppu_wy_write(byte val) {}

byte ppu_wx_read() {}
void ppu_wx_write(byte val) {}