#include "bus.h"
#include "system.h"
#include "cart.h"
#include "timer.h"
#include "cpu.h"
#include "ppu.h"
#include "interrupt.h"
#include "input.h"
#include "dma.h"

#include <stdio.h>

/* Memory bus -- 16-bit address bus, 8-bit data bus. */

/* TODO: Refactor IO using array of register descriptors (?) */
static byte io_read(uint16_t addr);
static void io_write(uint16_t addr, byte val);
static inline uint16_t map_echo_to_wram(uint16_t addr);

/* For the CPU - (Attempt to) read memory at addr. */
byte bus_read_cpu(uint16_t addr)
{
    region_type region = get_addr_region(addr);

    /* HRAM is always accessible. */
    if (region == HRAM)
        return hram_read(addr);
    
    if (dma_is_active())
        return 0xFF;

    ppu_mode mode = ppu_get_mode();
    switch (region) {
        case BANK0:
        case BANK1:
        case EXT_RAM:
            return cart_read(addr);
        case VRAM:
            if (mode == MODE3_DRAW)
                return 0xFF;
            return vram_read(addr);
        case ECHO:
            addr = map_echo_to_wram(addr);
        case WRAM:
            return wram_read(addr);
        case OAM:
            if (mode == MODE2_OAM || mode == MODE3_DRAW)
                return 0xFF;
            return oam_read(addr);
        case IO_REGS:
            return io_read(addr);
    }
    
    return 0xFF;
}

/* For the CPU - (Attempt to) write val to addr in memory. */
void bus_write_cpu(uint16_t addr, byte val)
{
    region_type region = get_addr_region(addr);

    /* HRAM is always accessible. */
    if (region == HRAM) {
        hram_write(addr, val);
        return;
    }

    if (dma_is_active())
        return;

    ppu_mode mode = ppu_get_mode();
    switch (region) {
        case BANK0:
        case BANK1:
        case EXT_RAM:
            cart_write(addr, val);
            break;
        case VRAM:
            if (mode == MODE3_DRAW)
                break;
            vram_write(addr, val);
            break;
        case ECHO:
            addr = map_echo_to_wram(addr);
        case WRAM:
            wram_write(addr, val);
            break;
        case OAM:
            if (mode == MODE2_OAM || mode == MODE3_DRAW)
                return;
            oam_write(addr, val);
            break;
        case IO_REGS:
            io_write(addr, val);
            break;
    }
}

/* For PPU - Read memory at addr. 
   Only VRAM and OAM can be read. */
byte bus_read_ppu(uint16_t addr)
{
    region_type region = get_addr_region(addr);
    if (region == VRAM)
        return vram_read(addr);
    else if (region == OAM)
        /* TODO: Check OAM DMA transfer (?)
           PPU reading OAM during DMA transfer is just glitch behavior. */
        return oam_read(addr);
    
    return 0xFF;
}

void bus_copy_dma(uint16_t src, uint16_t dst)
{
    byte val;
    region_type src_region = get_addr_region(src);

    /* TODO: Expand accessible source regions (?) */
    switch (src_region) {
        case BANK0:
        case BANK1:
        case EXT_RAM:
            val = cart_read(src);
            break;
        case VRAM:
            val = vram_read(src);
            break;
        case WRAM:
            val = wram_read(src);
            break;
        default:
            val = 0xFF;
    }

    oam_write(dst, val);
}

region_type get_addr_region(uint16_t addr) {
    if (addr < BANK0_START + BANK0_SIZE)
        return BANK0;
    else if (addr < BANK1_START + BANK1_SIZE)
        return BANK1;
    else if (addr < VRAM_START + VRAM_SIZE)
        return VRAM;
    else if (addr < EXT_RAM_START + EXT_RAM_SIZE)
        return EXT_RAM;
    else if (addr < WRAM_START + WRAM_SIZE)
        return WRAM;
    else if (addr < ECHO_START + ECHO_SIZE)
        return ECHO;
    else if (addr < OAM_START + OAM_SIZE)
        return OAM;
    else if (addr < UNUSED_START + UNUSED_SIZE)
        return UNUSED;
    else if (addr < IO_REGS_START + IO_REGS_SIZE || addr == IE_REG)
        return IO_REGS;
    else if (addr < HRAM_START + HRAM_SIZE)
        return HRAM;
    
    return UNUSED;
}

static byte io_read(uint16_t addr)
{
    switch (addr) {
        case JOYP_REG: return input_joyp_read();
        /* ... */
        case DIV_REG:  return timer_div_read();
        case TIMA_REG: return timer_tima_read();
        case TMA_REG:  return timer_tma_read();
        case TAC_REG:  return timer_tac_read();
        case IF_REG:   return interrupt_if_read();
        /* ... */
        case LCDC_REG: return ppu_lcdc_read();
        case STAT_REG: return ppu_stat_read();
        case SCY_REG:  return ppu_scy_read();
        case SCX_REG:  return ppu_scx_read();
        case LY_REG:   return ppu_ly_read();
        case LYC_REG:  return ppu_lyc_read();
        case DMA_REG:  return dma_dma_read();
        case BGP_REG:  return ppu_bgp_read();
        case OBP0_REG: return ppu_obp0_read();
        case OBP1_REG: return ppu_obp1_read();
        case WY_REG:   return ppu_wy_read();
        case WX_REG:   return ppu_wx_read();
        /* ... */
        case IE_REG:   return interrupt_ie_read();
        default: return 0xFF;
    }
}

static void io_write(uint16_t addr, byte val)
{
    switch (addr) {
        case JOYP_REG: input_joyp_write(val);   break;
        /* ... */
        case DIV_REG:  timer_div_write(val);    break;
        case TIMA_REG: timer_tima_write(val);   break;
        case TMA_REG:  timer_tma_write(val);    break;
        case TAC_REG:  timer_tac_write(val);    break;
        case IF_REG:   interrupt_if_write(val); break;
        /* ... */
        case LCDC_REG: ppu_lcdc_write(val);     break;
        case STAT_REG: ppu_stat_write(val);     break;
        case SCY_REG:  ppu_scy_write(val);      break;
        case SCX_REG:  ppu_scx_write(val);      break;
        case LY_REG:   ppu_ly_write(val);       break;
        case LYC_REG:  ppu_lyc_write(val);      break;
        case DMA_REG:  dma_dma_write(val);      break;
        case BGP_REG:  ppu_bgp_write(val);      break;
        case OBP0_REG: ppu_obp0_write(val);     break;
        case OBP1_REG: ppu_obp1_write(val);     break;
        case WY_REG:   ppu_wy_write(val);       break;
        case WX_REG:   ppu_wx_write(val);       break;
        /* ... */
        case IE_REG:   interrupt_ie_write(val); break;
    }
}

/* Echo region maps to WRAM. */
static inline uint16_t map_echo_to_wram(uint16_t addr) {
    return overlay_masked(addr, 0xC000, 0xE000);
}
