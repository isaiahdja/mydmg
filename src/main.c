#include "main.h"
#include <SDL3/SDL_main.h>
#include "mem.h"
#include "cpu.h"
#include "input.h"

const int gb_width = 160;
const int gb_height = 144;
const int scale_factor = 3;
const int window_width = gb_width * scale_factor;
const int window_height = gb_height * scale_factor;

SDL_IOStream *rom_io;

static const int t_cycles_per_sec = (1 << 22);
static const int t_cycles_per_frame = 70244;
static const int t_m_ratio = 4;
static const int m_cycles_per_frame = t_cycles_per_frame / t_m_ratio;
static const double target_spf
    = (double)t_cycles_per_frame / (double)t_cycles_per_sec;

static bool running = true;
static void update(void);

int main(int argc, char *argv[])
{
    /* Initalize SDL, open ROM file. */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        goto failure;

    if (argc < 2) {
        SDL_SetError("Usage: ./gbemu [ROM PATH]");
        goto failure;
    }

    rom_io = SDL_IOFromFile(argv[1], "rb");
    if (rom_io == NULL)
        goto failure;

    SDL_Window *window = SDL_CreateWindow(
        "gbemu", window_width, window_height, 0);
    if (window == NULL)
        goto failure;
#if DEBUG
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
#endif

    /* Initialize subsystems. */
    if (!(
        mem_init()
        && cpu_init()
    ))
        goto failure;

    /* Main loop. */
    Uint64 counter_freq = SDL_GetPerformanceFrequency();
    while (running) {
        Uint64 start = SDL_GetPerformanceCounter();

        update();

        Uint64 end = SDL_GetPerformanceCounter();
        double secs_elapsed = (double)(end - start) / (double)counter_freq;
        double secs_delay = target_spf - secs_elapsed;
        Uint64 nanos_delay = secs_delay * 1e9;
        if (secs_delay >= 0)
            SDL_DelayPrecise(nanos_delay);
        else
            SDL_LogCritical(
                SDL_LOG_CATEGORY_SYSTEM, "Frame took longer than target");

        /* TODO: Scale and display frame. */
    }

    /* Deinitialize. */
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
failure:
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s", SDL_GetError());
    SDL_Quit();
    return -1;
}

/* Tick all subsystems until a full frame is completed. */
static void update()
{
    poll_inputs();
    for (int i = 0; i < m_cycles_per_frame; i++) {
        /* TODO: Tick subsystems. */
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                running = false;
                break;
        }
    }
}