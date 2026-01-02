#include "main.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include "system.h"
#include "cart.h"

#include <stdlib.h>
#include <stdio.h>

static double target_secs_per_frame
    = (double)T_CYCLES_PER_FRAME / (double)T_CYCLES_PER_SEC;

static int scale_factor;
static int window_width;
static int window_height;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *window_tex;

#define NUM_PALETTES 2
SDL_Palette *palettes[NUM_PALETTES];
int active_palette;
SDL_Palette *bw_palette, *olive_palette;
static inline SDL_Color color_from_hex(uint32_t hex) {
    return (SDL_Color){
        (hex >> 16) & 0xFF,
        (hex >>  8) & 0xFF,
        (hex >>  0) & 0xFF,
        0xFF
    };
}

static bool running = true;

static SDL_Thread *system_thread;
static SDL_Semaphore *frame_sema;
static void loop_window(void);
static int loop_system(void* data);

int main(int argc, char *argv[])
{
    bool sdl_initialized = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (!sdl_initialized)
        goto failure;

    if (argc < 2) {
        SDL_Log("Usage: %s [path to ROM file]", argv[0]);
        SDL_SetError("Missing ROM path");
        goto failure;
    }

    scale_factor = 4;
    window_width = GB_WIDTH * scale_factor;
    window_height = GB_HEIGHT * scale_factor;
    SDL_CreateWindowAndRenderer("MyDMG", window_width, window_height, SDL_WINDOW_OPENGL,
        &window, &renderer);
    if (window == NULL || renderer == NULL)
        goto failure;
    if (!SDL_SetRenderVSync(renderer, 1))
        SDL_Log("Could not initialize renderer with VSync");
    
    window_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8,
            SDL_TEXTUREACCESS_STREAMING, GB_WIDTH, GB_HEIGHT);
    if (window_tex == NULL)
        goto failure;
    SDL_SetTextureScaleMode(window_tex, SDL_SCALEMODE_PIXELART);

    bw_palette = SDL_CreatePalette(5);
    SDL_Color bw_colors[5] = {
        color_from_hex(0xE0E0E0),
        color_from_hex(0xB0B0B0),
        color_from_hex(0x707070),
        color_from_hex(0x303030),
        color_from_hex(0xF0F0F0)
    };
    SDL_SetPaletteColors(bw_palette, bw_colors,0, 5);
    palettes[0] = bw_palette;

    olive_palette = SDL_CreatePalette(5);
    SDL_Color olive_colors[5] = {
        color_from_hex(0x9CA142),
        color_from_hex(0x4C722B),
        color_from_hex(0x0C440C),
        color_from_hex(0x1F2F0F),
        color_from_hex(0xAAAF48)
    };
    SDL_SetPaletteColors(olive_palette, olive_colors,0, 5);
    palettes[1] = olive_palette;

    active_palette = 0;
    SDL_SetTexturePalette(window_tex, palettes[active_palette]);

#ifdef DEBUG
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
    SDL_Log("Debug build");
#endif

    if (!sys_init() || !cart_init(argv[1]))
        goto failure;
    frame_sema = SDL_CreateSemaphore(0);
    system_thread = SDL_CreateThread(&loop_system, "System", NULL);
    loop_window();
    SDL_WaitThread(system_thread, NULL);

    cart_deinit();
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(window_tex);
    for (int i = 0; i < NUM_PALETTES; i++)
        SDL_DestroyPalette(palettes[i]);
    SDL_Quit();
    return 0;
failure:
    /* TODO: Show message box (?) */
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s", SDL_GetError());
    if (window != NULL)
        SDL_DestroyWindow(window);
    if (renderer != NULL)
        SDL_DestroyRenderer(renderer);
    if (window_tex != NULL)
        SDL_DestroyTexture(window_tex);
    if (sdl_initialized)
        SDL_Quit();
    return -1;
}

static void loop_window()
{
    /* Main loop. */
    while (running) {
        SDL_WaitSemaphore(frame_sema);
        SDL_RenderClear(renderer);
        SDL_UpdateTexture(window_tex, NULL, sys_get_frame_buffer(), GB_WIDTH);
        SDL_RenderTexture(renderer, window_tex, NULL, NULL);
        SDL_RenderPresent(renderer);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.scancode >= SDL_SCANCODE_1 && event.key.scancode <= SDL_SCANCODE_9) {
                        scale_factor = event.key.scancode - SDL_SCANCODE_1 + 1;
                        window_width = scale_factor * GB_WIDTH;
                        window_height = scale_factor * GB_HEIGHT;
                        SDL_SetWindowSize(window, window_width, window_height);
                    }
                    if (event.key.scancode == SDL_SCANCODE_P) {
                        active_palette = (active_palette + 1) % NUM_PALETTES;
                        SDL_SetTexturePalette(window_tex, palettes[active_palette]);
                    }
                    break;
            }
        }
    }
}

static int loop_system(void* data)
{
    Uint64 counter_freq = SDL_GetPerformanceFrequency();
    Uint64 next_frame = SDL_GetPerformanceCounter();
    while (running) {
        next_frame += (Uint64)(target_secs_per_frame * counter_freq);

        sys_start_frame();
        for (int i = 0; i < M_CYCLES_PER_FRAME; i++)
            sys_tick();
        SDL_SignalSemaphore(frame_sema);

        Uint64 now = SDL_GetPerformanceCounter();
        Sint64 delta = (Sint64)(next_frame - now);
        if (delta > 0) {
            SDL_DelayPrecise((double)delta / (double)counter_freq * 1e9);
        }
    }
    return 0;
}