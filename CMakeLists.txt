cmake_minimum_required(VERSION 3.22)
project(mydmg)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_library(sm83 SHARED src/cpu.c)
    target_include_directories(sm83 PRIVATE src)
    target_compile_definitions(sm83 PRIVATE SM83 DEBUG)
endif()

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
)
target_include_directories(mydmg PRIVATE src)
target_link_libraries(mydmg PRIVATE SDL3::SDL3)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(mydmg PRIVATE DEBUG)
    target_compile_options(mydmg PRIVATE -Wall -Wextra -Wpedantic -Og)
else()
    target_compile_options(mydmg PRIVATE -O3)
endif()