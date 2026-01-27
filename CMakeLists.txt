cmake_minimum_required(VERSION 3.10)
project(GB-EMU)

# Estándar C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Rutas de cabeceras (.h)
include_directories(
    ${CMAKE_SOURCE_DIR}
    core
    core/cpu
    core/cpu/ppu
    core/cpu/mmu
    core/cartridge
    core/cartridge/IMBC
)

# 2. Archivos fuente del NÚCLEO (Compartidos entre Web y Desktop)
set(CORE_SOURCES
    core/cpu/cpu.cpp
    core/cpu/ppu/ppu.cpp
    core/cpu/mmu/mmu.cpp
    core/cartridge/cartridge.cpp
    core/cartridge/IMBC/type_cartridge/RomOnly.cpp
    core/cartridge/IMBC/type_cartridge/MBC1.cpp
)

# 3. Lógica de Detección (Desktop vs Web)
if(EMSCRIPTEN)
    # --- CONFIGURACIÓN PARA WEB (WASM) ---
    message(STATUS "Compilando para WebAssembly (Emscripten)...")

    # Usamos emc_main.cpp como punto de entrada
    add_executable(gb-emu emc_main.cpp ${CORE_SOURCES})

    # Configuración de salida (.html)
    set_target_properties(gb-emu PROPERTIES SUFFIX ".html")
    
    # Definimos la ruta absoluta a la carpeta de roms para evitar errores
    set(ROMS_PATH "${CMAKE_SOURCE_DIR}/roms")

    # Flags de enlace (Linker flags)
    # CORRECCIÓN: Usamos "SHELL:" para agrupar el flag y su valor como un solo argumento.
    # Esto evita que CMake o Emscripten confundan "NO_EXIT_RUNTIME=0" con un nombre de archivo.
    target_link_options(gb-emu PRIVATE
        "SHELL:--preload-file ${ROMS_PATH}@/roms" 
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"     
        "SHELL:-s NO_EXIT_RUNTIME=0"
    )

else()
    # --- CONFIGURACIÓN PARA DESKTOP (LINUX) ---
    message(STATUS "Compilando para Desktop (Nativo)...")

    # Usamos emc_main.cpp (el main híbrido)
    add_executable(gb-emu emc_main.cpp ${CORE_SOURCES})

endif()