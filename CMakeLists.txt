cmake_minimum_required(VERSION 3.20)

project(GB_EMU
    VERSION 0.1
    LANGUAGES CXX
)

# -------------------------
# C++ standard
# -------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------------
# Export compile_commands.json (VS Code IntelliSense)
# -------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -------------------------
# Sources
# -------------------------
set(SOURCES
    core/cartridge/cartridge.cpp
    core/cartridge/IMBC/type_cartridge/MBC1.cpp
    core/cartridge/IMBC/type_cartridge/RomOnly.cpp
    core/cpu/cpu.cpp
    core/cpu/mmu/mmu.cpp
)

# (si luego tienes main.cpp)
# list(APPEND SOURCES main.cpp)

# -------------------------
# Executable
# -------------------------
add_executable(gb-emu ${SOURCES})

# -------------------------
# Include directories
# -------------------------
# UN SOLO ROOT → core/
target_include_directories(gb-emu PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

# -------------------------
# Warnings (opcional pero recomendado)
# -------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gb-emu PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
