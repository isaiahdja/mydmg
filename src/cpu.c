#include "cpu.h"
#ifndef SM83
#include "bus.h"
#include "interrupt.h"
#endif

#include <stddef.h>
#include <stdio.h>

/* Central Processing Unit core. */

/* High RAM. */
#ifndef SM83
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
static pending_int_fn pending_int;
static receive_int_fn receive_int;

static bool cb_prefixed;
static bool cond;
static int adj;
static int set_ime;
static bool halted;

static void fetch_and_decode(void);
static void nop(void);
static void di(void);
static void call_int(void);

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

/* Z */
static inline bit get_zero(void) {
    return get_bit(state.af_reg, 7);
}
static inline void set_zero(bit val) {
    state.af_reg = set_bit(state.af_reg, 7, val);
}
/* N */
static inline bit get_subtraction(void) {
    return get_bit(state.af_reg, 6);
}
static inline void set_subtraction(bit val) {
    state.af_reg = set_bit(state.af_reg, 6, val);
}
/* H */
static inline bit get_half_carry(void) {
    return get_bit(state.af_reg, 5);
}
static inline void set_half_carry(bit val) {
    state.af_reg = set_bit(state.af_reg, 5, val);
}
/* C */
static inline bit get_carry(void) {
    return get_bit(state.af_reg, 4);
}
static inline void set_carry(bit val) {
    state.af_reg = set_bit(state.af_reg, 4, val);
}

