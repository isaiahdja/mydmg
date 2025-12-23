#include "cpu.h"
#ifndef CPU_TEST
#include "bus.h"
#include "interrupt.h"
#endif

#include <stdio.h>

/* Central Processing Unit core. */

/* High RAM. */
#ifndef CPU_TEST
static byte hram[HRAM_SIZE];
#endif

static cpu_state state;
static uint16_t wz_latch;
static byte instr_reg;

static void (*instr_func)(void);
static int instr_cycle;
static bool instr_complete;

static read_fn memory_read;
static write_fn memory_write;
static receive_int_fn receive_int;

static bool cb_prefixed = false;
static void fetch_and_decode(void);

static inline byte get_w_latch(void) {
    return get_hi_byte(wz_latch);
}
static inline void set_w_latch(byte val) {
    wz_latch = set_hi_byte(wz_latch, val);
}
static inline byte get_z_latch(void) {
    return get_lo_byte(wz_latch);
}
static inline void set_z_latch(byte val) {
    wz_latch = set_lo_byte(wz_latch, val);
}
/* C */
static inline bit get_carry(void) {
    return get_bit(state.af_reg, 4);
}
static inline void set_carry(bit val) {
    state.af_reg = set_bit(state.af_reg, 4, val);
}
/* H */
static inline bit get_half_carry(void) {
    return get_bit(state.af_reg, 5);
}
static inline void set_half_carry(bit val) {
    state.af_reg = set_bit(state.af_reg, 5, val);
}
/* N */
static inline bit get_subtraction(void) {
    return get_bit(state.af_reg, 6);
}
static inline void set_subtraction(bit val) {
    state.af_reg = set_bit(state.af_reg, 6, val);
}
/* Z */
static inline bit get_zero(void) {
    return get_bit(state.af_reg, 7);
}
static inline void set_zero(bit val) {
    state.af_reg = set_bit(state.af_reg, 7, val);
}

static byte get_r8(int code);
static void set_r8(int code, byte val);
static uint16_t get_r16(int code);
static void set_r16(int code, uint16_t val);
static uint16_t get_r16stk(int code);
static void set_r16stk(int code, uint16_t val);
static byte read_r16mem(int code);
static void write_r16mem(int code, byte val);
static bool check_cond(int code);

static byte add_byte_flags(byte num1, byte num2, bool route_carry,
    bit *z_flag_out, bit *n_flag_out, bit *h_flag_out, bit *c_flag_out);
static byte sub_byte_flags(byte num1, byte num2,
    bit *z_flag_out, bit *n_flag_out, bit *h_flag_out, bit *c_flag_out);

static void nop(void);
static void ld_r16_imm16(void);
static void ld_r16mem_a(void);
static void ld_a_r16mem(void);
static void ld_imm16_sp(void);
static void inc_r16(void);
static void dec_r16(void);
static void add_hl_r16(void);
static void inc_r8(void);
static void inc_hlmem(void);
static void dec_r8(void);
static void dec_hlmem(void);
static void ld_r8_imm8(void);
static void ld_hlmem_imm8(void);
static void rlca(void);
static void rla(void);
static void rrca(void);
static void rra(void);
static void daa(void);
static void cpl(void);
static void scf(void);
static void ccf(void);
static void jr_imm8(void);
static void jr_cond_imm8(void);
static void stop(void);
static void halt(void);
static void ld_hlmem_r8(void);
static void ld_r8_hlmem(void);
static void ld_r8_r8(void);

#ifndef CPU_TEST
bool cpu_init(void)
{
    memory_read = &bus_read_cpu;
    memory_write = &bus_write_cpu;
    receive_int = &interrupt_send_interrupt;

    /* DMG boot handoff state. */
    state = (cpu_state){
        0x0108,
        0x0013,
        0x00D8,
        0x014D,
        0xFFFE,
        0x0100,
        0
    };

    instr_cycle = 0;
    instr_complete = false;
    instr_func = &nop;

    return true;
}
#endif
void cpu_tick(void)
{
    /* The current instruction function will set instr_completed to true if it
       has completed, allowing the next instruction to be fetched. This models
       the CPU fetch/execute overlap.
       Note that the final cycle of an instruction must not use the memory bus,
       as it is reserved for fetching. */
    instr_func();

    if (instr_complete) {
        uint16_t jump_vec;
        if (state.ime_flag == 1 && receive_int(&jump_vec)) {
            /* TODO: Interrupt received. */
        }
        fetch_and_decode(); /* Resets instr_cycle and instr_complete. */
    }
    else
        instr_cycle++;
}

