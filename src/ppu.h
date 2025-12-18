#pragma once
#include "byte.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MODE0_HBLANK,
    MODE1_VBLANK,
    MODE2_OAM,
    MODE3_DRAW
} ppu_mode;

bool ppu_init(void);
void ppu_tick(void);

ppu_mode ppu_get_mode(void);

byte oam_read(uint16_t addr);
void oam_write(uint16_t addr, byte val);

byte ppu_lcdc_read(void);
void ppu_lcdc_write(byte val);
byte ppu_stat_read(void);
void ppu_stat_write(byte val);
byte ppu_scy_read(void);
void ppu_scy_write(byte val);
byte ppu_scx_read(void);
void ppu_scx_write(byte val);
byte ppu_ly_read(void);
void ppu_ly_write(byte val);
byte ppu_lyc_read(void);
void ppu_lyc_write(byte val);
byte ppu_dma_read(void);
void ppu_dma_write(byte val);
byte ppu_bgp_read(void);
void ppu_bgp_write(byte val);
byte ppu_obp0_read(void);
void ppu_obp0_write(byte val);
byte ppu_obp1_read(void);
void ppu_obp1_write(byte val);
byte ppu_wy_read(void);
void ppu_wy_write(byte val);
byte ppu_wx_read(void);
void ppu_wx_write(byte val);