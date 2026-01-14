#include "apu.h"
#include "bus.h"
#include <stdlib.h>
#include <SDL3/SDL.h>
#include "system.h"

#define NR52_RW_MASK 0x80
static byte nr50_reg;
static int get_right_vol(void) {
    return get_bits(nr50_reg, 2, 0);
}
static int get_left_vol(void) {
    return get_bits(nr50_reg, 6, 4);
}

static byte nr51_reg;
static bit get_ch1_right(void) {
    return get_bit(nr51_reg, 0);
}
static bit get_ch1_left(void) {
    return get_bit(nr51_reg, 4);
}
static bit get_ch2_right(void) {
    return get_bit(nr51_reg, 1);
}
static bit get_ch2_left(void) {
    return get_bit(nr51_reg, 5);
}

static byte nr52_reg;
static bit get_audio_enable(void) {
    return get_bit(nr52_reg, 7);
}

static int div_apu;

static uint8_t pulse_waveforms[4][8] = {
    { 1, 1, 1, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 1, 1, 0, 0, 0 },
    { 1, 0, 0, 0, 0, 0, 0, 1 }
};

/* */

#define NR10_RW_MASK 0x7F
#define NR11_RW_MASK 0xC0
#define NR14_RW_MASK 0x40
static byte nr10_reg;
static bit ch1_period_dir() {
    return get_bit(nr10_reg, 3);
}
static int ch1_period_step() {
    return get_bits(nr10_reg, 2, 0);
}

static byte nr11_reg;
static int get_ch1_duty_cycle(void) {
    return get_bits(nr11_reg, 7, 6);
}

static byte nr12_reg;
static byte nr13_reg;
static byte nr14_reg;
static bit ch1_length_enable() {
    return get_bit(nr14_reg, 6);
}

static int ch1_period_pace;

static int ch1_length, latched_ch1_length;

static int ch1_vol;
static bit ch1_envelope_dir;
static int ch1_envelope_pace;

static int ch1_period;
static int ch1_dot_divider;
static int ch1_period_divider;
static int ch1_duty_step;
static bool ch1_dac_enabled(void) {
    return (nr12_reg & 0xF8) != 0;
}
static bool get_ch1_on(void) {
    if (!ch1_dac_enabled())
        nr52_reg = set_bit(nr52_reg, 0, 0);
    return get_bit(nr52_reg, 0) == 1;
}
static void set_ch1_on(bit b) {
    nr52_reg = set_bit(nr52_reg, 0, b);
}

/* */

#define NR21_RW_MASK 0xC0
#define NR24_RW_MASK 0x40
static byte nr21_reg;
static int get_ch2_duty_cycle(void) {
    return get_bits(nr21_reg, 7, 6);
}

static byte nr22_reg;
static byte nr23_reg;
static byte nr24_reg;
static bit ch2_length_enable() {
    return get_bit(nr24_reg, 6);
}

static int ch2_length, latched_ch2_length;

static int ch2_vol;
static bit ch2_envelope_dir;
static int ch2_envelope_pace;

static int ch2_period;
static int ch2_dot_divider;
static int ch2_period_divider;
static int ch2_duty_step;
static bool ch2_dac_enabled(void) {
    return (nr22_reg & 0xF8) != 0;
}
static bool get_ch2_on(void) {
    if (!ch2_dac_enabled())
        nr52_reg = set_bit(nr52_reg, 1, 0);
    return get_bit(nr52_reg, 1) == 1;
}
static void set_ch2_on(bit b) {
    nr52_reg = set_bit(nr52_reg, 1, b);
}

/* */

#define GB_SAMPLE_RATE (1 << 22)
#define HOST_SAMPLE_RATE 44100
#define STEREO_CHANNELS 2

static float gb_samples[((GB_SAMPLE_RATE / HOST_SAMPLE_RATE) + 1) * STEREO_CHANNELS];
static int gb_samples_head = 0;
static float gb_sample_acc = 0;

#define HOST_SAMPLES_BUFFER 1024 * STEREO_CHANNELS
static float host_samples[HOST_SAMPLES_BUFFER];
static int host_samples_head = 0;
static SDL_AudioSpec spec;
static SDL_AudioStream *stream;

bool apu_init()
{
    /* DMG boot handoff state. */
    nr10_reg = 0x80;
    nr11_reg = 0xBF;
    nr12_reg = 0xF3;
    nr13_reg = 0xFF;
    nr14_reg = 0xBF;

    nr21_reg = 0x3F; /* Different from CH1 ? */
    nr22_reg = 0x00; /* Different from CH1 ? */
    nr23_reg = 0xFF;
    nr24_reg = 0xBF;

    nr50_reg = 0x77;
    nr51_reg = 0xF3;
    nr52_reg = 0xF1;

    div_apu = 0;

    spec = (SDL_AudioSpec){ SDL_AUDIO_F32, 2, HOST_SAMPLE_RATE };
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &spec, NULL, NULL);
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
}

