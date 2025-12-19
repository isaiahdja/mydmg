#pragma once
#include "byte.h"
#include <stdint.h>

#define BANK0_START  0x0000
#define BANK0_SIZE          0x4000
#define BANK1_START  0x4000
#define BANK1_SIZE          0x4000
#define VRAM_START    0x8000
#define VRAM_SIZE            0x2000
#define EXT_RAM_START 0xA000
#define EXT_RAM_SIZE         0x2000
#define WRAM_START    0xC000
#define WRAM_SIZE            0x1000
#define ECHO_START    0xE000
#define ECHO_SIZE            0x1E00
#define OAM_START     0xFE00
#define OAM_SIZE             0x00A0
#define UNUSED_START  0xFEA0
#define UNUSED_SIZE          0x0060
#define IO_REGS_START 0xFF00
#define IO_REGS_SIZE         0x0080
#define HRAM_START    0xFF80
#define HRAM_SIZE            0x007F

typedef enum {
    BANK0,
    BANK1,
    VRAM,
    EXT_RAM,
    WRAM,
    ECHO,
    OAM,
    UNUSED,
    IO_REGS,
    HRAM,
} region_type;

#define JOYP_REG 0xFF00
/* ... */
#define DIV_REG  0xFF04
#define TIMA_REG 0xFF05
#define TMA_REG  0xFF06
#define TAC_REG  0xFF07
#define IF_REG   0xFF0F
/* ... */
#define LCDC_REG 0xFF40
#define STAT_REG 0xFF41
#define SCY_REG  0xFF42
#define SCX_REG  0xFF43
#define LY_REG   0xFF44
#define LYC_REG  0xFF45
#define DMA_REG  0xFF46
#define BGP_REG  0xFF47
#define OBP0_REG 0xFF48
#define OBP1_REG 0xFF49
#define WY_REG   0xFF4A
#define WX_REG   0xFF4B
/* ... */
#define IE_REG   0xFFFF

byte bus_read_cpu(uint16_t addr);
void bus_write_cpu(uint16_t addr, byte val);

byte bus_read_ppu(uint16_t addr);

void bus_copy_dma(uint16_t src, uint16_t dst);

region_type get_addr_region(uint16_t addr);