#ifndef CPU_TEST
byte hram_read(uint16_t addr) {
    return hram[addr - HRAM_START];
}
void hram_write(uint16_t addr, byte val) {
    hram[addr - HRAM_START] = val;
}
#endif

static void fetch_and_decode()
{
    instr_cycle = 0;
    instr_complete = false;
    instr_func = &nop;
    
    instr_reg = memory_read(state.pc_reg++);

    if (cb_prefixed) {
        /* TODO: 0xCB prefix instructions. */
#pragma region
#pragma endregion
    }
    else if (instr_reg == 0xCB) {
        cb_prefixed = true;
        return;
    }

    /* "Block 0." */
#pragma region
    if (instr_reg == 0x00) {
        instr_func = &nop;
        return;
    }

    int b_7_6 = get_bits(instr_reg, 7, 6);
    int b_3_0 = get_bits(instr_reg, 3, 0);
    if (b_7_6 == 0 && b_3_0 == 1) {
        instr_func = &ld_r16_imm16;
        return;
    }
    if (b_7_6 == 0 && b_3_0 == 0x2) {
        instr_func = &ld_r16mem_a;
        return;
    }
    if (b_7_6 == 0 && b_3_0 == 0xA) {
        instr_func = &ld_a_r16mem;
        return;
    }
    if (instr_reg == 0x08) {
        instr_func = &ld_imm16_sp;
        return;
    }

    if (b_7_6 == 0 && b_3_0 == 0x3) {
        instr_func = &inc_r16;
        return;
    }
    if (b_7_6 == 0 && b_3_0 == 0xB) {
        instr_func = &dec_r16;
        return;
    }
    if (b_7_6 == 0 && b_3_0 == 0x9) {
        instr_func = &add_hl_r16;
        return;
    }


    int b_2_0 = get_bits(instr_reg, 2, 0);
    if (b_7_6 == 0 && b_2_0 == 4) {
        if (instr_reg == 0x34)
            instr_func = &inc_hlmem;
        else
            instr_func = &inc_r8;
        return;
    }
    if (b_7_6 == 0 && b_2_0 == 5) {
        if (instr_reg == 0x35)
            instr_func = &dec_hlmem;
        else
            instr_func = &dec_r8;
        return;
    }

    if (b_7_6 == 0 && b_2_0 == 6) {
        if (instr_reg == 0x36)
            instr_func = &ld_hlmem_imm8;
        else
            instr_func = &ld_r8_imm8;
        return;
    }

    if (instr_reg == 0x07) {
        instr_func = &rlca;
        return;
    }
    if (instr_reg == 0x0F) {
        instr_func = &rrca;
        return;
    }
    if (instr_reg == 0x17) {
        instr_func = &rla;
        return;
    }
    if (instr_reg == 0x1f) {
        instr_func = &rra;
        return;
    }
    if (instr_reg == 0x27) {
        instr_func = &daa;
        return;
    }
    if (instr_reg == 0x2F) {
        instr_func = &cpl;
        return;
    }
    if (instr_reg == 0x37) {
        instr_func = &scf;
        return;
    }
    if (instr_reg == 0x3f) {
        instr_func = &ccf;
        return;
    }

    int b_7_5 = get_bits(instr_reg, 7, 5);
    if (instr_reg == 0x18) {
        instr_func = &jr_imm8;
        return;
    }
    if (b_7_5 == 1 && b_2_0 == 0) {
        instr_func = &jr_cond_imm8;
        return;
    }

    if (instr_reg == 0x10) {
        instr_func = &stop;
        return;
    }
#pragma endregion

    /* "Block 1." "*/
#pragma region
    if (instr_reg == 0x76) {
        instr_func = &halt;
        return;
    }
    int b_5_3 = get_bits(instr_reg, 5, 3);
    if (b_7_6 == 1) {
        if (b_5_3 == 6)
            instr_func = &ld_hlmem_r8;
        else if (b_2_0 == 6)
            instr_func = &ld_r8_hlmem;
        else
            instr_func = &ld_r8_r8;
        return;
    }
#pragma endregion
}

