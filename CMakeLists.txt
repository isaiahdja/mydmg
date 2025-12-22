cmake_minimum_required(VERSION 3.22)
project(mydmg)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(cpu_test SHARED
    src/cpu.c
)
target_include_directories(cpu_test PRIVATE src)
target_compile_definitions(cpu_test PRIVATE
    CPU_TEST
    DEBUG
)

set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_executable(mydmg
    src/main.c
    src/cart.c
    src/system.c
    src/bus.c
    src/timer.c
    src/cpu.c
    src/ppu.c
    src/interrupt.c
    src/input.c
    src/dma.c

    src/mbc/mbc0.c
)
target_include_directories(mydmg PRIVATE src)
target_link_libraries(mydmg PRIVATE SDL3::SDL3)
target_compile_definitions(mydmg PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)
target_compile_options(mydmg PRIVATE
    $<$<CONFIG:Debug>:-Wall -Wextra -Wpedantic>
)
