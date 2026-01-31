cmake_minimum_required(VERSION 3.10)
project(GB-EMU)

# Forzamos C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Seguridad: Si intentas compilar esto con g++ normal, dará error y te avisará
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Este proyecto ahora es SOLO WEB. Usa 'emcmake cmake ..")
endif()

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

# 2. Archivos fuente
set(CORE_SOURCES
    core/cpu/cpu.cpp
    core/cpu/mmu/mmu.cpp
    core/cpu/ppu/ppu.cpp
    core/cartridge/cartridge.cpp
    core/cartridge/IMBC/type_cartridge/RomOnly.cpp
    core/cartridge/IMBC/type_cartridge/MBC1.cpp
)

# 3. Configuración del Ejecutable
message(STATUS "Configurando build para WebAssembly...")

add_executable(gb-emu emc_main.cpp ${CORE_SOURCES})

# Generar un .html en lugar de un .js suelto
set_target_properties(gb-emu PROPERTIES SUFFIX ".html")

# 4. Flags de Emscripten (Linker)
target_link_options(gb-emu PRIVATE
        "SHELL:-s USE_SDL=0"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s NO_EXIT_RUNTIME=1"
        "SHELL:-s EXPORTED_FUNCTIONS=['_main','_get_video_buffer']"
        "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPU8']" 
        
        # --- AQUÍ ESTÁ EL CAMBIO ---
        # Sintaxis: Ruta_en_tu_PC @ Ruta_virtual_en_el_navegador
        "SHELL:--preload-file /home/developer/projects/roms_backup@/roms"
        
        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/src/index.html"
)