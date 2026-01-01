
#### MyDMG -- Game Boy emulator

MyDMG is a Game Boy (DMG) emulator written in C.  
It attempts to implement an "M-cycle"-accurate CPU and "T-cycle"-accurate PPU (i.e. LCD display controller).

##### Building

    mkdir build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build

##### Running

    ./mydmg [ROM PATH]

##### Default Controls

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

##### Features

- Supported MBCs (memory bank controllers):
    - No MBC
    - MBC 1
    - MBC 3
- Savegames
    - Any battery-backed RAM will be written to a file in the same directory and with the same name as the provided ROM file, plus a ".sav" extension. These files will be automatically detected when reloading the same ROM file.