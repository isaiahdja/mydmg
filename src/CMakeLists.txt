cmake_minimum_required(VERSION 3.22)
project(gbemu)

add_executable(gbemu
    hello.c
)

target_include_directories(gbemu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gbemu
    PRIVATE
        SDL3::SDL3
)