static void generate_gb_sample() {
    /* DAC. */

    uint8_t ch1_out = 0;
    if (get_ch1_on() == 1)
        ch1_out = pulse_waveforms[get_ch1_duty_cycle()][ch1_duty_step] * ch1_vol;
    float ch1_analog = 0;
    if (ch1_dac_enabled())
        ch1_analog = ((float)ch1_out / 7.5) - 1.0;

    uint8_t ch2_out = 0;
    if (get_ch2_on() == 1)
        ch2_out = pulse_waveforms[get_ch2_duty_cycle()][ch2_duty_step] * ch2_vol;
    float ch2_analog = 0;
    if (ch2_dac_enabled())
        ch2_analog = ((float)ch2_out / 7.5) - 1.0;

    /* Mixing. */
    float left = 0;
    if (get_ch1_left() == 1)
        left += ch1_analog;
    if (get_ch2_left() == 1)
        left += ch2_analog;

    float right = 0;
    if (get_ch1_right() == 1)
        right += ch1_analog;
    if (get_ch2_right() == 1)
        right += ch2_analog;

    /* Amplification. */
    left  /= (4.0 + (8.0 - ((float)get_left_vol()  + 1.0)));
    right /= (4.0 + (8.0 - ((float)get_right_vol() + 1.0)));

    /* Push Game Boy 4 MiHz samples. */
    gb_samples[gb_samples_head] = left;
    gb_samples[gb_samples_head + 1] = right;
    gb_samples_head += 2;

    gb_sample_acc += (float)HOST_SAMPLE_RATE / (float)GB_SAMPLE_RATE;
    if (gb_sample_acc >= 1.0) {
        gb_sample_acc -= 1.0;

        /* Downsampling to host 44.1 KHz samples (compute averages). */
        float host_left = 0;
        for (int i = 0; i < gb_samples_head; i += 2)
            host_left += gb_samples[i];
        host_left /= (float)(gb_samples_head / 2);
    
        float host_right = 0;
        for (int i = 1; i < gb_samples_head; i += 2)
            host_right += gb_samples[i];
        host_right /= (float)(gb_samples_head / 2);

        gb_samples_head = 0;

        host_samples[host_samples_head] = host_left;
        host_samples[host_samples_head + 1] = host_right;
        host_samples_head += 2;
        if (host_samples_head >= HOST_SAMPLES_BUFFER) {
            /* Push our batch of 1024 samples to the audio stream. */
            SDL_PutAudioStreamData(stream,
                host_samples,HOST_SAMPLES_BUFFER * sizeof(float));
            host_samples_head = 0;
        }
    }
}

static void apu_dot()
{
    if (++ch1_dot_divider % 4 == 0) { /* 1 MiHz. */
        ch1_dot_divider = 0;

        if (++ch1_period_divider == 2048) {
            ch1_period_divider = ch1_period;
            
            if (++ch1_duty_step % 8 == 0) {
                ch1_duty_step = 0;
            }
        }
    }

    if (++ch2_dot_divider % 4 == 0) { /* 1 MiHz. */
        ch2_dot_divider = 0;

        if (++ch2_period_divider == 2048) {
            ch2_period_divider = ch2_period;
            
            if (++ch2_duty_step % 8 == 0) {
                ch2_duty_step = 0;
            }
        }
    }

    generate_gb_sample();
}

void apu_tick()
{
    if (get_audio_enable() == 0)
        return;

    for (int _ = 0; _ < T_M_RATIO; _++)
        apu_dot();
}

static uint8_t envelope_sweep_ticks = 0;
void incr_div_apu(void) /* 512 Hz. */
{
    if (++div_apu == 8)
        div_apu = 0;

    if (div_apu % 8 == 0) { /* 64 Hz. */
        if (++envelope_sweep_ticks == 7)
            envelope_sweep_ticks = 0;
        
        if (ch1_envelope_pace != 0 && envelope_sweep_ticks % ch1_envelope_pace == 0) {
            if (ch1_envelope_dir == 0) {
                if (--ch1_vol < 0x0) ch1_vol = 0;
            }
            else {
                if (++ch1_vol > 0xF) ch1_vol = 0xF;
            }
        }
        
        if (ch2_envelope_pace != 0 && envelope_sweep_ticks % ch2_envelope_pace == 0) {
            if (ch2_envelope_dir == 0) {
                if (--ch2_vol < 0x0) ch2_vol = 0;
            }
            else {
                if (++ch2_vol > 0xF) ch2_vol = 0xF;
            }
        }
    }
    if (div_apu % 4 == 0) { /* 128 Hz.*/
        /* TODO: Tick CH1 period sweep */
    }
    if (div_apu % 2 == 0) { /* 256 Hz. */
        if (ch1_length_enable() == 1) {
            if (++latched_ch1_length == 64) set_ch1_on(0);
        }

        if (ch2_length_enable() == 1) {
            if (++latched_ch2_length == 64) set_ch2_on(0);
        }
    }
}

