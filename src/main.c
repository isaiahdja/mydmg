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

static bool running = true;

static void loop(void);
static void run_frame(void);

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

    scale_factor = 3;
    window_width = GB_WIDTH * scale_factor;
    window_height = GB_HEIGHT * scale_factor;
    SDL_CreateWindowAndRenderer("MyDMG", window_width, window_height, SDL_WINDOW_OPENGL,
        &window, &renderer);
    if (window == NULL || renderer == NULL)
        goto failure;
    if (!SDL_SetRenderVSync(renderer, 1))
        SDL_Log("Could not initialize renderer with VSync");
    
    window_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, GB_WIDTH, GB_HEIGHT);
    if (window_tex == NULL)
        goto failure;
    SDL_SetTextureScaleMode(window_tex, SDL_SCALEMODE_PIXELART);

#ifdef DEBUG
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
    SDL_Log("Debug build");
#endif

    if (!sys_init() || !cart_init(argv[1]))
        goto failure;
    loop();

    cart_deinit();
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(window_tex);
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

static void loop()
{
    /* Main loop. */
    Uint64 counter_freq = SDL_GetPerformanceFrequency();
    while (running) {
        Uint64 start = SDL_GetPerformanceCounter();

        run_frame();
        
        SDL_RenderClear(renderer);
        SDL_UpdateTexture(window_tex, NULL, sys_get_frame_buffer(), GB_WIDTH * 4);
        SDL_RenderTexture(renderer, window_tex, NULL, NULL);
        SDL_RenderPresent(renderer);

        Uint64 end = SDL_GetPerformanceCounter();
        double secs_elapsed = (double)(end - start) / (double)counter_freq;
        double secs_delay = target_secs_per_frame - secs_elapsed;
        if (secs_delay >= 0) {
            Uint64 nanos_delay = secs_delay * 1e9;
            SDL_DelayPrecise(nanos_delay);
        }
        else {
            //SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Frame took longer than target by %lf seconds", -secs_delay);
        }
    }
}

static void run_frame()
{
    sys_start_frame();
    for (int i = 0; i < M_CYCLES_PER_FRAME; i++)
        sys_tick();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_EQUALS) {
                    scale_factor++;
                    window_width = scale_factor * GB_WIDTH;
                    window_height = scale_factor * GB_HEIGHT;
                    SDL_SetWindowSize(window, window_width, window_height);
                }
                else if (event.key.scancode == SDL_SCANCODE_MINUS) {
                    if (scale_factor > 1)
                        scale_factor--;
                    else
                        break;
                    window_width = scale_factor * GB_WIDTH;
                    window_height = scale_factor * GB_HEIGHT;
                    SDL_SetWindowSize(window, window_width, window_height);
                }
                break;
        }
    }
}