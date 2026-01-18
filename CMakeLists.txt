cmake_minimum_required(VERSION 3.10)
project(GB_EMU LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB CPU_SOURCES
    core/cpu/*.cpp
    core/cpu/mmu/*.cpp
    core/cpu/instructions/*.cpp
)

file(GLOB CART_SOURCES
    core/cartridge/*.cpp
    core/cartridge/*/*.cpp
)

add_executable(gb_emu
    main.cpp
    ${CPU_SOURCES}
    ${CART_SOURCES}
)

target_include_directories(gb_emu PRIVATE
    core
    core/cpu
    core/cpu/mmu
    core/cpu/instructions
    core/cartridge
    core/cartridge/IMBC
)
