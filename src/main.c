#include "main.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include "system.h"

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
static bool init_palettes()
{
    bw_palette = SDL_CreatePalette(5);
    if (bw_palette == NULL)
        return false;
    SDL_Color bw_colors[5] = {
        color_from_hex(0xE0E0E0),
        color_from_hex(0xB0B0B0),
        color_from_hex(0x707070),
        color_from_hex(0x303030),
        color_from_hex(0xF0F0F0)
    };
    if (!SDL_SetPaletteColors(bw_palette, bw_colors,0, 5))
        return false;
    palettes[0] = bw_palette;

    olive_palette = SDL_CreatePalette(5);
    if (olive_palette == NULL)
        return false;
    SDL_Color olive_colors[5] = {
        color_from_hex(0x9CA142),
        color_from_hex(0x4C722B),
        color_from_hex(0x0C440C),
        color_from_hex(0x1F2F0F),
        color_from_hex(0xAAAF48)
    };
    if (!SDL_SetPaletteColors(olive_palette, olive_colors, 0, 5))
        return false;
    palettes[1] = olive_palette;

    return true;
}

const char *rom_path;
static bool running = true;

SDL_Mutex *frame_mux;
static SDL_Thread *system_thread;
static void loop_window(void);
static int loop_system(void* data);

int main(int argc, char *argv[])
{
    int code = 0;

    bool sdl_init = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS); 
    if (!sdl_init) goto failure;
    
    scale_factor = 4;
    window_width = GB_WIDTH * scale_factor;
    window_height = GB_HEIGHT * scale_factor;
    window = SDL_CreateWindow("MyDMG", window_width, window_height,
        SDL_WINDOW_KEYBOARD_GRABBED);
    if (window == NULL) goto failure;
    renderer = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
    if (renderer == NULL) goto failure;
    if (!SDL_SetRenderVSync(renderer, 1))
        SDL_Log("Could not initialize renderer with VSync");
    
    window_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8,
            SDL_TEXTUREACCESS_STREAMING, GB_WIDTH, GB_HEIGHT);
    if (window_tex == NULL) goto failure;
    if (!SDL_SetTextureScaleMode(window_tex, SDL_SCALEMODE_PIXELART)) goto failure;

    if (!init_palettes()) goto failure;
    active_palette = 0;
    if (!SDL_SetTexturePalette(window_tex, palettes[active_palette]))
        goto failure;

    frame_mux = SDL_CreateMutex();
    if (frame_mux == NULL) goto failure;

    if (argc >= 2)
        rom_path = strdup(argv[1]);
    else {
        while (true) {
            SDL_Event event;
            SDL_WaitEvent(&event);
            if (event.type == SDL_EVENT_DROP_FILE) {
                rom_path = strdup(event.drop.data);
                break;
            }
            else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                goto close;
        }
    }

    system_args sys_args = (system_args){ rom_path, frame_mux };
    if (!sys_init(sys_args))
        goto failure;
    system_thread = SDL_CreateThread(&loop_system, "MyDMG system", NULL);
    loop_window();
    SDL_WaitThread(system_thread, NULL);
    sys_deinit();
    
close:
    if (window != NULL) SDL_DestroyWindow(window);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window_tex != NULL) SDL_DestroyTexture(window_tex);
    for (int i = 0; i < NUM_PALETTES; i++) {
        if (palettes[i] != NULL) SDL_DestroyPalette(palettes[i]);
    }
    if (frame_mux != NULL) SDL_DestroyMutex(frame_mux);
    if (rom_path != NULL) free(rom_path);
    if (sdl_init) SDL_Quit();
    return code;
failure:
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), window);
    code = -1;
    goto close;
}

static void loop_window()
{
    /* Main loop. */
    while (running) {
        SDL_RenderClear(renderer);
        SDL_LockMutex(frame_mux);
        void *pixels;
        int pitch;
        SDL_LockTexture(window_tex, NULL, &pixels, &pitch);
        memcpy(pixels, sys_get_frame_buffer(), GB_WIDTH * GB_HEIGHT);
        SDL_UnlockTexture(window_tex);
        SDL_UnlockMutex(frame_mux);
        SDL_RenderTexture(renderer, window_tex, NULL, NULL);
        SDL_RenderPresent(renderer);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
                break;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode >= SDL_SCANCODE_1 &&
                    event.key.scancode <= SDL_SCANCODE_9) {
                    scale_factor = event.key.scancode - SDL_SCANCODE_1 + 1;
                    window_width = scale_factor * GB_WIDTH;
                    window_height = scale_factor * GB_HEIGHT;
                    SDL_SetWindowSize(window, window_width, window_height);
                }
                else if (event.key.scancode == SDL_SCANCODE_P) {
                    active_palette = (active_palette + 1) % NUM_PALETTES;
                    SDL_SetTexturePalette(window_tex, palettes[active_palette]);
                }
            }
        }
    }
}

static int loop_system(void *_)
{
    Uint64 counter_freq = SDL_GetPerformanceFrequency();
    Uint64 next_frame = SDL_GetPerformanceCounter();
    while (running) {
        next_frame += (Uint64)(target_secs_per_frame * counter_freq);

        sys_start_frame();
        for (int i = 0; i < M_CYCLES_PER_FRAME; i++)
            sys_tick();

        Uint64 now = SDL_GetPerformanceCounter();
        Sint64 delta = (Sint64)(next_frame - now);
        if (delta > 0) {
            SDL_DelayPrecise((double)delta / (double)counter_freq * 1e9);
        }
    }
    return 0;
}