#include "input.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include "mem.h"

struct button {
    SDL_Scancode scancode;
    bool pressed;
};

#define NUM_BUTTONS 8

/* NOTE:
The order here mirrors the button order from most-significant to
least-significant bit in the lower nibble of the joypad register.
Changing the order will break get_joyp_nibble, as it relies on this. */
static struct button buttons[NUM_BUTTONS] = {
    /* Action buttons */
    { SDL_SCANCODE_U, false }, /* Start  */
    { SDL_SCANCODE_I, false }, /* Select */
    { SDL_SCANCODE_J, false }, /*   B    */
    { SDL_SCANCODE_K, false }, /*   A    */
    /* Direction buttons */
    { SDL_SCANCODE_S, false }, /*  Down  */
    { SDL_SCANCODE_W, false }, /*   Up   */
    { SDL_SCANCODE_A, false }, /*  Left  */
    { SDL_SCANCODE_D, false }  /* Right  */
};

/* Update stored input state. */
void poll_inputs()
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    for (int i = 0; i < NUM_BUTTONS; i++)
        buttons[i].pressed = keys[buttons[i].scancode];
}

/* Get lower nibble of the joypad register according to the most recent
input state.
action = true - reads the action buttons (SsBA).
direction = true - reads the D-pad. */
uint8_t get_joyp_nibble(bool action, bool direction)
{
    uint8_t nibble = 0x0F;
    if (action)
        for (int i = 0; i < 4; i++)
            nibble &= ~(buttons[i].pressed << (3 - i));
    if (direction)
        for (int i = 0; i < 4; i++)
            nibble &= ~(buttons[i + 4].pressed << (3 - i));
    return nibble;
}

/* TODO: Refactor joypad nibble handling in relation to mem.c functions (?) */