#ifndef SM83
bool cpu_init(void)
{
    memory_read = &bus_read_cpu;
    memory_write = &bus_write_cpu;
    receive_int = &interrupt_send_interrupt;
    pending_int = &interrupt_pending;

    /* DMG boot handoff state. */
    state = (cpu_state){
        0x01B0,
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
    set_ime = -1;
    cb_prefixed = false;
    halted = false;

    return true;
}
#endif

void cpu_tick(void)
{
    if (halted) {
        if (pending_int()) {
            halted = false;
            instr_func = &nop;
        }
        else
            return;
    }
    if (set_ime != -1) {
        state.ime_flag = set_ime;
        set_ime = -1;
    }
    
    /* The current instruction function will set instr_completed to true if it
       has completed, allowing the next instruction to be fetched. This models
       the CPU fetch/execute overlap.
       Note that the final cycle of an instruction must not use the memory bus,
       as it is reserved for fetching. */
    instr_func();

    if (instr_complete)  {
        /* The fetch between a CB prefix and the opcode is non-interruptible. */
        bool was_cb_prefixed = cb_prefixed;
        /* Like EI, DI is technically delayed by a cycle, but "there is extra
        circuitry(!) to check if the currently executed instruction is a DI,
        and defer interrupt dispatch by one cycle (which end up being a whole
        instruction).*/
        bool was_di = instr_func == &di;
        /* fetch_and_decode() will reset instr_func, instr_cycle,
           instr_complete, cb_prefixed... */
        fetch_and_decode();

        if (!was_cb_prefixed && !was_di && (state.ime_flag == 1 && pending_int())) {
            halted = false;
            state.ime_flag = 0;
            instr_func = &call_int;
            cb_prefixed = false;
        }
    }
    else
        instr_cycle++;
}

#ifndef SM83
byte hram_read(uint16_t addr) {
    return hram[addr - HRAM_START];
}
void hram_write(uint16_t addr, byte val) {
    hram[addr - HRAM_START] = val;
}
#endif

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
static uint16_t get_r16stk(int code)
{
    switch (code) {
        case 0: return state.bc_reg;
        case 1: return state.de_reg;
        case 2: return state.hl_reg;
        case 3: return state.af_reg;
    }
}
static void set_r16stk(int code, uint16_t val)
{
    switch (code) {
        case 0: state.bc_reg = val; break;
        case 1: state.de_reg = val; break;
        case 2: state.hl_reg = val; break;
        case 3: state.af_reg = val & 0xFFF0; break;
    }
}
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
static void check_cond(int code)
{
    switch (code) {
        case 0: cond = !get_zero();  break;
        case 1: cond = get_zero();   break;
        case 2: cond = !get_carry(); break;
        case 3: cond = get_carry();  break;
    }
}

uint8_t add_u8_u8(uint8_t num1, uint8_t num2, bit c_in,
    bit *z_out, bit *n_out, bit *h_out, bit *c_out)
{
    uint8_t result = 0x00;
    bit carry = c_in;
    for (int i = 0; i < 8; i++) {
        bit num1_i = get_bit(num1, i);
        bit num2_i = get_bit(num2, i);

        bit sum = num1_i ^ num2_i ^ carry;
        carry = (num1_i & num2_i) | (num1_i & carry) | (num2_i & carry);

        if (i == 3 && h_out != NULL)
            *h_out = carry;
        if (i == 7 && c_out != NULL)
            *c_out = carry;

        result |= (sum << i);
    }

    if (z_out != NULL)
        *z_out = result == 0x00 ? 1 : 0;
    if (n_out != NULL)
        *n_out = 0;
    return result;
}
uint8_t sub_u8_u8(uint8_t num1, uint8_t num2, bit b_in,
    bit *z_out, bit *n_out, bit *h_out, bit *c_out)
{
    uint8_t result = 0x00;
    bit borrow = b_in;
    for (int i = 0; i < 8; i++) {
        bit num1_i = get_bit(num1, i);
        bit num2_i = get_bit(num2, i);

        bit diff = num1_i ^ num2_i ^ borrow;
        borrow = (!num1_i & num2_i) | (!(num1_i ^ num2_i) & borrow);

        if (i == 3 && h_out != NULL)
            *h_out = borrow;
        if (i == 7 && c_out != NULL)
            *c_out = borrow;

        result |= (diff << i);
    }

    if (z_out != NULL)
        *z_out = result == 0x00 ? 1 : 0;
    if (n_out != NULL)
        *n_out = 1;
    return result;
}
static int calc_adj(bit carry, bit sign)
{
    if (carry == 1 && sign == 0)
        return 1;
    if (carry == 0 && sign == 1)
        return -1;
    return 0;
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
    int code;
    switch (instr_cycle) {
        case 0:
            code = get_bits(instr_reg, 5, 4);
            set_r16(code, get_r16(code) + 1);
            break;
        case 1:
            instr_complete = true;
            break;
    }
}
static void dec_r16()
{
    int code;
    switch (instr_cycle) {
        case 0:
            code = get_bits(instr_reg, 5, 4);
            set_r16(code, get_r16(code) - 1);
            break;
        case 1:
            instr_complete = true;
            break;
    }
}
static void add_hl_r16()
{
    int code = get_bits(instr_reg, 5, 4);
    bit n_flag, h_flag, c_flag;
    byte sum;
    switch (instr_cycle) {
        case 0:
            sum = add_u8_u8(get_lo_byte(state.hl_reg), get_lo_byte(get_r16(code)), 0,
                NULL, &n_flag, &h_flag, &c_flag);
            state.hl_reg = set_lo_byte(state.hl_reg, sum);
            break;
        case 1:
            sum = add_u8_u8(get_hi_byte(state.hl_reg), get_hi_byte(get_r16(code)), get_carry(),
                NULL, &n_flag, &h_flag, &c_flag);
            state.hl_reg = set_hi_byte(state.hl_reg, sum);
            instr_complete = true;
            break;
    }
    set_subtraction(n_flag);
    set_half_carry(h_flag);
    set_carry(c_flag);
}
static void inc_r8()
{
    int code = get_bits(instr_reg, 5, 3);
    bit z_flag, n_flag, h_flag;
    set_r8(code, add_u8_u8(get_r8(code), 1, 0,
        &z_flag, &n_flag, &h_flag, NULL));
    set_zero(z_flag);
    set_subtraction(n_flag);
    set_half_carry(h_flag);
    instr_complete = true;
}
static void inc_hlmem()
{
    bit z_flag, n_flag, h_flag;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            memory_write(state.hl_reg, add_u8_u8(get_z_latch(), 1, 0,
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
static void dec_r8()
{
    bit z_flag, n_flag, h_flag;
    int code = get_bits(instr_reg, 5, 3);
    set_r8(code, sub_u8_u8(get_r8(code), 1, 0,
        &z_flag, &n_flag, &h_flag, NULL));
    set_zero(z_flag);
    set_subtraction(n_flag);
    set_half_carry(h_flag);
    instr_complete = true;
}
static void dec_hlmem()
{
    bit z_flag, n_flag, h_flag;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            memory_write(state.hl_reg, sub_u8_u8(get_z_latch(), 1, 0,
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
            /* We could use add_u8_u8 and the corresponding adjustment here, but
               it is not necessary as this operation is not split between
               cycles ("ALU and IDU magic").
               C will handle the two's complement with the cast. */
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
            check_cond(get_bits(instr_reg, 4, 3));
            break;
        case 1:
            if (cond)
            /* We could use add_u8_u8 and the corresponding adjustment here, but
               it is not necessary as this operation is not split between cycles
               and C will handle the two's complement with the cast. */
                wz_latch = state.pc_reg + (int8_t)get_z_latch();
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
#ifdef DEBUG
    printf("STOP\n");
#endif
}
static void halt()
{
/*
#ifdef DEBUG
    printf("HALT\n");
#endif
*/
    halted = true;
    instr_complete = true;
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
static void add_a_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    bit z_flag, n_flag, h_flag, c_flag;
    byte sum = add_u8_u8(get_hi_byte(state.af_reg), get_r8(code), 0,
        &z_flag, &n_flag, &h_flag, &c_flag);
    set_zero(z_flag);
    set_subtraction(n_flag);
    set_half_carry(h_flag);
    set_carry(c_flag);
    state.af_reg = set_hi_byte(state.af_reg, sum);
    instr_complete = true;
}
static void add_a_hlmem()
{
    bit z_flag, n_flag, h_flag, c_flag;
    byte sum;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            sum = add_u8_u8(get_hi_byte(state.af_reg), get_z_latch(), 0,
                &z_flag, &n_flag, &h_flag, &c_flag);
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            state.af_reg = set_hi_byte(state.af_reg, sum);
            instr_complete = true;
            break;
    }
}
static void adc_a_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    bit z_flag, n_flag, h_flag, c_flag;
    byte sum = add_u8_u8(get_hi_byte(state.af_reg), get_r8(code), get_carry(),
        &z_flag, &n_flag, &h_flag, &c_flag);
    set_zero(z_flag);
    set_subtraction(n_flag);
    set_half_carry(h_flag);
    set_carry(c_flag);
    state.af_reg = set_hi_byte(state.af_reg, sum);
    instr_complete = true;
}
static void adc_a_hlmem()
{
    bit z_flag, n_flag, h_flag, c_flag;
    byte sum;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            sum = add_u8_u8(get_hi_byte(state.af_reg), get_z_latch(), get_carry(),
                &z_flag, &n_flag, &h_flag, &c_flag);
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            state.af_reg = set_hi_byte(state.af_reg, sum);
            instr_complete = true;
            break;
    }
}
static void sub_a_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    bit z_flag, n_flag, h_flag, c_flag;
    byte diff = sub_u8_u8(get_hi_byte(state.af_reg), get_r8(code), 0,
        &z_flag, &n_flag, &h_flag, &c_flag);
    set_zero(z_flag);
    set_subtraction(n_flag);
    set_half_carry(h_flag);
    set_carry(c_flag);
    state.af_reg = set_hi_byte(state.af_reg, diff);
    instr_complete = true;
}
static void sub_a_hlmem()
{
    bit z_flag, n_flag, h_flag, c_flag;
    byte diff;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            diff = sub_u8_u8(get_hi_byte(state.af_reg), get_z_latch(), 0,
                &z_flag, &n_flag, &h_flag, &c_flag);
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            state.af_reg = set_hi_byte(state.af_reg, diff);
            instr_complete = true;
            break;
    }
}
static void sbc_a_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    bit z_flag, n_flag, h_flag, c_flag;
    byte diff = sub_u8_u8(get_hi_byte(state.af_reg), get_r8(code), get_carry(),
        &z_flag, &n_flag, &h_flag, &c_flag);
    set_zero(z_flag);
    set_subtraction(n_flag);
    set_half_carry(h_flag);
    set_carry(c_flag);
    state.af_reg = set_hi_byte(state.af_reg, diff);
    instr_complete = true;
}
static void sbc_a_hlmem()
{
    bit z_flag, n_flag, h_flag, c_flag;
    byte diff;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            diff = sub_u8_u8(get_hi_byte(state.af_reg), get_z_latch(), get_carry(),
                &z_flag, &n_flag, &h_flag, &c_flag);
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            state.af_reg = set_hi_byte(state.af_reg, diff);
            instr_complete = true;
            break;
    }
}
static void and_a_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte and = get_hi_byte(state.af_reg) & get_r8(code);
    state.af_reg = set_hi_byte(state.af_reg, and);
    set_zero(and == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(1);
    set_carry(0);
    instr_complete = true;
}
static void and_a_hlmem()
{
    byte and;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            and = get_hi_byte(state.af_reg) & get_z_latch();
            state.af_reg = set_hi_byte(state.af_reg, and);
            set_zero(and == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(1);
            set_carry(0);
            instr_complete = true;
            break;
    }
}
static void xor_a_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte xor = get_hi_byte(state.af_reg) ^ get_r8(code);
    state.af_reg = set_hi_byte(state.af_reg, xor);
    set_zero(xor == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(0);
    instr_complete = true;
}
static void xor_a_hlmem()
{
    byte xor;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            xor = get_hi_byte(state.af_reg) ^ get_z_latch();
            state.af_reg = set_hi_byte(state.af_reg, xor);
            set_zero(xor == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(0);
            instr_complete = true;
            break;
    }
}
static void or_a_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte or = get_hi_byte(state.af_reg) | get_r8(code);
    state.af_reg = set_hi_byte(state.af_reg, or);
    set_zero(or == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(0);
    instr_complete = true;
}
static void or_a_hlmem()
{
    byte or;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            or = get_hi_byte(state.af_reg) | get_z_latch();
            state.af_reg = set_hi_byte(state.af_reg, or);
            set_zero(or == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(0);
            instr_complete = true;
            break;
    }
}
static void cp_a_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    bit z_flag, n_flag, h_flag, c_flag;
    sub_u8_u8(get_hi_byte(state.af_reg), get_r8(code), 0,
        &z_flag, &n_flag, &h_flag, &c_flag);
    set_zero(z_flag);
    set_subtraction(n_flag);
    set_half_carry(h_flag);
    set_carry(c_flag);
    instr_complete = true;
}
static void cp_a_hlmem()
{
    bit z_flag, n_flag, h_flag, c_flag;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            sub_u8_u8(get_hi_byte(state.af_reg), get_z_latch(), 0,
                &z_flag, &n_flag, &h_flag, &c_flag);
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            instr_complete = true;
            break;
    }
}
static void add_a_imm8()
{
    bit z_flag, n_flag, h_flag, c_flag;
    byte sum;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            sum = add_u8_u8(get_hi_byte(state.af_reg), get_z_latch(), 0,
                &z_flag, &n_flag, &h_flag, &c_flag);
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            state.af_reg = set_hi_byte(state.af_reg, sum);
            instr_complete = true;
            break;
    }
}
static void adc_a_imm8()
{
    bit z_flag, n_flag, h_flag, c_flag;
    byte sum;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            sum = add_u8_u8(get_hi_byte(state.af_reg), get_z_latch(), get_carry(),
                &z_flag, &n_flag, &h_flag, &c_flag);
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            state.af_reg = set_hi_byte(state.af_reg, sum);
            instr_complete = true;
            break;
    }
}
static void sub_a_imm8()
{
    bit z_flag, n_flag, h_flag, c_flag;
    byte diff;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            diff = sub_u8_u8(get_hi_byte(state.af_reg), get_z_latch(), 0,
                &z_flag, &n_flag, &h_flag, &c_flag);
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            state.af_reg = set_hi_byte(state.af_reg, diff);
            instr_complete = true;
            break;
    }
}
static void sbc_a_imm8()
{
    bit z_flag, n_flag, h_flag, c_flag;
    byte diff;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            diff = sub_u8_u8(get_hi_byte(state.af_reg), get_z_latch(), get_carry(),
                &z_flag, &n_flag, &h_flag, &c_flag);
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            state.af_reg = set_hi_byte(state.af_reg, diff);
            instr_complete = true;
            break;
    }
}
static void and_a_imm8()
{
    byte and;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            and = get_hi_byte(state.af_reg) & get_z_latch();
            state.af_reg = set_hi_byte(state.af_reg, and);
            set_zero(and == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(1);
            set_carry(0);
            instr_complete = true;
            break;
    }
}
static void xor_a_imm8()
{
    byte xor;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            xor = get_hi_byte(state.af_reg) ^ get_z_latch();
            state.af_reg = set_hi_byte(state.af_reg, xor);
            set_zero(xor == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(0);
            instr_complete = true;
            break;
    }
}
static void or_a_imm8()
{
    byte or;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            or = get_hi_byte(state.af_reg) | get_z_latch();
            state.af_reg = set_hi_byte(state.af_reg, or);
            set_zero(or == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(0);
            instr_complete = true;
            break;
    }
}
static void cp_a_imm8()
{
    bit z_flag, n_flag, h_flag, c_flag;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            sub_u8_u8(get_hi_byte(state.af_reg), get_z_latch(), 0,
                &z_flag, &n_flag, &h_flag, &c_flag);
            set_zero(z_flag);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            instr_complete = true;
            break;
    }
}
static void ret_cond()
{
    switch (instr_cycle) {
        case 0:
            check_cond(get_bits(instr_reg, 4, 3));
            break;
        case 1:
            if (cond)
                set_z_latch(memory_read(state.sp_reg++));
            else
                instr_complete = true;
            break;
        case 2:
            set_w_latch(memory_read(state.sp_reg++));
            break;
        case 3:
            state.pc_reg = wz_latch;
            break;
        case 4:
            instr_complete = true;
            break;
    }
}
static void ret()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.sp_reg++));
            break;
        case 1:
            set_w_latch(memory_read(state.sp_reg++));
            break;
        case 2:
            state.pc_reg = wz_latch;
            break;
        case 3:
            instr_complete = true;
            break;
    }
}
static void reti()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.sp_reg++));
            break;
        case 1:
            set_w_latch(memory_read(state.sp_reg++));
            break;
        case 2:
            state.pc_reg = wz_latch;
            state.ime_flag = 1;
            break;
        case 3:
            instr_complete = true;
            break;
    }
}
static void jp_cond_imm16()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            set_w_latch(memory_read(state.pc_reg++));
            check_cond(get_bits(instr_reg, 4, 3));
            break;
        case 2:
            if (cond)
                state.pc_reg = wz_latch;
            else
                instr_complete = true;
            break;
        case 3:
            instr_complete = true;
            break;
    }
}
static void jp_imm16()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            set_w_latch(memory_read(state.pc_reg++));
            break;
        case 2:
            state.pc_reg = wz_latch;
            break;
        case 3:
            instr_complete = true;
            break;
    }
}
static void jp_hl()
{
    state.pc_reg = state.hl_reg;
    instr_complete = true;
}
static void call_cond_imm16()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            set_w_latch(memory_read(state.pc_reg++));
            check_cond(get_bits(instr_reg, 4, 3));
            break;
        case 2:
            if (cond)
                state.sp_reg--;
            else
                instr_complete = true;
            break;
        case 3:
            memory_write(state.sp_reg--, get_hi_byte(state.pc_reg));
            break;
        case 4:
            memory_write(state.sp_reg, get_lo_byte(state.pc_reg));
            state.pc_reg = wz_latch;
            break;
        case 5:
            instr_complete = true;
            break;
    }
}
static void call_imm16()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            set_w_latch(memory_read(state.pc_reg++));
            break;
        case 2:
            state.sp_reg--;
            break;
        case 3:
            memory_write(state.sp_reg--, get_hi_byte(state.pc_reg));
            break;
        case 4:
            memory_write(state.sp_reg, get_lo_byte(state.pc_reg));
            state.pc_reg = wz_latch;
            break;
        case 5:
            instr_complete = true;
            break;
    }
}
static void rst_tgt3()
{
    switch (instr_cycle) {
        case 0:
            state.sp_reg--;
            break;
        case 1:
            memory_write(state.sp_reg--, get_hi_byte(state.pc_reg));
            break;
        case 2:
            memory_write(state.sp_reg, get_lo_byte(state.pc_reg));
            state.pc_reg = get_bits(instr_reg, 5, 3) << 3;
            break;
        case 3:
            instr_complete = true;
            break;
    }
}
static void pop_r16stk()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.sp_reg++));
            break;
        case 1:
            set_w_latch(memory_read(state.sp_reg++));
            break;
        case 2:
            set_r16stk(get_bits(instr_reg, 5, 4), wz_latch);
            instr_complete = true;
            break;
    }   
}
static void push_r16stk()
{
    switch (instr_cycle) {
        case 0:
            /* "Because PUSH and POP use the IDU, and the IDU can only do
            post-increment and post-decrement (so, no pre-increment or
            pre-decrement), there is an extra delay cycle in PUSH for this
            reason." */
            state.sp_reg--;
            break;
        case 1:
            memory_write(state.sp_reg--, get_hi_byte(
                get_r16stk(get_bits(instr_reg, 5, 4))));
            break;
        case 2:
            memory_write(state.sp_reg, get_lo_byte(
                get_r16stk(get_bits(instr_reg, 5, 4))));
            break;
        case 3:
            instr_complete = true;
            break;
    }
}
static void ldh_cmem_a()
{
    switch (instr_cycle) {
        case 0:
            memory_write(0xFF00 | get_lo_byte(state.bc_reg),
                get_hi_byte(state.af_reg));
            break;
        case 1:
            instr_complete = true;
            break;
    }
}
static void ldh_imm8mem_a()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            memory_write(0xFF00 | get_z_latch(), get_hi_byte(state.af_reg));
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void ld_imm16mem_a()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            set_w_latch(memory_read(state.pc_reg++));
            break;
        case 2:
            memory_write(wz_latch, get_hi_byte(state.af_reg));
            break;
        case 3:
            instr_complete = true;
            break;
    }
}
static void ld_a_cmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(0xFF00 | get_lo_byte(state.bc_reg)));
            break;
        case 1:
            state.af_reg = set_hi_byte(state.af_reg, get_z_latch());
            instr_complete = true;
            break;
    }
}
static void ldh_a_imm8mem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            set_z_latch(memory_read(0xFF00 | get_z_latch()));
            break;
        case 2:
            state.af_reg = set_hi_byte(state.af_reg, get_z_latch());
            instr_complete = true;
            break;
    }
}
static void ld_a_imm16mem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            set_w_latch(memory_read(state.pc_reg++));
            break;
        case 2:
            set_z_latch(memory_read(wz_latch));
            break;
        case 3:
            state.af_reg = set_hi_byte(state.af_reg, get_z_latch());
            instr_complete = true;
            break;
    }
}
static void add_sp_imm8()
{
    bit n_flag, h_flag, c_flag;
    byte sum;
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            sum = add_u8_u8(get_lo_byte(state.sp_reg), get_z_latch(), 0,
                NULL, &n_flag, &h_flag, &c_flag);
            adj = calc_adj(c_flag, get_bit(get_z_latch(), 7));
            set_zero(0);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            set_z_latch(sum);
            break;
        case 2:
            set_w_latch(get_hi_byte(state.sp_reg) + adj);
            break;
        case 3:
            state.sp_reg = wz_latch;
            instr_complete = true;
            break;
    }
}
static void ld_hl_sp_imm8()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.pc_reg++));
            break;
        case 1:
            bit n_flag, h_flag, c_flag;
            byte sum = add_u8_u8(get_lo_byte(state.sp_reg), get_z_latch(), 0,
                NULL, &n_flag, &h_flag, &c_flag);
            adj = calc_adj(c_flag, get_bit(get_z_latch(), 7));
            set_zero(0);
            set_subtraction(n_flag);
            set_half_carry(h_flag);
            set_carry(c_flag);
            state.hl_reg = set_lo_byte(state.hl_reg, sum);
            break;
        case 2:
            state.hl_reg = set_hi_byte(state.hl_reg, get_hi_byte(state.sp_reg) + (byte)adj);
            instr_complete = true;
            break;
    }
}
static void ld_sp_hl()
{
    switch (instr_cycle) {
        case 0:
            state.sp_reg = state.hl_reg;
            break;
        case 1:
            instr_complete = true;
            break;
    }
}
static void di()
{
    set_ime = 0;
    instr_complete = true;
}
static void ei()
{
    set_ime = 1;
    instr_complete = true;
}
static void rlc_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte reg = get_r8(code);
    bit b7 = get_bit(reg, 7);
    byte shift = (reg << 1) | b7;
    set_r8(code, shift);
    set_zero(shift == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b7);
    instr_complete = true;
}
static void rlc_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            byte reg = get_z_latch();
            bit b7 = get_bit(reg, 7);
            byte shift = (reg << 1) | b7;
            memory_write(state.hl_reg, shift);
            set_zero(shift == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(b7);
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void rrc_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte reg = get_r8(code);
    bit b0 = get_bit(reg, 0);
    byte shift = (reg >> 1) | (b0 << 7);
    set_r8(code, shift);
    set_zero(shift == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b0);
    instr_complete = true;
}
static void rrc_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            byte reg = get_z_latch();
            bit b0 = get_bit(reg, 0);
            byte shift = (reg >> 1) | (b0 << 7);
            memory_write(state.hl_reg, shift);
            set_zero(shift == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(b0);
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void rl_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte reg = get_r8(code);
    bit b7 = get_bit(reg, 7);
    byte shift = (reg << 1) | get_carry();
    set_r8(code, shift);
    set_zero(shift == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b7);
    instr_complete = true;
}
static void rl_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            byte reg = get_z_latch();
            bit b7 = get_bit(reg, 7);
            byte shift = (reg << 1) | get_carry();
            memory_write(state.hl_reg, shift);
            set_zero(shift == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(b7);
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void rr_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte reg = get_r8(code);
    bit b0 = get_bit(reg, 0);
    byte shift = (reg >> 1) | (get_carry() << 7);
    set_r8(code, shift);
    set_zero(shift == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b0);
    instr_complete = true;
}
static void rr_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            byte reg = get_z_latch();
            bit b0 = get_bit(reg, 0);
            byte shift = (reg >> 1) | (get_carry() << 7);
            memory_write(state.hl_reg, shift);
            set_zero(shift == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(b0);
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void sla_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte reg = get_r8(code);
    bit b7 = get_bit(reg, 7);
    byte shift = (reg << 1);
    set_r8(code, shift);
    set_zero(shift == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b7);
    instr_complete = true;
}
static void sla_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            byte reg = get_z_latch();
            bit b7 = get_bit(reg, 7);
            byte shift = (reg << 1);
            memory_write(state.hl_reg, shift);
            set_zero(shift == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(b7);
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void sra_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte reg = get_r8(code);
    bit b0 = get_bit(reg, 0);
    byte b7_no_shift = reg & 0x80;
    byte shift = (reg >> 1) | b7_no_shift;
    set_r8(code, shift);
    set_zero(shift == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b0);
    instr_complete = true;
}
static void sra_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            byte reg = get_z_latch();
            bit b0 = get_bit(reg, 0);
            byte b7_no_shift = reg & 0x80;
            byte shift = (reg >> 1) | b7_no_shift;
            memory_write(state.hl_reg, shift);
            set_zero(shift == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(b0);
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void swap_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte reg = get_r8(code);
    byte swap = (get_lo_nibble(reg) << 4) | (get_hi_nibble(reg));
    set_r8(code, swap);
    set_zero(swap == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(0);
    instr_complete = true;
}
static void swap_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            int code = get_bits(instr_reg, 2, 0);
            byte reg = get_z_latch();
            byte swap = (get_lo_nibble(reg) << 4) | (get_hi_nibble(reg));
            memory_write(state.hl_reg, swap);
            set_zero(swap == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(0);
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void srl_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    byte reg = get_r8(code);
    bit b0 = get_bit(reg, 0);
    byte shift = (reg >> 1);
    set_r8(code, shift);
    set_zero(shift == 0x00 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(0);
    set_carry(b0);
    instr_complete = true;
}
static void srl_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            byte reg = get_z_latch();
            bit b0 = get_bit(reg, 0);
            byte shift = (reg >> 1);
            memory_write(state.hl_reg, shift);
            set_zero(shift == 0x00 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(0);
            set_carry(b0);
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void bit_b3_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    int idx = get_bits(instr_reg, 5, 3);
    bit b = get_bit(get_r8(code), idx);
    set_zero(b == 0 ? 1 : 0);
    set_subtraction(0);
    set_half_carry(1);
    instr_complete = true;
}
static void bit_b3_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            int idx = get_bits(instr_reg, 5, 3);
            bit b = get_bit(get_z_latch(), idx);
            set_zero(b == 0 ? 1 : 0);
            set_subtraction(0);
            set_half_carry(1);
            instr_complete = true;
            break;
    }
}
static void res_b3_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    int idx = get_bits(instr_reg, 5, 3);
    set_r8(code, set_bit(get_r8(code), idx, 0));
    instr_complete = true;
}
static void res_b3_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            int code = get_bits(instr_reg, 2, 0);
            int idx = get_bits(instr_reg, 5, 3);
            memory_write(state.hl_reg, set_bit(get_z_latch(), idx, 0));
            break;
        case 2:
            instr_complete = true;
            break;
    }
}
static void set_b3_r8()
{
    int code = get_bits(instr_reg, 2, 0);
    int idx = get_bits(instr_reg, 5, 3);
    set_r8(code, set_bit(get_r8(code), idx, 1));
    instr_complete = true;
}
static void set_b3_hlmem()
{
    switch (instr_cycle) {
        case 0:
            set_z_latch(memory_read(state.hl_reg));
            break;
        case 1:
            int code = get_bits(instr_reg, 2, 0);
            int idx = get_bits(instr_reg, 5, 3);
            memory_write(state.hl_reg, set_bit(get_z_latch(), idx, 1));
            break;
        case 2:
            instr_complete = true;
            break;
    }
}