static byte get_r8(int code)
{
    switch (code) {
        case 0: return get_hi_byte(state.bc_reg);
        case 1: return get_lo_byte(state.bc_reg);
        case 2: return get_hi_byte(state.de_reg);
        case 3: return get_lo_byte(state.de_reg);
        case 4: return get_hi_byte(state.hl_reg);
        case 5: return get_lo_byte(state.hl_reg);
        //case 6: return memory_read(state.hl_reg);
        case 7: return get_hi_byte(state.af_reg);
    }
}
static void set_r8(int code, byte val)
{
    switch (code) {
        case 0: state.bc_reg = set_hi_byte(state.bc_reg, val); break;
        case 1: state.bc_reg = set_lo_byte(state.bc_reg, val); break;
        case 2: state.de_reg = set_hi_byte(state.de_reg, val); break;
        case 3: state.de_reg = set_lo_byte(state.de_reg, val); break;
        case 4: state.hl_reg = set_hi_byte(state.hl_reg, val); break;
        case 5: state.hl_reg = set_lo_byte(state.hl_reg, val); break;
        //case 6: memory_write(state.hl_reg, val);          break;
        case 7: state.af_reg = set_hi_byte(state.af_reg, val); break;
    }
}
static uint16_t get_r16(int code)
{
    switch (code) {
        case 0: return state.bc_reg;
        case 1: return state.de_reg;
        case 2: return state.hl_reg;
        case 3: return state.sp_reg;
    }
}
static void set_r16(int code, uint16_t val)
{
    switch (code) {
        case 0: state.bc_reg = val; break;
        case 1: state.de_reg = val; break;
        case 2: state.hl_reg = val; break;
        case 3: state.sp_reg = val; break;
    }
}
static uint16_t get_r16stk(int code);
static void set_r16stk(int code, uint16_t val);
static byte read_r16mem(int code)
{
    switch (code) {
        case 0: return memory_read(state.bc_reg);
        case 1: return memory_read(state.de_reg);
        case 2: return memory_read(state.hl_reg++);
        case 3: return memory_read(state.hl_reg--);
    }
}
static void write_r16mem(int code, byte val)
{
    switch (code) {
        case 0: memory_write(state.bc_reg, val);   break;
        case 1: memory_write(state.de_reg, val);   break;
        case 2: memory_write(state.hl_reg++, val); break;
        case 3: memory_write(state.hl_reg--, val); break;
    }
}
static bool check_cond(int code)
{
    switch (code) {
        case 0: return !get_zero();
        case 1: return get_zero();
        case 2: return !get_carry();
        case 3: return get_carry();
    }
}

static byte add_byte_flags(byte num1, byte num2, bool route_carry,
    bit *z_flag_out, bit *n_flag_out, bit *h_flag_out, bit *c_flag_out)
{
    static bit carry = 0;

    if (n_flag_out != NULL)
        *n_flag_out = 0;

    uint8_t lo_sum = get_lo_nibble(num1) + get_lo_nibble(num2);
    if (route_carry)
        lo_sum += carry;
    bit half_carry = lo_sum > 0x0F ? 1 : 0;
    if (h_flag_out != NULL)
        *h_flag_out = half_carry;
    lo_sum &= 0x0F;

    uint8_t hi_sum = get_hi_nibble(num1) + get_hi_nibble(num2) + half_carry;
    carry = hi_sum > 0x0F ? 1 : 0;
    if (c_flag_out != NULL)
        *c_flag_out = carry;
    hi_sum &= 0x0F;

    uint8_t final = (hi_sum << 4) | lo_sum;
    if (z_flag_out != NULL)
        *z_flag_out = final == 0x00 ? 1 : 0;
    return final;
}
static byte sub_byte_flags(byte num1, byte num2,
    bit *z_flag_out, bit *n_flag_out, bit *h_flag_out, bit *c_flag_out)
{
    if (n_flag_out != NULL)
        *n_flag_out = 1;

    uint8_t lo_diff = get_lo_nibble(num1) - get_lo_nibble(num2);
    bit half_carry = lo_diff > 0x0F ? 1 : 0;
    if (h_flag_out != NULL)
        *h_flag_out = half_carry;
    lo_diff &= 0x0F;

    uint8_t hi_diff = get_hi_nibble(num1) - get_hi_nibble(num2) - half_carry;
    bit carry = hi_diff > 0x0F ? 1 : 0;
    if (c_flag_out != NULL)
        *c_flag_out = carry;
    hi_diff &= 0x0F;

    uint8_t final = (hi_diff << 4) | lo_diff;
    if (z_flag_out != NULL)
        *z_flag_out = final == 0x00 ? 1 : 0;
    return final;
}