byte apu_ch1_read(uint16_t addr)
{
    switch (addr) {
        case NR10_REG: return nr10_reg;
        case NR11_REG: return nr11_reg;
        case NR12_REG: return nr12_reg;
        case NR13_REG: return nr13_reg;
        case NR14_REG: return nr14_reg;
    }
}
void apu_ch1_write(uint16_t addr, byte val)
{
    /* TODO: Make channel length timer writeable with audio disabled. */

    if (get_audio_enable() == 0)
        return;

    switch (addr) {
        case NR10_REG:
            nr10_reg = overlay_masked(nr10_reg, val, NR10_RW_MASK);
            break;
        case NR11_REG:
            nr11_reg = overlay_masked(nr11_reg, val, NR11_RW_MASK);
            ch1_length = get_bits(val, 5, 0);
            break;
        case NR12_REG:
            nr12_reg = val;
            break;
        case NR13_REG:
            ch1_period = (ch1_period & 0xF00) | val;
            break;
        case NR14_REG:
            nr14_reg = overlay_masked(nr14_reg, val, NR14_RW_MASK);
            ch1_period = (ch1_period & 0x0FF) | ((val & 0x7) << 8);

            if (get_bit(val, 7) == 1 && ch1_dac_enabled()) {
                set_ch1_on(1);

                if (latched_ch1_length == 64)
                    latched_ch1_length = ch1_length;

                ch1_vol = get_bits(nr12_reg, 7, 4);
                ch1_envelope_dir = get_bit(nr12_reg, 3);
                ch1_envelope_pace = get_bits(nr12_reg, 2, 0);
                
                ch1_period_divider = ch1_period;
                ch1_period_pace = get_bits(nr10_reg, 6, 4);
            }
            break;
    }
}

byte apu_ch2_read(uint16_t addr)
{
    switch (addr) {
        case NR21_REG: return nr21_reg;
        case NR22_REG: return nr22_reg;
        case NR23_REG: return nr23_reg;
        case NR24_REG: return nr24_reg;
    }
}
void apu_ch2_write(uint16_t addr, byte val)
{
    /* TODO: Make channel length timer writeable with audio disabled. */

    if (get_audio_enable() == 0)
        return;

    switch (addr) {
        case NR21_REG:
            nr21_reg = overlay_masked(nr21_reg, val, NR21_RW_MASK);
            ch2_length = get_bits(val, 5, 0);
            break;
        case NR22_REG:
            nr22_reg = val;
            break;
        case NR23_REG:
            ch2_period = (ch2_period & 0xF00) | val;
            break;
        case NR24_REG:
            nr24_reg = overlay_masked(nr24_reg, val, NR24_RW_MASK);
            ch2_period = (ch2_period & 0x0FF) | ((val & 0x7) << 8);

            if (get_bit(val, 7) == 1 && ch2_dac_enabled()) {
                set_ch2_on(1);

                if (latched_ch2_length == 64)
                    latched_ch2_length = ch2_length;

                ch2_vol = get_bits(nr22_reg, 7, 4);
                ch2_envelope_dir = get_bit(nr22_reg, 3);
                ch2_envelope_pace = get_bits(nr22_reg, 2, 0);
                
                ch2_period_divider = ch1_period;
            }
            break;
    }
}

byte apu_ctrl_read(uint16_t addr)
{
    if (!ch1_dac_enabled())
        nr52_reg = set_bit(nr52_reg, 0, 0);
    if (!ch2_dac_enabled())
        nr52_reg = set_bit(nr52_reg, 1, 0);

    switch (addr) {
        case NR50_REG: return nr50_reg;
        case NR51_REG: return nr51_reg;
        case NR52_REG: return nr52_reg;
    }
}
void apu_ctrl_write(uint16_t addr, byte val)
{
    if (addr == NR52_REG) {
        nr52_reg = overlay_masked(nr52_reg, val, NR52_RW_MASK);
        if (get_audio_enable() == 0) {
            /* TODO: Clear registers and samples ? */
        }
        return;
    }

    if (get_audio_enable() == 0)
        return;

    switch (addr) {
        case NR50_REG:
            nr50_reg = val;
            break;
        case NR51_REG:
            nr51_reg = val;
            break;
    }
}