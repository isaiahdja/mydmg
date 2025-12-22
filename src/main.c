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

static bool running = true;

static void loop(void);
static void run_frame(void);

int main(int argc, char *argv[])
{
    bool sdl_initialized = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (!sdl_initialized)
        goto failure;

    if (argc < 2) {
        SDL_Log("Usage: %s [ROM PATH]", argv[0]);
        SDL_SetError("Missing ROM path");
        goto failure;
    }

    scale_factor = 3;
    window_width = GB_WIDTH * scale_factor;
    window_height = GB_HEIGHT * scale_factor;
    /* TODO: SDL_CreateWindowAndRenderer (?) */
    window = SDL_CreateWindow(
        "MyDMG", window_width, window_height, 0);
    if (window == NULL)
        goto failure;

#ifdef DEBUG
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
    SDL_Log("Debug build");
#endif

    /* Initialize the cartridge before the system as the CPU initialization will
       attempt to read the first instruction from the cartridge. */
    if (!cart_init(argv[1]) || !sys_init())
        goto failure;
    loop();

    cart_deinit();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
failure:
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s", SDL_GetError());
    if (window != NULL)
        SDL_DestroyWindow(window);
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

        Uint64 end = SDL_GetPerformanceCounter();
        double secs_elapsed = (double)(end - start) / (double)counter_freq;
        double secs_delay = target_secs_per_frame - secs_elapsed;
        Uint64 nanos_delay = secs_delay * 1e9;
        if (secs_delay >= 0)
            SDL_DelayPrecise(nanos_delay);
        else
            SDL_LogCritical(
                SDL_LOG_CATEGORY_SYSTEM, "Frame took longer than target");
        
        /* TODO: Scale and display frame. */
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
        }
    }
}