static void nop()
{
    instr_complete = true;
}
static void ld_r16_imm16()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            set_w_latch(memory_read(state.pc_reg++));
            break;
        case 2:
            set_r16(get_bits(instr_reg, 5, 4), wz_latch);
            instr_complete = true;
            break;
    }
}
static void ld_r16mem_a()
{
    switch (instr_cycle) {
        case 0:
            write_r16mem(get_bits(instr_reg, 5, 4), get_hi_byte(state.af_reg));
            break;
        case 1:
            instr_complete = true;
            break;
    }
}
static void ld_a_r16mem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(read_r16mem(get_bits(instr_reg, 5, 4)));
            break;
        case 1:
            state.af_reg = set_hi_byte(state.af_reg, get_z_latch());
            instr_complete = true;
            break;
    }
}
static void ld_imm16_sp()
{
    switch (instr_cycle) {
        case 0:
            wz_latch = set_lo_byte(wz_latch, memory_read(state.pc_reg++));
            break;
        case 1:
            wz_latch = set_hi_byte(wz_latch, memory_read(state.pc_reg++));
            break;
        case 2:
            memory_write(wz_latch++, get_lo_byte(state.sp_reg));
            break;
        case 3:
            memory_write(wz_latch, get_hi_byte(state.sp_reg));
            break;
        case 4:
            instr_complete = true;
            break;
    }
}
static void inc_r16()
{
    switch (instr_cycle) {
        case 0:
            int code = get_bits(instr_reg, 5, 4);
            set_r16(code, get_r16(code) + 1);
            break;
        case 1:
            instr_complete = true;
            break;
    }
}
static void dec_r16()
{
    switch (instr_cycle) {
        case 0:
            int code = get_bits(instr_reg, 5, 4);
            set_r16(code, get_r16(code) - 1);
            break;
        case 1:
            instr_complete = true;
            break;
    }
}
static void add_hl_r16()
{
    /* TODO: Same logic for inc r16 (?) */
    byte sum;
    switch (instr_cycle) {
        case 0:
            sum = add_byte_flags(
                get_r8(5), get_lo_byte(get_r16(get_bits(instr_reg, 5, 4))),
                false, NULL, NULL, NULL, NULL);
            set_r8(5, sum);
            break;
        case 1:
            bit n_flag, h_flag, c_flag;
            sum = add_byte_flags(
                get_r8(4), get_hi_byte(get_r16(get_bits(instr_reg, 5, 4))),
                true, NULL, &n_flag, &h_flag, &c_flag);
            set_r8(4, sum);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            instr_complete = true;
            break;
    }
}
static void inc_r8()
{
    int code = get_bits(instr_reg, 5, 3);
    bit z_flag, n_flag, h_flag;
    set_r8(code, add_byte_flags(
        get_r8(code), 1,
        false, &z_flag, &n_flag, &h_flag, NULL));
    set_zero(z_flag);
    set_subtraction(n_flag);
    set_half_carry(h_flag);
    instr_complete = true;
}
static void inc_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            bit z_flag, n_flag, h_flag;
            memory_write(state.hl_reg, add_byte_flags(
                get_z_latch(), 1,
                false, &z_flag, &n_flag, &h_flag, NULL));
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void dec_r8()
{
    int code = get_bits(instr_reg, 5, 3);
    bit z_flag, n_flag, h_flag;
    set_r8(code, sub_byte_flags(
        get_r8(code), 1,
        &z_flag, &n_flag, &h_flag, NULL));
    set_zero(z_flag);
    set_subtraction(n_flag);
    set_half_carry(h_flag);
    instr_complete = true;
}
static void dec_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            bit z_flag, n_flag, h_flag;
            memory_write(state.hl_reg, sub_byte_flags(
                get_z_latch(), 1,
                &z_flag, &n_flag, &h_flag, NULL));
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void ld_r8_imm8()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            set_r8(get_bits(instr_reg, 5, 3), get_z_latch());
            instr_complete = true;
            break;
    }
}
static void ld_hlmem_imm8()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            memory_write(state.hl_reg, get_z_latch());
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void rlca()
{
    byte a_reg = get_hi_byte(state.af_reg);
    bit b7 = get_bit(a_reg, 7);
    state.af_reg = set_hi_byte(state.af_reg, (a_reg << 1) | b7);
    set_zero(0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b7);
    instr_complete = true;
}
static void rla()
{
    byte a_reg = get_hi_byte(state.af_reg);
    bit b7 = get_bit(a_reg, 7);
    state.af_reg = set_hi_byte(state.af_reg, (a_reg << 1) | get_carry());
    set_zero(0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b7);
    instr_complete = true;
}
static void rrca()
{
    byte a_reg = get_hi_byte(state.af_reg);
    bit b0 = get_bit(a_reg, 0);
    state.af_reg = set_hi_byte(state.af_reg, (a_reg >> 1) | (b0 << 7));
    set_zero(0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b0);
    instr_complete = true;
}
static void rra()
{
    byte a_reg = get_hi_byte(state.af_reg);
    bit b0 = get_bit(a_reg, 0);
    state.af_reg = set_hi_byte(state.af_reg, (a_reg >> 1) | (get_carry() << 7));
    set_zero(0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b0);
    instr_complete = true;
}
static void daa()
{
    /* https://blog.ollien.com/posts/gb-daa/.*/
    byte a_reg = get_hi_byte(state.af_reg);
    bit subtraction = get_subtraction();
    bit half_carry = get_half_carry();
    bit carry = get_carry();
    byte offset = 0x00;
    if ((subtraction == 0 && get_lo_nibble(a_reg) > 0x9) || half_carry == 1) {
        offset |= 0x06;
    }
    if ((subtraction == 0 && a_reg > 0x99) || carry == 1) {
        offset |= 0x60;
        set_carry(1);
    }
    
    if (subtraction == 0)
        a_reg += offset;
    else
        a_reg -= offset;

    set_half_carry(0);
    set_zero(a_reg == 0x00 ? 1 : 0);
    state.af_reg = set_hi_byte(state.af_reg, a_reg);
    instr_complete = true;
}
static void cpl()
{
    byte a_reg = get_hi_byte(state.af_reg);
    state.af_reg = set_hi_byte(state.af_reg, ~a_reg);
    set_subtraction(1);
    set_half_carry(1);
    instr_complete = true;
}
static void scf()
{
    set_subtraction(0);
    set_half_carry(0);
    set_carry(1);
    instr_complete = true;
}
static void ccf()
{
    set_subtraction(0);
    set_half_carry(0);
    set_carry(!get_carry());
    instr_complete = true;
}
static void jr_imm8()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            wz_latch = state.pc_reg + (int8_t)get_z_latch();
            break;
        case 2:
            state.pc_reg = wz_latch;
            instr_complete = true;
            break;
    }
}
static void jr_cond_imm8()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            if (check_cond(get_bits(instr_reg, 4, 3))) {
                wz_latch = state.pc_reg + (int8_t)get_z_latch();
            }
            else
                instr_complete = true;
            break;
        case 2:
            state.pc_reg = wz_latch;
            instr_complete = true;
            break;
    }
}
static void stop()
{
    /* TODO: STOP instruction. */
}
static void halt()
{
    /* TODO: HALT instruction. */
}
static void ld_hlmem_r8()
{
    switch (instr_cycle) {
        case 0:
            memory_write(state.hl_reg, get_r8(get_bits(instr_reg, 2, 0)));
            break;
        case 1:
            instr_complete = true;
            break;
    }
}
static void ld_r8_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            set_r8(get_bits(instr_reg, 5, 3), get_z_latch());
            instr_complete = true;
            break;
    }
}
static void ld_r8_r8()
{
    set_r8(get_bits(instr_reg, 5, 3), get_r8(get_bits(instr_reg, 2, 0)));
    instr_complete = true;
}

bool cpu_test_init(read_fn _read, write_fn _write, receive_int_fn _receive_int)
{
    memory_read = _read;
    memory_write = _write;
    receive_int = _receive_int;

    instr_func = &nop;
    instr_cycle = 0;
    instr_complete = false;

    return true;
}
cpu_state cpu_test_get_state() {
    return state;
}
void cpu_test_set_state(cpu_state _state) {
    state = _state;
    fetch_and_decode();
}