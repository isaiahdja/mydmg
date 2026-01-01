
# MyDMG

MyDMG is an emulator for the original Nintendo Game Boy (DMG), written in C.  
It attempts to implement an "M-cycle"-accurate CPU and "T-cycle"-accurate PPU (i.e. LCD display controller).

![Kirby's Dream Land gameplay](img/kirby.gif)

## Building

MyDMG uses Simple DirectMedia Layer (SDL3) and can be built with CMake:

    mkdir build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build

## Running

    ./mydmg [path to ROM file]

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
- Debug window to view registers and memory, with single-stepping capability
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

## Testing

- [SM83 SingleStepTests](https://github.com/SingleStepTests/sm83)
    - CPU logic is compiled as its own library for unit testing
    - Python script using ctypes runs the tests by loading the initial state from JSON, ticking the CPU by the requisite number of cycles, and comparing the final state
- [Mooneye Test Suite](https://github.com/Gekkio/mooneye-test-suite)
- [Blargg's Gameboy hardware test ROMs](https://github.com/retrio/gb-test-roms)
- [dmg-acid2](https://github.com/mattcurrie/dmg-acid2)

MyDMG has been built for the Windows Subsytem for Linux (WSL2) with GCC, and Windows 11 with MSVC.

## Resources

- [Pan Docs](https://gbdev.io/pandocs/About.html)
- [Game Boy: Complete Technical Reference](https://gekkio.fi/files/gb-docs/gbctr.pdf)
- [gbops opcode table](https://izik1.github.io/gbops/index.html)
- [Game Boy CPU Internals](https://gist.github.com/SonoSooS/c0055300670d678b5ae8433e20bea595)
- [Demystifying the GameBoy/SM83’s DAA Instruction](https://blog.ollien.com/posts/gb-daa/)
- [Game Boy carry and half-carry flags](https://gist.github.com/meganesu/9e228b6b587decc783aa9be34ae27841)
- [Rendering Internals (Pan Docs fork)](https://github.com/ISSOtm/pandocs/blob/rendering-internals/src/Rendering_Internals.md) 
- [Rewriting My Game Boy Emulator: The Pixel FIFO](https://jsgroth.dev/blog/posts/gb-rewrite-pixel-fifo/)

## Disclaimer

This project was created as a learning exercise, and is not primarily intended to serve as an end-user product.  
"Game Boy" is a registered trademark of Nintendo of America, Inc.