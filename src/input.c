#include "input.h"
#include "bus.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include "interrupt.h"

/* P1 / JOYP input. */

#define JOYP_RW_MASK 0x30

static byte joyp_reg;

typedef struct {
    bool pressed;

    SDL_Scancode scancode;
} button;

typedef enum {
    START, SELECT, B, A,
    DOWN, UP, LEFT, RIGHT
} button_type;

#define NUM_BUTTONS 8

static button buttons[NUM_BUTTONS] = {
    [START]  = { false, SDL_SCANCODE_U },
    [SELECT] = { false, SDL_SCANCODE_I },
    [B]      = { false, SDL_SCANCODE_J },
    [A]      = { false, SDL_SCANCODE_K },
    [DOWN]   = { false, SDL_SCANCODE_S },
    [UP]     = { false, SDL_SCANCODE_W },
    [LEFT]   = { false, SDL_SCANCODE_A },
    [RIGHT]  = { false, SDL_SCANCODE_D }
};

static void load_joyp_nibble();

bool input_init()
{
    /* DMG boot handoff state. */
    joyp_reg = 0xCF;

    return true;
}

void input_poll_and_load()
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    for (int i = 0; i < NUM_BUTTONS; i++)
        buttons[i].pressed = keys[buttons[i].scancode];
    load_joyp_nibble();
}

static void load_joyp_nibble()
{
    /* For the button bits, 0 = pressed, 1 = not pressed (active-low).
       For the selection bits, 0 = selected, 1 = unselected. */

    bool action = !get_bit(joyp_reg, 5);
    bool direction = !get_bit(joyp_reg, 4);

    bool bit_0_pressed =
        (buttons[A].pressed     && action) ||
        (buttons[RIGHT].pressed && direction);
    bool bit_1_pressed =
        (buttons[B].pressed    && action) ||
        (buttons[LEFT].pressed && direction);
    bool bit_2_pressed =
        (buttons[SELECT].pressed && action) ||
        (buttons[UP].pressed     && direction);
    bool bit_3_pressed =
        (buttons[START].pressed && action) ||
        (buttons[DOWN].pressed  && direction);

    byte prev_nibble = joyp_reg & 0x0F;
    byte next_nibble = (
        (!bit_0_pressed << 0) |
        (!bit_1_pressed << 1) |
        (!bit_2_pressed << 2) |
        (!bit_3_pressed << 3));

    if (detect_falling_edge(prev_nibble, next_nibble))
        request_interrupt(ITR_JOYPAD);

    joyp_reg = overlay_masked(joyp_reg, next_nibble, 0x0F);
}

byte input_joyp_read() {
    return joyp_reg;
}
void input_joyp_write(byte val) {
    joyp_reg = overlay_masked(joyp_reg, val, JOYP_RW_MASK);
    load_joyp_nibble();
}