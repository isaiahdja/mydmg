#include "input.h"
#include <SDL3/SDL.h>
#include <stdio.h>

struct button {
    SDL_Scancode scancode;
    bool pressed;
};

#define NUM_BUTTONS 8

static struct button buttons[NUM_BUTTONS] = {
    /* Action buttons */
    { SDL_SCANCODE_U,   false },    /* Start  */
    { SDL_SCANCODE_I,   false },    /* Select */
    { SDL_SCANCODE_J,   false },    /*   B    */
    { SDL_SCANCODE_K,   false },    /*   A    */
    /* Direction buttons */
    { SDL_SCANCODE_S,   false },    /*  Down  */
    { SDL_SCANCODE_W,   false },    /*   Up   */
    { SDL_SCANCODE_A,   false },    /*  Left  */
    { SDL_SCANCODE_D,   false }     /* Right  */
};

static void print_byte(uint8_t byte)
{
    for (int i = 7; i >= 0; i--)
        printf("%d", (byte >> i) & 1);
    printf("\n");
}

void poll_inputs()
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    for (int i = 0; i < NUM_BUTTONS; i++)
        buttons[i].pressed = keys[buttons[i].scancode];
}

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