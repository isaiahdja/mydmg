
# MyDMG

MyDMG is an emulator for the original Nintendo Game Boy (DMG), written in C. 
It attempts to implement an "M-cycle"-accurate CPU and "T-cycle"-accurate PPU (i.e. LCD display controller).

## Building

MyDMG uses Simple DirectMedia Layer (SDL3) and can be built with CMake:

    mkdir build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build

## Running

    ./mydmg [ROM PATH]

## Default controls

| Button | Key    |
| ------ | ------ |
| START  | RETURN |
| SELECT | RSHIFT |
| B      | Z      |
| A      | X      |
| UP     | UP     |
| DOWN   | DOWN   |
| LEFT   | LEFT   |
| RIGHT  | RIGHT  |

## Features

- Supported memory bank controllers (MBCs):
    - No MBC
    - MBC 1
    - MBC 3
- Save games
    - Battery-backed cartridge RAM is written to a .sav file
    - Save files are stored in the same directory and share the provided ROM’s filename
    - Save files are automatically loaded when reopening the same ROM

## Potential future improvements
- Further debugging and general accuracy improvements
- Audio support
- Additional MBC support
- Performance optimizations
- User customization options and other extra features, e.g.:
    - Configurable window scaling, controls, mono palette, etc.
    - Save states
- Refactoring to reduce copied / boilerplate code
- Serial (link-cable) data transfer support

## Game Boy (DMG) hardware overview

- "DMG-CPU" system on a chip (SoC) design
- ~4.19 MHz clock (T-cycles), or ~1.05 MHz clock (M-cycles)
- Sharp SM83 core (custom 8-bit CPU) with 5 hardware interrupt sources
- 160×144 pixel monochrome LCD with 2-bit color depth tile-based rendering
- 64 KiB address space (16-bit address bus, 8-bit data bus), supporting cartridge bank-switching
- Memory-mapped I/O
- Direct memory access (DMA) mechanism for transferring object attribute memory

## Disclaimer

This project was created as a learning exercise, and is not primarily intended to serve as a recreational tool.
"Game Boy" is a registered trademark of Nintendo of America, Inc.