static uint16_t jump_vec;
static void call_int()
{
    switch (instr_cycle) {
        case 0:
            state.pc_reg--;
            break;
        case 1:
            state.sp_reg--;
            break;
        case 2:
            memory_write(state.sp_reg--, get_hi_byte(state.pc_reg));

            /* Only now do we get the jump vector and reset the bit in IF, but
               only if an interrupt is still pending!
               Note that this also means that the original interrupt that was
               going to be serviced may now be replaced by a higher-priority
               interrupt too. */
            if (!receive_int(&jump_vec)) {
#ifdef DEBUG
                printf("Interrupt glitch triggered\n");
#endif
                jump_vec = 0x0000;
            }
            break;
        case 3:
            memory_write(state.sp_reg, get_lo_byte(state.pc_reg));
            state.pc_reg = jump_vec;
            break;
        case 4:
            instr_complete = true;
            break;
    }
}

static void fetch_and_decode()
{   
    instr_complete = false;
    instr_func = &nop;
    instr_cycle = 0;

    instr_reg = memory_read(state.pc_reg);
    //printf("PC = %04x, IR = %02X\n", state.pc_reg, instr_reg);
    if (!halted)
        state.pc_reg++;

    int b_7_6 = get_bits(instr_reg, 7, 6);
    int b_2_0 = get_bits(instr_reg, 2, 0);
    if (cb_prefixed) {
#pragma region
        cb_prefixed = false;
        int b_7_3 = get_bits(instr_reg, 7, 3);
        if (b_7_3 == 0) {
            if (instr_reg == 0x06)
                instr_func = &rlc_hlmem;
            else
                instr_func = &rlc_r8;
            return;
        }
        if (b_7_3 == 1) {
            if (instr_reg == 0x0E)
                instr_func = &rrc_hlmem;
            else
                instr_func = &rrc_r8;
            return;
        }
        if (b_7_3 == 2) {
            if (instr_reg == 0x16)
                instr_func = &rl_hlmem;
            else
                instr_func = &rl_r8;
            return;
        }
        if (b_7_3 == 3) {
            if (instr_reg == 0x1E)
                instr_func = &rr_hlmem;
            else
                instr_func = &rr_r8;
            return;
        }
        if (b_7_3 == 4) {
            if (instr_reg == 0x26)
                instr_func = &sla_hlmem;
            else
                instr_func = &sla_r8;
            return;
        }
        if (b_7_3 == 5) {
            if (instr_reg == 0x2E)
                instr_func = &sra_hlmem;
            else
                instr_func = &sra_r8;
            return;
        }
        if (b_7_3 == 6) {
            if (instr_reg == 0x36)
                instr_func = &swap_hlmem;
            else
                instr_func = &swap_r8;
            return;
        }
        if (b_7_3 == 7) {
            if (instr_reg == 0x3E)
                instr_func = &srl_hlmem;
            else
                instr_func = &srl_r8;
            return;
        }

        if (b_7_6 == 1) {
            if (b_2_0 == 6)
                instr_func = &bit_b3_hlmem;
            else
                instr_func = &bit_b3_r8;
            return;
        }
        if (b_7_6 == 2) {
            if (b_2_0 == 6)
                instr_func = &res_b3_hlmem;
            else
                instr_func = &res_b3_r8;
            return;
        }
        if (b_7_6 == 3) {
            if (b_2_0 == 6)
                instr_func = &set_b3_hlmem;
            else
                instr_func = &set_b3_r8;
            return;
        }
#pragma endregion
    }
    else if (instr_reg == 0xCB) {
        cb_prefixed = true; /* NOP. */
        return;
    }

    /* "Block 0." */
#pragma region
    if (instr_reg == 0x00) {
        instr_func = &nop;
        return;
    }

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
    
    /* "Block 2." */
#pragma region
    int b_7_3 = get_bits(instr_reg, 7, 3);
    if (b_7_3 == 16) {
        if (instr_reg == 0x86)
            instr_func = &add_a_hlmem;
        else
            instr_func = &add_a_r8;
        return;
    }
    if (b_7_3 == 17) {
        if (instr_reg == 0x8E)
            instr_func = &adc_a_hlmem;
        else
            instr_func = &adc_a_r8;
        return;
    }
    if (b_7_3 == 18) {
        if (instr_reg == 0x96)
            instr_func = &sub_a_hlmem;
        else
            instr_func = &sub_a_r8;
        return;
    }
    if (b_7_3 == 19) {
        if (instr_reg == 0x9E)
            instr_func = &sbc_a_hlmem;
        else
            instr_func = &sbc_a_r8;
        return;
    }
    if (b_7_3 == 20) {
        if (instr_reg == 0xA6)
            instr_func = &and_a_hlmem;
        else
            instr_func = &and_a_r8;
        return;
    }
    if (b_7_3 == 21) {
        if (instr_reg == 0xAE)
            instr_func = &xor_a_hlmem;
        else
            instr_func = &xor_a_r8;
        return;
    }
    if (b_7_3 == 22) {
        if (instr_reg == 0xB6)
            instr_func = &or_a_hlmem;
        else
            instr_func = &or_a_r8;
        return;
    }
    if (b_7_3 == 23) {
        if (instr_reg == 0xBE)
            instr_func = &cp_a_hlmem;
        else
            instr_func = &cp_a_r8;
        return;
    }
#pragma endregion

    /* "Block 3." */
#pragma region
    if (instr_reg == 0xC6) {
        instr_func = &add_a_imm8;
        return;
    }
    if (instr_reg == 0xCE) {
        instr_func = &adc_a_imm8;
        return;
    }
    if (instr_reg == 0xD6) {
        instr_func = &sub_a_imm8;
        return;
    }
    if (instr_reg == 0xDE) {
        instr_func = &sbc_a_imm8;
        return;
    }
    if (instr_reg == 0xE6) {
        instr_func = &and_a_imm8;
        return;
    }
    if (instr_reg == 0xEE) {
        instr_func = &xor_a_imm8;
        return;
    }
    if (instr_reg == 0xF6) {
        instr_func = &or_a_imm8;
        return;
    }
    if (instr_reg == 0xFE) {
        instr_func = &cp_a_imm8;
        return;
    }

    if (b_7_5 == 6 && b_2_0 == 0) {
        instr_func = &ret_cond;
        return;
    }
    if (instr_reg == 0xC9) {
        instr_func = &ret;
        return;
    }
    if (instr_reg == 0xD9) {
        instr_func = &reti;
        return;
    }
    if (b_7_5 == 6 && b_2_0 == 2) {
        instr_func = &jp_cond_imm16;
        return;
    }
    if (instr_reg == 0xC3) {
        instr_func = &jp_imm16;
        return;
    }
    if (instr_reg == 0xE9) {
        instr_func = &jp_hl;
        return;
    }
    if (b_7_5 == 6 && b_2_0 == 4) {
        instr_func = &call_cond_imm16;
        return;
    }
    if (instr_reg == 0xCD) {
        instr_func = &call_imm16;
        return;
    }
    if (b_7_6 == 3 && b_2_0 == 7) {
        instr_func = &rst_tgt3;
        return;
    }

    if (b_7_6 == 3 && b_3_0 == 1) {
        instr_func = &pop_r16stk;
        return;
    }
    if (b_7_6 == 3 && b_3_0 == 5) {
        instr_func = &push_r16stk;
        return;
    }

    if (instr_reg == 0xE2) {
        instr_func = &ldh_cmem_a;
        return;
    }
    if (instr_reg == 0xE0) {
        instr_func = &ldh_imm8mem_a;
        return;
    }
    if (instr_reg == 0xEA) {
        instr_func = &ld_imm16mem_a;
        return;
    }
    if (instr_reg == 0xF2) {
        instr_func = &ld_a_cmem;
        return;
    }
    if (instr_reg == 0xF0) {
        instr_func = &ldh_a_imm8mem;
        return;
    }
    if (instr_reg == 0xFA) {
        instr_func = &ld_a_imm16mem;
        return;
    }

    if (instr_reg == 0xE8) {
        instr_func = &add_sp_imm8;
        return;
    }
    if (instr_reg == 0xF8) {
        instr_func = &ld_hl_sp_imm8;
        return;
    }
    if (instr_reg == 0xF9) {
        instr_func = &ld_sp_hl;
        return;
    }

    if (instr_reg == 0xF3) {
        instr_func = &di;
        return;
    }
    if (instr_reg == 0xFB) {
        instr_func = &ei;
        return;
    }
#pragma endregion
}

#ifdef SM83
bool sm83_init(read_fn _read, write_fn _write,
    pending_int_fn _pending_int, receive_int_fn _receive_int)
{
    memory_read = _read;
    memory_write = _write;
    pending_int = _pending_int;
    receive_int = _receive_int;

    instr_func = &nop;
    instr_cycle = 0;
    instr_complete = false;

    return true;
}
cpu_state sm83_get_state() {
    return state;
}
void sm83_set_state(cpu_state _state)
{
    _state.af_reg &= 0xFFF0;
    state = _state;
    set_ime = -1;
    cb_prefixed = false;
    halted = false;
    fetch_and_decode();
}
#endif