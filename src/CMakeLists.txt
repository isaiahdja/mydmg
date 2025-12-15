cmake_minimum_required(VERSION 3.22)
project(gbemu)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(gbemu
    main.c
    mem.c
    cpu.c
    input.c
)

target_include_directories(gbemu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gbemu
    PRIVATE
        SDL3::SDL3
)